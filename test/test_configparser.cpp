#include "TestRunner.hpp"
#include "config/ConfigParser.hpp"
#include "utils/StringUtils.hpp"
#include <iostream>
#include <vector>
#include <fstream>
#include <map>
#include <cstdio> // for remove

// Helper to print configuration
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

void test_valid_config(TestRunner& runner, const std::string& config_path)
{
	runner.startTest("Parse Valid Configuration");
	try {
		wsv::ConfigParser parser(config_path);
		parser.parse();
		const std::vector<wsv::ServerConfig>& servers = parser.getServers();

		if (servers.size() != 3) {
			throw std::runtime_error("Expected 3 servers, got " + StringUtils::toString((int)servers.size()));
		}

		// Server 1 assertions
		const wsv::ServerConfig& s1 = servers[0];
		if (s1.listen_port != 8080) throw std::runtime_error("Server 1: Wrong port");
		if (s1.host != "127.0.0.1") throw std::runtime_error("Server 1: Wrong host");
		if (s1.server_names[0] != "localhost") throw std::runtime_error("Server 1: Wrong server_name");
		if (s1.error_pages.at(404) != "/errors/404.html") throw std::runtime_error("Server 1: Wrong error page 404");

		// Server 1 Location assertions
		const wsv::LocationConfig* loc_root = s1.findLocation("/");
		if (!loc_root) throw std::runtime_error("Server 1: Location / not found");
		if (!loc_root->autoindex) throw std::runtime_error("Server 1: autoindex should be on for /");

		const wsv::LocationConfig* loc_upload = s1.findLocation("/uploads");
		if (!loc_upload) throw std::runtime_error("Server 1: Location /uploads not found");
		if (!loc_upload->upload_enable) throw std::runtime_error("Server 1: upload should be enabled for /uploads");

		// Server 3 assertions (simple one)
		const wsv::ServerConfig& s3 = servers[2];
		if (s3.listen_port != 8084) throw std::runtime_error("Server 3: Wrong port");

		runner.pass();

		// Print config for visual check
		printConfig(servers); 

	} catch (const std::exception& e) {
		runner.fail(std::string("Exception: ") + e.what());
	}
}

void test_invalid_config(TestRunner& runner)
{
	runner.startTest("Parse Invalid Configuration");
	std::string filename = "temp_invalid.conf";
	std::ofstream out(filename.c_str());
	out << "server { listen 8080"; // Syntax error: unclosed brace
	out.close();

	try {
		wsv::ConfigParser parser(filename);
		parser.parse();
		runner.fail("Should have thrown exception for invalid config");
	} catch (const std::exception& e) {
		runner.pass(); // Expected exception
	}
	std::remove(filename.c_str());
}

void test_location_matching(TestRunner& runner, const std::string& config_path)
{
	runner.startTest("Location Matching Logic");
	try {
		wsv::ConfigParser parser(config_path);
		parser.parse();
		const wsv::ServerConfig& server = parser.getServers()[0]; // Use first server

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
			if (loc == NULL)
				throw std::runtime_error("Location not found for path: " + test_cases[i].path);
			if (loc->path != test_cases[i].expected)
				throw std::runtime_error("Expected location " + test_cases[i].expected + " for path " + test_cases[i].path + ", but got " + loc->path);
		}
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

int main(int argc, char** argv)
{
	if (argc != 2)
	{
		std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
		return 1;
	}
	
	TestRunner runner;
	std::string config_path = argv[1];

	std::cout << BOLD << "=== Config Parser Tests ===" << RESET << std::endl;

	test_valid_config(runner, config_path);
	test_location_matching(runner, config_path);
	test_invalid_config(runner);

	runner.summary();
	return runner.allPassed() ? 0 : 1;
}
