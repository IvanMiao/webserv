# CGI

CGI(Common Gateway Interface), 通用网关接口，是一种规范。

它定义了Web server如何与一个外部的、独立的app进行通信。这个外部app的工作是生成动态网页内容，如：

- 显示当前时间
- 从数据库中查询数据并展示

这些任务无法通过一个只会读取文件的 web server来完成，它需要一种方法来执行一个程序，程序来返回工作的结果。

# 🔁 CGI Execution Flow Summary

## Overview

The CGI (Common Gateway Interface) execution flow describes how a web server executes an external program (CGI script) to generate dynamic HTTP responses. The server controls the execution while the script runs in an isolated child process.

---

## High-Level Flow

```
Parent Process
│
├─ Prepare environment variables
├─ Create pipes (stdin / stdout)
├─ fork()
│   ├─ Child process → execve(CGI)
│   └─ Parent process → communicate & wait
└─ Parse CGI output → HTTP response
```

---

## Step-by-Step Summary

### 1. Prepare Environment Variables

* HTTP request data is converted into CGI variables
* Examples:

  * `REQUEST_METHOD`
  * `QUERY_STRING`
  * `CONTENT_LENGTH`
* Passed to the CGI script via `execve()`

---

### 2. Create Pipes

Two pipes enable inter-process communication:

| Pipe        | Direction      | Purpose            |
| ----------- | -------------- | ------------------ |
| stdin pipe  | Parent → Child | Send request body  |
| stdout pipe | Child → Parent | Receive CGI output |

---

### 3. fork()

The server process splits into:

* **Child process** → runs CGI script
* **Parent process** → controls execution

---

### 4. Child Process

* Sets execution timeout using `alarm()`
* Redirects stdin/stdout with `dup2()`
* Executes CGI script using `execve()`

---

### 5. Parent Process

* Writes HTTP request body to child stdin
* Reads CGI output from child stdout
* Waits for child termination (`waitpid()`)
* Detects timeout or execution errors

---

### 6. Parse CGI Output

* Splits output into:

  * CGI headers (e.g. `Status`, `Content-Type`)
  * Response body
* Converts result into an HTTP response

---

## Key Design Principle

> The CGI script runs in an isolated child process, while the parent process maintains full control over execution, timing, and data flow.

### Benefits

* Process isolation
* Timeout protection
* Resource control
* CGI standard compliance

---

## Result

A fully formed HTTP response generated dynamically by the CGI script and safely managed by the web server.


