#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>

#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>
#include <map>

#include "Client.hpp"

#define MAX_EVENTS	1024
#define	ROOT_DIR	"./www/index.html"

namespace wsv
{

class Server
{
private:
	int			_port;
	int			_server_fd;
	int			_epoll_fd;
	sockaddr_in	_address;

	std::map<int, Client> _clients;

private:
	// helper functions
	void	_init_server_socket();
	void	_init_epoll();
	void	_add_to_epoll(int fd, EPOLL_EVENTS events);
	void	_modify_epoll(int fd, EPOLL_EVENTS events);
	void	_handle_new_connection();
	void	_handle_client_data(int client_fd);
	void	_handle_client_write(int client_fd);

	std::string	_process_request(const std::string& request_data);

public:
	Server( int port );
	~Server();

	void start();
};


} // namespace wsv

#endif
