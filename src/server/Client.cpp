#include "Client.hpp"

namespace wsv {

Client::Client():
client_fd(-1),
state(CLIENT_READING_REQUEST),
configs(NULL)
{ }

Client::Client(int fd, sockaddr_in addr, const std::vector<ServerConfig>* configs):
client_fd(fd),
address(addr),
state(CLIENT_READING_REQUEST),
configs(configs)
{ }

Client::~Client()
{ }

} // namespace wsv
