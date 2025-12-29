#include "HttpResponse.hpp"
#include <sstream>
#include <ctime>

namespace wsv
{

// HttpResponse Constructor
// ============================================================================
// Initializes the response with a 200 status code, HTTP/1.1 version,
// and sets default headers (Server, Date, Connection).
HttpResponse::HttpResponse()
    : _status_code(200),
      _version("HTTP/1.1")
{
    this->_setDefaultHeaders();
}

HttpResponse::~HttpResponse()
{ }


// Get the HTTP status code
int HttpResponse::getStatus() const
{
    return _status_code;
}

// Get a specific header value (returns empty string if not found)
std::string HttpResponse::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end())
    {
        return it->second;
    }
    return "";  // Header not found
}

// Get the response body
const std::string& HttpResponse::getBody() const
{
    return _body;
}


// setStatus - Sets the HTTP status code
void HttpResponse::setStatus(int code)
{
    this->_status_code = code;
}


// ## setHeader - Adds or updates an HTTP header
// If the header key already exists, it will be overwritten.
void HttpResponse::setHeader(const std::string& key, const std::string& value)
{
    this->_headers[key] = value;
}

// ## setBody - Sets the response body and updates Content-Length
// Automatically sets the Content-Length header based on the body size.
void HttpResponse::setBody(const std::string& body)
{
    this->_body = body;
    this->setContentLength(this->_body.size());
}

// ## appendBody - Appends data to the existing body
// Updates Content-Length header after appending.
void HttpResponse::appendBody(const std::string& data)
{
    this->_body += data;
    this->setContentLength(this->_body.size());
}

// setContentType - Sets the Content-Type header
void HttpResponse::setContentType(const std::string& type)
{
    this->setHeader("Content-Type", type);
}

// ## setContentLength - Sets the Content-Length header
// Converts the size_t length to string format for HTTP header.
void HttpResponse::setContentLength(size_t length)
{
    std::ostringstream oss;
    oss << length;
    this->setHeader("Content-Length", oss.str());
}

// ## _setDefaultHeaders - Initializes default HTTP response headers
// Sets Server, Date, and Connection headers.
// Date is formatted according to RFC 2822 HTTP date format.
void HttpResponse::_setDefaultHeaders()
{
    // Set Server identification header
    this->setHeader("Server", "Webserv/1.0");

    // Generate current date in HTTP format: "Day, DD Mon YYYY HH:MM:SS GMT"
    time_t now = std::time(NULL);
    char date_buf[100];
    std::strftime(date_buf, sizeof(date_buf),
             "%a, %d %b %Y %H:%M:%S GMT",
             std::gmtime(&now));

    this->setHeader("Date", date_buf);

    // Indicate that the connection will be closed after this response
    this->setHeader("Connection", "close");
}


// ## serialize - Generates the complete HTTP response string
// Format: STATUS_LINE \r\n HEADERS \r\n \r\n BODY
// Returns the raw HTTP response ready to be sent to the client.
std::string HttpResponse::serialize() const
{
    std::ostringstream oss;

    // Build status line: "HTTP/1.1 200 OK\r\n"
    oss << this->_version << " "
        << this->_status_code << " "
        << getStatusMessage(this->_status_code) << "\r\n";

    // Build headers: "Header-Name: header-value\r\n"
    for (std::map<std::string, std::string>::const_iterator it =
             this->_headers.begin();
         it != this->_headers.end(); ++it)
    {
        oss << it->first << ": " << it->second << "\r\n";
    }

    // Empty line separates headers from body: "\r\n"
    oss << "\r\n";

    // Append body content (if any)
    oss << this->_body;

    return oss.str();
}


// ========== Static Factory Methods ==========

// ## createErrorResponse - Factory method to create error responses
// Creates an HTML formatted error page with the given status code.
// Optional message parameter provides custom error description;
// if empty, the standard HTTP status message is used.
HttpResponse HttpResponse::createErrorResponse(int code,
                                               const std::string& message)
{
    HttpResponse response;
    response.setStatus(code);

    // Use custom message if provided, otherwise use standard HTTP message
    std::string msg = message.empty() ? getStatusMessage(code) : message;

    // Build HTML error page
    std::ostringstream body;
    body << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head><title>Error " << code << "</title></head>\n"
         << "<body>\n"
         << "<h1>" << code << " "
         << getStatusMessage(code) << "</h1>\n"
         << "<p>" << msg << "</p>\n"
         << "<hr>\n"
         << "<p><i>Webserv/1.0</i></p>\n"
         << "</body>\n"
         << "</html>\n";

    response.setBody(body.str());
    response.setContentType("text/html");

    return response;
}

// ## createRedirectResponse - Factory method to create redirect responses
// Creates an HTTP redirect response with the given status code and location.
// Includes the Location header and an HTML page with a clickable link.
HttpResponse HttpResponse::createRedirectResponse(int code,
                                                  const std::string& location)
{
    HttpResponse response;
    response.setStatus(code);
    response.setHeader("Location", location);

    // Build HTML page with redirect link for browsers that don't auto-redirect
    std::ostringstream body;
    body << "<!DOCTYPE html>\n"
         << "<html>\n"
         << "<head><title>Redirecting...</title></head>\n"
         << "<body>\n"
         << "<h1>Redirecting to <a href=\""
         << location << "\">"
         << location << "</a></h1>\n"
         << "</body>\n"
         << "</html>\n";

    response.setBody(body.str());
    response.setContentType("text/html");

    return response;
}

// ## createOkResponse - Factory method to create success responses
// Creates a 200 OK response with the given body and content type.
HttpResponse HttpResponse::createOkResponse(
    const std::string& body,
    const std::string& content_type)
{
    HttpResponse response;
    response.setStatus(200);
    response.setBody(body);
    response.setContentType(content_type);

    return response;
}


// ========== HTTP Utils ==========

// getStatusMessage - Returns standard HTTP status message for a code
std::string HttpResponse::getStatusMessage(int code)
{
    switch (code)
    {
        // 2xx Success
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        
        // 3xx Redirection
        case 301: return "Moved Permanently";
        case 302: return "Found";
        
        // 4xx Client Error
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        
        // 5xx Server Error
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 504: return "Gateway Timeout";
        case 505: return "HTTP Version Not Supported";
        case 507: return "Insufficient Storage";
        
        // Unknown code
        default:  return "Unknown";
    }
}

} // namespace wsv
