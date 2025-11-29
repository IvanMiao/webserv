
## Resource
- [HTTP Intro](https://textbook.cs168.io/applications/http.html)


- [RFC 7230](https://datatracker.ietf.org/doc/html/rfc7230)
- [RFC 7231](https://datatracker.ietf.org/doc/html/rfc7231)


## Structure

HTTP 报文结构：
```
start-line      		(起始行)
*( header-field CRLF )  (零个或多个头部字段)
CRLF            		(空行，表示头部结束)
[ message-body ] 		(可选的消息体)
```

- start-line: 
	- Request: `METHOD target HTTP-version` (例如: `GET /index.html HTTP/1.1`)
	- Response: `HTTP-version status-code reason-phrase` (例如: `HTTP/1.1 200 OK`)
- Host Header:
	- e.g. `Host: www.example.com`
	- RFC 7230 规定，在 HTTP/1.1 中，Host 头部字段是必须的
	- 一台服务器（一个 IP）可能托管了 a.com 和 b.com 两个网站。
	- 这就是为什么在本地开发时配置 `Nginx/Apache` 反向代理需要设置 `server_name`。因为服务器不是靠 IP，而是靠 `Host Header` 来区分请求是发给哪个网站的。如果客户端发请求没带 Host，服务器会直接报 `400 Bad Request`。
- Connection Header:
	- e.g. `Connection: keep-alive`



The following example illustrates a typical message exchange for a GET request (Section 4.3.1 of [RFC7231]) on the URI "http://www.example.com/hello.txt":

```
Client request:

	GET /hello.txt HTTP/1.1
	User-Agent: curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3
	Host: www.example.com
	Accept-Language: en, mi
```

```
Server response:

	HTTP/1.1 200 OK
	Date: Mon, 27 Jul 2009 12:28:53 GMT
	Server: Apache
	Last-Modified: Wed, 22 Jul 2009 19:15:56 GMT
	ETag: "34aa387-d-1568eb00"
	Accept-Ranges: bytes
	Content-Length: 51
	Vary: Accept-Encoding
	Content-Type: text/plain

	Hello World! My payload includes a trailing CRLF.
```
