# HTTP 协议实现

本项目实现了一个符合 HTTP/1.1 标准的 Web 服务器。本节介绍 HTTP 协议的基础知识以及在本项目中的具体实现。

## 1. HTTP 报文结构

HTTP 报文由起始行 (Start-line)、头部字段 (Header fields)、空行 (CRLF) 和可选的消息体 (Message body) 组成。

```
start-line
*( header-field CRLF )
CRLF
[ message-body ]
```

### 1.1 请求报文 (Request)
- **格式**: `METHOD target HTTP-version`
- **示例**: `GET /index.html HTTP/1.1`
- **本项目支持的方法**: `GET`, `POST`, `DELETE`。

### 1.2 响应报文 (Response)
- **格式**: `HTTP-version status-code reason-phrase`
- **示例**: `HTTP/1.1 200 OK`

## 2. 核心类设计

项目中的 HTTP 处理逻辑封装在 `src/http/` 目录下：

### 2.1 `HttpRequest` 类
负责解析客户端发送的原始字符串。
- **状态机解析**：支持渐进式解析（Incremental Parsing），能够处理分包到达的 TCP 数据流。
- **Chunked 传输**：支持 `Transfer-Encoding: chunked` 编码的解析。
- **数据提取**：解析并存储 Method, Path, Query String, Headers 和 Body。

### 2.2 `HttpResponse` 类
负责构建并序列化发往客户端的响应。
- **状态码管理**：支持常见的 HTTP 状态码（200, 301, 400, 403, 404, 405, 500, 504 等）。
- **自动头部生成**：根据响应内容自动设置 `Content-Length`, `Content-Type`, `Date`, `Server` 等头部。
- **错误页生成**：提供静态方法快速生成标准或自定义的错误页面。

## 3. 关键特性实现

### 3.1 Host 头部处理
根据 RFC 7230，HTTP/1.1 请求必须包含 `Host` 头部。本项目不实现**虚拟主机 (Virtual Hosting)**(同一个 IP 和端口可以根据不同的域名分发到不同的 `ServerConfig`)，本项目的同一个IP:port只对应一个ServerConfig。

### 3.2 Connection: keep-alive
支持长连接。如果请求头包含 `Connection: keep-alive`（或 HTTP/1.1 默认），服务器在发送完响应后不会立即关闭连接，而是重置 `HttpRequest` 对象，等待下一个请求。

### 3.3 静态资源与 MIME 类型
服务器根据请求的文件后缀名（如 `.html`, `.jpg`, `.css`）自动识别并设置 `Content-Type`。

## 4. 交互示例

### 客户端请求
```http
GET /hello.txt HTTP/1.1
Host: www.example.com
User-Agent: curl/7.16.3
Accept: */*
```

### 服务器响应
```http
HTTP/1.1 200 OK
Date: Mon, 27 Jul 2009 12:28:53 GMT
Server: Webserv/1.0
Content-Type: text/plain
Content-Length: 13

Hello World!
```

## 参考资源
- [RFC 7230: Message Syntax and Routing](https://datatracker.ietf.org/doc/html/rfc7230)
- [RFC 7231: Semantics and Content](https://datatracker.ietf.org/doc/html/rfc7231)
- [HTTP Intro - CS168](https://textbook.cs168.io/applications/http.html)
