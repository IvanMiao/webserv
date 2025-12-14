#include "Client.hpp"

#include "Client.hpp"
#include <unistd.h> // 需要 close 函数

Client::Client()
    : client_fd(-1) // 初始化 fd 为安全值
{
    // address 结构体不需要特殊初始化，零初始化即可
}

Client::Client(int fd, const sockaddr_in& addr)
    : client_fd(fd), address(addr)
{
    // 初始化缓冲区为空
    request_buffer.clear();
    response_buffer.clear();
}

Client::~Client()
{
    // **注意：** 您的 Server.cpp 负责关闭 fd。
    // 但是，如果您决定在 Client 析构时关闭 fd，则需要在此处实现：
    /* if (client_fd >= 0) {
        close(client_fd);
    }
    */
    // 由于您的 Server.cpp 已经在 map 擦除前调用了 close(client_fd)，
    // 此处留空是合理的，可以避免重复关闭。
}
