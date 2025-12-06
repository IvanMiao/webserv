#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <netinet/in.h>

class Client
{
public:
	int			client_fd;
	sockaddr_in	address;
	std::string	request_buffer;

public:
	Client();
	Client(int fd, sockaddr_in addr);
	~Client(); // fd is closed by Server
};



#endif
