#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <netinet/in.h>
#include "ConfigParser.hpp"
#include "http/HttpRequest.hpp"

namespace wsv {

enum ClientState
{
	CLIENT_READING_REQUEST,
	CLIENT_PROCESSING,
	CLIENT_WRITING_RESPONSE
};

class Client
{
public:
	int			client_fd;
	sockaddr_in	address;
	std::string	request_buffer;
	std::string response_buffer;

	HttpRequest request;

	ClientState	state;
	const std::vector<ServerConfig>* configs; // Associated server configs for the port

public:
	Client();
	Client(int fd, sockaddr_in addr, const std::vector<ServerConfig>* configs);
	~Client(); // fd is closed by Server
};

} // namespace wsv



#endif
