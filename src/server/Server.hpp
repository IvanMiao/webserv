#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>

#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>


#define MAX_EVENTS	1024
#define TIMEOUT		1024

namespace wsv
{

class Server
{
private:
	int			_port;
	int			_server_fd;
	sockaddr_in	_address;

public:
	Server( int port );
	~Server();

	void start();
};


} // namespace wsv

#endif
