# HTTP Protocol Implementation

This project implements a Web server compliant with the **HTTP/1.1** specification.  
This section introduces the fundamentals of the HTTP protocol and explains how it is implemented in this project.

---

## 1. HTTP Message Structure

An HTTP message consists of a **start-line**, **header fields**, an **empty line (CRLF)**, and an optional **message body**.

```text
start-line
*( header-field CRLF )
CRLF
[ message-body ]
````

---

## Plot 1 — HTTP Message Layout

```text
┌─────────────────────────────────────────┐
│            Start Line                   │
│  METHOD /path HTTP/1.1                  │
├─────────────────────────────────────────┤
│            Headers                      │
│  Key: Value                             │
│  Key: Value                             │
├─────────────────────────────────────────┤
│        Empty Line (CRLF)                │
├─────────────────────────────────────────┤
│        Message Body (Optional)          │
└─────────────────────────────────────────┘
```

---

## 1.1 Request Message

* **Format**: `METHOD target HTTP-version`
* **Example**:

  ```
  GET /index.html HTTP/1.1
  ```
* **Methods supported by this project**:

  * `GET`
  * `POST`
  * `DELETE`

---

## 1.2 Response Message

* **Format**: `HTTP-version status-code reason-phrase`
* **Example**:

  ```
  HTTP/1.1 200 OK
  ```

---

## 2. Core Class Design

All HTTP-related logic is located in the `src/http/` directory.

---

### 2.1 `HttpRequest` Class

Responsible for parsing raw data received from the client.

**Key features**:

* **State-machine-based parsing**
  Supports **incremental parsing**, allowing correct handling of TCP packet fragmentation.
* **Chunked Transfer Encoding**
  Supports parsing of `Transfer-Encoding: chunked`.
* **Structured extraction**
  Parses and stores:

  * Method
  * Path
  * Query string
  * Headers
  * Body

---

### 2.2 `HttpResponse` Class

Responsible for building and serializing responses sent to the client.

**Key features**:

* **Status code management**
  Supports common HTTP status codes such as:
  `200`, `301`, `400`, `403`, `404`, `405`, `500`, `504`, etc.
* **Automatic header generation**
  Automatically sets:

  * `Content-Length`
  * `Content-Type`
  * `Date`
  * `Server`
* **Error page generation**
  Provides static helpers to generate standard or custom error pages.

---

## 3. Key Features Implementation

---

### 3.1 Host Header Handling

According to **RFC 7230**, HTTP/1.1 requests must include the `Host` header.

This project **does not implement virtual hosting**
(i.e., routing different domain names to different server configurations on the same IP and port).

👉 In this project:

* One `IP:port` corresponds to **exactly one `ServerConfig`**

---

### 3.2 `Connection: keep-alive`

The server supports **persistent connections**.

* If the request includes `Connection: keep-alive`
* Or if HTTP/1.1 default behavior applies

Then:

* The server does **not** close the connection immediately after responding
* The `HttpRequest` object is reset
* The server waits for the next request on the same connection

---

## Plot 2 — Keep-Alive Connection Flow

```text
Client                    Server
  |        Request 1        |
  | ----------------------> |
  |        Response 1       |
  | <---------------------- |
  |        Request 2        |
  | ----------------------> |
  |        Response 2       |
  | <---------------------- |
  |        (timeout)        |
  |        connection close |
```

---

### 3.3 Static Resources and MIME Types

The server determines the correct `Content-Type` based on the requested file extension, such as:

* `.html`
* `.css`
* `.js`
* `.jpg`
* `.png`

This ensures browsers correctly interpret static resources.

---

## 4. Interaction Examples

---

### Client Request

```http
GET /hello.txt HTTP/1.1
Host: www.example.com
User-Agent: curl/7.16.3
Accept: */*
```

**Request Components Visualization**

```text
┌─────────────────────────────────────────┐
│            Request Line                 │
│  GET /index.html HTTP/1.1               │
│  ────  ──────────  ────────             │
│  Method     Path        Version          │
├─────────────────────────────────────────┤
│            Headers                      │
│  Host: localhost:8080                  │
│  User-Agent: Chrome/120.0              │
│  Accept: text/html                     │
├─────────────────────────────────────────┤
│        Empty Line (Required!)           │
├─────────────────────────────────────────┤
│        Request Body (Optional)          │
│  name=John&age=30                      │
└─────────────────────────────────────────┘
```

---

### Server Response

```http
HTTP/1.1 200 OK
Date: Mon, 27 Jul 2009 12:28:53 GMT
Server: Webserv/1.0
Content-Type: text/plain
Content-Length: 13

Hello World!
```

**Response Components Visualization**

```text
┌─────────────────────────────────────────┐
│            Status Line                  │
│  HTTP/1.1 200 OK                        │
│  ────────  ───  ──                      │
│  Version   Status Code  Description     │
├─────────────────────────────────────────┤
│            Headers                     │
│  Content-Type: text/html               │
│  Content-Length: 1234                  │
│  Server: webserv/1.0                   │
├─────────────────────────────────────────┤
│        Empty Line (Required!)           │
├─────────────────────────────────────────┤
│            Response Body               │
│  <html>...</html>                      │
└─────────────────────────────────────────┘
```

---

## 5. HTTP Status Code Overview

```text
┌────────────────────────────────────────────────┐
│  1xx – Informational (rarely used)              │
│  100 Continue – Continue sending request body   │
├────────────────────────────────────────────────┤
│  2xx – Success                                  │
│  200 OK – Request succeeded                     │
│  201 Created – Resource created (after upload)  │
│  204 No Content – Success with no content        │
│        (after successful DELETE)                │
├────────────────────────────────────────────────┤
│  3xx – Redirection                              │
│  301 Moved Permanently – Permanent redirect     │
│  302 Found – Temporary redirect                 │
│  304 Not Modified – Resource not modified       │
│        (cache-related)                          │
├────────────────────────────────────────────────┤
│  4xx – Client Errors                            │
│  400 Bad Request – Invalid request format       │
│  403 Forbidden – Access denied                  │
│  404 Not Found – Resource does not exist        │
│  405 Method Not Allowed – Method not allowed    │
│  413 Payload Too Large – Request body too large │
├────────────────────────────────────────────────┤
│  5xx – Server Errors                            │
│  500 Internal Server Error – Internal error     │
│  501 Not Implemented – Method not implemented   │
│  502 Bad Gateway – Gateway error (proxy-related)│
│  504 Gateway Timeout – Gateway timeout (CGI)    │
└────────────────────────────────────────────────┘
```

---

## Final Notes

* HTTP parsing is fully state-driven and safe for partial TCP reads
* Persistent connections improve performance
* Correct status codes and headers are critical for browser compatibility
* The design aligns with the epoll-based, event-driven architecture described earlier

---

```
```


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

Composant:
┌─────────────────────────────────────────┐
│          Request Line                   │
│  GET /index.html HTTP/1.1               │
│  ────  ──────────  ────────             │
│  Method     Path        Version         │
├─────────────────────────────────────────┤
│          Headers                        │
│  Host: localhost:8080                  │
│  User-Agent: Chrome/120.0              │
│  Accept: text/html                     │
├─────────────────────────────────────────┤
│          Empty Line (Required!)         │
├─────────────────────────────────────────┤
│          Request Body (Optional)        │
│  name=John&age=30                      │
└─────────────────────────────────────────┘

```

### 服务器响应
```http
HTTP/1.1 200 OK
Date: Mon, 27 Jul 2009 12:28:53 GMT
Server: Webserv/1.0
Content-Type: text/plain
Content-Length: 13

Hello World!

Composant:
┌─────────────────────────────────────────┐
│           Status Line                   │
│  HTTP/1.1 200 OK                        │
│  ────────  ───  ──                      │
│   Version   Status Code  Description    │
├─────────────────────────────────────────┤
│           Headers                       │
│  Content-Type: text/html                │
│  Content-Length: 1234                   │
│  Server: webserv/1.0                    │
├─────────────────────────────────────────┤
│          Empty Line (Required!)         │
├─────────────────────────────────────────┤
│           Response Body                 │
│  <html>...</html>                       │
└─────────────────────────────────────────┘

```

## 5. Error Types

```
┌────────────────────────────────────────────────┐
│  1xx – Informational Status Codes (rarely used) │
│  100 Continue – Continue sending the request body│
├────────────────────────────────────────────────┤
│  2xx – Success                                  │
│  200 OK – Request succeeded                     │
│  201 Created – Resource created (after upload)  │
│  204 No Content – Success with no content        │
│        (after successful DELETE)                │
├────────────────────────────────────────────────┤
│  3xx – Redirection                              │
│  301 Moved Permanently – Permanent redirect     │
│  302 Found – Temporary redirect                 │
│  304 Not Modified – Resource not modified       │
│        (cache-related)                          │
├────────────────────────────────────────────────┤
│  4xx – Client Errors                            │
│  400 Bad Request – Invalid request format       │
│  403 Forbidden – Access denied                  │
│  404 Not Found – Resource does not exist         │
│  405 Method Not Allowed – Method not allowed    │
│  413 Payload Too Large – Request body too large │
├────────────────────────────────────────────────┤
│  5xx – Server Errors                            │
│  500 Internal Server Error – Internal error     │
│  501 Not Implemented – Method not implemented   │
│  502 Bad Gateway – Gateway error (proxy-related)│
│  504 Gateway Timeout – Gateway timeout (CGI)    │
└────────────────────────────────────────────────┘

## 参考资源
- [RFC 7230: Message Syntax and Routing](https://datatracker.ietf.org/doc/html/rfc7230)
- [RFC 7231: Semantics and Content](https://datatracker.ietf.org/doc/html/rfc7231)
- [HTTP Intro - CS168](https://textbook.cs168.io/applications/http.html)
