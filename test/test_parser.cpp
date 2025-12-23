#include "ConfigParser.hpp"
#include <iostream>
#include <cassert>

static void printConfig(const std::vector<wsv::ServerConfig>& servers)
{
	std::cout << "=== Configuration (" << servers.size() << " servers) ===" << std::endl;
	for (size_t i = 0; i < servers.size(); i++)
	{
		const wsv::ServerConfig& s = servers[i];
		std::cout << "Server #" << i + 1 << " [" << s.host << ":" << s.listen_port
				  << "] root: " << s.root << " max_body: " << s.client_max_body_size << std::endl;
		
		if (!s.server_names.empty()) {
			std::cout << "  Names: ";
			for (size_t n = 0; n < s.server_names.size(); ++n)
				std::cout << s.server_names[n] << (n == s.server_names.size() - 1 ? "" : ", ");
			std::cout << std::endl;
		}

		if (!s.error_pages.empty()) {
			std::cout << "  Errors: ";
			for (std::map<int, std::string>::const_iterator it = s.error_pages.begin(); it != s.error_pages.end(); ++it)
				std::cout << it->first << "->" << it->second << " ";
			std::cout << std::endl;
		}

		for (size_t j = 0; j < s.locations.size(); j++)
		{
			const wsv::LocationConfig& l = s.locations[j];
			std::cout << "  - Loc [" << l.path << "] root: " << l.root << ", index: " << l.index;
			
			if (!l.allow_methods.empty()) {
				std::cout << ", methods: [";
				for (size_t m = 0; m < l.allow_methods.size(); ++m)
					std::cout << l.allow_methods[m] << (m == l.allow_methods.size() - 1 ? "" : ",");
				std::cout << "]";
			}

			if (l.autoindex) std::cout << ", autoindex: on";
			if (l.hasRedirect()) std::cout << ", redirect: " << l.redirect_code << "->" << l.redirect_url;
			if (l.upload_enable) std::cout << ", upload: " << l.upload_path;
			if (!l.cgi_extension.empty()) std::cout << ", cgi: " << l.cgi_extension << " (" << l.cgi_path << ")";
			
			std::cout << std::endl;
		}
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
		return 1;
	}
	
	try
	{
		wsv::ConfigParser parser(argv[1]);
		parser.parse();
		
		// Print configuration
		printConfig(parser.getServers());
		
		// Test location matching
		std::cout << "\n=== Testing Location Matching ===" << std::endl;
		
		const std::vector<wsv::ServerConfig>& servers = parser.getServers();
		if (!servers.empty())
		{
			const wsv::ServerConfig& server = servers[0];
			
			struct {
				std::string path;
				std::string expected;
			} test_cases[] = {
				{"/", "/"},
				{"/index.html", "/"},
				{"/uploads", "/uploads"},
				{"/uploads/file.txt", "/uploads"},
				{"/old-page", "/old-page"},
				{"/cgi-bin/script.py", "/cgi-bin"},
				{"/nonexistent", "/"}
			};
			
			for (size_t i = 0; i < sizeof(test_cases) / sizeof(test_cases[0]); i++)
			{
				const wsv::LocationConfig* loc = server.findLocation(test_cases[i].path);
				assert(loc != NULL && loc->path == test_cases[i].expected);
				std::cout << "Path: " << test_cases[i].path << " -> Location: " << loc->path << std::endl;
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
