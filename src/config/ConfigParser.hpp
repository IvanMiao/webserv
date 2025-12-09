#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>


class LocationConfig
{
public:
	std::string		path;
	std::string		root;
	std::vector<std::string>	allow_methods; // 允许的 HTTP 方法
	std::string		index;
	bool			autoindex;
	// ... add more params

	// 重定向
    int		redirect_code;  // 301, 302
    std::string	redirect_url;   // /new-path
    
    // 上传
    bool	upload_enable;
    std::string	upload_path;
    
    // CGI
    std::string	cgi_extension;  // CGI 文件扩展名: .py, .php
    std::string	cgi_path;       // CGI 可执行路径 /usr/bin/python3

public:
	LocationConfig();

			// 辅助方法
    bool isMethodAllowed(const std::string& method) const;
    bool hasRedirect() const { return redirect_code != 0; }
};


class ServerConfig
{
public:
	std::string	host;               // 监听 IP，127.0.0.1
	int			listen_port;
	//std::string	server_name;
	std::string	root;
	size_t		client_max_body_size; // 最大请求体大小，默认 1MB
	// ... add more params
	std::vector<std::string> server_names; // 服务器名称，可多个
	std::map<int, std::string>	error_pages; // 错误页映射，key=HTTP状态码
	std::vector<LocationConfig>	locations; // 所有 location 配置

public:
	ServerConfig();

	// 查找最佳匹配的 location
    const LocationConfig* findLocation(const std::string& path) const;
};


class ConfigParser
{
private:
	std::string					_filepath;
	std::vector<ServerConfig>	_servers;

	 // 解析辅助方法
    void parseServerBlock(std::ifstream& file, std::string& line);
    void parseLocationBlock(std::ifstream& file, std::string& line, 
                           ServerConfig& server);
    
    // 工具方法
    std::string trim(const std::string& str) const;
    std::vector<std::string> split(const std::string& str, 
                                   const std::string& delimiters) const;
    size_t parseSize(const std::string& str) const;
    bool startsWith(const std::string& str, const std::string& prefix) const;
    std::string removeSemicolon(const std::string& str) const;


public:
	ConfigParser(const std::string& file_path);
	~ConfigParser();

	void parse();
	const std::vector<ServerConfig>& getServers() const;

	 // 配置打印, 调试输出
    void printConfig() const;
};

#endif
