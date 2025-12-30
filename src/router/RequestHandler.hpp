#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include <string>
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "config/ConfigParser.hpp"
#include "utils/Logger.hpp"
#include "utils/StringUtils.hpp"

namespace wsv
{

/**
 * RequestHandler - Main HTTP request processing class
 * 
 * Features:
 * - Routes HTTP requests based on method and location configuration
 * - Supports GET, POST, DELETE methods
 * - Integrates with CGI scripts, file serving, and error handling
 * - Builds filesystem paths from URIs
 */
class RequestHandler
{
private:
    // Server configuration reference
    const ServerConfig& _config;

public:
    explicit RequestHandler(const ServerConfig& config);
    ~RequestHandler();

    /**
     * Main entry point for request processing
     * Routes requests based on HTTP method, location, and file type
     * @param request Parsed HTTP request
     * @return HttpResponse generated for the request
     */
    HttpResponse handleRequest(const HttpRequest& request);

private:
    // ========================================
    // HTTP Method Handlers
    // ========================================

    /**
     * Handle GET requests
     * @param request HTTP request
     * @param location_config Location-specific configuration
     * @param decoded_path URL-decoded path from request
     * @return HttpResponse for GET request
     */
    HttpResponse _handleGet(const HttpRequest& request,
                            const LocationConfig& location_config,
                            const std::string& decoded_path);

    /**
     * Handle POST requests
     * @param request HTTP request
     * @param location_config Location-specific configuration
     * @param decoded_path URL-decoded path from request
     * @return HttpResponse for POST request
     */
    HttpResponse _handlePost(const HttpRequest& request,
                             const LocationConfig& location_config,
                             const std::string& decoded_path);

    /**
     * Handle DELETE requests
     * @param request HTTP request
     * @param location_config Location-specific configuration
     * @param decoded_path URL-decoded path from request
     * @return HttpResponse for DELETE request
     */
    HttpResponse _handleDelete(const HttpRequest& request,
                               const LocationConfig& location_config,
                               const std::string& decoded_path);


    // ===== Helper Functions =====

    /**
     * Build filesystem path from URI
     * Note: uri_path should already be URL-decoded before calling this function
     * @param uri_path Decoded URI from HTTP request
     * @param location_config Location configuration
     * @return Full filesystem path
     */
    std::string _buildFilePath(const std::string& uri_path,
                               const LocationConfig& location_config);

    /**
     * Check if the given path corresponds to a CGI script
     * @param file_path Full filesystem path
     * @param location_config Location configuration
     * @return true if the file should be handled via CGI
     */
    // bool _isCgiRequest(const std::string& file_path, const LocationConfig& location_config) const;

    /**
     * Serve a static file
     * @param file_path Filesystem path to file
     * @return HttpResponse with file content or error
     */
    HttpResponse _serve_file(const std::string& file_path);

    /**
     * Serve directory content
     * @param dir_path Filesystem path to directory
     * @param location_config Location configuration
     * @return HttpResponse with directory listing or index file
     */
    HttpResponse _serve_directory(const std::string& dir_path,
                                   const LocationConfig& location_config);

    /**
     * Execute CGI script
     * @param request HTTP request
     * @param file_path Filesystem path to CGI script
     * @param location_config Location configuration
     * @return HttpResponse from CGI execution
     */
    //HttpResponse _execute_cgi(const HttpRequest& request,
    //                          const std::string& file_path,
    //                          const LocationConfig& location_config);
};

} // namespace wsv

#endif // REQUEST_HANDLER_HPP
