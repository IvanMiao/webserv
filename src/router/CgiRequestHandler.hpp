#ifndef CGI_REQUEST_HANDLER_HPP
#define CGI_REQUEST_HANDLER_HPP

#include <string>
#include <map>
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ConfigParser.hpp"

namespace wsv
{

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
     * Execute a CGI script and return the HTTP response
     * @param request The parsed HTTP request
     * @param script_path Filesystem path to CGI script
     * @param location_config Location-specific configuration
     * @param server_config Server-wide configuration
     * @return HttpResponse generated from CGI output
     */
    static HttpResponse execute_cgi(const HttpRequest& request,
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
        const LocationConfig& location_config,
        const ServerConfig& server_config
    );

    /**
     * Run the CGI script using fork/exec and capture its output
     * @param env_vars Environment variables for CGI
     * @param script_path Path to the CGI script
     * @param input Optional input for CGI stdin
     * @return CGI output as string
     */
    static std::string _execute_cgi_script(
        const std::map<std::string, std::string>& env_vars,
        const std::string& script_path,
        const std::string& input = ""
    );

    /**
     * Parse CGI output into headers and body
     * @param cgi_output Raw output from CGI script
     * @param headers Output map of headers
     * @param body Output body content
     */
    static void _parse_cgi_output(
        const std::string& cgi_output,
        std::map<std::string, std::string>& headers,
        std::string& body
    );

    // ========================================
    // Private Member Variables (if any)
    // ========================================
    // (Currently, CgiRequestHandler is fully static, so no instance variables)
};

} // namespace wsv

#endif // CGI_REQUEST_HANDLER_HPP
