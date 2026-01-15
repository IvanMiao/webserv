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

    // 0. Pre-check for error events (Handle broken pipes/errors via epoll flags)
    if (events & (EPOLLERR | EPOLLHUP))
    {
        // For output_fd, EPOLLHUP is expected when CGI finishes, 
        // but for input_fd, it usually means the child closed stdin unexpectedly.
        if (cgi_fd == client.cgi_input_fd)
        {
            Logger::debug("CGI input pipe error or HUP");
            handler->closeStdin();
            handler->markStdinClosed();
            _remove_from_epoll(cgi_fd);
            _cgi_fd_map.erase(cgi_fd);
            client.cgi_input_fd = -1;
            // No return here, check if we can still read from stdout
        }
    }

    // 1. Write to CGI Stdin
    if (cgi_fd == client.cgi_input_fd && (events & EPOLLOUT))
    {
        const std::string& input = handler->getInput();
        size_t remaining = input.size() - client.cgi_write_offset;
        
        if (remaining == 0)
        {
            handler->closeStdin();
            handler->markStdinClosed();
            _remove_from_epoll(cgi_fd);
            _cgi_fd_map.erase(cgi_fd);
            client.cgi_input_fd = -1;
        }
        else
        {
            ssize_t written = write(cgi_fd, input.c_str() + client.cgi_write_offset, remaining);
            
            if (written > 0)
            {
                client.cgi_write_offset += written;
                client.updateActivity();
                
                if (client.cgi_write_offset >= input.size())
                {
                    handler->closeStdin();
                    handler->markStdinClosed();
                    _remove_from_epoll(cgi_fd);
                    _cgi_fd_map.erase(cgi_fd);
                    client.cgi_input_fd = -1;
                }
            }
            else if (written == 0)
            {
                // Pipe closed by CGI script
                handler->closeStdin();
                handler->markStdinClosed();
                _remove_from_epoll(cgi_fd);
                _cgi_fd_map.erase(cgi_fd);
                client.cgi_input_fd = -1;
            }
            // If written == -1, we do NOTHING. We trust epoll to tell us again next time.
        }
    }
    
    // 2. Read from CGI Stdout
    if (cgi_fd == client.cgi_output_fd && (events & (EPOLLIN | EPOLLHUP)))
    {
        char buffer[READ_BUFFER_SIZE];
        ssize_t bytes = read(cgi_fd, buffer, sizeof(buffer));

        if (bytes > 0)
        {
            client.response_buffer.append(buffer, bytes);
            client.updateActivity();
        }
        else if (bytes == 0 || (events & EPOLLHUP))
        {
            // CGI finished: Either pipe closed or HUP received
            Logger::info("CGI stdout closed or HUP, processing response");
            
            int status;
            // Use WNOHANG to check if child exited, or wait if it's already done
            waitpid(handler->getChildPid(), &status, 0); 

            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            {
                Logger::error("CGI process failed or exited with status: {}", WEXITSTATUS(status));
                client.response_buffer = HttpResponse::createErrorResponse(500).serialize(); // [TUDO]500 Internal Server Error
            }
            else
            {
                CgiHandler::HeaderMap cgi_headers;
                std::string body;
                CgiHandler::parseCgiOutput(client.response_buffer, cgi_headers, body);

                HttpResponse response;
                response.setBody(body);
                
                if (cgi_headers.count("Status"))
                {
                    std::istringstream iss(cgi_headers["Status"]);
                    int code = 200;
                    iss >> code;
                    response.setStatus(code);
                }
                else
                    response.setStatus(200);

                for (std::map<std::string, std::string>::iterator it = cgi_headers.begin(); it != cgi_headers.end(); ++it)
                {
                    if (it->first != "Status")
                        response.setHeader(it->first, it->second);
                }

                if (client.keep_alive)
                    response.setHeader("Connection", "keep-alive");
                else
                    response.setHeader("Connection", "close");

                client.response_buffer = response.serialize();
            }

            // Cleanup CGI resources
            _remove_from_epoll(cgi_fd);
            close(cgi_fd);
            _cgi_fd_map.erase(cgi_fd);
            client.cgi_output_fd = -1;
            if (client.cgi_handler)
                client.cgi_handler->markStdoutClosed();

            delete client.cgi_handler;
            client.cgi_handler = NULL;

            // Final state transition
            client.state = CLIENT_WRITING_RESPONSE;
            _modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
        }
        // If bytes == -1, we do NOTHING. Wait for next epoll event.
    }
}

} // namespace wsv
