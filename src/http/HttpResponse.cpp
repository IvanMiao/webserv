#include "HttpResponse.hpp"
#include <sstream>
#include <ctime>

namespace wsv
{

HttpResponse::HttpResponse()
    : _status_code(200),
      _version("HTTP/1.1")
{
    this->_setDefaultHeaders();
}

HttpResponse::~HttpResponse()
{
}

void HttpResponse::setStatus(int code)
{
    this->_status_code = code;
}

void HttpResponse::setHeader(const std::string& key, const std::string& value)
{
    this->_headers[key] = value;
}

void HttpResponse::setBody(const std::string& body)
{
    this->_body = body;
    this->setContentLength(this->_body.size());
}

void HttpResponse::appendBody(const std::string& data)
{
    this->_body += data;
    this->setContentLength(this->_body.size());
}

void HttpResponse::setContentType(const std::string& type)
{
    this->setHeader("Content-Type", type);
}

void HttpResponse::setContentLength(size_t length)
{
    std::ostringstream oss;
    oss << length;
    this->setHeader("Content-Length", oss.str());
}

void HttpResponse::_setDefaultHeaders()
{
    // Server
    this->setHeader("Server", "Webserv/1.0");

    // Date
    time_t now = time(NULL);
    char date_buf[100];
    strftime(date_buf, sizeof(date_buf),
             "%a, %d %b %Y %H:%M:%S GMT",
             gmtime(&now));

    this->setHeader("Date", date_buf);

    // Connection
    this->setHeader("Connection", "close");
}

std::string HttpResponse::serialize() const
{
    std::ostringstream oss;

    // Status line
    oss << this->_version << " "
        << this->_status_code << " "
        << getStatusMessage(this->_status_code) << "\r\n";

    // Headers
    for (std::map<std::string, std::string>::const_iterator it =
             this->_headers.begin();
         it != this->_headers.end(); ++it)
    {
        oss << it->first << ": " << it->second << "\r\n";
    }

    // Empty line
    oss << "\r\n";

    // Body
    oss << this->_body;

    return oss.str();
}

HttpResponse HttpResponse::createErrorResponse(int code,
                                               const std::string& message)
{
    HttpResponse response;
    response.setStatus(code);

    std::string msg = message.empty()
                        ? getStatusMessage(code)
                        : message;

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

HttpResponse HttpResponse::createRedirectResponse(int code,
                                                  const std::string& location)
{
    HttpResponse response;
    response.setStatus(code);
    response.setHeader("Location", location);

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

std::string HttpResponse::getStatusMessage(int code)
{
    switch (code)
    {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 301: return "Moved Permanently";
        case 302: return "Found";
        case 303: return "See Other";
        case 304: return "Not Modified";
        case 307: return "Temporary Redirect";
        case 308: return "Permanent Redirect";
        case 400: return "Bad Request";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 408: return "Request Timeout";
        case 413: return "Payload Too Large";
        case 414: return "URI Too Long";
        case 415: return "Unsupported Media Type";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        case 505: return "HTTP Version Not Supported";
        default:  return "Unknown";
    }
}

} // namespace wsv


