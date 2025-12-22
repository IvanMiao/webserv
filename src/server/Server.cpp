#include "Server.hpp"
#include "../utils/Logger.hpp"
#include "../http/HttpRequest.hpp"
#include "../http/HttpResponse.hpp"
#include "../config/ConfigParser.hpp"
#include "../request/RequestHandler.hpp"
#include "Client.hpp" // Client class definition

#include <stdexcept>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/epoll.h>
#include <sys/stat.h>
#include <errno.h>
#include <signal.h>
#include <fstream>
#include <sstream>

#ifndef MAX_EVENTS
#define MAX_EVENTS 100 // 假设 MAX_EVENTS 在某个头文件中定义
#endif

namespace wsv
{

// --------------------------------------------------------------------------
// 构造与析构
// --------------------------------------------------------------------------

Server::Server(const ServerConfig& config)
    : _config(config), _server_fd(-1), _epoll_fd(-1)
{
    // 忽略 SIGPIPE 信号，防止在向已关闭连接写入时进程崩溃
    signal(SIGPIPE, SIG_IGN);

    // 初始化所有 location 的上传文件夹
    _initializeUploadDirectories();

    _init_server_socket();
    _init_epoll();
}

Server::~Server()
{
    if (_server_fd >= 0)
        close(_server_fd);
    if (_epoll_fd >= 0)
        close(_epoll_fd);
    
    // 清理所有活动的客户端连接
    for (std::map<int, Client>::iterator it = _clients.begin(); it != _clients.end(); ++it)
        close(it->first);
}

// --------------------------------------------------------------------------
// 初始化
// --------------------------------------------------------------------------

void Server::_initializeUploadDirectories()
{
    // 遍历所有 location 配置，创建上传目录
    for (size_t i = 0; i < _config.locations.size(); ++i)
    {
        const LocationConfig& loc = _config.locations[i];
        
        // 如果此 location 启用了上传功能
        if (loc.upload_enable && !loc.upload_path.empty())
        {
            // 使用 mkdir 递归创建目录（需要处理多级路径）
            if (mkdir(loc.upload_path.c_str(), 0755) == 0)
            {
                Logger::info("Created upload directory: {}", loc.upload_path);
            }
            else if (errno == EEXIST)
            {
                // 目录已存在，这是正常情况
                Logger::info("Upload directory already exists: {}", loc.upload_path);
            }
            else
            {
                // 创建失败但不中止服务器启动
                Logger::warning("Failed to create upload directory: " + loc.upload_path);
            }
        }
    }
}

void Server::_init_server_socket()
{
    _server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_server_fd < 0)
        throw std::runtime_error("Cannot create socket");
    
    // 允许地址复用
    int opt = 1;
    if (setsockopt(_server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
        throw std::runtime_error("Cannot set socket options.");

    // 设置为非阻塞模式
    int flags = fcntl(_server_fd, F_GETFL, 0);
    if (flags == -1 || fcntl(_server_fd, F_SETFL, flags | O_NONBLOCK) == -1)
        throw std::runtime_error("Cannot set socket to non-blocking");

    // --- 使用配置数据绑定 ---
    _address.sin_family = AF_INET;
    _address.sin_port = htons(_config.listen_port);
    
    if (_config.host == "0.0.0.0")
        _address.sin_addr.s_addr = INADDR_ANY;
    else
    {
        // 假设配置中的 host 是有效的 IP 字符串
        if (inet_pton(AF_INET, _config.host.c_str(), &(_address.sin_addr)) <= 0)
            throw std::runtime_error("Invalid IP address in config: " + _config.host);
    }

    if (bind(_server_fd, (struct sockaddr *)&_address, sizeof(_address)) < 0)
    {
        std::stringstream port_ss;
        port_ss << _config.listen_port;
        throw std::runtime_error("Cannot bind to " + _config.host + ":" + port_ss.str());
    }

    if (listen(_server_fd, 10) < 0) // 积压队列大小设置为 10
        throw std::runtime_error("Cannot listen to socket");

    Logger::info("Server is listening on {}:{} ...", _config.host, _config.listen_port);
}

void Server::_init_epoll()
{
    _epoll_fd = epoll_create(1024);
    if (_epoll_fd < 0)
        throw std::runtime_error("epoll_create failed");

    // 监听 Server Socket 上的可读事件
    _add_to_epoll(_server_fd, EPOLLIN);
}

void Server::_add_to_epoll(int fd, EPOLL_EVENTS events)
{
    struct epoll_event event;
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_ADD, fd, &event) < 0)
        throw std::runtime_error("epoll_ctl add failed");
}

void Server::_modify_epoll(int fd, EPOLL_EVENTS events)
{
    struct epoll_event event;
    event.events = events;
    event.data.fd = fd;

    if (epoll_ctl(_epoll_fd, EPOLL_CTL_MOD, fd, &event) < 0)
        throw std::runtime_error("epoll_ctl mod failed");
}


// --------------------------------------------------------------------------
// 请求处理核心 (使用配置)
// --------------------------------------------------------------------------

/*
* 处理 HttpRequest 对象，根据 ServerConfig 和 LocationConfig 构建响应。
*/
std::string Server::_process_request(const HttpRequest& request)
{
    // Use RequestHandler to process the request
    RequestHandler handler(_config);
    HttpResponse response = handler.handleRequest(request);
    
    // Convert HttpResponse to HTTP response string
    return response.serialize();
}

// --------------------------------------------------------------------------
// 主循环与 I/O 处理
// --------------------------------------------------------------------------

void Server::start()
{
    struct epoll_event events[MAX_EVENTS];
    Logger::info("Server starting main event loop..."); 

    while (true)
    {
        // -1 表示阻塞直到事件发生
        int nfds = epoll_wait(_epoll_fd, events, MAX_EVENTS, -1); 
        
        if (nfds < 0)
        {
            if (errno == EINTR)
                continue; // 被信号打断，重新等待
            Logger::error("epoll_wait error: {}", strerror(errno));
            break;
        }

        for (int i = 0; i < nfds; i++)
        {
            int current_fd = events[i].data.fd;
            uint32_t events_flag = events[i].events;

            if (current_fd == _server_fd)
                _handle_new_connection();
            else
            {
                // 可读事件 (新数据到达)
                if (events_flag & EPOLLIN)
                    _handle_client_data(current_fd);
                
                // 可写事件 (需要发送响应)
                // 必须检查 client_fd 是否仍然存在，因为 _handle_client_data 可能已经关闭了连接
                if (_clients.count(current_fd) && (events_flag & EPOLLOUT))
                    _handle_client_write(current_fd);
            }
        }
    }
}

void Server::_handle_new_connection()
{
    sockaddr_in client_addr;
    socklen_t addrlen = sizeof(_address);
    int client_fd = accept(_server_fd, (struct sockaddr *)&client_addr, &addrlen);

    if (client_fd < 0)
    {
        // 在非阻塞模式下，EAGAIN/EWOULDBLOCK 是正常情况
        if (errno == EAGAIN || errno == EWOULDBLOCK)
            return; 
        Logger::error("Faild to accept connection: {}", strerror(errno));
        return;
    }

    // 设置新连接为非阻塞
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags != -1)
        fcntl(client_fd, F_SETFL, flags | O_NONBLOCK);

    // 将新客户端加入 map 并监听读事件
    _clients.insert(std::make_pair(client_fd, Client(client_fd, client_addr)));
    _add_to_epoll(client_fd, EPOLLIN);
    
    // 将 IP 转换为字符串进行日志记录
	char ip_str[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &(client_addr.sin_addr), ip_str, INET_ADDRSTRLEN);
    
    std::stringstream port_stream;
    port_stream << ntohs(client_addr.sin_port);

    std::string client_address = std::string(ip_str) + ":" + port_stream.str();

    Logger::info("New connection accepted. FD {} from {}", client_fd, client_address);
}

void Server::_handle_client_data(int client_fd)
{
    Client& client = _clients[client_fd];
    char buffer[4096];
    ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));

    if (bytes_read > 0)
    {
        client.request_buffer.append(buffer, bytes_read);

        // --- HTTP 结束检测 ---
        // 检查是否已收到完整的 HTTP 请求（包括 body）
        if (_is_request_complete(client.request_buffer))
        {
            Logger::info("----- Full Request from client FD {} -----\n{}", client_fd, client.request_buffer);
            
            try {
                // 1. 解析请求
                HttpRequest request(client.request_buffer); 

                // 2. 处理请求，生成响应 
                std::string response = _process_request(request);

                // 3. 准备响应发送
                client.response_buffer = response;
                client.request_buffer.clear(); // 清空请求缓冲区，准备下一次请求

                // 4. 监听写事件
                _modify_epoll(client_fd, static_cast<EPOLL_EVENTS>(EPOLLIN | EPOLLOUT));

                Logger::info("Request received, preparing to send response to FD {}...", client_fd);
            } 
            catch (const std::exception& e) {
                // 解析失败或内部错误
                Logger::error("Request handling error on FD {}: {}", client_fd, e.what());
                client.response_buffer = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
                client.request_buffer.clear();
                _modify_epoll(client_fd, static_cast<EPOLL_EVENTS>(EPOLLIN | EPOLLOUT));
            }
        }
    }
    else if (bytes_read == 0)
    {
        // 客户端主动断开连接
        Logger::info("Client {} disconnected.", client_fd);
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        _clients.erase(client_fd);
    }
    else if (bytes_read < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
    {
        // 真正的读取错误
        Logger::error("Read error on FD {}: {}", client_fd, strerror(errno));
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        _clients.erase(client_fd);
    }
}

void Server::_handle_client_write(int client_fd)
{
    Client& client = _clients.at(client_fd); // 使用 at() 确保客户端存在
    std::string& buffer = client.response_buffer;

    if (buffer.empty()) return;

    ssize_t bytes_sent = send(client_fd, buffer.c_str(), buffer.length(), 0);
    
    if (bytes_sent < 0)
    {
        // 发送错误
        Logger::error("Send error on FD {}: {}", client_fd, strerror(errno));
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        _clients.erase(client_fd);
        return;
    }

    buffer.erase(0, bytes_sent); // 从缓冲区移除已发送的数据

    if (buffer.empty())
    {
        Logger::info("Response sent fully to FD {}", client_fd);

        // HTTP/1.1 默认 Keep-Alive, 但在实现完整的 Keep-Alive 逻辑前，选择关闭连接 (短连接)
        // [TODO]: 根据请求和配置决定是否保持连接
        epoll_ctl(_epoll_fd, EPOLL_CTL_DEL, client_fd, NULL);
        close(client_fd);
        _clients.erase(client_fd);
    }
}

/**
 * Check if a complete HTTP request has been received
 * Returns true if headers are complete and body (if any) is fully received
 */
bool Server::_is_request_complete(const std::string& request_buffer)
{
    // First, check if headers are complete (look for \r\n\r\n)
    size_t headers_end = request_buffer.find("\r\n\r\n");
    if (headers_end == std::string::npos)
    {
        // Headers not complete yet
        return false;
    }
    
    // Headers are complete. Now check if body is complete
    headers_end += 4; // Move past \r\n\r\n
    
    // Extract Content-Length from headers
    size_t content_length_pos = request_buffer.find("Content-Length:");
    if (content_length_pos == std::string::npos)
    {
        // No Content-Length header, request is complete (headers only)
        return true;
    }
    
    // Parse Content-Length value
    content_length_pos += 15; // Length of "Content-Length:"
    
    // Skip whitespace
    while (content_length_pos < request_buffer.length() && 
           (request_buffer[content_length_pos] == ' ' || request_buffer[content_length_pos] == '\t'))
    {
        content_length_pos++;
    }
    
    // Extract the number
    size_t number_end = content_length_pos;
    while (number_end < request_buffer.length() && 
           request_buffer[number_end] >= '0' && request_buffer[number_end] <= '9')
    {
        number_end++;
    }
    
    if (number_end == content_length_pos)
    {
        // Could not parse Content-Length value
        return false;
    }
    
    std::string content_length_str = request_buffer.substr(content_length_pos, number_end - content_length_pos);
    size_t content_length = 0;
    for (size_t i = 0; i < content_length_str.length(); i++)
    {
        content_length = content_length * 10 + (content_length_str[i] - '0');
    }
    
    // Check if we have received the full body
    size_t body_received = request_buffer.length() - headers_end;
    
    if (body_received >= content_length)
    {
        // Full body has been received
        return true;
    }
    
    // Still waiting for more body data
    return false;
}

} // namespace wsv
