#include "HttpRequest.hpp"
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <iostream>

HttpRequest::HttpRequest()
    : _state(PARSING_REQUEST_LINE),
      _content_length(0),
      _body_received(0),
      _chunk_size(0),
      _chunk_finished(true)
{
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
