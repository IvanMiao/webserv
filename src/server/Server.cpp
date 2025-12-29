#include "Server.hpp"

#include <fstream>
#include <sstream>

namespace wsv
{

// Initialize static member
volatile sig_atomic_t Server::_shutdown_requested = 0;

Server::Server(ConfigParser& config):
_config(config), _epoll_fd(-1)
{
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, Server::signalHandler);
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
	{
		_shutdown_requested = 1;
	}
}

/*
	Server start and main loop
*/
void Server::start()
{
	_init_listening_sockets();
	_init_epoll();

	struct epoll_event events[MAX_EVENTS];

	Logger::info("Server started. Press Ctrl+C to stop.");

	while (!_shutdown_requested)
	{
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
	// [TODO] Should we add cleanup() again in the start function?
	// cleanup();
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
		client.request_buffer.append(buffer, bytes_read);
		client.request.parse(buffer, bytes_read);

		if (client.request.hasError())
		{
			Logger::error("Bad Request from client FD {}", client_fd);
			std::string response = HttpResponse::createErrorResponse(400).serialize();
			client.response_buffer = response;
			_modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
			return;
		}

		// HTTP end detection
		if (client.request.isComplete())
		{
			Logger::info("----- Full Request from client FD {} -----\n{}",
						client_fd, client.request_buffer);
			
			// 1. Handle request, generate response 
			std::string response = _process_request(client_fd, client.request);
			// 2. Save the response to Client's response_buffer
			client.response_buffer = response;
			// 3. modify epoll to care about EPOLLOUT(write)
			// [TODO]: short connection vs Keep-Alive
			_modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
		}
	}
	else if (bytes_read == 0)
	{
		Logger::info("Client {} disconnected.", client_fd);
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
		close(client_fd);
		_clients.erase(client_fd);
	}
	else
	{
		Logger::error("Read error on FD {}", client_fd);
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
		close(client_fd);
		_clients.erase(client_fd);
	}
}

// Client - Write
void Server::_handle_client_write(int client_fd)
{
	Client& client = _clients[client_fd];
	std::string& buffer = client.response_buffer;

	if (buffer.empty())
		return;

	ssize_t bytes_sent = send(client_fd, buffer.c_str(), buffer.length(), 0);
	if (bytes_sent < 0)
	{
		Logger::error("Send error on FD {}", client_fd);
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
		close(client_fd);
		_clients.erase(client_fd);
		return;
	}

	buffer.erase(0, bytes_sent);

	if (buffer.empty())
	{
		Logger::info("Response sent fully to FD {}", client_fd);

		// [TODO]: short-connection or Kepp-Alive
		// _modify_epoll(client_fd, EPOLLIN);
		// client.request.reset();

		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
		close(client_fd);
		_clients.erase(client_fd);
	}
}

/*
	Process request using RequestHandler
*/
std::string Server::_process_request(int client_fd, const HttpRequest& request)
{
	Logger::info("Request received, preparing to send response...");
	Client& client = _clients[client_fd];
	const ServerConfig* config = client.config;

	if (!config)
	{
		Logger::error("No server config found for client FD {}", client_fd);
		return HttpResponse::createErrorResponse(500).serialize();
	}

	// Create RequestHandler and process the request
	RequestHandler handler(*config);
	HttpResponse response = handler.handleRequest(request);
	
	Logger::info("Response built - Status: {}, Request: {} {}", 
	             response.getStatus(), request.getMethod(), request.getPath());
	
	return response.serialize();
}

} // namespace wsv
