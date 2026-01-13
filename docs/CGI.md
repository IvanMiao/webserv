# CGI (Common Gateway Interface) Module

The CGI module allows the web server to execute external programs (scripts) and return dynamically generated content to the client.  
This project implements **non-blocking CGI**, ensuring that time-consuming scripts do not block the server's main event loop.

---

## 1. Architecture Design (Non-Blocking)

Traditional CGI implementations are often **blocking**:  
The server `fork`s a child process and waits using `read()`, blocking until the child finishes output.  
During this time, the server cannot handle other client requests.

**In this project, we use a Reactor pattern combined with `epoll` to implement asynchronous CGI handling:**

1. **Request routing**  
   `CgiRequestHandler` identifies CGI requests and prepares environment variables.

2. **Process startup**  
   `CgiHandler` calls `fork()` + `execve()`, and creates **non-blocking pipes**.

3. **Event registration**  
   - Register CGI `stdout` (read end) with `epoll` for `EPOLLIN`.  
   - If there is POST data, register `stdin` (write end) with `EPOLLOUT`.

4. **State saving**  
   The `Client` object stores CGI FDs and the `CgiHandler` instance, switching its state to `CLIENT_CGI_PROCESSING`.

5. **Event loop**  
   - The `Server` main loop continues handling other connections.  
   - When a CGI pipe is ready, `Server::_handle_cgi_data` is triggered.

6. **Data transfer**  
   - **Write input**: send request body to CGI (non-blocking).  
   - **Read output**: read CGI response into a buffer.

7. **Completion**  
   - When pipes close or EOF is reached, the server reaps the child process (`waitpid`).  
   - Parse CGI output (split Headers and Body).  
   - Build `HttpResponse` and send it to the client.

---

## Plot 1 — Non-Blocking CGI Flow

```text
Client                   Server
   |        Request         |
   | -------------------->  |
   |      Route CGI          |
   |       fork+exec         |
   |      Non-blocking pipe  |
   |                         |
   |       epoll loop         |
   | <-------------------    |
   |   CGI stdout ready       |
   |   _handle_cgi_data       |
   |   Read/Write data        |
   | -------------------->  |
   |     Response ready       |
   | <--------------------   |
````

---

## 2. Core Classes & Responsibilities

| Class               | Responsibility                                                                                                                       |
| :------------------ | :----------------------------------------------------------------------------------------------------------------------------------- |
| `CgiHandler`        | Handles low-level `pipe`, `fork`, `dup2`, `execve`. Manages environment variables. Provides non-blocking FDs for epoll registration. |
| `CgiRequestHandler` | Prepares CGI environment variables according to RFC 3875 (`REQUEST_METHOD`, `SCRIPT_NAME`, etc.). Initializes CGI processing.        |
| `Server`            | Integrates `epoll` to monitor CGI pipes. Dispatches I/O events via `_handle_cgi_data`. Handles timeouts and child process reaping.   |

---

## 3. Core Environment Variables (RFC 3875)

The server passes the following standard environment variables to the CGI script:

| Variable          | Description            | Example                    |
| :---------------- | :--------------------- | :------------------------- |
| `REQUEST_METHOD`  | HTTP request method    | `GET`, `POST`              |
| `QUERY_STRING`    | URL parameters         | `id=123&name=test`         |
| `CONTENT_LENGTH`  | Request body length    | `42`                       |
| `CONTENT_TYPE`    | Request body type      | `application/json`         |
| `SCRIPT_NAME`     | Script request path    | `/cgi-bin/test.py`         |
| `SCRIPT_FILENAME` | Absolute physical path | `/var/www/cgi-bin/test.py` |
| `SERVER_PROTOCOL` | HTTP version           | `HTTP/1.1`                 |

---

## 4. Testing Guide

The project includes automated tests in the `test/` directory to verify CGI functionality, concurrency, and robustness.

### 4.1 Prerequisites

Ensure `python3` is installed.

### 4.2 Start Server

From the project root:

```bash
make re
./webserv config/default.conf
```

### 4.3 Run Test Scripts

#### A. Functional Tests

Verify basic GET/POST methods, environment variables, and header parsing:

```bash
python3 test/test_cgi_methods.py
```

* **Expected result**: All checks show `PASS`.

---

#### B. Concurrency Tests

Test that slow CGI scripts **do not block the server**.
A slow CGI request (5s delay) runs alongside a static file request.

```bash
python3 test/test_cgi_concurrency.py
```

* **Expected result**: Static file request returns immediately (<0.1s).

---

#### C. Load Tests

Simulate 100+ concurrent CGI requests:

```bash
python3 test/test_cgi_load.py
```

* **Expected result**: All requests succeed (`200 OK`), no connection failures.

---

#### D. Error Handling

Test crash scripts, missing scripts, and timeouts:

```bash
python3 test/test_cgi_errors.py
```

* **Expected result**:

  * Crash Script → `500 Internal Server Error`
  * Missing Script → `404 Not Found`

---

## 5. Provided CGI Scripts

Located in `www/cgi-bin/` for testing:

| Script            | Purpose                                  |
| :---------------- | :--------------------------------------- |
| `echo_env.py`     | Prints all environment variables         |
| `echo_body.py`    | Echoes POST request body                 |
| `sleep.py`        | Sleeps 5s (for concurrency testing)      |
| `crash.py`        | Simulates crash (non-zero exit code)     |
| `large_output.py` | Generates large output to test buffering |

---

## Plot 2 — CGI Lifecycle

```text
Client        Server        CGI Process
  |            |               |
  |  Request   |               |
  | ---------> |               |
  |            | fork+exec     |
  |            | --------------> |
  |            | Non-blocking pipe|
  |            | <-------------  |
  |            | epoll monitors  |
  |            | read/write data |
  | <--------- |                |
  | Response   |                |
```

```
```


# CGI (Common Gateway Interface) 模块

CGI 模块允许 Web 服务器执行外部程序（脚本）并将动态生成的内容返回给客户端。本项目采用 **非阻塞（Non-Blocking）** 架构实现 CGI，确保耗时的脚本执行不会阻塞服务器的主事件循环。

## 1. 架构设计 (Non-Blocking Architecture)

传统的 CGI 实现往往是阻塞的：服务器 `fork` 子进程后，父进程通过 `read()` 阻塞等待子进程输出。这会导致在脚本运行期间，服务器无法处理其他客户端请求。

**本项目采用 Reactor 模式配合 `epoll` 实现异步 CGI 处理：**

1.  **请求路由**：`CgiRequestHandler` 识别 CGI 请求，准备环境变量。
2.  **进程启动**：`CgiHandler` 调用 `fork()` 和 `execve()`，并创建 **非阻塞管道** (Non-blocking Pipes)。
3.  **事件注册**：
    *   将 CGI 的 `stdout` 管道（读端）注册到 `epoll` (EPOLLIN)。
    *   如果有 POST 数据，将 `stdin` 管道（写端）注册到 `epoll` (EPOLLOUT)。
4.  **状态保存**：`Client` 对象保存 CGI 相关的 FDs 和 `CgiHandler` 实例，并将状态切换为 `CLIENT_CGI_PROCESSING`。
5.  **事件循环**：
    *   `Server` 主循环继续处理其他连接。
    *   当 CGI 管道就绪时，`Server::_handle_cgi_data` 被触发。
6.  **数据传输**：
    *   **写输入**：向 CGI 进程写入请求体（非阻塞）。
    *   **读输出**：从 CGI 进程读取响应数据到缓冲区。
7.  **完成处理**：
    *   当管道关闭或遇到 EOF，Server 回收子进程 (`waitpid`)。
    *   解析 CGI 输出（分离 Headers 和 Body）。
    *   构建 `HttpResponse` 并发送给客户端。

### 核心类与职责

*   **`CgiHandler`**: 
    *   负责底层的 `pipe`, `fork`, `dup2`, `execve`。
    *   管理环境变量转换。
    *   提供非阻塞 FD 供外部注册。
*   **`CgiRequestHandler`**: 
    *   构建符合 RFC 3875 标准的环境变量 (`REQUEST_METHOD`, `SCRIPT_NAME` 等)。
    *   初始化 CGI 流程。
*   **`Server`**: 
    *   集成 `epoll` 监听 CGI 管道。
    *   分发 I/O 事件 (`_handle_cgi_data`)。
    *   处理超时和进程回收。

## 2. 核心环境变量 (RFC 3875)

服务器会向 CGI 脚本传递以下标准环境变量：

| 变量名 | 描述 | 示例 |
| :--- | :--- | :--- |
| `REQUEST_METHOD` | HTTP 请求方法 | `GET`, `POST` |
| `QUERY_STRING` | URL 参数 | `id=123&name=test` |
| `CONTENT_LENGTH` | 请求体长度 | `42` |
| `CONTENT_TYPE` | 请求体类型 | `application/json` |
| `SCRIPT_NAME` | 脚本请求路径 | `/cgi-bin/test.py` |
| `SCRIPT_FILENAME` | 脚本物理绝对路径 | `/var/www/cgi-bin/test.py` |
| `SERVER_PROTOCOL` | HTTP 版本 | `HTTP/1.1` |

## 3. 测试指南 (Testing Guide)

本项目包含一套完整的自动化测试脚本，位于 `test/` 目录下，用于验证 CGI 的功能、并发性和健壮性。

### 3.1 准备工作

确保已安装 `python3`。

### 3.2 启动服务器

在项目根目录下编译并启动服务器：

```bash
make re
./webserv config/default.conf
```

### 3.3 运行测试脚本

在另一个终端中运行以下测试。

#### A. 功能测试 (Functional Tests)
验证基本的 GET/POST 方法、环境变量传递和 Header 解析。

```bash
python3 test/test_cgi_methods.py
```
*   **预期结果**: 所有检查项显示 `PASS`。

#### B. 并发测试 (Concurrency Tests)
**核心测试**：验证慢速 CGI 脚本是否会阻塞服务器。该脚本会发起一个耗时 5 秒的 CGI 请求，同时发起一个静态文件请求。

```bash
python3 test/test_cgi_concurrency.py
```
*   **预期结果**: 静态文件请求应立即返回（< 0.1s），不受 CGI 脚本影响。

#### C. 负载测试 (Load Tests)
模拟并发发送 100+ 个 CGI 请求。

```bash
python3 test/test_cgi_load.py
```
*   **预期结果**: 所有请求均成功 (200 OK)，无连接失败。

#### D. 错误处理测试 (Error Handling)
测试脚本崩溃、脚本不存在、超时等异常情况。

```bash
python3 test/test_cgi_errors.py
```
*   **预期结果**: 
    *   Crash Script -> 500 Internal Server Error (而不是 200)
    *   Missing Script -> 404 Not Found

## 4. 提供的 CGI 脚本

`www/cgi-bin/` 目录下预置了以下脚本供测试使用：

*   `echo_env.py`: 打印所有环境变量。
*   `echo_body.py`: 回显 POST 请求体。
*   `sleep.py`: 睡眠 5 秒（用于并发测试）。
*   `crash.py`: 模拟进程崩溃（非零退出码）。
*   `large_output.py`: 生成大量数据测试缓冲区。
