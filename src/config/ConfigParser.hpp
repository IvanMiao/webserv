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
	std::vector<std::string>	allow_methods;
	std::string		index;
	bool			autoindex;
	// ... add more params

public:
	LocationConfig();
};


class ServerConfig
{
public:
	int			listen_port;
	std::string	server_name;
	std::string	root;
	size_t		client_max_body_size;
	// ... add more params
	std::map<int, std::string>	error_pages;
	std::vector<LocationConfig>	locations;

public:
	ServerConfig();
};


class ConfigParser
{
private:
	std::string					_filepath;
	std::vector<ServerConfig>	_servers;

public:
	ConfigParser(const std::string& file_path);
	~ConfigParser();

	void parse();
	const std::vector<ServerConfig>& getServers() const;
};

#endif
