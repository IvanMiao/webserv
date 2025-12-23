#include "Client.hpp"

namespace wsv {

Client::Client():
client_fd(-1), configs(NULL)
{ }

Client::Client(int fd, sockaddr_in addr, const std::vector<ServerConfig>* configs):
client_fd(fd), address(addr), configs(configs)
{ }

Client::~Client()
{ }

} // namespace wsv
