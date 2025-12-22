#include "HttpRequest.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>

namespace wsv
{

// ========================================
// Constructors & Destructor
// ========================================

HttpRequest::HttpRequest()
    : _state(PARSING_REQUEST_LINE)
    , _content_length(0)
    , _body_received(0)
    , _chunk_size(0)
    , _chunk_finished(true)
{
}

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
    {
        throw std::runtime_error("Incomplete or malformed HTTP request");
    }
}

HttpRequest::~HttpRequest()
{
}

// ========================================
// Core Methods
// ========================================

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

// ========================================
// Request Line Parsing
// ========================================

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

// ========================================
// Headers Parsing
// ========================================

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
    std::string key = _toLower(_trim(line.substr(0, colon_pos)));
    
    // Extract and trim value
    std::string value = _trim(line.substr(colon_pos + 1));

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
    {
        _state = PARSING_BODY;
    }
    else
    {
        // No body, request is complete
        _state = PARSE_COMPLETE;
    }
}

// ========================================
// Body Parsing
// ========================================

bool HttpRequest::_tryParseBody()
{
    if (isChunked())
    {
        return _parseChunkedBody();
    }
    else
    {
        return _parseContentLengthBody();
    }
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

// ========================================
// Validation Methods
// ========================================

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

// ========================================
// Getters
// ========================================

std::string HttpRequest::getHeader(const std::string& key) const
{
    // Case-insensitive lookup
    HeaderMap::const_iterator it = _headers.find(_toLower(key));
    
    if (it != _headers.end())
    {
        return it->second;
    }
    
    return "";
}

bool HttpRequest::hasHeader(const std::string& key) const
{
    return _headers.find(_toLower(key)) != _headers.end();
}

bool HttpRequest::isChunked() const
{
    HeaderMap::const_iterator it = _headers.find("transfer-encoding");
    
    if (it != _headers.end())
    {
        return _toLower(it->second) == "chunked";
    }
    
    return false;
}

// ========================================
// Utility Methods
// ========================================

std::string HttpRequest::_trim(const std::string& str) const
{
    size_t start = 0;
    size_t end = str.size();

    // Trim leading whitespace
    while (start < end && std::isspace(str[start]))
    {
        start++;
    }

    // Trim trailing whitespace
    while (end > start && std::isspace(str[end - 1]))
    {
        end--;
    }

    return str.substr(start, end - start);
}

std::string HttpRequest::_toLower(const std::string& str) const
{
    std::string result = str;
    
    for (size_t i = 0; i < result.size(); ++i)
    {
        result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
    }
    
    return result;
}

} // namespace wsv

/* #include "HttpRequest.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>

namespace wsv
{

HttpRequest::HttpRequest()
    : _state(PARSING_REQUEST_LINE),
      _content_length(0),
      _body_received(0),
      _chunk_size(0),
      _chunk_finished(true)
{
}

HttpRequest::HttpRequest(const std::string& raw_request) 
    : _state(PARSING_REQUEST_LINE), _content_length(0), 
      _body_received(0), _chunk_size(0), _chunk_finished(false)
{
    // 调用解析函数，将整个原始请求作为数据传入
    ParseState result = parse(raw_request.c_str(), raw_request.length());
    
    // 如果请求没有完全解析完毕，抛出异常或进行处理
    if (result != PARSE_COMPLETE)
    {
        // 假设您有一个适当的异常类
        throw std::runtime_error("Incomplete or malformed request provided to constructor.");
    }
}

HttpRequest::~HttpRequest()
{
}

void HttpRequest::reset()
{
    this->_state = PARSING_REQUEST_LINE;
    this->_method.clear();
    this->_path.clear();
    this->_query.clear();
    this->_version.clear();
    this->_headers.clear();
    this->_body.clear();
    this->_buffer.clear();
    this->_content_length = 0;
    this->_body_received = 0;
    this->_chunk_size = 0;
    this->_chunk_finished = true;
}

ParseState HttpRequest::parse(const char* data, size_t len)
{
    this->_buffer.append(data, len);

    while (!this->_buffer.empty())
    {
        if (this->_state == PARSING_REQUEST_LINE)
        {
            size_t pos = this->_buffer.find("\r\n");
            if (pos == std::string::npos)
            {
                if (this->_buffer.size() > MAX_REQUEST_LINE_SIZE)
                {
                    this->_state = PARSE_ERROR;
                    return this->_state;
                }
                break;
            }

            std::string line = this->_buffer.substr(0, pos);
            this->_buffer.erase(0, pos + 2);
            this->_parseRequestLine(line);
            this->_state = PARSING_HEADERS;
        }
        else if (this->_state == PARSING_HEADERS)
        {
            size_t pos = this->_buffer.find("\r\n");
            if (pos == std::string::npos)
            {
                if (this->_buffer.size() > MAX_HEADER_SIZE)
                {
                    this->_state = PARSE_ERROR;
                    return this->_state;
                }
                break;
            }

            std::string line = this->_buffer.substr(0, pos);
            this->_buffer.erase(0, pos + 2);

            if (line.empty())
            {
                if (this->isChunked() || this->_content_length > 0)
                    this->_state = PARSING_BODY;
                else
                    this->_state = PARSE_COMPLETE;
                continue;
            }

            this->_parseHeaderLine(line);
        }
        else if (this->_state == PARSING_BODY)
        {
            if (this->isChunked())
            {
                if (!this->_parseChunkedBody())
                    break;
            }
            else
            {
                size_t to_read = this->_content_length - this->_body_received;
                if (this->_buffer.size() < to_read)
                    to_read = this->_buffer.size();

                this->_body.append(this->_buffer.substr(0, to_read));
                this->_buffer.erase(0, to_read);
                this->_body_received += to_read;

                if (this->_body_received >= this->_content_length)
                    this->_state = PARSE_COMPLETE;
            }
        }
        else
        {
            break;
        }
    }

    return this->_state;
}

// -------------------------
// Chunked body parsing
// -------------------------

bool HttpRequest::_parseChunkedBody()
{
    while (true)
    {
        if (this->_chunk_finished)
        {
            size_t pos = this->_buffer.find("\r\n");
            if (pos == std::string::npos)
                return false;

            std::string line = this->_buffer.substr(0, pos);
            this->_buffer.erase(0, pos + 2);

            this->_chunk_size = this->_parseChunkSize(line);
            if (this->_chunk_size == 0)
            {
                this->_state = PARSE_COMPLETE;
                return true;
            }
            this->_chunk_finished = false;
        }

        if (this->_buffer.size() < this->_chunk_size + 2)
            return false;

        this->_body.append(this->_buffer.substr(0, this->_chunk_size));
        this->_buffer.erase(0, this->_chunk_size + 2);
        this->_chunk_finished = true;
    }
}

size_t HttpRequest::_parseChunkSize(const std::string& line)
{
    size_t size = 0;
    std::istringstream iss(line);
    iss >> std::hex >> size;
    return size;
}

// -------------------------
// Helper functions
// -------------------------

void HttpRequest::_parseRequestLine(const std::string& line)
{
    std::istringstream iss(line);
    std::string method, url, version;

    if (!(iss >> method >> url >> version))
    {
        this->_state = PARSE_ERROR;
        return;
    }

    if (!this->_validateMethod(method) || !this->_validateVersion(version))
    {
        this->_state = PARSE_ERROR;
        return;
    }

    this->_method = method;
    this->_version = version;
    this->_parseUrl(url);
}

void HttpRequest::_parseHeaderLine(const std::string& line)
{
    size_t pos = line.find(':');
    if (pos == std::string::npos)
        return;

    std::string key = this->_toLower(this->_trim(line.substr(0, pos)));
    std::string value = this->_trim(line.substr(pos + 1));

    this->_headers[key] = value;

    if (key == "content-length")
        this->_content_length = static_cast<size_t>(atoi(value.c_str()));
}

void HttpRequest::_parseUrl(const std::string& url)
{
    size_t pos = url.find('?');
    if (pos != std::string::npos)
    {
        this->_path = url.substr(0, pos);
        this->_query = url.substr(pos + 1);
    }
    else
    {
        this->_path = url;
        this->_query.clear();
    }
}

bool HttpRequest::_validateMethod(const std::string& method)
{
    return method == "GET" || method == "POST" || method == "DELETE";
}

bool HttpRequest::_validateVersion(const std::string& version)
{
    return version == "HTTP/1.0" || version == "HTTP/1.1";
}

std::string HttpRequest::getHeader(const std::string& key) const
{
    std::map<std::string, std::string>::const_iterator it =
        this->_headers.find(this->_toLower(key));

    if (it != this->_headers.end())
        return it->second;
    return "";
}

bool HttpRequest::hasHeader(const std::string& key) const
{
    return this->_headers.find(this->_toLower(key)) != this->_headers.end();
}

bool HttpRequest::isChunked() const
{
    std::map<std::string, std::string>::const_iterator it =
        this->_headers.find("transfer-encoding");

    if (it != this->_headers.end())
        return this->_toLower(it->second) == "chunked";
    return false;
}

std::string HttpRequest::_trim(const std::string& str) const
{
    size_t start = 0;
    size_t end = str.size();

    while (start < end && isspace(str[start])) start++;
    while (end > start && isspace(str[end - 1])) end--;

    return str.substr(start, end - start);
}

std::string HttpRequest::_toLower(const std::string& str) const
{
    std::string result = str;
    for (size_t i = 0; i < result.size(); ++i)
        result[i] = static_cast<char>(tolower(result[i]));
    return result;
}

} // namespace wsv */
