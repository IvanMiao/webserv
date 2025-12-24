#include "ConfigParser.hpp"
#include "../utils/StringUtils.hpp"
#include <cctype> 
#include <cstdlib> 
#include <algorithm>

namespace wsv
{

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
	
	// Find longest prefix match
	for (size_t i = 0; i < locations.size(); i++)
	{
		const LocationConfig& loc = locations[i];
		const std::string& loc_path = loc.path;
		
		// Check if it's a prefix match
		if (path.size() >= loc_path.size() &&
			path.substr(0, loc_path.size()) == loc_path)
		{
			// Ensure it's a full path match
			// 1. If loc_path ends with '/' (e.g., "/" or "/images/"), prefix match is enough
			// 2. Otherwise, it must be an exact match, or the next character in path must be '/' (e.g., "/upload" matches "/upload/file")
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
{ }

ConfigParser::~ConfigParser()
{ }

void ConfigParser::parse()
{
	std::ifstream file(_filepath.c_str());
	if (!file.is_open())
		throw std::runtime_error("Error: Cannot open config file: " + _filepath);
	
	std::string line;
	while (std::getline(file, line))
	{
		line = StringUtils::trim(line);
		
		// Skip empty lines and comments
		if (line.empty() || line[0] == '#')
			continue;
		
		// Find server block
		if (StringUtils::startsWith(line, "server"))
			_parseServerBlock(file, line);
	}
	
	file.close();
	
	if (_servers.empty())
		throw std::runtime_error("Error: No server configuration found");
}

void ConfigParser::_parseServerBlock(std::ifstream& file, std::string& line)
{
	ServerConfig server;
	
	// find '{'
	if (line.find('{') == std::string::npos)
	{
		// Read next line to find '{'
		std::getline(file, line);
		line = StringUtils::trim(line);
		if (line != "{")
		{
			throw std::runtime_error("Error: Expected '{' after 'server'");
		}
	}
	
	// Parse server block content
	while (std::getline(file, line))
	{
		line = StringUtils::trim(line);
		
		if (line.empty() || line[0] == '#')
			continue;
		
		// End of server block
		if (line == "}" || line == "};")
		{
			_servers.push_back(server);
			return;
		}
		
		// listen 127.0.0.1:8080;
		if (StringUtils::startsWith(line, "listen"))
		{
			std::string value = line.substr(6);  // Jump "listen"
			value = StringUtils::trim(value);
			value = StringUtils::removeSemicolon(value);
			
			size_t colon_pos = value.find(':');
			if (colon_pos != std::string::npos)  // host:port
			{
				server.host = value.substr(0, colon_pos);
				std::string port_str = value.substr(colon_pos + 1);
				server.listen_port = std::atoi(port_str.c_str());
			}
			else  // just port
				server.listen_port = std::atoi(value.c_str());
		}
		// server_name localhost example.com;
		else if (StringUtils::startsWith(line, "server_name"))
		{
			std::string value = line.substr(11);
			value = StringUtils::trim(value);
			value = StringUtils::removeSemicolon(value);
			
			std::vector<std::string> names = StringUtils::split(value, " \t");
			for (size_t i = 0; i < names.size(); i++)
			{
				if (!names[i].empty())
					server.server_names.push_back(names[i]);
			}
		}
		// root /var/www/html;
		else if (StringUtils::startsWith(line, "root"))
		{
			std::string value = line.substr(4);
			value = StringUtils::trim(value);
			server.root = StringUtils::removeSemicolon(value);
		}
		// client_max_body_size 10M;
		else if (StringUtils::startsWith(line, "client_max_body_size"))
		{
			std::string value = line.substr(20);
			value = StringUtils::trim(value);
			value = StringUtils::removeSemicolon(value);
			server.client_max_body_size = StringUtils::parseSize(value);
		}
		// error_page 404 /404.html;
		else if (StringUtils::startsWith(line, "error_page"))
		{
			std::string value = line.substr(10);
			value = StringUtils::trim(value);
			value = StringUtils::removeSemicolon(value);
			
			std::vector<std::string> parts = StringUtils::split(value, " \t");
			if (parts.size() >= 2)
			{
				int code = std::atoi(parts[0].c_str());
				server.error_pages[code] = parts[1];
			}
		}
		// location / {
		else if (StringUtils::startsWith(line, "location"))
			_parseLocationBlock(file, line, server);
	}
	
	// If end of file is reached without finding '}'
	_servers.push_back(server);
}

void ConfigParser::_parseLocationBlock(std::ifstream& file, std::string& line,
									 ServerConfig& server)
{
	LocationConfig location;
	
	// Extract path: location /uploads {
	std::string value = line.substr(8);  // Skip "location"
	value = StringUtils::trim(value);
	
	// Remove '{'
	size_t brace_pos = value.find('{');
	if (brace_pos != std::string::npos)
	{
		location.path = StringUtils::trim(value.substr(0, brace_pos));
	}
	else
	{
		location.path = value;
		// Read next line to find '{'
		std::getline(file, line);
	}
	
	// Parse location block content
	while (std::getline(file, line))
	{
		line = StringUtils::trim(line);
		
		if (line.empty() || line[0] == '#')
			continue;
		
		// End of location block
		if (line == "}" || line == "};")
		{
			server.locations.push_back(location);
			return;
		}
		
		// root /var/www/uploads;
		if (StringUtils::startsWith(line, "root"))
		{
			std::string value = line.substr(4);
			value = StringUtils::trim(value);
			location.root = StringUtils::removeSemicolon(value);
		}
		// allow_methods GET POST DELETE;
		else if (StringUtils::startsWith(line, "allow_methods") || 
				 StringUtils::startsWith(line, "allowed_methods"))
		{
			size_t pos = line.find("methods");
			std::string value = line.substr(pos + 7);
			value = StringUtils::trim(value);
			value = StringUtils::removeSemicolon(value);
			
			location.allow_methods.clear();
			std::vector<std::string> methods = StringUtils::split(value, " \t");
			for (size_t i = 0; i < methods.size(); i++)
			{
				if (!methods[i].empty())
					location.allow_methods.push_back(methods[i]);
			}
		}
		// index index.html index.htm;
		else if (StringUtils::startsWith(line, "index"))
		{
			std::string value = line.substr(5);
			value = StringUtils::trim(value);
			value = StringUtils::removeSemicolon(value);
			
			// Take the first index file
			std::vector<std::string> indexes = StringUtils::split(value, " \t");
			if (!indexes.empty())
				location.index = indexes[0];
		}
		// autoindex on;
		else if (StringUtils::startsWith(line, "autoindex"))
		{
			std::string value = line.substr(9);
			value = StringUtils::trim(value);
			value = StringUtils::removeSemicolon(value);
			location.autoindex = (value == "on");
		}
		// return 301 /new-path;
		else if (StringUtils::startsWith(line, "return"))
		{
			std::string value = line.substr(6);
			value = StringUtils::trim(value);
			value = StringUtils::removeSemicolon(value);
			
			std::vector<std::string> parts = StringUtils::split(value, " \t");
			if (parts.size() >= 2)
			{
				location.redirect_code = std::atoi(parts[0].c_str());
				location.redirect_url = parts[1];
			}
		}
		// upload_path /var/www/uploads;
		else if (StringUtils::startsWith(line, "upload_path"))
		{
			std::string value = line.substr(11);
			value = StringUtils::trim(value);
			location.upload_path = StringUtils::removeSemicolon(value);
			location.upload_enable = true;
		}
		// upload_enable on;
		else if (StringUtils::startsWith(line, "upload_enable"))
		{
			std::string value = line.substr(13);
			value = StringUtils::trim(value);
			value = StringUtils::removeSemicolon(value);
			location.upload_enable = (value == "on");
		}
		// cgi_extension .py;
		else if (StringUtils::startsWith(line, "cgi_extension"))
		{
			std::string value = line.substr(13);
			value = StringUtils::trim(value);
			location.cgi_extension = StringUtils::removeSemicolon(value);
		}
		// cgi_path /usr/bin/python3;
		else if (StringUtils::startsWith(line, "cgi_path"))
		{
			std::string value = line.substr(8);
			value = StringUtils::trim(value);
			location.cgi_path = StringUtils::removeSemicolon(value);
		}
	}
	
	server.locations.push_back(location);
}

const std::vector<ServerConfig>& ConfigParser::getServers() const
{
	return _servers;
}

} // namespace wsv
