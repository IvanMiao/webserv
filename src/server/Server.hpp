#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>

namespace wsv
{

class Server
{
private:
	int			_port;
	int			_server_fd;
	sockaddr_in	_address;

public:
	Server(int port);
	~Server();

	void start();
};


} // namespace wsv

#endif
