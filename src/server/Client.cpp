#include "Client.hpp"

namespace wsv {

Client::Client():
client_fd(-1),
state(CLIENT_READING_REQUEST),
config(NULL),
keep_alive(true),
requests_count(0)
{
	last_activity = std::time(NULL);
}

Client::Client(int fd, sockaddr_in addr, const ServerConfig* config):
client_fd(fd),
address(addr),
state(CLIENT_READING_REQUEST),
config(config),
keep_alive(true),
requests_count(0)
{
	last_activity = std::time(NULL);
}

Client::~Client()
{ }

void Client::updateActivity()
{
	last_activity = std::time(NULL);
}

long Client::getIdleTime() const
{
	std::time_t now = std::time(NULL);
	return static_cast<long>(std::difftime(now, last_activity));
}

} // namespace wsv
