#include "ConfigParser.hpp"
#include <iostream>

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }
    
    try
    {
        ConfigParser parser(argv[1]);
        parser.parse();
        
        // 打印配置
        parser.printConfig();
        
        // 测试 location 匹配
        std::cout << "\n=== Testing Location Matching ===" << std::endl;
        
        const std::vector<ServerConfig>& servers = parser.getServers();
        if (!servers.empty())
        {
            const ServerConfig& server = servers[0];
            
            std::string test_paths[] = {
                "/",
                "/index.html",
                "/uploads",
                "/uploads/file.txt",
                "/old-page",
                "/cgi-bin/script.py",
                "/nonexistent"
            };
            
            for (size_t i = 0; i < 7; i++)
            {
                const std::string& path = test_paths[i];
                const LocationConfig* loc = server.findLocation(path);
                
                std::cout << "Path: " << path << " -> ";
                if (loc)
                {
                    std::cout << "Location: " << loc->path << std::endl;
                }
                else
                {
                    std::cout << "No matching location" << std::endl;
                }
            }
        }
        
        std::cout << "\nConfig parsing successful!" << std::endl;
        
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}