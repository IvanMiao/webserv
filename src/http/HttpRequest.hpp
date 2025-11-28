#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <string>
#include <map>


/*
State Machine for parsing the Request
	- START_LINE : start line, e.g. GET /index.html HTTP/1.1
	- HEADERS
	- BODY
	- CHUNK...
*/
enum ParseState
{

};

class HttpRequest
{
private:
	std::string _raw_buffer;

	std::string _method;
	std::string _uri_target;
	std::string _http_version;

	std::map<std::string, std::string> _headers;

public:
	HttpRequest();
	~HttpRequest();

	bool parseRawData( const char *data, size_t len );
};

#endif
