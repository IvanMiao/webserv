#include "Client.hpp"

namespace wsv {

Client::Client():
client_fd(-1),
state(CLIENT_READING_REQUEST),
config(NULL)
{ }

Client::Client(int fd, sockaddr_in addr, const ServerConfig* config):
client_fd(fd),
address(addr),
state(CLIENT_READING_REQUEST),
config(config)
{ }

Client::~Client()
{ }

} // namespace wsv
