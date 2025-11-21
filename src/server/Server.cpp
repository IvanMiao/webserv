#include "Server.hpp"

namespace wsv
{

Server::Server(int port):
_port(port)
{
	_server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (_server_fd < 0)
		throw std::runtime_error("Error: Cannot create socket");
	
	_address.sin_family = AF_INET;
	_address.sin_addr.s_addr = INADDR_ANY; // 监听所有端口，需要修改
	_address.sin_port = htons(_port);

	if (bind(_server_fd, (struct sockaddr *)&_address, sizeof(_address)) < 0)
		throw std::runtime_error("Error: Cannot bind to port");
}

Server::~Server()
{
	if (_server_fd >= 0)
		close(_server_fd);
}

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

		std::cout << "New connection accepted" << std::endl;

		char buffer[1024] = {0};
		read(client_socket, buffer, 1024); // TODO: read Request
		std::cout << "--- Request ---" << std::endl << buffer << "---------------" << std::endl;

		// TODO: build Reaponse and send it 
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 48\r\n\r\n<html><body><h1>Hello, Browser!</h1></body></html>";
		send(client_socket, response.c_str(), response.length(), 0);

		close(client_socket);
	}
}


} // namespace wsv
