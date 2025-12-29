#include "ErrorHandler.hpp"
#include "FileHandler.hpp"
#include <map>

namespace wsv {

/**
 * Generate HTTP error response 
 * Tries to load custom error page from server config, falls back to default HTML
 * @param status_code HTTP status code (e.g., 404, 500)
 * @param config Server configuration containing custom error page paths
 * @return HttpResponse object with status code and body content
*/
HttpResponse ErrorHandler::get_error_page(int status_code, const ServerConfig& config)
{
    // Create response with appropriate HTTP status code
    HttpResponse response;
    response.setStatus(status_code);

    // Look for custom error page configured for this status code
    std::map<int, std::string>::const_iterator it = config.error_pages.find(status_code);

    std::string body_content;

    // Serve custom error page if it exists
    if (it != config.error_pages.end())
    {
        // Build full filesystem path: root + error_page_path
        // Example: config.root = "./www", it->second = "/errors/404.html"
        // Result: "./www/errors/404.html"
        std::string error_page_path = config.root + it->second;
        
        // Attempt to read custom error page file
        body_content = FileHandler::read_file(error_page_path);
        if (!body_content.empty())
        {
            response.setBody(body_content);
            response.setHeader("Content-Type", FileHandler::get_mime_type(error_page_path));
            return response;
        }
    }

    // If no custom page found, use the standard error response from HttpResponse
    return HttpResponse::createErrorResponse(status_code, "");
}

} // namespace wsv
