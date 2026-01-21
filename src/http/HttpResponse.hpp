#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <vector>

namespace wsv
{
/**
 * HttpResponse - Represents an HTTP response message
 * * Features:
 * - Manages HTTP status codes and versions
 * - Handles response headers and body content
 * - Provides serialization into raw HTTP format
 * - Includes factory methods for common responses (Error, Redirect, OK)
 */
class HttpResponse
{
private:
    int _status_code;                                    // HTTP status code
    std::string _version;                                // HTTP version (e.g., "HTTP/1.1")
    std::map<std::string, std::string> _headers;         // Response headers
    std::string _body;                                   // Response body

    /**
     * Initialize default headers
     * Sets Server, Date, and Connection headers
     */
    void _setDefaultHeaders();

public:
    /**
     * Constructor
     * Initializes response with HTTP 200 status and default headers
     */
    HttpResponse();
    ~HttpResponse();

    // ========================================
    // Setters
    // ========================================

    void setStatus(int code);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    void appendBody(const std::string& data);

    // ========================================
    // Getters
    // ========================================

    int getStatus() const;
    std::string getHeader(const std::string& key) const;
    const std::string& getBody() const;

    // ========================================
    // Serialization
    // ========================================

    /**
     * Serialize the entire response into HTTP format
     * Builds status line, headers, and appends body
     * @return Raw HTTP response string
     */
    std::string serialize() const;

    // ========================================
    // Convenience Methods
    // ========================================

    /**
     * Set the Content-Type header
     * @param type MIME type (e.g., "text/html")
     */
    void setContentType(const std::string& type);

    /**
     * Set the Content-Length header
     * @param length Size in bytes
     */
    void setContentLength(size_t length);

    // ========================================
    // Static Factory Methods
    // ========================================

    /**
     * Create a complete error response with HTML body
     * @param code HTTP error code
     * @param message Optional custom error message
     * @return Fully formed HttpResponse object
     */
    static HttpResponse createErrorResponse(int code, const std::string& message = "");

    /**
     * Create a redirect response
     * @param code Redirect status code (e.g., 301, 302)
     * @param location Target URI for Location header
     * @return Fully formed HttpResponse object
     */
    static HttpResponse createRedirectResponse(int code, const std::string& location);

    /**
     * Create a 200 OK response
     * @param body Body content
     * @param content_type MIME type
     * @return Fully formed HttpResponse object
     */
    static HttpResponse createOkResponse(const std::string& body, const std::string& content_type = "text/html");

    // ========================================
    // HTTP Status Code Utilities
    // ========================================

    /**
     * Get the standard status message for a code
     * @param code HTTP status code
     * @return Status string (e.g., "Not Found" for 404)
     */
    static std::string getStatusMessage(int code);
};
} // namespace wsv

#endif // HTTP_RESPONSE_HPP
