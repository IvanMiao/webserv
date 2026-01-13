#ifndef HTTPREQUEST_HPP
#define HTTPREQUEST_HPP

#include <string>
#include <map>

#include "utils/StringUtils.hpp"

namespace wsv
{

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
 *   while (receiving_data)
 *   {
 *       request.parse(data, len);
 *       if (request.isComplete()) break;
 *       if (request.hasError()) handle_error();
 *   }
 *   std::string body = request.getBody();
 */
class HttpRequest
{
public:
    // ===== Type Definitions =====
    typedef std::map<std::string, std::string> HeaderMap;
    
    // ===== Constants =====
    static const size_t MAX_REQUEST_LINE_SIZE = 8192;   // 8KB max for request line
    static const size_t MAX_HEADER_SIZE = 8192;         // 8KB max for all headers
    static const size_t MAX_CHUNK_SIZE_LINE = 256;      // Max length for chunk size line

private:
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

public:

    HttpRequest();
    explicit HttpRequest(const std::string& raw_request);
    ~HttpRequest();

    // ===== Core Methods =====
    /**
     * Parse incoming data progressively
     * Can be called multiple times with partial data
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
    
    // Reset request to initial state
    // Allows reusing the same object for multiple requests
    void reset();


    // ===== Getters - Request Line =====
    std::string getMethod() const { return _method; }
    std::string getPath() const { return _path; }
    std::string getQuery() const { return _query; }
    std::string getVersion() const { return _version; }


    // ===== Getters - Headers =====

    // Get header value by key (e.g., "Content-Type")
    std::string getHeader(const std::string& key) const;

    // Check if header exists (case-insensitive)
    bool hasHeader(const std::string& key) const;

    // Get all headers (keys are lowercase)
    HeaderMap getHeaders() const { return _headers; }


    // ===== Getters - Body =====
    std::string getBody() const { return _body; }
    size_t getContentLength() const { return _content_length; }
    size_t getBodyReceived() const { return _body_received; }


    // ===== State Queries =====
    bool isComplete() const { return _state == PARSE_COMPLETE; }
    bool hasError() const { return _state == PARSE_ERROR; }
    ParseState getState() const { return _state; }
    
    // true if "Transfer-Encoding: chunked" is present
    bool isChunked() const;

private:
    // ===== Parsing State Machine Methods =====

    /**
     * Try to parse request line/headers/body from buffer
     * @return true if parsed successfully, false if need more data
     */
    bool _tryParseRequestLine();
    bool _tryParseHeaders();
    bool _tryParseBody();

    // Transition from headers to body or complete state
    void _transitionToBodyOrComplete();


    // ===== Request Line Parsing =====
    void _parseRequestLine(const std::string& line);
    void _parseUrl(const std::string& url);
    

    // ===== Header Parsing =====
    void _parseHeaderLine(const std::string& line);


    // ===== Body Parsing =====
    
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


    // ===== Validation Methods =====
    
    // true if method is GET, POST, DELETE, HEAD
    bool _validateMethod(const std::string& method) const;
    
    // true if version is HTTP/1.0 or HTTP/1.1
    bool _validateVersion(const std::string& version) const;

};

} // namespace wsv

#endif
