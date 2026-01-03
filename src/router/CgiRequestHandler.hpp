#ifndef CGI_REQUEST_HANDLER_HPP
#define CGI_REQUEST_HANDLER_HPP

#include <string>
#include <map>
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ConfigParser.hpp"

namespace wsv
{
class Client;

/**
 * CgiRequestHandler - Handles execution of CGI scripts
 * 
 * Features:
 * - Executes CGI scripts based on HTTP requests
 * - Builds CGI environment variables (RFC 3875)
 * - Returns HttpResponse objects
 */
class CgiRequestHandler
{
public:
    /**
     * Start async CGI execution
     * @param client Client object 
     * @param script_path Filesystem path to CGI script
     * @param location_config Location configuration
     * @param server_config Server configuration
     */
    static void startCgi(Client& client,
                         const std::string& script_path,
                         const LocationConfig& location_config,
                         const ServerConfig& server_config);



private:
    // ========================================
    // Private Helper Methods
    // ========================================

    /**
     * Build the environment variables for CGI execution
     * according to RFC 3875
     * @param request The HTTP request
     * @param location_config Location configuration
     * @param server_config Server configuration
     * @return Map of environment variable names to values
     */
    static std::map<std::string, std::string> _build_cgi_environment(
        const HttpRequest& request,
        const std::string& script_path,
        const LocationConfig& location_config,
        const ServerConfig& server_config
    );
};

} // namespace wsv

#endif // CGI_REQUEST_HANDLER_HPP
