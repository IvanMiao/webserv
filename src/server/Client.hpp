#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <netinet/in.h>
#include <sys/socket.h> // 包含 sockaddr_in 所需的头文件

class Client
{
	public:
		int         client_fd;
		sockaddr_in address;
		std::string request_buffer;
		std::string response_buffer;

	public:
		Client();
		Client(int fd, const sockaddr_in& addr); 
		~Client(); 
};

#endif
