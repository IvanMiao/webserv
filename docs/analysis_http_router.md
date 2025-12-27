# HTTP & Router 模块架构

本文档分析了 `src/http/` 和 `src/router/` 目录下各个类的作用，并提出了将这些模块集成到现有 `Server` 类的方案。

## 1. 模块类分析

### 1.1 HTTP 模块 (`src/http/`)

此模块负责 HTTP 协议的解析与构建，不涉及具体的业务逻辑。

*   **`HttpRequest`**
    *   **作用**: HTTP 请求解析器。
    *   **核心功能**:
        *   **渐进式解析 (`parse`)**: 能够处理分包到达的 TCP 数据流。内部维护状态机 (`ParseState`)，支持从 `PARSING_REQUEST_LINE` 到 `PARSE_COMPLETE` 的状态流转。
        *   **数据结构化**: 将原始 HTTP 报文解析为 Method, Path, Query, Version, Headers, Body 等结构化数据。
        *   **Chunked 支持**: 支持 `Transfer-Encoding: chunked` 的解析。
    *   **生命周期**: 每个连接 (`Client`) 应持有一个 `HttpRequest` 实例，用于持续接收和解析数据，直到请求完整。

*   **`HttpResponse`**
    *   **作用**: HTTP 响应封装类。
    *   **核心功能**:
        *   **构建响应**: 提供设置 Status Code, Headers, Body 的接口。
        *   **序列化 (`serialize`)**: 将响应对象转换为符合 HTTP 协议的字符串（Status Line + Headers + Body），准备发送给客户端。
        *   **工厂方法**: 提供 `createErrorResponse`, `createRedirectResponse`, `createOkResponse` 等静态方法快速构建常用响应。

### 1.2 Router 模块 (`src/router/`)

此模块负责根据请求分发逻辑，调用相应的处理器生成响应。

*   **`RequestHandler`**
    *   **作用**: 核心控制器 (Controller)，请求处理的入口。
    *   **核心功能**:
        *   **路由分发 (`handleRequest`)**: 根据 `HttpRequest` 的 Method (GET, POST, DELETE) 和 URI，结合 `ServerConfig` 和 `LocationConfig` 决定如何处理请求。
        *   **配置匹配**: 负责找到请求对应的 `LocationConfig`。
        *   **调度**:
            *   如果是 CGI 脚本 -> 调用 `CgiRequestHandler`。
            *   如果是文件上传 -> 调用 `UploadHandler`。
            *   如果是静态资源 -> 调用 `FileHandler`。
            *   如果发生错误 -> 调用 `ErrorHandler`。

*   **`FileHandler`**
    *   **作用**: 静态文件处理器。
    *   **核心功能**:
        *   **读取文件**: 读取磁盘文件内容。
        *   **MIME 类型**: 根据文件后缀判断 `Content-Type`。
        *   **目录处理**: 处理目录请求，支持索引文件 (`index.html`) 查找或生成自动索引页面 (Autoindex)。

*   **`CgiRequestHandler`**
    *   **作用**: CGI 脚本执行器。
    *   **核心功能**:
        *   **环境准备**: 根据 RFC 3875 准备 CGI 环境变量 (QUERY_STRING, REQUEST_METHOD 等)。
        *   **脚本执行**: 使用 `fork` + `execve` 执行脚本，并通过管道 (`pipe`) 传递 Request Body 和捕获 Script Output。
        *   **输出解析**: 解析 CGI 脚本返回的头部和内容，封装成 `HttpResponse`。

*   **`UploadHandler`**
    *   **作用**: 文件上传处理器。
    *   **核心功能**:
        *   处理 `POST` 请求，解析 `multipart/form-data` 或纯二进制 Body。
        *   将上传的文件保存到服务器配置的上传目录。

*   **`ErrorHandler`**
    *   **作用**: 统一错误响应生成器。
    *   **核心功能**:
        *   根据 Status Code 生成错误页面。
        *   优先使用配置文件中定义的自定义错误页 (`error_page`)，如果未配置则生成默认的 HTML 错误页。

---

## 2. 集成方案

目前的 `Server` 类 (`src/server/Server.cpp`) 使用了简单的字符串查找 (`find("\r\n\r\n")`) 和硬编码的响应生成逻辑。为了集成上述模块，需要进行以下改造：

### 2.1 修改 `Client` 类

`Client` 类目前仅使用 `std::string request_buffer` 缓存原始数据。需要引入 `HttpRequest` 对象来接管解析工作。

**修改建议**:
*   在 `Client` 类中添加 `HttpRequest _request` 成员变量。
*   `request_buffer` 可以保留作为临时读取缓冲，或者直接将读取到的数据传给 `_request.parse()`。

### 2.2 改造 `Server::_handle_client_data` (读取与解析)

**当前逻辑**:
*   `read` 到 `buffer`。
*   `append` 到 `client.request_buffer`。
*   检查 `\r\n\r\n` 判断请求结束。

**新逻辑**:
1.  `read` 数据到临时 buffer。
2.  调用 `client._request.parse(buffer, bytes_read)`。
3.  检查 `client._request.isComplete()`。
    *   如果 **是**: 说明请求接收完毕，进入处理流程 (`_process_request`)。
    *   如果 **否**: 继续等待下一次 `EPOLLIN` 事件。
    *   如果 `client._request.hasError()`: 直接返回 400 Bad Request 响应并关闭连接。

### 2.3 改造 `Server::_process_request` (业务处理)

**当前逻辑**:
*   手动判断 `/echo` 或其他路径。
*   手动拼接 HTTP 响应字符串。

**新逻辑**:
1.  **获取配置**: 根据请求的 `Host` 头部，在 `_listen_fds` 中找到对应的 `ServerConfig` (处理虚拟主机)。
2.  **创建处理器**: 实例化 `RequestHandler`，传入匹配到的 `ServerConfig`。
3.  **处理请求**: 调用 `HttpResponse response = requestHandler.handleRequest(client._request)`。
4.  **序列化**: 调用 `std::string response_str = response.serialize()`。
5.  **发送准备**: 将 `response_str` 赋值给 `client.response_buffer`，并修改 epoll 监听 `EPOLLOUT` 事件。
6.  **重置**: 如果是 Keep-Alive 连接，处理完后调用 `client._request.reset()` 准备接收下一个请求。

### 2.4 流程图解

```mermaid
graph TD
    A[Epoll Event: EPOLLIN] --> B[Server::_handle_client_data]
    B --> C[read socket]
    C --> D[client.request.parse()]
    D --> E{isComplete?}
    E -- No --> F[Wait for more data]
    E -- Yes --> G[Server::_process_request]
    
    G --> H[Match ServerConfig]
    H --> I[RequestHandler::handleRequest]
    I --> J{Route Type}
    
    J -- Static File --> K[FileHandler]
    J -- CGI Script --> L[CgiRequestHandler]
    J -- Upload --> M[UploadHandler]
    J -- Error --> N[ErrorHandler]
    
    K --> O[HttpResponse]
    L --> O
    M --> O
    N --> O
    
    O --> P[response.serialize()]
    P --> Q[client.response_buffer]
    Q --> R[Epoll Event: EPOLLOUT]
```

## 3. 设计优势

*   **解耦**: `Server` 类仅负责网络 IO，协议解析和业务逻辑完全分离。
*   **健壮性**: 状态机解析器能有效应对网络拥塞导致的分包问题。
*   **可扩展性**: 增加新的处理逻辑（如 WebDAV）只需添加新的 Handler 并在 `RequestHandler` 中注册即可。
