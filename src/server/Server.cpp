#include "Server.hpp"

#include <fstream>
#include <sstream>
#include <sys/wait.h>

namespace wsv
{

// Initialize static member
volatile sig_atomic_t Server::_shutdown_requested = 0;

Server::Server(ConfigParser& config):
_config(config), _epoll_fd(-1)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, Server::signalHandler);
	// Default behavior: children remain zombies until waitpid is called
	signal(SIGCHLD, SIG_DFL);
	_shutdown_requested = 0;
}

Server::~Server()
{
	cleanup();
}

void Server::cleanup()
{
	Logger::info("Starting server cleanup...");

	// Close all client connections
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		Logger::info("Closing client FD {}", it->first);
		if (it->first >= 0)
		{
			epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, it->first, NULL);
			close(it->first);
		}
	}
	_clients.clear();

	// Close all listening sockets
	for (std::map<int, ServerConfig>::iterator it = _listen_fds.begin(); it != _listen_fds.end(); ++it)
	{
		Logger::info("Closing listening socket FD {}", it->first);
		if (it->first >= 0)
		{
			epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, it->first, NULL);
			close(it->first);
		}
	}
	_listen_fds.clear();

	// Close epoll file descriptor
	if (_epoll_fd >= 0)
	{
		Logger::info("Closing epoll FD {}", _epoll_fd);
		close(_epoll_fd);
		_epoll_fd = -1;
	}

	Logger::info("Server cleanup completed");
}

void Server::signalHandler(int signum)
{
	if (signum == SIGINT)
		_shutdown_requested = 1;
}

/**
 * @brief CORE. Server start and main loop
*/
void Server::start()
{
	_init_listening_sockets();
	_init_epoll();

	struct epoll_event events[MAX_EVENTS];

	Logger::info("Server started. Press Ctrl+C to stop.");

	while (!_shutdown_requested)
	{
		// Check for client timeouts periodically
		_check_client_timeouts();

		int nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, EPOLL_TIMEOUT);
		if (nfds < 0)
		{
			if (errno == EINTR)
			{
				if (_shutdown_requested)
					break;
				continue;
			}
			Logger::error("epoll_wait error");
			break;
		}

		for (int i = 0; i < nfds; i++)
		{
			int current_fd = events[i].data.fd;
			uint32_t events_flag = events[i].events;

			if (_listen_fds.find(current_fd) != _listen_fds.end())
				_handle_new_connection(current_fd);
			else if (_cgi_fd_map.find(current_fd) != _cgi_fd_map.end())
				_handle_cgi_data(current_fd, events_flag);
			else
			{
				// Read first, then handle Write
				if (events_flag & EPOLLIN)
					_handle_client_data(current_fd);
				// _handle_client_data could close connection, so we should check if the fd is still there
				if (_clients.find(current_fd) != _clients.end() && (events_flag & EPOLLOUT))
					_handle_client_write(current_fd);
			}
		}
	}

	Logger::info("Shutdown signal received. Cleaning up...");
}


// _init_listening_sockets + _create_listening_socket
// create listing sockets and bind them to serverconfigs -> _listen_fds
void Server::_init_listening_sockets()
{
	const std::vector<ServerConfig>& configs = _config.getServers();

	for (size_t i = 0; i < configs.size(); ++i)
	{
		const ServerConfig& conf = configs[i];
		int fd = _create_listening_socket(conf.host, conf.listen_port);
		_listen_fds[fd] = conf;
		Logger::info("Server is listening on {}:{} ...", conf.host, conf.listen_port);
	}
}

int Server::_create_listening_socket(const std::string& host, int port)
{
	int fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		throw std::runtime_error("Cannot create socket");
	
	int opt = SOCKET_REUSE_OPT;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		close(fd);
		throw std::runtime_error("Cannot set socket options.");
	}

	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1)
	{
		close(fd);
		throw std::runtime_error("Cannot get socket flags");
	}
	if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
	{
		close(fd);
		throw std::runtime_error("Cannot set socket to non-blocking");
	}

	if (fcntl(fd, F_SETFD, FD_CLOEXEC) == -1)
	{
		close(fd);
		throw std::runtime_error("Cannot set socket to FD_CLOEXEC");
	}

	struct addrinfo hints = {};
	struct addrinfo *res;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	std::string port_str = StringUtils::toString(port);
	int status = getaddrinfo(host.c_str(), port_str.c_str(), &hints, &res);
	if (status != 0)
	{
		close(fd);
		throw std::runtime_error("getaddrinfo failed for " + host + ": " + gai_strerror(status));
	}

	if (bind(fd, res->ai_addr, res->ai_addrlen) < 0)
	{
		freeaddrinfo(res);
		close(fd);
		throw std::runtime_error("Cannot bind to " + host + ":" + StringUtils::toString(port));
	}
	freeaddrinfo(res);
	
	if (listen(fd, LISTEN_BACKLOG) < 0)
	{
		close(fd);
		throw std::runtime_error("Cannot listen on socket");
	}

	return fd;
}

void Server::_init_epoll()
{
	_epoll_fd = epoll_create(42);
	if (_epoll_fd < 0)
		throw std::runtime_error("epoll_create failed");

	if (fcntl(_epoll_fd, F_SETFD, FD_CLOEXEC) == -1)
	{
		close(_epoll_fd);
		throw std::runtime_error("Cannot set epoll fd to FD_CLOEXEC");
	}

	for (std::map<int, ServerConfig>::iterator it = _listen_fds.begin(); it != _listen_fds.end(); ++it)
	{
		_add_to_epoll(it->first, EPOLLIN);
	}
}

void Server::_add_to_epoll(int fd, uint32_t events)
{
	struct epoll_event event;
	event.events = events;
	event.data.fd = fd;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0)
		throw std::runtime_error("epoll_ctl add failed");
}

void Server::_modify_epoll(int fd, uint32_t events)
{
	struct epoll_event event;
	event.events = events;
	event.data.fd = fd;

	if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0)
		throw std::runtime_error("epoll_ctl mod failed");
}

void Server::_remove_from_epoll(int fd)
{
	if (epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, fd, NULL) < 0)
	{
		// ENOENT = FD was not in epoll (benign if we just want to ensure removal)
		// EBADF = FD invalid or closed (benign if closed elsewhere)
		if (errno == ENOENT || errno == EBADF)
			Logger::debug("epoll_ctl del skipped (benign): fd {} not in epoll or invalid", fd);
		else
			Logger::error("epoll_ctl del failed for fd {}", fd);
	}
}

// create Client
void Server::_handle_new_connection(int listen_fd)
{
	sockaddr_in client_addr;
	socklen_t addrlen = sizeof(client_addr);
	int client_fd = accept(listen_fd, (struct sockaddr *)&client_addr, &addrlen);

	if (client_fd < 0)
	{
		Logger::error("Failed to accept connection");
		return;
	}

	int flags = fcntl(client_fd, F_GETFL, 0);
	fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);
	fcntl(client_fd, F_SETFD, FD_CLOEXEC);

	// Get the ServerConfig for this listening port
	const ServerConfig* config = &_listen_fds[listen_fd];
	Client client = Client(client_fd, client_addr, config);
	_clients.insert(std::make_pair(client_fd, client));

	_add_to_epoll(client_fd, EPOLLIN);
	Logger::info("New connection accepted on fd {}. Client socket fd: {}", listen_fd, client_fd);
}

// Client - Read
void Server::_handle_client_data(int client_fd)
{
	char buffer[READ_BUFFER_SIZE];
	ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
	Client& client = _clients[client_fd];

	if (bytes_read > 0)
	{
		// Update client activity timestamp
		client.updateActivity();

		client.request_buffer.append(buffer, bytes_read);
		client.request.parse(buffer, bytes_read);

		if (client.request.hasError())
		{
			Logger::error("Bad Request from client FD {}", client_fd);
			std::string response = HttpResponse::createErrorResponse(400).serialize();
			client.response_buffer = response;
			client.keep_alive = false; // Close connection on error
			client.state = CLIENT_WRITING_RESPONSE;
			_modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
			return;
		}

		// HTTP end detection
		if (client.request.isComplete())
		{
			Logger::info("----- Full Request from client FD {} -----", client_fd);
			
			// Increment request count
			client.requests_count++;

			// Determine if connection should be kept alive
			client.keep_alive = _should_keep_alive(client.request);
			if (client.requests_count >= KEEP_ALIVE_MAX_REQUESTS)
			{
				Logger::info("Client FD {} reached max requests limit", client_fd);
				client.keep_alive = false;
			}
			
			// Handle request
			_process_request(client_fd);

			// If state became WRITING_RESPONSE, enable write event
			// If state is CGI_PROCESSING, _process_request disabled events already
			if (client.state == CLIENT_WRITING_RESPONSE)
			{
				_modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
			}
		}
	}
	else if (bytes_read == 0)
	{
		Logger::info("Client {} disconnected.", client_fd);
		_close_client(client_fd);
	}
	else
	{
		Logger::error("Read error on FD {}", client_fd);
		_close_client(client_fd);
	}
}

// Client - Write
void Server::_handle_client_write(int client_fd)
{
	Client& client = _clients[client_fd];
	std::string& buffer = client.response_buffer;

	// Only write to client when explicitly in the WRITING_RESPONSE state
	// This prevents writing raw CGI stdout (which is temporarily stored in
	// client.response_buffer while a CGI is running) before the CGI output is
	// parsed and converted into a full HTTP response.
	if (client.state != CLIENT_WRITING_RESPONSE)
		return;

	if (buffer.empty())
		return;

	ssize_t bytes_sent = send(client_fd, buffer.c_str(), buffer.length(), 0);
	if (bytes_sent < 0)
	{
		Logger::error("Send error on FD {}", client_fd);
		_close_client(client_fd);
		return;
	}

	buffer.erase(0, bytes_sent);

	if (buffer.empty())
	{
		Logger::info("##### Response sent fully to FD {} #####\n", client_fd);

		// Update activity timestamp
		client.updateActivity();

		// Keep-Alive: reset client state for next request
		if (client.keep_alive)
		{
			Logger::info("Keep-alive: waiting for next request on FD {}", client_fd);
			_modify_epoll(client_fd, EPOLLIN);
			client.request.reset();
			client.request_buffer.clear();
			client.response_buffer.clear();
			client.state = CLIENT_READING_REQUEST;
		}
		else
		{
			Logger::info("Closing connection to FD {} (no keep-alive)", client_fd);
			_close_client(client_fd);
		}
	}
}

/*
	Process request using RequestHandler
*/
void Server::_process_request(int client_fd)
{
	Logger::info("Request received, preparing to send response...");
	Client& client = _clients[client_fd];
	const ServerConfig* config = client.config;

	if (!config)
	{
		Logger::error("No server config found for client FD {}", client_fd);
		client.response_buffer = HttpResponse::createErrorResponse(500).serialize();
		client.state = CLIENT_WRITING_RESPONSE;
		return;
	}

	// Create RequestHandler and process the request
	RequestHandler handler(*config);
	
	// Handles CGI start internally. Checks client.state for async changes.
	HttpResponse response = handler.handleRequest(client);
	
	if (client.state == CLIENT_CGI_PROCESSING)
	{
		Logger::info("Async CGI started for client FD {}", client_fd);
		
		// Register CGI pipes to epoll
		if (client.cgi_input_fd != -1)
		{
			_add_to_epoll(client.cgi_input_fd, EPOLLOUT);
			_cgi_fd_map[client.cgi_input_fd] = client_fd;
		}
		if (client.cgi_output_fd != -1)
		{
			_add_to_epoll(client.cgi_output_fd, EPOLLIN);
			_cgi_fd_map[client.cgi_output_fd] = client_fd;
		}

		// Disable client socket events while waiting for CGI
		// We don't want to read more requests or write anything yet
		_modify_epoll(client_fd, 0);
		return;
	}

	// Normal synchronous response
	// Set Connection header based on keep-alive status
	if (client.keep_alive)
		response.setHeader("Connection", "keep-alive");
	else
		response.setHeader("Connection", "close");
	
	Logger::info("Response built - Status: {}, Request: {} {}",
				response.getStatus(), client.request.getMethod(), client.request.getPath());
	
	client.response_buffer = response.serialize();
	client.state = CLIENT_WRITING_RESPONSE;
}

/*
	Check if connection should be kept alive based on request headers
*/
bool Server::_should_keep_alive(const HttpRequest& request) const
{
	std::string connection = StringUtils::toLower(request.getHeader("Connection"));
	std::string version = request.getVersion();
	
	// HTTP/1.1 defaults to keep-alive unless "Connection: close" is specified
	if (version == "HTTP/1.1")
	{
		if (connection == "close")
			return false;
		return true;
	}
	// HTTP/1.0 defaults to close unless "Connection: keep-alive" is specified
	else if (version == "HTTP/1.0")
	{
		if (connection == "keep-alive")
			return true;
		return false;
	}
	
	return false;
}

/*
	Check for client timeouts and close idle connections
*/
void Server::_check_client_timeouts()
{
	std::vector<int> to_close;
	
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
	{
		Client& client = it->second;
		long idle_time = client.getIdleTime();

		// CGI timeout handling - parent process enforced
		if (client.state == CLIENT_CGI_PROCESSING)
		{
			if (idle_time > CGI_TIMEOUT)
			{
				Logger::error("CGI timeout for client FD {} after {} seconds", it->first, idle_time);
				
				// Forcibly kill the CGI child process
				if (client.cgi_handler && client.cgi_handler->getChildPid() > 0)
				{
					kill(client.cgi_handler->getChildPid(), SIGKILL);
					waitpid(client.cgi_handler->getChildPid(), NULL, WNOHANG);
				}
				
				// Clean up CGI pipe FDs
				if (client.cgi_input_fd != -1)
				{
					_remove_from_epoll(client.cgi_input_fd);
					_cgi_fd_map.erase(client.cgi_input_fd);
					close(client.cgi_input_fd);
					client.cgi_input_fd = -1;
					if (client.cgi_handler)
						client.cgi_handler->markStdinClosed();  // Prevent double-close in destructor
				}
				if (client.cgi_output_fd != -1)
				{
					_remove_from_epoll(client.cgi_output_fd);
					_cgi_fd_map.erase(client.cgi_output_fd);
					close(client.cgi_output_fd);
					client.cgi_output_fd = -1;
					if (client.cgi_handler)
						client.cgi_handler->markStdoutClosed();  // Prevent double-close in destructor
				}
				
				// Clean up CGI handler
				delete client.cgi_handler;
				client.cgi_handler = NULL;
				
				// Send 504 Gateway Timeout response
				client.response_buffer = HttpResponse::createErrorResponse(504).serialize();
				client.state = CLIENT_WRITING_RESPONSE;
				client.keep_alive = false; // Close connection after timeout
				_modify_epoll(it->first, EPOLLIN | EPOLLOUT);
			}
			// Don't check regular idle timeout while CGI is processing [NOTE: that's for test]
			continue;
		}
		
		// Regular client idle timeout
		long timeout = client.keep_alive ? KEEP_ALIVE_TIMEOUT : CLIENT_IDLE_TIMEOUT;
		
		if (idle_time > timeout)
		{
			Logger::info("Client FD {} timed out after {} seconds (timeout: {} seconds)",
						it->first, idle_time, timeout);
			to_close.push_back(it->first);
		}
	}
	
	// Close timed-out clients
	for (size_t i = 0; i < to_close.size(); ++i)
	{
		_close_client(to_close[i]);
	}
}

/*
	Close a client connection and clean up resources
*/
void Server::_close_client(int client_fd)
{
	// Clean up CGI resources if active
	if (_clients.find(client_fd) != _clients.end())
	{
		Client& client = _clients[client_fd];
		if (client.cgi_input_fd != -1)
		{
			_remove_from_epoll(client.cgi_input_fd);
			_cgi_fd_map.erase(client.cgi_input_fd);
			close(client.cgi_input_fd);
			client.cgi_input_fd = -1;
			if (client.cgi_handler)
				client.cgi_handler->markStdinClosed();
		}
		if (client.cgi_output_fd != -1)
		{
			_remove_from_epoll(client.cgi_output_fd);
			_cgi_fd_map.erase(client.cgi_output_fd);
			close(client.cgi_output_fd);
			client.cgi_output_fd = -1;
			if (client.cgi_handler)
				client.cgi_handler->markStdoutClosed();
		}
		// CgiHandler is deleted in Client destructor, which is called when erasing from _clients map
	}

	epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
	close(client_fd);
	_clients.erase(client_fd);
}

} // namespace wsv
