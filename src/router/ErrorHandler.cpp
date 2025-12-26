#include "ErrorHandler.hpp"
#include "FileHandler.hpp"
#include "utils/StringUtils.hpp"
#include <map>

namespace wsv {

// ============================================================================
// Generate HTTP error response
// Tries to load custom error page from server config, falls back to default HTML
// ============================================================================
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
        // Attempt to read custom error page file
        body_content = FileHandler::read_file(it->second);
        if (!body_content.empty())
        {
            response.setBody(body_content);
            response.setHeader("Content-Type", FileHandler::get_mime_type(it->second));
            return response;
        }
    }

    // If no custom page found, generate default HTML error page
    body_content = _generate_default_error_page(status_code);
    response.setBody(body_content);
    response.setHeader("Content-Type", "text/html");

    return response;
}

// ============================================================================
// Generate default HTML error page
// ============================================================================
std::string ErrorHandler::_generate_default_error_page(int status_code)
{
    std::string status_text;

    // Map common status codes to human-readable text
    switch (status_code)
    {
        case 400: status_text = "Bad Request"; break;
        case 403: status_text = "Forbidden"; break;
        case 404: status_text = "Not Found"; break;
        case 405: status_text = "Method Not Allowed"; break;
        case 413: status_text = "Payload Too Large"; break;
        case 500: status_text = "Internal Server Error"; break;
        case 501: status_text = "Not Implemented"; break;
        case 504: status_text = "Gateway Timeout"; break;
        default: status_text = "Error"; break;
    }

    // Build simple HTML page
    std::string html = "<html><head><title>" + StringUtils::toString(status_code) +
                       " " + status_text + "</title></head>";
    html += "<body><h1>" + StringUtils::toString(status_code) + " " +
            status_text + "</h1>";
    html += "<hr><p>Web Server</p></body></html>";

    return html;
}

} // namespace wsv
