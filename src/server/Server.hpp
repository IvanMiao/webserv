#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <signal.h>
#include <netdb.h>

#include <cstring>
#include <string>
#include <iostream>
#include <stdexcept>
#include <map>
#include <vector>

#include "Client.hpp"
#include "../config/ConfigParser.hpp"
#include "../utils/StringUtils.hpp"

#define MAX_EVENTS	1024
#define	ROOT_DIR	"./www/index.html"

namespace wsv
{

class Server
{
private:
	ConfigParser&	_config;
	int				_epoll_fd;

	// Map of listening socket FDs to their associated server configurations
	std::map<int, std::vector<ServerConfig> > _listen_fds;
	std::map<int, Client> _clients;

public:
	Server( ConfigParser& config );
	~Server();

	void start();

private:
	// Forbidden copy constructor
	Server(const Server&);
	Server& operator=(const Server&);

	// helper functions
	void	_init_listening_sockets();
	void	_init_epoll();
	void	_add_to_epoll(int fd, uint32_t events);
	void	_modify_epoll(int fd, uint32_t events);
	void	_handle_new_connection(int listen_fd);
	void	_handle_client_data(int client_fd);
	void	_handle_client_write(int client_fd);

	std::string	_process_request(int client_fd, const std::string& request_data);
};


} // namespace wsv

#endif
