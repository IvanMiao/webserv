# webserv
A Nginx-like webserver in C++

## Workflow

1. write a Nginx conf file
	- Main Context: `server`
	- Server Context: `listen`(port), `host`(host IP), `error_page` (code + route), `client_max_body_size`，
	`root`
	- Location Context: `allow_methods`, `root`, `autoindex`, `return`(redirection), CGI conf

2. use `epoll`

3. HTTP Request & Response specification
	- [RFC 7230](https://datatracker.ietf.org/doc/html/rfc7230)
	- [RFC 7231](https://datatracker.ietf.org/doc/html/rfc7231)
	- state-machine based parser to handle chunked transfer encoding and partial requests

## Code Spec

1. Each header file has a #define guard
2. Each file (.cpp, .hpp) is wrapped in a namespace, the main namespace is **`wsv`**

### Names

1. class name: UpperCamelCase format
2. Private variables and private functions: always start with `_`
3. Function names: `myFunction()` (first word lowercase, each subsequent word capitalized)
4. Variable names: `snake_case` (all lowercase, words separated by underscores)


```c++
namespace wsv{
class Name
{
private:
	std::string _name;

public:
	void	getName();
};
} // namespace wsv
```
