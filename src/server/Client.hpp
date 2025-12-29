#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <vector>
#include <netinet/in.h>
#include <ctime>
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
	const ServerConfig* config; // Associated server config for this connection

	// Keep-alive and timeout management
	std::time_t last_activity;	// Last activity timestamp (seconds since epoch)
	bool keep_alive;			// Whether connection should be kept alive
	int requests_count;			// Number of requests handled on this connection

public:
	Client();
	Client(int fd, sockaddr_in addr, const ServerConfig* config);
	~Client(); // fd is closed by Server

	// Update the last activity timestamp to current time
	void updateActivity();

	// Get elapsed time since last activity in seconds
	long getIdleTime() const;
};

} // namespace wsv



#endif
