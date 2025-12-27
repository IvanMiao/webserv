                              # WebServer Project Architecture / WebServ项目架构

## English Version

### High-Level Architecture Flow

```
┌─────────────────────────────────────────────────────────────────┐
│                         CLIENT REQUEST                           │
│                    (HTTP/1.1 Request)                            │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                   SERVER (main.cpp)                              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  1. Parse Configuration (ConfigParser)                    │  │
│  │  2. Initialize Server Socket                             │  │
│  │  3. Setup Epoll Event Loop                               │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                  EPOLL EVENT LOOP (Server.cpp)                   │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  Wait for events on listening socket and client sockets  │  │
│  │  ├─ New Connection → _handle_new_connection()            │  │
│  │  ├─ Client Read → _handle_client_data()                  │  │
│  │  └─ Client Write → _handle_client_write()                │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│            REQUEST PROCESSING (_handle_client_data)              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  1. Parse Raw HTTP Data → HttpRequest                    │  │
│  │  2. Call _process_request(HttpRequest)                   │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│          REQUEST HANDLER (RequestHandler.cpp)                    │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  handleRequest(HttpRequest)                              │  │
│  │  ├─ Find matching LocationConfig                         │  │
│  │  ├─ Validate HTTP Method                                 │  │
│  │  ├─ Check Request Size (client_max_body_size)            │  │
│  │  └─ Route to appropriate handler:                        │  │
│  │     ├─ GET/HEAD → _handleGet()                           │  │
│  │     ├─ POST → _handlePost()                              │  │
│  │     └─ DELETE → _handleDelete()                          │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
        ┌─────────────┼─────────────┬──────────────┐
        ▼             ▼             ▼              ▼
    ┌────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐
    │  GET   │  │  POST    │  │ DELETE   │  │  REDIRECT    │
    └────────┘  └──────────┘  └──────────┘  └──────────────┘
        │             │             │
        ▼             ▼             ▼
    ┌────────────────────────────────────────┐
    │  Check if static file exists           │
    └────────────────────────────────────────┘
        │
        ├─────────────────┬──────────────────┐
        ▼                 ▼                  ▼
    ┌────────┐    ┌──────────────┐   ┌──────────────┐
    │  File  │    │  Directory   │   │   CGI Script │
    └────────┘    └──────────────┘   └──────────────┘
        │                │                   │
        ▼                ▼                   ▼
   FileHandler    Directory Index    CgiRequestHandler
   Serve static   (autoindex)         Execute script
   file content   List files          Get output
        │                │                   │
        └────────────────┴───────────────────┘
                      │
        ┌─────────────┼──────────────┐
        ▼             ▼              ▼
    ┌──────────┐ ┌──────────┐  ┌───────────┐
    │ UPLOAD   │ │ DELETE   │  │   ERROR   │
    │ (POST)   │ │ (DELETE) │  │ (404/405) │
    └──────────┘ └──────────┘  └───────────┘
        │             │              │
        ▼             ▼              ▼
  ┌─────────────────────────────────────────┐
  │  UploadHandler      FileHandler       ErrorHandler
  │  ├─ Parse multipart │ Check exists    │ Generate
  │  ├─ Extract filename│ Delete file     │ error page
  │  ├─ Save to disk   │ Return 204      │ with 4xx/5xx
  │  └─ Return 201     │                 │ status codes
  └─────────────────────────────────────────┘
        │             │              │
        └─────────────┴──────────────┘
                      │
                      ▼
        ┌─────────────────────────────────┐
        │  ├─ Status Code                 │
        │  ├─ Headers                     │
        │  └─ Body                        │
        │  serialize() → HTTP string    HttpResponse                   │
        │    │
        └─────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│            RESPONSE BUFFER (Client)                              │
│  Write serialized response to client socket                      │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                    CLIENT RESPONSE                               │
│                  (HTTP/1.1 Response)                             │
└─────────────────────────────────────────────────────────────────┘
```

### Component Responsibilities

```
┌─────────────────────────────────────────────────────────────────┐
│                      CONFIGURATION LAYER                         │
├─────────────────────────────────────────────────────────────────┤
│  ConfigParser.cpp                                                │
│  ├─ Parse config/default.conf                                  │
│  ├─ Create ServerConfig objects                                 │
│  ├─ Define server listen port, host                            │
│  └─ Define LocationConfig (routes, upload settings, etc.)      │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                    NETWORKING LAYER                              │
├─────────────────────────────────────────────────────────────────┤
│  Server.cpp / Client.cpp                                         │
│  ├─ Initialize TCP socket binding                              │
│  ├─ Setup Epoll for async I/O                                  │
│  ├─ Accept incoming connections                                │
│  ├─ Read HTTP requests from clients                            │
│  └─ Write HTTP responses to clients                            │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     HTTP PARSING LAYER                           │
├─────────────────────────────────────────────────────────────────┤
│  HttpRequest.cpp                                                 │
│  ├─ Parse raw HTTP request string                              │
│  ├─ Extract method, path, headers, body                        │
│  └─ Provide getter methods for request data                    │
│                                                                  │
│  HttpResponse.cpp                                                │
│  ├─ Create HTTP response with status, headers, body            │
│  ├─ Serialize to HTTP string format                            │
│  └─ Handle redirects and error responses                       │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                   REQUEST ROUTING LAYER                          │
├─────────────────────────────────────────────────────────────────┤
│  RequestHandler.cpp                                              │
│  ├─ Find best-matching LocationConfig                          │
│  ├─ Validate HTTP method is allowed                            │
│  ├─ Route to specific handler (GET, POST, DELETE)             │
│  ├─ Prevent path traversal attacks                             │
│  └─ Check request size limits                                  │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                   REQUEST HANDLER LAYER                          │
├─────────────────────────────────────────────────────────────────┤
│  FileHandler.cpp                                                 │
│  ├─ Check if file/directory exists                             │
│  ├─ Read file content                                          │
│  ├─ Determine MIME type                                        │
│  └─ Delete files                                               │
│                                                                  │
│  UploadHandler.cpp                                               │
│  ├─ Parse multipart/form-data                                  │
│  ├─ Extract and validate filename                              │
│  ├─ Save uploaded file to disk                                 │
│  └─ Return upload success/failure                              │
│                                                                  │
│  CgiRequestHandler.cpp                                           │
│  ├─ Build CGI environment variables                            │
│  ├─ Execute CGI script                                         │
│  ├─ Capture script output                                      │
│  └─ Return as HTTP response                                    │
│                                                                  │
│  ErrorHandler.cpp                                                │
│  ├─ Load custom error pages from config                        │
│  ├─ Generate default error HTML                                │
│  └─ Return with appropriate status code                        │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     UTILITY LAYER                                │
├─────────────────────────────────────────────────────────────────┤
│  StringHelper.cpp                                                │
│  ├─ Split strings                                              │
│  ├─ Trim whitespace                                            │
│  ├─ Convert to lowercase                                       │
│  ├─ Detect path traversal attempts                             │
│  └─ Encode/decode strings                                      │
│                                                                  │
│  Logger.cpp                                                      │
│  ├─ Log server events (formatted output)                       │
│  └─ Debug information                                          │
└─────────────────────────────────────────────────────────────────┘
```

### Request Processing Sequence

```
1. Client connects → Server.start() (Epoll Loop)
                     ↓
2. _handle_new_connection()
   └─ Accept socket, add to epoll
                     ↓
3. Client sends HTTP request
   └─ Epoll detects read event
                     ↓
4. _handle_client_data()
   ├─ Read data into Client.request_buffer
   ├─ Check if request complete (headers + body)
   └─ Parse with HttpRequest constructor
                     ↓
5. _process_request(HttpRequest)
   └─ Create RequestHandler
   └─ Call handleRequest()
                     ↓
6. RequestHandler::handleRequest()
   ├─ Find LocationConfig via findLocation(path)
   ├─ Check if method allowed
   ├─ Validate content length
   └─ Route to _handleGet/POST/DELETE
                     ↓
7. Handler executes:
   ├─ Build file path
   ├─ Check if file exists
   ├─ Determine content type
   ├─ Execute appropriate handler
   └─ Create HttpResponse
                     ↓
8. HttpResponse.serialize()
   └─ Format as HTTP/1.1 response string
                     ↓
9. _handle_client_write()
   └─ Write response to socket
                     ↓
10. Client receives response
    └─ Connection may close or stay open
```

---

## 中文版本

### 高级架构流程

```
┌─────────────────────────────────────────────────────────────────┐
│                       客户端请求                                  │
│                    (HTTP/1.1 请求)                              │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                   服务器 (main.cpp)                              │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  1. 解析配置文件 (ConfigParser)                            │  │
│  │  2. 初始化服务器Socket                                    │  │
│  │  3. 设置Epoll事件循环                                     │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                 Epoll事件循环 (Server.cpp)                       │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  监听Socket事件和客户端Socket事件                          │  │
│  │  ├─ 新连接 → _handle_new_connection()                     │  │
│  │  ├─ 客户端读 → _handle_client_data()                      │  │
│  │  └─ 客户端写 → _handle_client_write()                     │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│              请求处理 (_handle_client_data)                      │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  1. 解析原始HTTP数据 → HttpRequest                        │  │
│  │  2. 调用 _process_request(HttpRequest)                    │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│          请求处理器 (RequestHandler.cpp)                         │
│  ┌───────────────────────────────────────────────────────────┐  │
│  │  handleRequest(HttpRequest)                              │  │
│  │  ├─ 查找匹配的LocationConfig                              │  │
│  │  ├─ 验证HTTP方法                                         │  │
│  │  ├─ 检查请求大小 (client_max_body_size)                  │  │
│  │  └─ 路由到相应处理器:                                    │  │
│  │     ├─ GET/HEAD → _handleGet()                           │  │
│  │     ├─ POST → _handlePost()                              │  │
│  │     └─ DELETE → _handleDelete()                          │  │
│  └───────────────────────────────────────────────────────────┘  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
        ┌─────────────┼─────────────┬──────────────┐
        ▼             ▼             ▼              ▼
    ┌────────┐  ┌──────────┐  ┌──────────┐  ┌──────────────┐
    │  GET   │  │  POST    │  │ DELETE   │  │  重定向      │
    └────────┘  └──────────┘  └──────────┘  └──────────────┘
        │             │             │
        ▼             ▼             ▼
    ┌────────────────────────────────────────┐
    │  检查静态文件是否存在                    │
    └────────────────────────────────────────┘
        │
        ├─────────────────┬──────────────────┐
        ▼                 ▼                  ▼
    ┌────────┐    ┌──────────────┐   ┌──────────────┐
    │  文件  │    │  目录        │   │   CGI脚本    │
    └────────┘    └──────────────┘   └──────────────┘
        │                │                   │
        ▼                ▼                   ▼
   FileHandler    目录列表索引        CgiRequestHandler
   提供静态文件   (autoindex)         执行脚本
   内容           列出文件            获取输出
        │                │                   │
        └────────────────┴───────────────────┘
                      │
        ┌─────────────┼──────────────┐
        ▼             ▼              ▼
    ┌──────────┐ ┌──────────┐  ┌───────────┐
    │ 上传     │ │ 删除     │  │   错误    │
    │ (POST)   │ │ (DELETE) │  │ (404/405) │
    └──────────┘ └──────────┘  └───────────┘
        │             │              │
        ▼             ▼              ▼
  ┌─────────────────────────────────────────┐
  │  UploadHandler   FileHandler   ErrorHandler
  │  ├─ 解析multipart │ 检查文件   │ 生成
  │  ├─ 提取文件名    │ 删除文件   │ 错误页面
  │  ├─ 保存到磁盘   │ 返回204    │ 4xx/5xx
  │  └─ 返回201      │            │ 状态码
  └─────────────────────────────────────────┘
        │             │              │
        └─────────────┴──────────────┘
                      │
                      ▼
        ┌─────────────────────────────────┐
        │  HttpResponse                   │
        │  ├─ 状态码                       │
        │  ├─ 响应头                       │
        │  └─ 响应体                       │
        │  serialize() → HTTP字符串        │
        └─────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│            响应缓冲区 (Client)                                   │
│  将序列化的响应写入客户端Socket                                  │
└─────────────────────┬───────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────────────────┐
│                    客户端响应                                    │
│                  (HTTP/1.1 响应)                                │
└─────────────────────────────────────────────────────────────────┘
```

### 组件职责

```
┌─────────────────────────────────────────────────────────────────┐
│                     配置解析层                                   │
├─────────────────────────────────────────────────────────────────┤
│  ConfigParser.cpp                                                │
│  ├─ 解析 config/default.conf                                    │
│  ├─ 创建 ServerConfig 对象                                      │
│  ├─ 定义服务器监听端口、主机                                    │
│  └─ 定义 LocationConfig (路由、上传设置等)                      │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     网络通信层                                   │
├─────────────────────────────────────────────────────────────────┤
│  Server.cpp / Client.cpp                                         │
│  ├─ 初始化TCP Socket绑定                                       │
│  ├─ 设置Epoll异步I/O                                           │
│  ├─ 接受传入连接                                               │
│  ├─ 从客户端读取HTTP请求                                       │
│  └─ 向客户端写入HTTP响应                                       │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     HTTP解析层                                   │
├─────────────────────────────────────────────────────────────────┤
│  HttpRequest.cpp                                                 │
│  ├─ 解析原始HTTP请求字符串                                     │
│  ├─ 提取方法、路径、头、体                                      │
│  └─ 提供获取请求数据的方法                                      │
│                                                                  │
│  HttpResponse.cpp                                                │
│  ├─ 创建带状态码、响应头、响应体的HTTP响应                      │
│  ├─ 序列化为HTTP字符串格式                                      │
│  └─ 处理重定向和错误响应                                        │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     请求路由层                                   │
├─────────────────────────────────────────────────────────────────┤
│  RequestHandler.cpp                                              │
│  ├─ 查找最佳匹配的LocationConfig                                │
│  ├─ 验证HTTP方法是否被允许                                      │
│  ├─ 路由到具体处理器 (GET、POST、DELETE)                       │
│  ├─ 防止路径遍历攻击                                            │
│  └─ 检查请求大小限制                                            │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     请求处理器层                                 │
├─────────────────────────────────────────────────────────────────┤
│  FileHandler.cpp                                                 │
│  ├─ 检查文件/目录是否存在                                       │
│  ├─ 读取文件内容                                               │
│  ├─ 判断MIME类型                                               │
│  └─ 删除文件                                                   │
│                                                                  │
│  UploadHandler.cpp                                               │
│  ├─ 解析 multipart/form-data                                    │
│  ├─ 提取并验证文件名                                            │
│  ├─ 保存上传的文件到磁盘                                        │
│  └─ 返回上传成功/失败                                           │
│                                                                  │
│  CgiRequestHandler.cpp                                           │
│  ├─ 构建CGI环境变量                                             │
│  ├─ 执行CGI脚本                                                 │
│  ├─ 捕获脚本输出                                                │
│  └─ 作为HTTP响应返回                                            │
│                                                                  │
│  ErrorHandler.cpp                                                │
│  ├─ 从配置加载自定义错误页面                                    │
│  ├─ 生成默认错误HTML                                            │
│  └─ 返回适当的状态码                                            │
└─────────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────────┐
│                     工具层                                       │
├─────────────────────────────────────────────────────────────────┤
│  StringHelper.cpp                                                │
│  ├─ 分割字符串                                                  │
│  ├─ 删除空白                                                    │
│  ├─ 转换为小写                                                  │
│  ├─ 检测路径遍历尝试                                            │
│  └─ 编码/解码字符串                                             │
│                                                                  │
│  Logger.cpp                                                      │
│  ├─ 记录服务器事件 (格式化输出)                                │
│  └─ 调试信息                                                    │
└─────────────────────────────────────────────────────────────────┘
```

### 请求处理序列

```
1. 客户端连接 → Server.start() (Epoll循环)
                ↓
2. _handle_new_connection()
   └─ 接受Socket，添加到epoll
                ↓
3. 客户端发送HTTP请求
   └─ Epoll检测读事件
                ↓
4. _handle_client_data()
   ├─ 读数据到 Client.request_buffer
   ├─ 检查请求是否完整 (头 + 体)
   └─ 用HttpRequest构造函数解析
                ↓
5. _process_request(HttpRequest)
   └─ 创建RequestHandler
   └─ 调用 handleRequest()
                ↓
6. RequestHandler::handleRequest()
   ├─ 通过findLocation(path)查找LocationConfig
   ├─ 检查方法是否被允许
   ├─ 验证内容长度
   └─ 路由到 _handleGet/POST/DELETE
                ↓
7. 处理器执行:
   ├─ 构建文件路径
   ├─ 检查文件是否存在
   ├─ 判断内容类型
   ├─ 执行相应处理器
   └─ 创建HttpResponse
                ↓
8. HttpResponse.serialize()
   └─ 格式化为HTTP/1.1响应字符串
                ↓
9. _handle_client_write()
   └─ 写响应到Socket
                ↓
10. 客户端接收响应
    └─ 连接可能关闭或保持打开
```

---

## File Structure / 文件结构

```
src/
├── main.cpp                          # 入口点 / Entry point
├── server/
│   ├── Server.cpp/hpp               # Epoll主循环 / Epoll event loop
│   └── Client.cpp/hpp               # 客户端连接管理 / Client connection management
├── config/
│   ├── ConfigParser.cpp/hpp         # 配置文件解析 / Config file parsing
│   └── (ServerConfig, LocationConfig in header)
├── http/
│   ├── HttpRequest.cpp/hpp          # HTTP请求解析 / HTTP request parsing
│   └── HttpResponse.cpp/hpp         # HTTP响应构建 / HTTP response building
├── request/
│   ├── RequestHandler.cpp/hpp       # 请求路由 / Request routing
│   ├── FileHandler.cpp/hpp          # 文件操作 / File operations
│   ├── UploadHandler.cpp/hpp        # 文件上传 / File upload
│   ├── CgiRequestHandler.cpp/hpp    # CGI执行 / CGI execution
│   └── ErrorHandler.cpp/hpp         # 错误页面 / Error pages
├── cgi/
│   └── CgiHandler.cpp/hpp           # CGI处理接口 / CGI interface
└── utils/
    ├── Logger.cpp/hpp               # 日志记录 / Logging
    └── StringHelper.cpp/hpp         # 字符串工具 / String utilities
```

