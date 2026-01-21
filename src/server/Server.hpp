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

#include <string>
#include <iostream>
#include <stdexcept>
#include <map>
#include <vector>

#include "Client.hpp"
#include "config/ConfigParser.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "router/RequestHandler.hpp"
#include "utils/StringUtils.hpp"
#include "utils/Logger.hpp"

// Epoll configuration
#define MAX_EVENTS			1024

// Socket configuration
#define LISTEN_BACKLOG		128
#define SOCKET_REUSE_OPT	1

// Buffer sizes
#define READ_BUFFER_SIZE	4096
#define WRITE_BUFFER_SIZE	8192

// Timeout values
#define EPOLL_TIMEOUT			1000   // epoll wait timeout: 1000ms = 1 second
#define CLIENT_IDLE_TIMEOUT		30     // 30 seconds idle timeout
#define KEEP_ALIVE_TIMEOUT		5      // 5 seconds for keep-alive connections
#define KEEP_ALIVE_MAX_REQUESTS	100    // Max requests per connection
#define CGI_TIMEOUT				30     // 30 seconds CGI execution timeout

namespace wsv
{

class Server
{
private:
	ConfigParser&	_config;
	int				_epoll_fd;

	// Map of listening socket FDs to their associated server configuration
	std::map<int, ServerConfig> _listen_fds;
	std::map<int, Client> _clients;

	// CGI Pipe FD -> Client FD
	std::map<int, int> _cgi_fd_map;

	// Shutdown flag
	static volatile sig_atomic_t _shutdown_requested;

public:
	Server( ConfigParser& config );
	~Server();

	void start();
	void cleanup();
	static void signalHandler(int signum);

private:
	// Forbidden copy constructor
	Server(const Server&);
	Server& operator=(const Server&);

protected:
	// helper functions
	void	_init_listening_sockets();
	void	_init_epoll();
	int		_create_listening_socket(const std::string& host, int port);

	void	_add_to_epoll(int fd, uint32_t events);
	void	_modify_epoll(int fd, uint32_t events);
	void	_remove_from_epoll(int fd);

	void	_handle_new_connection(int listen_fd);
	void	_handle_client_data(int client_fd);
	void	_handle_client_write(int client_fd);
	void	_handle_cgi_data(int cgi_fd, uint32_t events);

	void	_check_client_timeouts();
	void	_close_client(int client_fd);

	void	_process_request(int client_fd);
	bool	_should_keep_alive(const HttpRequest& request) const;
};


} // namespace wsv

#endif
