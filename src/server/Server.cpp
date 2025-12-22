#include "Server.hpp"
#include "../utils/Logger.hpp"

#include <fstream>
#include <sstream>

namespace wsv
{

Server::Server(int port):
_port(port), _server_fd(-1), _epoll_fd(-1)
{
	signal(SIGPIPE, SIG_IGN);

	_init_server_socket();
	_init_epoll();
}

Server::~Server()
{
	if (_server_fd >= 0)
		close(_server_fd);
	if (_epoll_fd >= 0)
		close(_epoll_fd);
	for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
		close(it->first);
}

void Server::_init_server_socket()
{
	_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_server_fd < 0)
		throw std::runtime_error("Cannot create socket");
	
	int opt = 1;
	if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
		throw std::runtime_error("Cannot set socket options.");

	int flags = fcntl(_server_fd, F_GETFL, 0);
	if (flags == -1)
		throw std::runtime_error("Cannot get socket flags");
	if (fcntl(_server_fd, F_SETFL, flags | O_NONBLOCK) == -1)
		throw std::runtime_error("Cannot set socket to non-blocking");

	_address.sin_family = AF_INET;  // IPv4
	_address.sin_addr.s_addr = INADDR_ANY;  // [TODO]: modify(?)
	_address.sin_port = htons(_port);

	if (bind(_server_fd, (struct sockaddr *)&_address, sizeof(_address)) < 0)
		throw std::runtime_error("Cannot bind to port");
	
	if (listen(_server_fd, 10) < 0)
		throw std::runtime_error("Cannot listen to socket");

	Logger::info("Server is listening on port {} ...", _port);
}

void Server::_init_epoll()
{
	// 1. create epoll fd (epoll_create)
	_epoll_fd = epoll_create(1024);
	if (_epoll_fd < 0)
		throw std::runtime_error("epoll_create failed");

	// 2. listen readable event(epoll_ctl)
	_add_to_epoll(_server_fd, EPOLLIN);
}

/* Server start and main loop */
void Server::start()
{
	struct epoll_event events[MAX_EVENTS];

	while (true)
	{
		int nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, -1);
		if (nfds < 0)
		{
			if (errno == EINTR)
				continue;
			Logger::error("epoll_wait error");
			break;
		}

		for (int i = 0; i < nfds; i++)
		{
			int current_fd = events[i].data.fd;
			uint32_t events_flag = events[i].events;

			if (current_fd == _server_fd)
				_handle_new_connection();
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


/*
TEMP: make response
TODO: the first param -> class type HttpRequest
*/
std::string Server::_process_request(const std::string& request_data)
{
	// Simple Routing
	if (request_data.find("GET /echo") != std::string::npos)
	{
		// Endpoint: /echo
		std::stringstream response_ss;
		response_ss << "HTTP/1.1 200 OK\r\n" << "Content-Type: text/plain\r\n"
					<< "Content-Length: " << request_data.length() << "\r\n"
					<< "Server: Webserv/1.0\r\n" << "\r\n"
					<< request_data;

		std::string response = response_ss.str();
		return response;
	}
	else
	{
		// Endpoint: / (or anything else), Returns index.html
		std::ifstream index_file(ROOT_DIR);
		std::ifstream error_file("./www/errors/error.html");
		std::stringstream buffer_ss;
		if (index_file)
			buffer_ss << index_file.rdbuf();  // Use rdbuf to read all the contents to buffer_ss
		else
			buffer_ss << error_file.rdbuf();
		
		std::string html_content = buffer_ss.str();
		
		std::stringstream response_ss;
		response_ss << "HTTP/1.1 200 OK\r\n" << "Content-Type: text/html\r\n"
					<< "Content-Length: " << html_content.length() << "\r\n"
					<< "Server: Webserv/1.0\r\n" << "\r\n"
					<< html_content;

		std::string response = response_ss.str();
		return response;
	}
}

void Server::_handle_new_connection()
{
	sockaddr_in client_addr;
	socklen_t addrlen = sizeof(_address);
	int client_fd = accept(_server_fd, (struct sockaddr *)&client_addr, &addrlen);

	if (client_fd < 0)
	{
		Logger::error("Faild to accept connection");
		return;
	}

	int flags = fcntl(client_fd, F_GETFL, 0);
	fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

	_clients.insert(std::make_pair(client_fd, Client(client_fd, client_addr)));

	_add_to_epoll(client_fd, EPOLLIN);
	Logger::info("New connection accepted. Client socket fd: {}", client_fd);
}

void Server::_handle_client_data(int client_fd)
{
	char buffer[4096];
	ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

	if (bytes_read > 0)
	{
		_clients[client_fd].request_buffer.append(buffer, bytes_read);

		// HTTP end detection
		if (_clients[client_fd].request_buffer.find("\r\n\r\n") != std::string::npos)
		{
            Logger::info("----- Full Request from client FD {} -----\n{}", client_fd, _clients[client_fd].request_buffer);
			
			// 1. Handle request, generate response 
			std::string response = _process_request(_clients[client_fd].request_buffer);
			// 2. Save the response to Client's response_buffer
			_clients[client_fd].response_buffer = response;
			// 3. modify epoll to care about EPOLLOUT(write), TODO: short connection vs Keep-Alive
			_modify_epoll(client_fd, static_cast<EPOLL_EVENTS>(EPOLLIN | EPOLLOUT));

			Logger::info("Request received, preparing to send response...");
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

void Server::_handle_client_write(int client_fd)
{
	Client& client = _clients[client_fd];
	std::string& buffer = client.response_buffer;

	if (buffer.empty()) return;

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
		epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
		close(client_fd);
		_clients.erase(client_fd);
	}
}


} // namespace wsv
