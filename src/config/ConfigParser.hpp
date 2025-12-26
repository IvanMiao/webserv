#ifndef CONFIG_PARSER_HPP
#define CONFIG_PARSER_HPP

#include <string>
#include <map>
#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>

namespace wsv
{

class LocationConfig
{
public:
	std::string		path;
	std::string		root;
	std::vector<std::string>	allow_methods; // allowed HTTP methods
	std::string		index;
	bool			autoindex;

	// Redirection
	int			redirect_code;  // 301, 302
	std::string	redirect_url;   // new-path
	
	// Upload
	bool		upload_enable;
	std::string	upload_path;
	
	// CGI
	std::string	cgi_extension;  // .py, .php
	std::string	cgi_path;       // CGI excutable path /usr/bin/python3

public:
	LocationConfig();

	// helper
	bool isMethodAllowed(const std::string& method) const;
	bool hasRedirect() const { return redirect_code != 0; }
};


class ServerConfig
{
public:
	std::string	host;  // 127.0.0.1
	int			listen_port;
	std::string	root;
	size_t		client_max_body_size; // Max request body size, default 1MB

	std::vector<std::string>	server_names; // Server names, can be multiple
	std::map<int, std::string>	error_pages; // Error page mapping, key=HTTP status code
	std::vector<LocationConfig>	locations; // All location configurations

public:
	ServerConfig();

	// Find the best matching location
	const LocationConfig* findLocation(const std::string& path) const;
};


class ConfigParser
{
private:
	std::string					_filepath;
	std::vector<ServerConfig>	_servers;

	// Parsing helper methods
	void _parseServerBlock(std::ifstream& file, std::string& line);
	void _parseLocationBlock(std::ifstream& file, std::string& line, 
						   ServerConfig& server);

public:
	ConfigParser(const std::string& file_path);
	~ConfigParser();

	void parse();
	const std::vector<ServerConfig>& getServers() const;
};

} // namespace wsv

#endif
