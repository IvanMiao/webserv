#include "Server.hpp"
#include <fstream>
#include <sstream>

namespace wsv
{

/* Constructor */
Server::Server(int port):
_port(port)
{
	_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_server_fd < 0)
		throw std::runtime_error("Error: Cannot create socket");
	
	_address.sin_family = AF_INET;  // IPv4
	_address.sin_addr.s_addr = INADDR_ANY;  // 监听所有端口，需要修改
	_address.sin_port = htons(_port);

	if (bind(_server_fd, (struct sockaddr *)&_address, sizeof(_address)) < 0)
		throw std::runtime_error("Error: Cannot bind to port");
}


/* Destructor */
Server::~Server()
{
	if (_server_fd >= 0)
		close(_server_fd);
}


/*
TEMP: make response
TODO: the first param -> class type HttpRequest
*/
static std::string _get_response(std::string request_data)
{
	// Simple Routing
	if (request_data.find("GET /echo") != std::string::npos)
	{
		// Endpoint: /echo
		// Returns the request headers as the response body
		std::stringstream response_ss;
		response_ss << "HTTP/1.1 200 OK\r\n";
		response_ss << "Content-Type: text/plain\r\n";
		response_ss << "Content-Length: " << request_data.length() << "\r\n";
		response_ss << "Server: Webserv/1.0\r\n";
		response_ss << "\r\n";
		response_ss << request_data;

		std::string response = response_ss.str();
		return response;
	}
	else
	{
		// Endpoint: / (or anything else)
		// Returns index.html
		std::ifstream index_file("./www/index.html");
		std::ifstream error_file("./www/error.html");  // Need to fix: error should be handled alone
		std::stringstream buffer_ss;
		if (index_file)
			buffer_ss << index_file.rdbuf();  // Use rdbuf to read all the contents to buffer_ss
		else
			buffer_ss << error_file.rdbuf();
		
		std::string html_content = buffer_ss.str();
		
		std::stringstream response_ss;
		response_ss << "HTTP/1.1 200 OK\r\n";
		response_ss << "Content-Type: text/html\r\n";
		response_ss << "Content-Length: " << html_content.length() << "\r\n";
		response_ss << "Server: Webserv/1.0\r\n";
		response_ss << "\r\n";
		response_ss << html_content;

		std::string response = response_ss.str();
		return response;
		}
}


/* Server start and main loop */
void Server::start()
{
	if (listen(_server_fd, 10) < 0)
		throw std::runtime_error("Error: Cannot listen to socket");

	std::cout << "Server is listening on port " << _port << "..." << std::endl;

	while (true)
	{
		int client_socket;
		socklen_t addrlen = sizeof(_address);

		// create client_socket to store the Request
		client_socket = accept(_server_fd, (struct sockaddr *)&_address, &addrlen);
		if (client_socket < 0)
		{
			std::cerr << "Error: Failed to accept connection" << std::endl;
			continue;
		}

		std::cout << "New connection accepted" << std::endl; // [DEBUG]

		char buffer[1024] = {0};
		read(client_socket, buffer, 1024);  // Use epoll to be non-blocking
		std::cout << "--- Request ---\n" << buffer << "---------------" << std::endl; // [DEBUG]

		std::string request_data = buffer;  // TODO: Call HttpRequest to parse the buffer data
		std::string response = _get_response(request_data);
		send(client_socket, response.c_str(), response.length(), 0);

		close(client_socket);
	}
}


} // namespace wsv
