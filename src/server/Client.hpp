#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <netinet/in.h>
#include "../config/ConfigParser.hpp"

namespace wsv {

class Client
{
public:
	int			client_fd;
	sockaddr_in	address;
	std::string	request_buffer;
	std::string response_buffer;
	const std::vector<ServerConfig>* configs; // Associated server configs for the port

public:
	Client();
	Client(int fd, sockaddr_in addr, const std::vector<ServerConfig>* configs);
	~Client(); // fd is closed by Server
};

} // namespace wsv



#endif
