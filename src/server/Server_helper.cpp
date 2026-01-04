#include "Server.hpp"
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <cstring>
#include <cerrno>

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
	if (!handler)
	{
		Logger::error("CGI handler missing for client {}", client_fd);
		return;
	}

	// 1. Write to CGI Stdin (if this fd is input_fd)
	if (cgi_fd == client.cgi_input_fd && (events & EPOLLOUT))
	{
		const std::string& input = handler->getInput();
		size_t remaining = input.size() - client.cgi_write_offset;
		
		if (remaining == 0)
		{
			// All data written, close stdin to signal EOF to CGI
			handler->closeStdin();
			handler->markStdinClosed();
			_remove_from_epoll(cgi_fd);
			_cgi_fd_map.erase(cgi_fd);
			client.cgi_input_fd = -1;
			return;
		}
		
		ssize_t written = write(cgi_fd, input.c_str() + client.cgi_write_offset, remaining);
		
		if (written > 0)
		{
			client.cgi_write_offset += written;
			// Update activity to prevent timeout during large POST body writes
			client.updateActivity();
			
			// Check if all data has been written
			if (client.cgi_write_offset >= input.size())
			{
				handler->closeStdin();
				handler->markStdinClosed();
				_remove_from_epoll(cgi_fd);
				_cgi_fd_map.erase(cgi_fd);
				client.cgi_input_fd = -1;
			}
			// Otherwise, wait for next EPOLLOUT to continue writing
		}
		else if (written == 0 || (written == -1 && errno != EAGAIN && errno != EWOULDBLOCK))
		{
			Logger::error("Failed to write to CGI stdin: {}", strerror(errno));
			handler->closeStdin();
			handler->markStdinClosed();
			_remove_from_epoll(cgi_fd);
			_cgi_fd_map.erase(cgi_fd);
			client.cgi_input_fd = -1;
		}
		// If EAGAIN/EWOULDBLOCK, just wait for next EPOLLOUT
	}
	
	// 2. Read from CGI Stdout (if this fd is output_fd)
	if (cgi_fd == client.cgi_output_fd && (events & (EPOLLIN | EPOLLHUP)))
	{
		char buffer[READ_BUFFER_SIZE];
		ssize_t bytes = read(cgi_fd, buffer, sizeof(buffer));

		if (bytes > 0)
			client.response_buffer.append(buffer, bytes);
		else if (bytes == 0 || (events & EPOLLHUP))
		{
			// CGI finished
			Logger::info("CGI stdout closed, processing response");
			
			// Wait for child
			int status;
			waitpid(handler->getChildPid(), &status, 0); 

			// Check exit status
			if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
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
			if (client.cgi_handler)
				client.cgi_handler->markStdoutClosed();  // Prevent double-close

			// Cleanup handler
			delete client.cgi_handler;
			client.cgi_handler = NULL;

			// Re-enable client socket
			_modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
		}
		else // Handle error... return 500
		{
			Logger::error("Read error from CGI stdout");
			_remove_from_epoll(cgi_fd);
			close(cgi_fd);
			_cgi_fd_map.erase(cgi_fd);
			client.cgi_output_fd = -1;
			if (client.cgi_handler)
			{
				client.cgi_handler->markStdoutClosed();
				delete client.cgi_handler;
				client.cgi_handler = NULL;
			}
			
			client.response_buffer = HttpResponse::createErrorResponse(500).serialize();
			client.state = CLIENT_WRITING_RESPONSE;
			_modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
		}
	}
}

} // namespace wsv
