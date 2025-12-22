
## Server


### Core

- `socket`: `int socket(int domain, int type, int protocol)`
	- 向操作系统申请创建一个通信端点（Endpoint）。它不包含地址或端口，仅仅是分配了资源。
	- domain: AF_INET (IPv4)
	- type: SOCK_STREAM (TCP)
	- protocol: 0 (默认协议，即 TCP)
	- 返回： 成功时返回一个 fd(server socket)，失败返回 -1

- `bind`： `int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)`
	- 将具体的 IP 地址和端口号（Port）“绑定”到刚才创建的 socket 上。
	- sockfd: socket() 返回的fd
	- addr: 一个结构体，包含 IP（通常是 INADDR_ANY，表示接受本机所有网卡的连接）和端口（经过 htons 处理的）。

- `listen`: `int listen(int sockfd, int backlog)`
	- 将 socket 标记为“被动”状态，准备接受传入的连接请求。它还会设置一个“排队队列”(backlog)，`backlog`指定连接队列的最大长度。
	- 调用后，服务器正式开始工作，可以接收 TCP 握手请求了。**注意**: 此时代码还没有阻塞，只是改变了 socket 的状态。

- `accept`: `int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)`
	- 从 listen 的队列中取出第一个等待的连接请求，并创建一个**全新的 socket** 来专门服务这个客户端。返回一个 fd(client socket)。
	- 这里涉及两个socket： 
		- Listening Socket/Server Socket: 即 socket() 创建的那个。一直处于监听状态。
		- Connected Socket/Client Socket: accept() 返回的新fd。负责和某个具体的客户端进行沟通。
	- 在 Webserv 中，不能直接阻塞在 accept 上。根据规则，需要先用 (e)poll() 监控监听 socket 。

- `send`: `ssize_t send(int sockfd, const void *buf, size_t len, int flags)`
	- 向 Client Socket 发送数据
	- 处理完 HTTP 请求（比如读取了 index.html），构建好 HTTP Response 字符串后，用 send 把数据发回给浏览器。
	- 同样受到 poll() 的限制。必须在 poll() 报告该 socket “可写 (POLLOUT)” 时才能调用 send，否则如果不小心发了太多数据塞满了缓冲区，程序会阻塞


### Utils

- `htons`： `uint16_t htons(uint16_t hostshort)`
	- **H**ost **TO** **N**etwork **S**hort。将“主机字节序”（Host Byte Order）转换为“网络字节序”（Network Byte Order）。
	- 计算机存储数字的方式不同（有的从低位开始存，有的从高位开始存）。网络协议规定统一使用“大端序”（Big Endian）。如果你的电脑是“小端序”（如 x86 架构），直接发送端口号 8080，网络对面读出来的可能是另一个数字。htons 负责这个翻译。
	- 在设置服务器监听端口（如 8080）时，必须用它包裹端口号。

- `setsockopt`: `setsockopt(int fd, int level, int optname, void *optval, socklen_t optlen)`
	- 在本项目用于端口复用(`SO_REUSEADDR`)。如果关闭服务器立刻重启，可能会报 Address already in use, 因为 TCP_TIME_WAIT 状态会占用端口几分钟时间。




### Notes

1. 我们使用 sockaddr_in 来在 Server class 中储存 address。因为系统调用(bind, accept, etc.)是通用设计，他们的参数类型是通用类型 `struct sockaddr *`，而sockaddr_in 是其中一种具体实现，专门用于存放 IPv4 的数据。

2. 对 Client Socket，我们既会 read，又会 send。这是因为client socket关联了两个独立的缓冲区： read buffer & write buffer。

3. [socket abstraction](https://textbook.cs168.io/end-to-end/end-to-end.html)
	> If you’re a user visiting a website in your browser, you don’t need to write any code to run the application (HTTP) over the Internet. However, if you were a programmer writing your own application, you probably need to write some code to interact with the network.

	> The **socket** abstraction gives programmers a convenient way to interact with the network. The socket abstraction exists entirely in software, and there are five basic operations that programmers can run:

	> We can **create** a new socket, corresponding to a new connection. In an object-oriented language like Java, this could be a constructor call.

	> We can call **connect**, which initiates a TCP connection to some remote machine. This is useful if we’re the client in a client-server connection.

	> We can call **listen** on a specific port. This does not start a connection, but allows others to initiate a connection with us on the specified port.

	> Once the connection is open, we can call **write** to send some bytes on the connection. We can also call **read**, which takes one argument N, to read N bytes from the connection.

	> This socket abstraction gives programmers a way to write applications without thinking about lower-level abstractions like TCP, IP, or Ethernet.

	> From the operating system perspective, each socket is associated with a Layer 4 port number. All packets to and from a single socket have the same port number, and the operating system can use the port number to de-multiplex and send packets to the correct socket.

4. 在 `bind` 之前，需要用 `setsockopt` 设置 `SO_REUSEADDOR`，防止服务器崩溃或重启后端口处于 `TIME_WAIT` 状态，导致 `bind` 失败，需要等待几分钟才能重启。

5. 
