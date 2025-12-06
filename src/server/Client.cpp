#include "Client.hpp"

Client::Client():
client_fd(-1)
{ }

Client::Client(int fd, sockaddr_in addr):
client_fd(fd), address(addr)
{ }

Client::~Client()
{ }
