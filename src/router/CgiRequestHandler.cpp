#include "CgiRequestHandler.hpp"
#include "cgi/CgiHandler.hpp"
#include "utils/StringUtils.hpp"
#include "utils/Logger.hpp"
#include "server/Client.hpp"
#include "ErrorHandler.hpp"
#include <sstream>

namespace wsv
{

void CgiRequestHandler::startCgi(Client& client,
                                 const std::string& script_path,
                                 const LocationConfig& location_config,
                                 const ServerConfig& server_config)
{
    // Ensure distinct handler for this client
    if (client.cgi_handler)
    {
        delete client.cgi_handler;
        client.cgi_handler = NULL;
    }

    try
    {
        CgiHandler* handler = new CgiHandler(location_config.cgi_path, script_path);
        client.cgi_handler = handler;

        // 1. Build CGI environment variables
        std::map<std::string, std::string> env_vars =
            _build_cgi_environment(client.request, script_path, location_config, server_config);

        for (std::map<std::string, std::string>::const_iterator it = env_vars.begin();
            it != env_vars.end(); ++it)
        {
            handler->setEnvironmentVariable(it->first, it->second);
        }

        // 2. Provide request body as CGI stdin if POST
        if (client.request.getMethod() == "POST")
            handler->setInput(client.request.getBody());

        // 3. Start the CGI process
        pid_t pid = handler->start();
        client.cgi_input_fd = handler->getStdinWriteFd();
        client.cgi_output_fd = handler->getStdoutReadFd();
        client.state = CLIENT_CGI_PROCESSING;
        
        Logger::info("Started CGI process {} for client FD {}", pid, client.client_fd);
    }
    catch (const std::exception& e)
    {
        Logger::error("Failed to start CGI: {}", e.what());
        delete client.cgi_handler;
        client.cgi_handler = NULL;
        
        // Return 500 error immediately
        HttpResponse error_res = ErrorHandler::get_error_page(500, server_config);
        client.response_buffer = error_res.serialize();
        client.state = CLIENT_WRITING_RESPONSE;
    }
}

// ============================================================================
// Build CGI environment variables (RFC 3875)
// Includes REQUEST_METHOD, QUERY_STRING, SCRIPT_FILENAME, SERVER_NAME, etc.
// ============================================================================
std::map<std::string, std::string> CgiRequestHandler::_build_cgi_environment(
    const HttpRequest& request,
    const std::string& script_path,
    const LocationConfig& location_config,
    const ServerConfig& server_config)
{
    (void)location_config;
    std::map<std::string, std::string> env_vars;

    // REQUEST_METHOD: GET, POST, etc.
    env_vars["REQUEST_METHOD"] = request.getMethod();

    // SCRIPT_FILENAME: full path on filesystem
    env_vars["SCRIPT_FILENAME"] = script_path;

    // SCRIPT_NAME: path portion of URL
    env_vars["SCRIPT_NAME"] = request.getPath();

    // QUERY_STRING: everything after '?'
    env_vars["QUERY_STRING"] = request.getQuery();

    // SERVER_PROTOCOL: HTTP/1.1, HTTP/1.0
    env_vars["SERVER_PROTOCOL"] = request.getVersion();

    // SERVER_NAME and SERVER_PORT
    env_vars["SERVER_NAME"] = server_config.host;
    env_vars["SERVER_PORT"] = StringUtils::toString(server_config.listen_port);

    // CONTENT_LENGTH and CONTENT_TYPE for POST/PUT
    if (request.hasHeader("Content-Length"))
        env_vars["CONTENT_LENGTH"] = request.getHeader("Content-Length");
    if (request.hasHeader("Content-Type"))
        env_vars["CONTENT_TYPE"] = request.getHeader("Content-Type");

    // PATH_INFO: additional path info
    env_vars["PATH_INFO"] = request.getPath();

    // GATEWAY_INTERFACE: CGI version
    env_vars["GATEWAY_INTERFACE"] = "CGI/1.1";

    return env_vars;
}

} // namespace wsv
