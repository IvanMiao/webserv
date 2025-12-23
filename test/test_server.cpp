#include "../src/server/Server.hpp"
#include "../src/server/Client.hpp"
#include "../src/config/ConfigParser.hpp"
#include <iostream>
#include <cassert>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>

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

// ==================== Client Tests ====================

void test_client_construction(TestRunner& runner)
{
	runner.startTest("Client default construction");
	try {
		wsv::Client client;
		
		assert(client.client_fd == -1);
		assert(client.state == wsv::CLIENT_READING_REQUEST);
		assert(client.configs == NULL);
		assert(client.request_buffer.empty());
		assert(client.response_buffer.empty());
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_client_construction_with_params(TestRunner& runner)
{
	runner.startTest("Client construction with parameters");
	try {
		sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(8080);
		inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
		
		std::vector<wsv::ServerConfig> configs;
		wsv::Client client(42, addr, &configs);
		
		assert(client.client_fd == 42);
		assert(client.state == wsv::CLIENT_READING_REQUEST);
		assert(client.configs == &configs);
		assert(client.request_buffer.empty());
		assert(client.response_buffer.empty());
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_client_state_transitions(TestRunner& runner)
{
	runner.startTest("Client state transitions");
	try {
		wsv::Client client;
		
		assert(client.state == wsv::CLIENT_READING_REQUEST);
		
		client.state = wsv::CLIENT_PROCESSING;
		assert(client.state == wsv::CLIENT_PROCESSING);
		
		client.state = wsv::CLIENT_WRITING_RESPONSE;
		assert(client.state == wsv::CLIENT_WRITING_RESPONSE);
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_client_buffer_operations(TestRunner& runner)
{
	runner.startTest("Client buffer operations");
	try {
		wsv::Client client;
		
		// Test request buffer
		client.request_buffer = "GET / HTTP/1.1\r\n";
		assert(client.request_buffer == "GET / HTTP/1.1\r\n");
		
		client.request_buffer.append("Host: localhost\r\n\r\n");
		assert(client.request_buffer.find("\r\n\r\n") != std::string::npos);
		
		// Test response buffer
		client.response_buffer = "HTTP/1.1 200 OK\r\n\r\n";
		assert(!client.response_buffer.empty());
		
		client.response_buffer.clear();
		assert(client.response_buffer.empty());
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Server Configuration Tests ====================

void test_server_config_defaults(TestRunner& runner)
{
	runner.startTest("ServerConfig default values");
	try {
		wsv::ServerConfig config;
		
		assert(config.host == "0.0.0.0");
		assert(config.listen_port == 8080);
		assert(config.root == "/var/www/html");
		assert(config.client_max_body_size == 1048576); // 1MB
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_location_config_defaults(TestRunner& runner)
{
	runner.startTest("LocationConfig default values");
	try {
		wsv::LocationConfig loc;
		
		assert(loc.path == "/");
		assert(loc.root == "/var/www/html");
		assert(loc.index == "index.html");
		assert(loc.autoindex == false);
		assert(loc.redirect_code == 0);
		assert(loc.upload_enable == false);
		assert(loc.allow_methods.size() == 1);
		assert(loc.allow_methods[0] == "GET");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_location_method_allowed(TestRunner& runner)
{
	runner.startTest("LocationConfig method checking");
	try {
		wsv::LocationConfig loc;
		
		assert(loc.isMethodAllowed("GET") == true);
		assert(loc.isMethodAllowed("POST") == false);
		assert(loc.isMethodAllowed("DELETE") == false);
		
		loc.allow_methods.push_back("POST");
		assert(loc.isMethodAllowed("POST") == true);
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_location_redirect_check(TestRunner& runner)
{
	runner.startTest("LocationConfig redirect detection");
	try {
		wsv::LocationConfig loc;
		
		assert(loc.hasRedirect() == false);
		
		loc.redirect_code = 301;
		loc.redirect_url = "/new-path";
		assert(loc.hasRedirect() == true);
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_find_location_exact_match(TestRunner& runner)
{
	runner.startTest("ServerConfig find location - exact match");
	try {
		wsv::ServerConfig server;
		
		wsv::LocationConfig loc1;
		loc1.path = "/";
		server.locations.push_back(loc1);
		
		wsv::LocationConfig loc2;
		loc2.path = "/api";
		server.locations.push_back(loc2);
		
		const wsv::LocationConfig* found = server.findLocation("/api");
		assert(found != NULL);
		assert(found->path == "/api");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_find_location_prefix_match(TestRunner& runner)
{
	runner.startTest("ServerConfig find location - prefix match");
	try {
		wsv::ServerConfig server;
		
		wsv::LocationConfig loc1;
		loc1.path = "/";
		server.locations.push_back(loc1);
		
		wsv::LocationConfig loc2;
		loc2.path = "/api";
		server.locations.push_back(loc2);
		
		const wsv::LocationConfig* found = server.findLocation("/api/users");
		assert(found != NULL);
		assert(found->path == "/api");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_find_location_longest_match(TestRunner& runner)
{
	runner.startTest("ServerConfig find location - longest match");
	try {
		wsv::ServerConfig server;
		
		wsv::LocationConfig loc1;
		loc1.path = "/";
		server.locations.push_back(loc1);
		
		wsv::LocationConfig loc2;
		loc2.path = "/api";
		server.locations.push_back(loc2);
		
		wsv::LocationConfig loc3;
		loc3.path = "/api/v1";
		server.locations.push_back(loc3);
		
		const wsv::LocationConfig* found = server.findLocation("/api/v1/users");
		assert(found != NULL);
		assert(found->path == "/api/v1");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_find_location_root_fallback(TestRunner& runner)
{
	runner.startTest("ServerConfig find location - root fallback");
	try {
		wsv::ServerConfig server;
		
		wsv::LocationConfig loc1;
		loc1.path = "/";
		server.locations.push_back(loc1);
		
		wsv::LocationConfig loc2;
		loc2.path = "/api";
		server.locations.push_back(loc2);
		
		const wsv::LocationConfig* found = server.findLocation("/unknown/path");
		assert(found != NULL);
		assert(found->path == "/");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Config Parser Tests ====================

void test_config_parser_valid_file(TestRunner& runner)
{
	runner.startTest("ConfigParser parse valid config file");
	try {
		wsv::ConfigParser parser("test/test.conf");
		parser.parse();
		
		const std::vector<wsv::ServerConfig>& servers = parser.getServers();
		assert(servers.size() > 0);
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_config_parser_invalid_file(TestRunner& runner)
{
	runner.startTest("ConfigParser invalid file handling");
	try {
		wsv::ConfigParser parser("test/nonexistent.conf");
		bool threw_exception = false;
		
		try {
			parser.parse();
		} catch (const std::runtime_error&) {
			threw_exception = true;
		}
		
		assert(threw_exception == true);
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Constants Tests ====================

void test_server_constants(TestRunner& runner)
{
	runner.startTest("Server constants are properly defined");
	try {
		assert(MAX_EVENTS == 1024);
		assert(LISTEN_BACKLOG == 128);
		assert(SOCKET_REUSE_OPT == 1);
		assert(READ_BUFFER_SIZE == 4096);
		assert(WRITE_BUFFER_SIZE == 8192);
		assert(EPOLL_TIMEOUT == -1);
		assert(CLIENT_TIMEOUT == 60000);
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Integration Tests ====================

void test_server_construction(TestRunner& runner)
{
	runner.startTest("Server construction with valid config");
	try {
		wsv::ConfigParser config("test/test.conf");
		config.parse();
		
		// Note: Server constructor will bind to ports, so this test
		// should be run when ports are available
		bool constructed = false;
		try {
			wsv::Server server(config);
			constructed = true;
		} catch (const std::runtime_error& e) {
			// Port might be in use - this is acceptable in test environment
			std::string msg(e.what());
			if (msg.find("bind") != std::string::npos || 
			    msg.find("Address already in use") != std::string::npos) {
				std::cout << YELLOW << "⚠ SKIP (port in use)" << RESET << std::endl;
				return;
			}
			throw;
		}
		
		assert(constructed == true);
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Socket Utility Tests ====================

void test_socket_address_setup(TestRunner& runner)
{
	runner.startTest("Socket address structure setup");
	try {
		sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr));
		
		addr.sin_family = AF_INET;
		addr.sin_port = htons(8080);
		
		// Test localhost
		assert(inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr) == 1);
		assert(addr.sin_family == AF_INET);
		assert(ntohs(addr.sin_port) == 8080);
		
		// Test any address
		addr.sin_addr.s_addr = INADDR_ANY;
		assert(addr.sin_addr.s_addr == INADDR_ANY);
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Main Test Runner ====================

int main(int argc, char** argv)
{
	(void)argc;
	(void)argv;

	std::cout << BOLD << "========================================" << RESET << std::endl;
	std::cout << BOLD << "  Webserv Unit Test Suite" << RESET << std::endl;
	std::cout << BOLD << "========================================" << RESET << std::endl << std::endl;

	TestRunner runner;

	// Client tests
	std::cout << BOLD << "--- Client Tests ---" << RESET << std::endl;
	test_client_construction(runner);
	test_client_construction_with_params(runner);
	test_client_state_transitions(runner);
	test_client_buffer_operations(runner);
	std::cout << std::endl;

	// Configuration tests
	std::cout << BOLD << "--- Configuration Tests ---" << RESET << std::endl;
	test_server_config_defaults(runner);
	test_location_config_defaults(runner);
	test_location_method_allowed(runner);
	test_location_redirect_check(runner);
	test_find_location_exact_match(runner);
	test_find_location_prefix_match(runner);
	test_find_location_longest_match(runner);
	test_find_location_root_fallback(runner);
	std::cout << std::endl;

	// Config parser tests
	std::cout << BOLD << "--- Config Parser Tests ---" << RESET << std::endl;
	test_config_parser_valid_file(runner);
	test_config_parser_invalid_file(runner);
	std::cout << std::endl;

	// Constants tests
	std::cout << BOLD << "--- Constants Tests ---" << RESET << std::endl;
	test_server_constants(runner);
	std::cout << std::endl;

	// Socket tests
	std::cout << BOLD << "--- Socket Utility Tests ---" << RESET << std::endl;
	test_socket_address_setup(runner);
	std::cout << std::endl;

	// Integration tests
	std::cout << BOLD << "--- Integration Tests ---" << RESET << std::endl;
	test_server_construction(runner);
	std::cout << std::endl;

	runner.summary();

	return runner.allPassed() ? 0 : 1;
}
