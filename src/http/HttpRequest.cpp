#include "HttpRequest.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>

namespace wsv
{

// Constructors & Destructor
HttpRequest::HttpRequest()
    : _state(PARSING_REQUEST_LINE)
    , _content_length(0)
    , _body_received(0)
    , _chunk_size(0)
    , _chunk_finished(true)
{ }

HttpRequest::HttpRequest(const std::string& raw_request) 
    : _state(PARSING_REQUEST_LINE)
    , _content_length(0)
    , _body_received(0)
    , _chunk_size(0)
    , _chunk_finished(true)
{
    // Parse entire request at once
    ParseState result = parse(raw_request.c_str(), raw_request.length());
    
    // Throw exception if parsing incomplete or failed
    if (result != PARSE_COMPLETE)
        throw std::runtime_error("Incomplete or malformed HTTP request");
}

HttpRequest::~HttpRequest()
{ }


// ===== Core Methods =====

void HttpRequest::reset()
{
    _state = PARSING_REQUEST_LINE;
    _method.clear();
    _path.clear();
    _query.clear();
    _version.clear();
    _headers.clear();
    _body.clear();
    _buffer.clear();
    _content_length = 0;
    _body_received = 0;
    _chunk_size = 0;
    _chunk_finished = true;
}

ParseState HttpRequest::parse(const char* data, size_t len)
{
    // Append new data to buffer
    _buffer.append(data, len);

    // Process buffer based on current state
    while (!_buffer.empty())
    {
        if (_state == PARSING_REQUEST_LINE)
        {
            if (!_tryParseRequestLine())
                break;  // Need more data
        }
        else if (_state == PARSING_HEADERS)
        {
            if (!_tryParseHeaders())
                break;  // Need more data
        }
        else if (_state == PARSING_BODY)
        {
            if (!_tryParseBody())
                break;  // Need more data
        }
        else
        {
            // PARSE_COMPLETE or PARSE_ERROR
            break;
        }
    }

    return _state;
}


// ===== Request Line Parsing =====

bool HttpRequest::_tryParseRequestLine()
{
    // Look for line ending
    size_t line_end = _buffer.find("\r\n");
    
    if (line_end == std::string::npos)
    {
        // Check if buffer exceeds maximum allowed size
        if (_buffer.size() > MAX_REQUEST_LINE_SIZE)
        {
            _state = PARSE_ERROR;
        }
        return false;  // Need more data
    }

    // Check if request line exceeds limit
    if (line_end > MAX_REQUEST_LINE_SIZE)
    {
        _state = PARSE_ERROR;
        return false;
    }

    // Extract and parse request line
    std::string request_line = _buffer.substr(0, line_end);
    _buffer.erase(0, line_end + 2);  // Remove line and \r\n
    
    _parseRequestLine(request_line);
    
    if (_state != PARSE_ERROR)
    {
        _state = PARSING_HEADERS;
    }
    
    return true;
}

void HttpRequest::_parseRequestLine(const std::string& line)
{
    std::istringstream stream(line);
    std::string method, url, version;

    // Parse: METHOD URL VERSION
    if (!(stream >> method >> url >> version))
    {
        _state = PARSE_ERROR;
        return;
    }

    // Validate method and version
    if (!_validateMethod(method) || !_validateVersion(version))
    {
        _state = PARSE_ERROR;
        return;
    }

    // Store parsed values
    _method = method;
    _version = version;
    _parseUrl(url);
}

void HttpRequest::_parseUrl(const std::string& url)
{
    // Split URL into path and query string
    // Example: "/api/users?id=123" -> path="/api/users", query="id=123"
    size_t query_start = url.find('?');
    
    if (query_start != std::string::npos)
    {
        _path = url.substr(0, query_start);
        _query = url.substr(query_start + 1);
    }
    else
    {
        _path = url;
        _query.clear();
    }
}

// ===== Headers Parsing =====

bool HttpRequest::_tryParseHeaders()
{
    while (true)
    {
        // Look for line ending
        size_t line_end = _buffer.find("\r\n");
        
        if (line_end == std::string::npos)
        {
            // Check if buffer exceeds maximum size
            if (_buffer.size() > MAX_HEADER_SIZE)
            {
                _state = PARSE_ERROR;
            }
            return false;  // Need more data
        }

        // Check if header line exceeds limit
        // Note: This is a simple check per line. For total header size limit, 
        // we would need to track total bytes consumed by headers.
        if (line_end > MAX_HEADER_SIZE)
        {
            _state = PARSE_ERROR;
            return false;
        }

        // Extract line
        std::string line = _buffer.substr(0, line_end);
        _buffer.erase(0, line_end + 2);

        // Empty line indicates end of headers
        if (line.empty())
        {
            _transitionToBodyOrComplete();
            return true;
        }

        // Parse header line
        _parseHeaderLine(line);
    }
}

void HttpRequest::_parseHeaderLine(const std::string& line)
{
    // Find colon separator
    size_t colon_pos = line.find(':');
    
    if (colon_pos == std::string::npos)
    {
        return;  // Malformed header, skip it
    }

    // Extract and normalize key (lowercase)
    std::string key = StringUtils::toLower(StringUtils::trim(line.substr(0, colon_pos)));
    
    // Extract and trim value
    std::string value = StringUtils::trim(line.substr(colon_pos + 1));

    // Store header
    _headers[key] = value;

    // Handle special headers
    if (key == "content-length")
    {
        _content_length = static_cast<size_t>(std::atoi(value.c_str()));
    }
}

void HttpRequest::_transitionToBodyOrComplete()
{
    // Check if request has a body
    if (isChunked() || _content_length > 0)
        _state = PARSING_BODY;
    else
        // No body, request is complete
        _state = PARSE_COMPLETE;
}

// ===== Body Parsing =====

bool HttpRequest::_tryParseBody()
{
    if (isChunked())
        return _parseChunkedBody();
    else
        return _parseContentLengthBody();
}

bool HttpRequest::_parseContentLengthBody()
{
    // Calculate how much to read
    size_t bytes_needed = _content_length - _body_received;
    size_t bytes_available = _buffer.size();
    size_t bytes_to_read = (bytes_available < bytes_needed) 
                          ? bytes_available 
                          : bytes_needed;

    // Append to body
    _body.append(_buffer.substr(0, bytes_to_read));
    _buffer.erase(0, bytes_to_read);
    _body_received += bytes_to_read;

    // Check if body is complete
    if (_body_received >= _content_length)
    {
        _state = PARSE_COMPLETE;
        return true;
    }

    return false;  // Need more data
}

bool HttpRequest::_parseChunkedBody()
{
    while (true)
    {
        // State 1: Reading chunk size line
        if (_chunk_finished)
        {
            if (!_tryReadChunkSize())
            {
                return false;  // Need more data
            }
            
            // Check for last chunk (size 0)
            if (_chunk_size == 0)
            {
                _state = PARSE_COMPLETE;
                return true;
            }
        }

        // State 2: Reading chunk data
        if (!_tryReadChunkData())
        {
            return false;  // Need more data
        }
    }
}

bool HttpRequest::_tryReadChunkSize()
{
    // Look for chunk size line ending
    size_t line_end = _buffer.find("\r\n");
    
    if (line_end == std::string::npos)
    {
        return false;  // Need more data
    }

    // Extract and parse chunk size
    std::string size_line = _buffer.substr(0, line_end);
    _buffer.erase(0, line_end + 2);

    _chunk_size = _parseChunkSize(size_line);
    _chunk_finished = false;
    
    return true;
}

bool HttpRequest::_tryReadChunkData()
{
    // Check if we have enough data for chunk + trailing \r\n
    if (_buffer.size() < _chunk_size + 2)
    {
        return false;  // Need more data
    }

    // Append chunk data to body
    _body.append(_buffer.substr(0, _chunk_size));
    
    // Remove chunk data and trailing \r\n
    _buffer.erase(0, _chunk_size + 2);
    
    // Mark chunk as finished
    _chunk_finished = true;
    
    return true;
}

size_t HttpRequest::_parseChunkSize(const std::string& line)
{
    // Chunk size is in hexadecimal
    // May be followed by chunk extensions after ';'
    // Example: "1a3" or "1a3;name=value"
    
    size_t size = 0;
    std::istringstream stream(line);
    
    // Parse hex number
    stream >> std::hex >> size;
    
    return size;
}


// ===== Validation Methods =====

bool HttpRequest::_validateMethod(const std::string& method) const
{
    return method == "GET" || 
           method == "POST" || 
           method == "DELETE" ||
           method == "HEAD" ||
           method == "PUT";
}

bool HttpRequest::_validateVersion(const std::string& version) const
{
    return version == "HTTP/1.0" || version == "HTTP/1.1";
}


// ===== Getters =====

std::string HttpRequest::getHeader(const std::string& key) const
{
    // Case-insensitive lookup
    HeaderMap::const_iterator it = _headers.find(StringUtils::toLower(key));
    
    if (it != _headers.end())
        return it->second;
    
    return "";
}

bool HttpRequest::hasHeader(const std::string& key) const
{
    return _headers.find(StringUtils::toLower(key)) != _headers.end();
}

bool HttpRequest::isChunked() const
{
    HeaderMap::const_iterator it = _headers.find("transfer-encoding");
    
    if (it != _headers.end())
        return StringUtils::toLower(it->second) == "chunked";
    
    return false;
}

} // namespace wsv
