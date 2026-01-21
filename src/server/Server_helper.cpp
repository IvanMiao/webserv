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
    if (events & EPOLLERR)
    {
        // EPOLLERR indicates a real error on the fd
        if (cgi_fd == client.cgi_input_fd)
        {
            Logger::debug("CGI input pipe error (EPOLLERR)");
            handler->closeStdin();
            handler->markStdinClosed();
            _remove_from_epoll(cgi_fd);
            _cgi_fd_map.erase(cgi_fd);
            client.cgi_input_fd = -1;
        }
        else if (cgi_fd == client.cgi_output_fd)
        {
            Logger::error("CGI output pipe error (EPOLLERR)");
            _remove_from_epoll(cgi_fd);
            close(cgi_fd);
            _cgi_fd_map.erase(cgi_fd);
            client.cgi_output_fd = -1;
            if (client.cgi_handler)
                client.cgi_handler->markStdoutClosed();
            delete client.cgi_handler;
            client.cgi_handler = NULL;
            client.response_buffer = HttpResponse::createErrorResponse(500).serialize();
            client.state = CLIENT_WRITING_RESPONSE;
            _modify_epoll(client_fd, EPOLLIN | EPOLLOUT);
            return;
        }
    }

    // Handle EPOLLHUP separately (expected for input_fd when CGI closes stdin)
    if ((events & EPOLLHUP) && cgi_fd == client.cgi_input_fd && client.cgi_input_fd != -1)
    {
        Logger::debug("CGI input pipe HUP");
        handler->closeStdin();
        handler->markStdinClosed();
        _remove_from_epoll(cgi_fd);
        _cgi_fd_map.erase(cgi_fd);
        client.cgi_input_fd = -1;
        // No return here, continue to check if we can read from stdout
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
            else // written == -1
            {
                // Non-blocking write not ready, wait for next EPOLLOUT event
                Logger::debug("CGI stdin write returned -1, waiting for next epoll event");
            }
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
                client.response_buffer = HttpResponse::createErrorResponse(500).serialize(); // 500 Internal Server Error
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
        else // bytes == -1
        {
            // Non-blocking read not ready, wait for next EPOLLIN event
            Logger::debug("CGI stdout read returned -1, waiting for next epoll event");
        }
    }
}

} // namespace wsv
