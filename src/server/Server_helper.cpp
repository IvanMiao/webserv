#include "Server.hpp"
#include <fstream>
#include <sstream>
#include <sys/wait.h>

namespace wsv {

/*
	Handle CGI data events
*/
void Server::_handle_cgi_data(int cgi_fd, uint32_t events)
{
	int client_fd = _cgi_fd_map[cgi_fd];
	Client& client = _clients[client_fd];

	if (client.state != CLIENT_CGI_PROCESSING)
	{
		Logger::error("CGI event for client {} not in CGI state", client_fd);
		_remove_from_epoll(cgi_fd);
		close(cgi_fd);
		_cgi_fd_map.erase(cgi_fd);
		return;
	}

	CgiHandler* handler = client.cgi_handler;
	if (!handler) {
		Logger::error("CGI handler missing for client {}", client_fd);
		return;
	}

	// 1. Write to CGI Stdin (if this fd is input_fd)
	if (cgi_fd == client.cgi_input_fd && (events & EPOLLOUT))
	{
		std::string input = handler->getInput();
		ssize_t written = write(cgi_fd, input.c_str(), input.size());
		
		if (written >= 0)
		{
			// Input written (or empty), close stdin to signal EOF to CGI
			handler->closeStdin();
			_remove_from_epoll(cgi_fd);
			_cgi_fd_map.erase(cgi_fd);
			client.cgi_input_fd = -1;
		}
		else
		{
			Logger::error("Failed to write to CGI stdin");
			handler->closeStdin();
			_remove_from_epoll(cgi_fd);
			_cgi_fd_map.erase(cgi_fd);
			client.cgi_input_fd = -1;
		}
	}
	
	// 2. Read from CGI Stdout (if this fd is output_fd)
	if (cgi_fd == client.cgi_output_fd && (events & (EPOLLIN | EPOLLHUP)))
	{
		char buffer[READ_BUFFER_SIZE];
		ssize_t bytes = read(cgi_fd, buffer, sizeof(buffer));

		if (bytes > 0)
		{
			client.response_buffer.append(buffer, bytes);
		}
		else if (bytes == 0 || (events & EPOLLHUP))
		{
			// CGI finished
			Logger::info("CGI stdout closed, processing response");
			
			// Wait for child
			int status;
			waitpid(handler->getChildPid(), &status, 0); 

			// Check exit status
			if (WIFEXITED(status) && WEXITSTATUS(status) != 0)
			{
				Logger::error("CGI process exited with error status: {}", WEXITSTATUS(status));
				client.response_buffer = HttpResponse::createErrorResponse(500).serialize();
				client.state = CLIENT_WRITING_RESPONSE;
			}
			else
			{
				// Parse Output
				// client.response_buffer currently contains RAW CGI output (headers + body)
				CgiHandler::HeaderMap cgi_headers;
				std::string body;
				CgiHandler::parseCgiOutput(client.response_buffer, cgi_headers, body);

				// Build HttpResponse
				HttpResponse response;
				response.setBody(body);
				
				// Parse Status header
				if (cgi_headers.count("Status"))
				{
					std::istringstream iss(cgi_headers["Status"]);
					int code = 200;
					iss >> code;
					response.setStatus(code);
				}
				else
					response.setStatus(200);

				// Add other headers
				for (std::map<std::string, std::string>::iterator it = cgi_headers.begin(); it != cgi_headers.end(); ++it)
				{
					if (it->first != "Status")
						response.setHeader(it->first, it->second);
				}

				// Keep-alive headers logic
				if (client.keep_alive)
					response.setHeader("Connection", "keep-alive");
				else
					response.setHeader("Connection", "close");

				client.response_buffer = response.serialize();
				client.state = CLIENT_WRITING_RESPONSE;
			}
			// Cleanup
			_remove_from_epoll(cgi_fd);
			close(cgi_fd);
			_cgi_fd_map.erase(cgi_fd);
			client.cgi_output_fd = -1;

			// Cleanup handler
			delete client.cgi_handler;
			client.cgi_handler = NULL;

			// Re-enable client socket
			_modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
		}
		else
		{
			Logger::error("Read error from CGI stdout");
			// Handle error... return 500
			_remove_from_epoll(cgi_fd);
			close(cgi_fd);
			_cgi_fd_map.erase(cgi_fd);
			client.cgi_output_fd = -1;
			
			client.response_buffer = HttpResponse::createErrorResponse(500).serialize();
			client.state = CLIENT_WRITING_RESPONSE;
			_modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
		}
	}
}

} // namespace wsv
