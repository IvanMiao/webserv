
## Server

- `socket`: `int socket(int domain, int type, int protocol)`
	- 向操作系统申请创建一个通信端点（Endpoint）。它不包含地址或端口，仅仅是分配了资源。
	- domain: AF_INET (IPv4)
	- type: SOCK_STREAM (TCP)
	- protocol: 0 (默认协议，即 TCP)
	- 返回： 成功时返回一个 fd，失败返回 -1

- `htons`： `uint16_t htons(uint16_t hostshort)`
	- **H**ost **TO** **N**etwork **S**hort。将“主机字节序”（Host Byte Order）转换为“网络字节序”（Network Byte Order）。
	- 计算机存储数字的方式不同（有的从低位开始存，有的从高位开始存）。网络协议规定统一使用“大端序”（Big Endian）。如果你的电脑是“小端序”（如 x86 架构），直接发送端口号 8080，网络对面读出来的可能是另一个数字。htons 负责这个翻译。
	- 在设置服务器监听端口（如 8080）时，必须用它包裹端口号。

- `bind`： `int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen)`
	- 将具体的 IP 地址和端口号（Port）“绑定”到刚才创建的 socket 上。
	- sockfd: socket() 返回的fd
	- addr: 一个结构体，包含 IP（通常是 INADDR_ANY，表示接受本机所有网卡的连接）和端口（经过 htons 处理的）。

- `listen`: `int listen(int sockfd, int backlog)`
	- 将 socket 标记为“被动”状态，准备接受传入的连接请求。它还会设置一个“排队队列”(backlog)。
	- 调用后，服务器正式开始工作，可以接收 TCP 握手请求了。**注意**: 此时代码还没有阻塞，只是改变了 socket 的状态。

- `accept`: `int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen)`
	- 从 listen 的队列中取出第一个等待的连接请求，并创建一个**全新的 socket** 来专门服务这个客户端。
	- 这里涉及两个socket： 
		- Listening Socket: 即 socket() 创建的那个。一直处于监听状态。
		- Connected Socket: accept() 返回的新fd。负责和某个具体的客户端进行沟通。
	- 在 Webserv 中，不能直接阻塞在 accept 上。根据规则，需要先用 (e)poll() 监控监听 socket 。

- `send`: `ssize_t send(int sockfd, const void *buf, size_t len, int flags)`
	- 向 连接socket 发送数据
	- 处理完 HTTP 请求（比如读取了 index.html），构建好 HTTP Response 字符串后，用 send 把数据发回给浏览器。
	- 同样受到 poll() 的限制。必须在 poll() 报告该 socket “可写 (POLLOUT)” 时才能调用 send，否则如果不小心发了太多数据塞满了缓冲区，程序会阻塞