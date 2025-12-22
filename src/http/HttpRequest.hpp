#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

namespace wsv
{

/**
 * ParseState - HTTP request parsing state machine states
 */
enum ParseState
{
    PARSING_REQUEST_LINE,   // Parsing "GET /path HTTP/1.1"
    PARSING_HEADERS,        // Parsing "Key: Value" headers
    PARSING_BODY,           // Parsing request body (POST data)
    PARSE_COMPLETE,         // Request fully parsed
    PARSE_ERROR             // Parse error occurred
};

/**
 * HttpRequest - Progressive HTTP/1.1 request parser
 * 
 * Features:
 * - Progressive parsing (can handle partial data)
 * - Chunked transfer encoding support
 * - Request line and header size limits
 * - Content-Length based body parsing
 * - Case-insensitive header lookups
 * 
 * Usage:
 *   HttpRequest request;
 *   while (receiving_data) {
 *       request.parse(data, len);
 *       if (request.isComplete()) break;
 *       if (request.hasError()) handle_error();
 *   }
 *   std::string body = request.getBody();
 */
class HttpRequest
{
public:
    // ========================================
    // Type Definitions
    // ========================================
    
    typedef std::map<std::string, std::string> HeaderMap;
    
    // ========================================
    // Constants
    // ========================================
    
    static const size_t MAX_REQUEST_LINE_SIZE = 8192;   // 8KB max for request line
    static const size_t MAX_HEADER_SIZE = 8192;         // 8KB max for all headers
    static const size_t MAX_CHUNK_SIZE_LINE = 256;      // Max length for chunk size line
    
    // ========================================
    // Constructors & Destructor
    // ========================================
    
    /**
     * Default constructor
     * Creates an empty request ready for parsing
     */
    HttpRequest();
    
    /**
     * Constructor with full request data
     * Parses the entire request immediately
     * @param raw_request Complete HTTP request as string
     * @throws std::runtime_error if parsing fails
     */
    explicit HttpRequest(const std::string& raw_request);
    
    ~HttpRequest();
    
    // ========================================
    // Core Methods
    // ========================================
    
    /**
     * Parse incoming data progressively
     * Can be called multiple times with partial data
     * 
     * @param data Pointer to data buffer
     * @param len Length of data
     * @return Current parse state
     * 
     * Example:
     *   char buffer[1024];
     *   ssize_t n = read(fd, buffer, sizeof(buffer));
     *   ParseState state = request.parse(buffer, n);
     *   if (state == PARSE_COMPLETE) { ... }
     */
    ParseState parse(const char* data, size_t len);
    
    /**
     * Reset request to initial state
     * Allows reusing the same object for multiple requests
     */
    void reset();
    
    // ========================================
    // Getters - Request Line
    // ========================================
    
    std::string getMethod() const { return _method; }
    std::string getPath() const { return _path; }
    std::string getQuery() const { return _query; }
    std::string getVersion() const { return _version; }
    
    // ========================================
    // Getters - Headers
    // ========================================
    
    /**
     * Get header value by key (case-insensitive)
     * @param key Header name (e.g., "Content-Type")
     * @return Header value, or empty string if not found
     */
    std::string getHeader(const std::string& key) const;
    
    /**
     * Check if header exists (case-insensitive)
     * @param key Header name
     * @return true if header exists
     */
    bool hasHeader(const std::string& key) const;
    
    /**
     * Get all headers
     * @return Map of all headers (keys are lowercase)
     */
    HeaderMap getHeaders() const { return _headers; }
    
    // ========================================
    // Getters - Body
    // ========================================
    
    std::string getBody() const { return _body; }
    size_t getContentLength() const { return _content_length; }
    size_t getBodyReceived() const { return _body_received; }
    
    // ========================================
    // State Queries
    // ========================================
    
    bool isComplete() const { return _state == PARSE_COMPLETE; }
    bool hasError() const { return _state == PARSE_ERROR; }
    ParseState getState() const { return _state; }
    
    /**
     * Check if request uses chunked transfer encoding
     * @return true if "Transfer-Encoding: chunked" is present
     */
    bool isChunked() const;

private:
    // ========================================
    // Parsing State Machine Methods
    // ========================================
    
    /**
     * Try to parse request line from buffer
     * @return true if parsed successfully, false if need more data
     */
    bool _tryParseRequestLine();
    
    /**
     * Try to parse headers from buffer
     * @return true if all headers parsed, false if need more data
     */
    bool _tryParseHeaders();
    
    /**
     * Try to parse body from buffer
     * @return true if body parsed, false if need more data
     */
    bool _tryParseBody();
    
    /**
     * Transition from headers to body or complete state
     */
    void _transitionToBodyOrComplete();
    
    // ========================================
    // Request Line Parsing
    // ========================================
    
    /**
     * Parse request line: "GET /path HTTP/1.1"
     * @param line The request line string
     */
    void _parseRequestLine(const std::string& line);
    
    /**
     * Parse URL into path and query string
     * Example: "/api/users?id=123" -> path="/api/users", query="id=123"
     * @param url The URL string
     */
    void _parseUrl(const std::string& url);
    
    // ========================================
    // Header Parsing
    // ========================================
    
    /**
     * Parse a single header line: "Key: Value"
     * @param line The header line string
     */
    void _parseHeaderLine(const std::string& line);
    
    // ========================================
    // Body Parsing
    // ========================================
    
    /**
     * Parse content-length based body
     * @return true if complete, false if need more data
     */
    bool _parseContentLengthBody();
    
    /**
     * Parse chunked request body
     * Handles chunk size lines and chunk data according to HTTP/1.1 spec
     * @return true if parsing can continue, false if more data needed
     */
    bool _parseChunkedBody();
    
    /**
     * Try to read chunk size line
     * @return true if size read, false if need more data
     */
    bool _tryReadChunkSize();
    
    /**
     * Try to read chunk data
     * @return true if chunk read, false if need more data
     */
    bool _tryReadChunkData();
    
    /**
     * Parse chunk size from hex string
     * Example: "1a3" -> 419, "0" -> 0 (last chunk)
     * @param line Chunk size line (may contain extensions after ';')
     * @return Chunk size in bytes
     */
    size_t _parseChunkSize(const std::string& line);
    
    // ========================================
    // Validation Methods
    // ========================================
    
    /**
     * Validate HTTP method
     * @param method Method string to validate
     * @return true if method is GET, POST, DELETE, HEAD, or PUT
     */
    bool _validateMethod(const std::string& method) const;
    
    /**
     * Validate HTTP version
     * @param version Version string to validate
     * @return true if version is HTTP/1.0 or HTTP/1.1
     */
    bool _validateVersion(const std::string& version) const;
    
    // ========================================
    // Utility Methods
    // ========================================
    
    /**
     * Trim leading and trailing whitespace
     * @param str String to trim
     * @return Trimmed string
     */
    std::string _trim(const std::string& str) const;
    
    /**
     * Convert string to lowercase
     * Used for case-insensitive header comparison
     * @param str String to convert
     * @return Lowercase string
     */
    std::string _toLower(const std::string& str) const;
    
    // ========================================
    // Member Variables
    // ========================================
    
    // Parsing state
    ParseState _state;
    
    // Request line components
    std::string _method;        // GET, POST, DELETE
    std::string _path;          // /api/users
    std::string _query;         // id=123&name=test
    std::string _version;       // HTTP/1.1
    
    // Headers (keys are lowercase)
    HeaderMap _headers;
    
    // Body
    std::string _body;
    size_t _content_length;
    size_t _body_received;
    
    // Parsing buffer (holds incomplete data)
    std::string _buffer;
    
    // Chunked encoding state
    size_t _chunk_size;         // Current chunk size being processed
    bool _chunk_finished;       // Whether current chunk is fully read
};

} // namespace wsv

#endif

/* #ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>
#include <vector>
#include <cstddef>

namespace wsv
{

enum ParseState
{
    PARSING_REQUEST_LINE,
    PARSING_HEADERS,
    PARSING_BODY,
    PARSE_COMPLETE,
    PARSE_ERROR
};

class HttpRequest
{
public:
    HttpRequest();
    HttpRequest(const std::string& raw_request);
    ~HttpRequest();

    // Incremental parse
    ParseState parse(const char* data, size_t len);

    void reset();  // Reset for keep-alive

    // Getters
    std::string getMethod() const { return _method; }
    std::string getPath() const { return _path; }
    std::string getQuery() const { return _query; }
    std::string getVersion() const { return _version; }
    std::string getBody() const { return _body; }

    std::string getHeader(const std::string& key) const;
    bool hasHeader(const std::string& key) const;

    size_t getContentLength() const { return _content_length; }
    bool isComplete() const { return _state == PARSE_COMPLETE; }
    bool hasError() const { return _state == PARSE_ERROR; }
    ParseState getState() const { return _state; }

    bool isChunked() const;
    std::string getHost() const { return getHeader("host"); }

private:
    // Parsing helpers
    void _parseRequestLine(const std::string& line);
    void _parseHeaderLine(const std::string& line);
    void _parseUrl(const std::string& url);
    bool _validateMethod(const std::string& method);
    bool _validateVersion(const std::string& version);

    // Chunked body helpers
    bool _parseChunkedBody();
    size_t _parseChunkSize(const std::string& line);

    // Utilities
    std::string _trim(const std::string& str) const;
    std::string _toLower(const std::string& str) const;

    // Request data
    std::string _method;
    std::string _path;
    std::string _query;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;

    // Parsing state
    ParseState _state;
    std::string _buffer;
    size_t _content_length;
    size_t _body_received;

    // Chunked parsing
    size_t _chunk_size;
    bool _chunk_finished;

    // Limits
    static const size_t MAX_HEADER_SIZE = 8192;
    static const size_t MAX_REQUEST_LINE_SIZE = 2048;
};

} // namespace wsv

#endif
 */
