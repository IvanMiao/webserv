#include "../src/config/ConfigParser.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <map>

// ANSI color codes for test output
#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"
#define BOLD "\033[1m"

class TestRunner
{
private:
	int _passed;
	int _failed;
	std::string _current_test;

public:
	TestRunner() : _passed(0), _failed(0) {}

	void startTest(const std::string& name)
	{
		_current_test = name;
		std::cout << YELLOW << "[TEST] " << RESET << name << " ... ";
		std::cout.flush();
	}

	void pass()
	{
		std::cout << GREEN << "PASS" << RESET << std::endl;
		_passed++;
	}

	void fail(const std::string& msg)
	{
		std::cout << RED << "FAIL" << RESET << std::endl;
		std::cout << "  Error: " << msg << std::endl;
		_failed++;
	}

	void summary()
	{
		std::cout << std::endl << BOLD << "=== Test Summary ===" << RESET << std::endl;
		std::cout << "Total: " << (_passed + _failed) << " tests" << std::endl;
		std::cout << GREEN << "Passed: " << _passed << RESET << std::endl;
		if (_failed > 0)
			std::cout << RED << "Failed: " << _failed << RESET << std::endl;
		else
			std::cout << "Failed: " << _failed << std::endl;
	}

	bool allPassed() const { return _failed == 0; }
};

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

void test_location_matching(TestRunner& runner, const wsv::ServerConfig& server)
{
	runner.startTest("Location Matching");
	try {
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
			if (loc == NULL) {
				throw std::runtime_error("Location not found for path: " + test_cases[i].path);
			}
			if (loc->path != test_cases[i].expected) {
				throw std::runtime_error("Expected location " + test_cases[i].expected + " for path " + test_cases[i].path + ", but got " + loc->path);
			}
			// std::cout << "Path: " << test_cases[i].path << " -> Location: " << loc->path << std::endl;
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

	try
	{
		runner.startTest("Config Parsing");
		wsv::ConfigParser parser(argv[1]);
		parser.parse();
		runner.pass();
		
		// Print configuration
		printConfig(parser.getServers());
		
		// Test location matching
		std::cout << "\n=== Testing Location Matching ===" << std::endl;
		
		const std::vector<wsv::ServerConfig>& servers = parser.getServers();
		if (!servers.empty())
		{
			test_location_matching(runner, servers[0]);
		}
		else
		{
			runner.startTest("Server Count");
			runner.fail("No servers found in configuration");
		}
	}
	catch (const std::exception& e)
	{
		runner.fail(std::string("Exception: ") + e.what());
	}

	runner.summary();
	return runner.allPassed() ? 0 : 1;
}
