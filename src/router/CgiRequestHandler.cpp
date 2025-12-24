#include "CgiRequestHandler.hpp"
#include "../cgi/CgiHandler.hpp"
#include "../utils/StringHelper.hpp"
#include <iostream>
#include <sstream>

namespace wsv
{

// ============================================================================
// Execute CGI script and process output
// Process: Build environment -> execute script -> parse output -> return response
// Supports both GET and POST CGI requests
// ============================================================================
HttpResponse CgiRequestHandler::execute_cgi(const HttpRequest& request,
                                            const std::string& script_path,
                                            const LocationConfig& location_config,
                                            const ServerConfig& server_config)
{
    // Create CGI executor with interpreter path and script file
    CgiHandler cgi_executor(location_config.cgi_path, script_path);

    // 1. Build CGI environment variables
    std::map<std::string, std::string> env_vars =
        _build_cgi_environment(request, location_config, server_config);

    for (std::map<std::string, std::string>::const_iterator it = env_vars.begin();
         it != env_vars.end(); ++it)
    {
        cgi_executor.setEnvironmentVariable(it->first, it->second);
    }

    // 2. Provide request body as CGI stdin if POST
    if (request.getMethod() == "POST")
    {
        cgi_executor.setInput(request.getBody());
    }

    // 3. Execute the CGI script
    try
    {
        cgi_executor.execute();
    }
    catch (const CgiHandler::Timeout& e)
    {
        std::cerr << "CGI Timeout: " << e.what() << std::endl;
        HttpResponse response;
        response.setStatus(504);  // Gateway Timeout
        return response;
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "CGI Error: " << e.what() << std::endl;
        HttpResponse response;
        response.setStatus(500);  // Internal Server Error
        return response;
    }

    // 4. Parse CGI output into HttpResponse
    HttpResponse response;
    std::map<std::string, std::string> cgi_headers =
        cgi_executor.getResponseHeaders();

    int status_code = 200; // default

    for (std::map<std::string, std::string>::const_iterator it = cgi_headers.begin();
         it != cgi_headers.end(); ++it)
    {
        if (it->first == "Status")
        {
            std::istringstream iss(it->second);
            iss >> status_code;
        }
        else
        {
            response.setHeader(it->first, it->second);
        }
    }

    response.setStatus(status_code);
    response.setBody(cgi_executor.getOutput());

    return response;
}

// ============================================================================
// Build CGI environment variables (RFC 3875)
// Includes REQUEST_METHOD, QUERY_STRING, SCRIPT_FILENAME, SERVER_NAME, etc.
// ============================================================================
std::map<std::string, std::string> CgiRequestHandler::_build_cgi_environment(
    const HttpRequest& request,
    const LocationConfig& location_config,
    const ServerConfig& server_config)
{
    std::map<std::string, std::string> env_vars;

    // REQUEST_METHOD: GET, POST, etc.
    env_vars["REQUEST_METHOD"] = request.getMethod();

    // SCRIPT_FILENAME: full path on filesystem
    {
        std::string relative_path = request.getPath();
        if (request.getPath().find(location_config.path) == 0)
            relative_path = request.getPath().substr(location_config.path.size());
        if (relative_path.empty() || relative_path[0] != '/')
            relative_path = "/" + relative_path;
        relative_path = StringHelper::urlDecode(relative_path);
        env_vars["SCRIPT_FILENAME"] = location_config.root + relative_path;
    }

    // SCRIPT_NAME: path portion of URL
    env_vars["SCRIPT_NAME"] = request.getPath();

    // QUERY_STRING: everything after '?'
    env_vars["QUERY_STRING"] = request.getQuery();

    // SERVER_PROTOCOL: HTTP/1.1, HTTP/1.0
    env_vars["SERVER_PROTOCOL"] = request.getVersion();

    // SERVER_NAME and SERVER_PORT
    env_vars["SERVER_NAME"] = server_config.host;
    env_vars["SERVER_PORT"] = StringHelper::toString(server_config.listen_port);

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
