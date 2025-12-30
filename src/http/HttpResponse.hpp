#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <vector>

namespace wsv
{

class HttpResponse
{
private:
    int _status_code;                                    // HTTP status code
    std::string _version;                                // HTTP version (e.g., "HTTP/1.1")
    std::map<std::string, std::string> _headers;         // Response headers
    std::string _body;                                   // Response body
    
    // Initialize default headers (Server, Date, Connection)
    void _setDefaultHeaders();

public:
    // Constructor: initializes response with HTTP 200 status and default headers
    HttpResponse();
    ~HttpResponse();

    // Setters
    void setStatus(int code); // set HTTP status code (e.g., 200, 404, 500)
    void setHeader(const std::string& key, const std::string& value); // set HTTP header (e.g., "Content-Type: text/html")
    void setBody(const std::string& body); // set HTTP body and automatically update Content-Length header
    void appendBody(const std::string& data); // Append data to the existing body and update Content-Length header

    // Getters
    int getStatus() const;
    std::string getHeader(const std::string& key) const; // Get a specific header value (returns empty string if not found)
    const std::string& getBody() const;


    // ========== Serialization ==========
    // Serialize the entire response into HTTP format (status line + headers + body)
    std::string serialize() const;

    // ========== Convenience Methods ==========
    
    // Set the Content-Type header (e.g., "text/html", "application/json")
    void setContentType(const std::string& type);
    
    // Set the Content-Length header based on body size
    void setContentLength(size_t length);

    // ========== Static Factory Methods ==========
    
    // Create a complete error response with HTML body (e.g., 404, 500)
    static HttpResponse createErrorResponse(int code,
                                           const std::string& message = "");
    
    // Create a redirect response with Location header and HTML body
    static HttpResponse createRedirectResponse(int code,
                                              const std::string& location);
    
    // Create a 200 OK response with the given body and content type
    static HttpResponse createOkResponse(
        const std::string& body,
        const std::string& content_type = "text/html"
    );

    // ========== HTTP Status Code Utilities ==========
    
    // Return the standard HTTP status message for a given code
    static std::string getStatusMessage(int code);
};

} // namespace wsv

#endif // HTTP_RESPONSE_HPP


