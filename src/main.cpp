#include "server/Server.hpp"
#include "utils/Logger.hpp"
#include "config/ConfigParser.hpp" // <--- 新增：包含配置解析器

#include <iostream>
#include <vector>
#include <cstdlib>

#define DEFAULT_CONFIG_PATH "config/default.conf" // 假设配置文件的路径

int main( void )
{
    // ------------------------------------------------
    // 1. 解析配置
    // ------------------------------------------------
    wsv::ConfigParser parser(DEFAULT_CONFIG_PATH);
    std::vector<wsv::ServerConfig> configs;

    try
    {
        parser.parse();
        configs = parser.getServers();

        if (configs.empty()) {
            wsv::Logger::error("Configuration file '{}' contains no server blocks.", DEFAULT_CONFIG_PATH);
            return 1;
        }
    }
    catch (const std::exception& e)
    {
        wsv::Logger::error("Configuration error: {}", e.what());
        return 1;
    }

    // ------------------------------------------------
    // 2. 实例化并启动服务器 (只启动第一个配置，以简化主循环)
    // ------------------------------------------------
    try
    {
        // 引用第一个解析出的 ServerConfig 对象
        const wsv::ServerConfig& server_config = configs[0]; 
        
        // 使用配置对象来构造 Server 实例
        wsv::Server my_server(server_config); 
        
        wsv::Logger::info("Starting server with config for host: {}", server_config.host);
        
        my_server.start(); // 进入 epoll 主循环
    }
    catch( const std::exception& e )
    {
        // 注意：此处是运行时错误 (如 socket 绑定失败)
        wsv::Logger::error("Runtime server error: {}", e.what());
        return 1;
    }

    return 0;
}

// #include "server/Server.hpp"
// #include "utils/Logger.hpp"

// #include <iostream>

// int	main( void )
// {
// 	try
// 	{
// 		wsv::Server my_server(8080);
// 		my_server.start();
// 	}
// 	catch( const std::exception& e )
// 	{
// 		wsv::Logger::error(e.what());
// 		return 1;
// 	}

// 	return 0;
// }
