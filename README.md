# webserv
A Nginx-like webserv in C++

## TODO

1. how to write a Nginx conf file? how many keywords/blocks... should we manage?
	- Main Context: `server`
	- Server Context: `listen`(port), `host`(host IP), `error_page` (code + route), `client_max_body_size`
	- Location Context: `allow_methods`, `root`, `index` (default index), `return`(redirection), CGI conf

2. how to initialize a server

3. choose `poll` or `epoll`?

4. a Logger? (DEBUG, INFO, ERROR)
	- a simple singleton class

5. HTTP Request & Response specification?
	- [RFC 7230](https://datatracker.ietf.org/doc/html/rfc7230)
	- [RFC 7231](https://datatracker.ietf.org/doc/html/rfc7231)
	- state-machine based parser to handle chunked transfer encoding and partial requests

## Code Spec

1. 每个 header 文件有 #define Guard
2. 每个文件(.cpp, .hpp)有 namespace 包裹， 主要的 namespace 为 **`wsv`**

### Names

1. class name: UpperCamelCase format
2. 私有变量和私有函数: 一律以 `_` 开头命名
3. 函数名： `myFunction()` (第一个单词小写，之后每一个新单词首字母大写)
4. 变量名： `snake_case` （全部小写，单词间用下划线分隔）


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

## Run test for unit
CgiHandler:
```
g++ -Wall -Wextra -Werror \
    -I./src/cgi -I./src/http -I./src/config -I./test \
    ./test/test_main.cpp \
    ./test/CgiHandlerTest.cpp \
    ./src/cgi/CgiHandler.cpp \
    ./src/http/HttpRequest.cpp \
    ./src/http/HttpResponse.cpp \
    ./src/config/ConfigParser.cpp \
    -o webserv_tests
```