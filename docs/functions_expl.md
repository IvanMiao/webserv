
# Webserv: Core Server and I/O Architecture

### Socket Lifecycle and Configuration

* **socket()**: Requests a communication endpoint from the kernel. This project utilizes `AF_INET` (IPv4) and `SOCK_STREAM` (TCP).
* **setsockopt()**: Essential for enabling `SO_REUSEADDR`. This allows the server to bind to its designated port immediately after a shutdown or crash, bypassing the standard TCP `TIME_WAIT` delay.
* **fcntl()**: Configures file descriptors to `O_NONBLOCK` mode. Non-blocking I/O is mandatory when using `epoll` to ensure that a slow operation on a single client does not block the entire event loop.
* **bind() and listen()**: Associates the socket with a specific IP address and port, then initializes a `backlog` queue to handle incoming connection requests.
* **accept()**: Generates a new, dedicated file descriptor for each client connection. The original listening socket remains active to monitor for new incoming requests.

### High-Performance I/O Multiplexing (epoll)

Instead of blocking on individual read or write calls, the server uses `epoll` to monitor an interest list of multiple file descriptors:

* **epoll_wait()**: The central mechanism of the `start()` loop. It pauses the process until an event, such as an incoming request or a ready-to-write buffer, occurs.
* **Event Flags**:
* `EPOLLIN`: Indicates that data is available to be read from the socket.
* `EPOLLOUT`: Indicates that the socket's buffer is ready to transmit data.


* **Dynamic State Management**: Using `epoll_ctl` with `EPOLL_CTL_MOD`, the server switches a client's status (e.g., from reading a request to writing a response) based on the current state of the HTTP transaction.

### Asynchronous CGI and Resource Management

* **Asynchronous CGI**: The server manages CGI execution by registering the pipes (stdin/stdout) to the `epoll` instance. This prevents the server from idling while the CGI script processes data, allowing it to serve other clients concurrently.
* **Signal Handling and Cleanup**: A `volatile sig_atomic_t` flag handles `SIGINT` for graceful shutdowns. The `cleanup()` method ensures all client connections, listening sockets, and epoll descriptors are closed to prevent resource leaks.
* **Timeout Logic**: The `_check_client_timeouts()` function identifies idle connections or hung CGI processes. It utilizes `kill()` and `waitpid()` to reclaim resources from unresponsive scripts.

---
```
```

# Webserv: 核心服务器与 I/O 架构

### 套接字生命周期与配置

* **socket()**: 向内核申请通信端点。本项目使用 `AF_INET` (IPv4) 和 `SOCK_STREAM` (TCP) 协议。
* **setsockopt()**: 用于设置 `SO_REUSEADDR`。该配置允许服务器在重启或崩溃后立即重新绑定原有端口，无需等待 TCP 的 `TIME_WAIT` 状态结束。
* **fcntl()**: 将文件描述符设置为 `O_NONBLOCK`（非阻塞）模式。这是 `epoll` 架构的核心要求，确保在处理某个客户端的慢速读写时，不会阻塞整个服务器的事件循环。
* **bind() 与 listen()**: 将套接字与指定的 IP 地址和端口关联，并初始化 `backlog` 队列以接收传入的连接请求。
* **accept()**: 为每个连接成功的客户端创建一个全新的文件描述符。原始的监听套接字继续保持开启，用于捕捉后续的新连接。

### 高性能 I/O 多路复用 (epoll)

服务器不再死等单个数据包，而是利用 `epoll` 同时监控大量文件描述符：

* **epoll_wait()**: `start()` 循环的核心。它会挂起进程，直到有感兴趣的事件（如数据到达或缓冲区可写）发生。
* **事件标志**:
* `EPOLLIN`: 表示套接字中有数据可供读取。
* `EPOLLOUT`: 表示发送缓冲区已准备好接收数据并发送。


* **状态动态切换**: 通过 `epoll_ctl` 配合 `EPOLL_CTL_MOD` 操作，服务器可以根据 HTTP 处理进度动态切换客户端的监听状态（例如：从接收请求切换为发送响应）。

### 异步 CGI 与 资源管理

* **异步 CGI 处理**: 服务器将 CGI 进程的输入输出管道（Pipes）注册到 `epoll` 实例中。这种方式避免了服务器在等待脚本执行结果时处于空闲状态，从而能够并发处理其他请求。
* **信号处理与优雅停机**: 使用 `volatile sig_atomic_t` 类型的标志位处理 `SIGINT` 信号。`cleanup()` 方法确保所有客户端连接、监听套接字和 epoll 描述符被正确关闭，防止资源泄漏。
* **超时检查机制**: `_check_client_timeouts()` 函数定期巡检空闲连接或卡死的 CGI 进程。通过 `kill()` 和 `waitpid()` 强制回收不响应的脚本资源，确保系统稳定性。

---
