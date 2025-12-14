#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>
#include <vector>
#include <cstddef>

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

#endif

