#include "ConfigParser.hpp"
#include <cctype> 
#include <cstdlib> 
#include <algorithm>

// ==================== LocationConfig ====================
LocationConfig::LocationConfig() 
    : path("/") 
    , root("/var/www/html") 
    , index("index.html") 
    , autoindex(false) 
    , redirect_code(0) 
    , upload_enable(false) 
{ 
    allow_methods.push_back("GET"); 
}

bool LocationConfig::isMethodAllowed(const std::string& method) const
{ 
    for (size_t i = 0; i < allow_methods.size(); i++) 
    { 
        if (allow_methods[i] == method) 
        return true; 
    } 
    return false; 
}

// ==================== ServerConfig ==================== 
ServerConfig::ServerConfig() 
    : host("0.0.0.0") 
    , listen_port(8080) 
    , root("/var/www/html") 
    , client_max_body_size(1048576)
    { }

const LocationConfig* ServerConfig::findLocation(const std::string& path) const
{
    const LocationConfig* best_match = NULL;
    size_t best_length = 0;
    
    // 找最长前缀匹配
    for (size_t i = 0; i < locations.size(); i++)
    {
        const LocationConfig& loc = locations[i];
        const std::string& loc_path = loc.path;
        
        // 检查是否是前缀匹配
        if (path.size() >= loc_path.size() &&
            path.substr(0, loc_path.size()) == loc_path)
        {
            // 确保是完整路径匹配
            // 1. 如果 loc_path 以 '/' 结尾 (如 "/" 或 "/images/")，则前缀匹配就足够了
            // 2. 否则，必须是完全匹配，或者 path 的下一个字符是 '/' (如 "/upload" 匹配 "/upload/file")
            if ((!loc_path.empty() && loc_path[loc_path.size() - 1] == '/') || 
                path.size() == loc_path.size() || 
                path[loc_path.size()] == '/')
            {
                if (loc_path.size() > best_length)
                {
                    best_length = loc_path.size();
                    best_match = &loc;
                }
            }
        }
    }
    
    return best_match;
}

// ==================== ConfigParser ====================

ConfigParser::ConfigParser(const std::string& file_path)
    : _filepath(file_path)
{
}

ConfigParser::~ConfigParser()
{
}

void ConfigParser::parse()
{
    std::ifstream file(_filepath.c_str());
    if (!file.is_open())
    {
        throw std::runtime_error("Error: Cannot open config file: " + _filepath);
    }
    
    std::string line;
    while (std::getline(file, line))
    {
        line = _trim(line);
        
        // 跳过空行和注释
        if (line.empty() || line[0] == '#')
            continue;
        
        // 找到 server 块
        if (_startsWith(line, "server"))
        {
            _parseServerBlock(file, line);
        }
    }
    
    file.close();
    
    if (_servers.empty())
        throw std::runtime_error("Error: No server configuration found");
}

void ConfigParser::_parseServerBlock(std::ifstream& file, std::string& line)
{
    ServerConfig server;
    
    // 检查是否有 '{'
    if (line.find('{') == std::string::npos)
    {
        // 读取下一行找 '{'
        std::getline(file, line);
        line = _trim(line);
        if (line != "{")
        {
            throw std::runtime_error("Error: Expected '{' after 'server'");
        }
    }
    
    // 解析 server 块内容
    while (std::getline(file, line))
    {
        line = _trim(line);
        
        if (line.empty() || line[0] == '#')
            continue;
        
        // server 块结束
        if (line == "}" || line == "};")
        {
            _servers.push_back(server);
            return;
        }
        
        // listen 127.0.0.1:8080;
        if (_startsWith(line, "listen"))
        {
            std::string value = line.substr(6);  // 跳过 "listen"
            value = _trim(value);
            value = _removeSemicolon(value);
            
            size_t colon_pos = value.find(':');
            if (colon_pos != std::string::npos)
            {
                // host:port 格式
                server.host = value.substr(0, colon_pos);
                std::string port_str = value.substr(colon_pos + 1);
                server.listen_port = std::atoi(port_str.c_str());
            }
            else
            {
                // 只有端口
                server.listen_port = std::atoi(value.c_str());
            }
        }
        // server_name localhost example.com;
        else if (_startsWith(line, "server_name"))
        {
            std::string value = line.substr(11);
            value = _trim(value);
            value = _removeSemicolon(value);
            
            std::vector<std::string> names = _split(value, " \t");
            for (size_t i = 0; i < names.size(); i++)
            {
                if (!names[i].empty())
                    server.server_names.push_back(names[i]);
            }
        }
        // root /var/www/html;
        else if (_startsWith(line, "root"))
        {
            std::string value = line.substr(4);
            value = _trim(value);
            server.root = _removeSemicolon(value);
        }
        // client_max_body_size 10M;
        else if (_startsWith(line, "client_max_body_size"))
        {
            std::string value = line.substr(20);
            value = _trim(value);
            value = _removeSemicolon(value);
            server.client_max_body_size = _parseSize(value);
        }
        // error_page 404 /404.html;
        else if (_startsWith(line, "error_page"))
        {
            std::string value = line.substr(10);
            value = _trim(value);
            value = _removeSemicolon(value);
            
            std::vector<std::string> parts = _split(value, " \t");
            if (parts.size() >= 2)
            {
                int code = std::atoi(parts[0].c_str());
                server.error_pages[code] = parts[1];
            }
        }
        // location / {
        else if (_startsWith(line, "location"))
            _parseLocationBlock(file, line, server);
    }
    
    // 如果到达文件末尾还没有找到 '}'
    _servers.push_back(server);
}

void ConfigParser::_parseLocationBlock(std::ifstream& file, std::string& line,
                                     ServerConfig& server)
{
    LocationConfig location;
    
    // 提取路径: location /uploads {
    std::string value = line.substr(8);  // 跳过 "location"
    value = _trim(value);
    
    // 移除 '{'
    size_t brace_pos = value.find('{');
    if (brace_pos != std::string::npos)
    {
        location.path = _trim(value.substr(0, brace_pos));
    }
    else
    {
        location.path = value;
        // 读取下一行找 '{'
        std::getline(file, line);
    }
    
    // 解析 location 块内容
    while (std::getline(file, line))
    {
        line = _trim(line);
        
        if (line.empty() || line[0] == '#')
            continue;
        
        // location 块结束
        if (line == "}" || line == "};")
        {
            server.locations.push_back(location);
            return;
        }
        
        // root /var/www/uploads;
        if (_startsWith(line, "root"))
        {
            std::string value = line.substr(4);
            value = _trim(value);
            location.root = _removeSemicolon(value);
        }
        // allow_methods GET POST DELETE;
        else if (_startsWith(line, "allow_methods") || 
                 _startsWith(line, "allowed_methods"))
        {
            size_t pos = line.find("methods");
            std::string value = line.substr(pos + 7);
            value = _trim(value);
            value = _removeSemicolon(value);
            
            location.allow_methods.clear();
            std::vector<std::string> methods = _split(value, " \t");
            for (size_t i = 0; i < methods.size(); i++)
            {
                if (!methods[i].empty())
                    location.allow_methods.push_back(methods[i]);
            }
        }
        // index index.html index.htm;
        else if (_startsWith(line, "index"))
        {
            std::string value = line.substr(5);
            value = _trim(value);
            value = _removeSemicolon(value);
            
            // 取第一个 index 文件
            std::vector<std::string> indexes = _split(value, " \t");
            if (!indexes.empty())
                location.index = indexes[0];
        }
        // autoindex on;
        else if (_startsWith(line, "autoindex"))
        {
            std::string value = line.substr(9);
            value = _trim(value);
            value = _removeSemicolon(value);
            location.autoindex = (value == "on");
        }
        // return 301 /new-path;
        else if (_startsWith(line, "return"))
        {
            std::string value = line.substr(6);
            value = _trim(value);
            value = _removeSemicolon(value);
            
            std::vector<std::string> parts = _split(value, " \t");
            if (parts.size() >= 2)
            {
                location.redirect_code = std::atoi(parts[0].c_str());
                location.redirect_url = parts[1];
            }
        }
        // upload_path /var/www/uploads;
        else if (_startsWith(line, "upload_path"))
        {
            std::string value = line.substr(11);
            value = _trim(value);
            location.upload_path = _removeSemicolon(value);
            location.upload_enable = true;
        }
        // upload_enable on;
        else if (_startsWith(line, "upload_enable"))
        {
            std::string value = line.substr(13);
            value = _trim(value);
            value = _removeSemicolon(value);
            location.upload_enable = (value == "on");
        }
        // cgi_extension .py;
        else if (_startsWith(line, "cgi_extension"))
        {
            std::string value = line.substr(13);
            value = _trim(value);
            location.cgi_extension = _removeSemicolon(value);
        }
        // cgi_path /usr/bin/python3;
        else if (_startsWith(line, "cgi_path"))
        {
            std::string value = line.substr(8);
            value = _trim(value);
            location.cgi_path = _removeSemicolon(value);
        }
    }
    
    server.locations.push_back(location);
}

const std::vector<ServerConfig>& ConfigParser::getServers() const
{
    return _servers;
}

// ==================== 工具方法 ====================

std::string ConfigParser::_trim(const std::string& str) const
{
    size_t start = 0;
    size_t end = str.size();
    
    while (start < end && std::isspace(str[start]))
        start++;
    
    while (end > start && std::isspace(str[end - 1]))
        end--;
    
    return str.substr(start, end - start);
}

std::vector<std::string> ConfigParser::_split(const std::string& str,
                                             const std::string& delimiters) const
{
    std::vector<std::string> result;
    size_t start = 0;
    size_t end = 0;
    
    while (end < str.size())
    {
        // 跳过分隔符
        while (start < str.size() && 
               delimiters.find(str[start]) != std::string::npos)
        {
            start++;
        }
        
        end = start;
        
        // 找到下一个分隔符
        while (end < str.size() && 
               delimiters.find(str[end]) == std::string::npos)
        {
            end++;
        }
        
        if (start < end)
        {
            result.push_back(str.substr(start, end - start));
        }
        
        start = end;
    }
    
    return result;
}

size_t ConfigParser::_parseSize(const std::string& str) const
{
    if (str.empty())
        return 0;
    
    size_t value = 0;
    size_t i = 0;
    
    // 读取数字
    while (i < str.size() && std::isdigit(str[i]))
    {
        value = value * 10 + (str[i] - '0');
        i++;
    }
    
    // 跳过空格
    while (i < str.size() && std::isspace(str[i]))
        i++;
    
    // 读取单位
    if (i < str.size())
    {
        char unit = std::toupper(str[i]);
        switch (unit)
        {
            case 'K':
                return value * 1024;
            case 'M':
                return value * 1024 * 1024;
            case 'G':
                return value * 1024 * 1024 * 1024;
            default:
                return value;
        }
    }
    
    return value;
}

bool ConfigParser::_startsWith(const std::string& str, 
                              const std::string& prefix) const
{
    if (str.size() < prefix.size())
        return false;
    
    return str.substr(0, prefix.size()) == prefix;
}

std::string ConfigParser::_removeSemicolon(const std::string& str) const
{
    std::string result = str;
    
    // 移除末尾的分号
    while (!result.empty() && result[result.size() - 1] == ';')
        result.erase(result.size() - 1);
    
    return _trim(result);
}


/*
FOR TESTING
*/
void ConfigParser::printConfig() const
{
    std::cout << "=== Configuration ===" << std::endl;
    std::cout << "Total servers: " << _servers.size() << std::endl << std::endl;
    
    for (size_t i = 0; i < _servers.size(); i++)
    {
        const ServerConfig& server = _servers[i];
        std::cout << "Server #" << (i + 1) << ":" << std::endl;
        std::cout << "  Host: " << server.host << std::endl;
        std::cout << "  Port: " << server.listen_port << std::endl;
        std::cout << "  Root: " << server.root << std::endl;
        std::cout << "  Max body size: " << server.client_max_body_size 
                  << " bytes" << std::endl;
        
        if (!server.server_names.empty())
        {
            std::cout << "  Server names: ";
            for (size_t j = 0; j < server.server_names.size(); j++)
            {
                std::cout << server.server_names[j];
                if (j < server.server_names.size() - 1)
                    std::cout << ", ";
            }
            std::cout << std::endl;
        }
        
        if (!server.error_pages.empty())
        {
            std::cout << "  Error pages:" << std::endl;
            for (std::map<int, std::string>::const_iterator it = server.error_pages.begin();
                 it != server.error_pages.end(); ++it)
            {
                std::cout << "    " << it->first << " -> " << it->second << std::endl;
            }
        }
        
        std::cout << "  Locations: " << server.locations.size() << std::endl;
        for (size_t j = 0; j < server.locations.size(); j++)
        {
            const LocationConfig& loc = server.locations[j];
            std::cout << "    [" << loc.path << "]" << std::endl;
            std::cout << "      Root: " << loc.root << std::endl;
            std::cout << "      Index: " << loc.index << std::endl;
            std::cout << "      Autoindex: " << (loc.autoindex ? "on" : "off") << std::endl;
            
            if (!loc.allow_methods.empty())
            {
                std::cout << "      Methods: ";
                for (size_t k = 0; k < loc.allow_methods.size(); k++)
                {
                    std::cout << loc.allow_methods[k];
                    if (k < loc.allow_methods.size() - 1)
                        std::cout << ", ";
                }
                std::cout << std::endl;
            }
            
            if (loc.hasRedirect())
            {
                std::cout << "      Redirect: " << loc.redirect_code 
                         << " -> " << loc.redirect_url << std::endl;
            }
            
            if (loc.upload_enable)
            {
                std::cout << "      Upload: enabled" << std::endl;
                std::cout << "      Upload path: " << loc.upload_path << std::endl;
            }
        }
        
        std::cout << std::endl;
    }
}
