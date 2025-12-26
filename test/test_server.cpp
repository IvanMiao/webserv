#include "server/Server.hpp"
#include "server/Client.hpp"
#include "http/HttpRequest.hpp"
#include "config/ConfigParser.hpp"
#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <fstream>

#include "TestRunner.hpp"

// Helper class to access protected members of Server
class TestServer : public wsv::Server {
public:
	TestServer(wsv::ConfigParser& config) : wsv::Server(config) {}
	
	// Expose protected methods for testing
	std::string process_request(const wsv::HttpRequest& request, const wsv::ServerConfig& config)
	{
		// Create RequestHandler and process the request directly
		wsv::RequestHandler handler(config);
		wsv::HttpResponse response = handler.handleRequest(request);
		return response.serialize();
	}
};

// ==================== Configuration Logic Tests ====================

void test_config_parser_values(TestRunner& runner)
{
	runner.startTest("ConfigParser parses correct values");
	try {
		wsv::ConfigParser parser("test/test.conf");
		parser.parse();
		
		const std::vector<wsv::ServerConfig>& servers = parser.getServers();
		if (servers.size() != 3) {
			throw std::runtime_error("Expected 3 servers, got " + StringUtils::toString(servers.size()));
		}
		
		// Check first server
		const wsv::ServerConfig& s1 = servers[0];
		if (s1.listen_port != 8080) throw std::runtime_error("Server 1 port mismatch");
		if (s1.host != "127.0.0.1") throw std::runtime_error("Server 1 host mismatch");
		if (s1.root != "./www") throw std::runtime_error("Server 1 root mismatch");
		
		// Check locations
		const wsv::LocationConfig* loc_upload = s1.findLocation("/uploads");
		if (!loc_upload) throw std::runtime_error("Location /uploads not found");
		if (!loc_upload->upload_enable) throw std::runtime_error("Upload not enabled for /uploads");
		if (loc_upload->autoindex) throw std::runtime_error("Autoindex should be off for /uploads");
		
		const wsv::LocationConfig* loc_cgi = s1.findLocation("/cgi-bin");
		if (!loc_cgi) throw std::runtime_error("Location /cgi-bin not found");
		if (loc_cgi->cgi_extension != ".py") throw std::runtime_error("CGI extension mismatch");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_location_matching_logic(TestRunner& runner)
{
	runner.startTest("Location matching logic (longest prefix)");
	try {
		wsv::ServerConfig server;
		
		wsv::LocationConfig loc_root;
		loc_root.path = "/";
		server.locations.push_back(loc_root);
		
		wsv::LocationConfig loc_api;
		loc_api.path = "/api";
		server.locations.push_back(loc_api);
		
		wsv::LocationConfig loc_api_v1;
		loc_api_v1.path = "/api/v1";
		server.locations.push_back(loc_api_v1);
		
		// Test cases
		if (server.findLocation("/")->path != "/") throw std::runtime_error("Failed match /");
		if (server.findLocation("/index.html")->path != "/") throw std::runtime_error("Failed match /index.html");
		if (server.findLocation("/api")->path != "/api") throw std::runtime_error("Failed match /api");
		if (server.findLocation("/api/users")->path != "/api") throw std::runtime_error("Failed match /api/users");
		if (server.findLocation("/api/v1")->path != "/api/v1") throw std::runtime_error("Failed match /api/v1");
		if (server.findLocation("/api/v1/users")->path != "/api/v1") throw std::runtime_error("Failed match /api/v1/users");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Server Request Processing Tests ====================

void test_server_static_file_response(TestRunner& runner)
{
	runner.startTest("Server handles static file request (index.html)");
	try {
		wsv::ConfigParser parser("test/test.conf");
		parser.parse();
		TestServer server(parser);
		
		const std::vector<wsv::ServerConfig>& servers = parser.getServers();
		if (servers.empty()) throw std::runtime_error("No server config found");
		
		std::string request_raw = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
		wsv::HttpRequest request(request_raw);
		std::string response = server.process_request(request, servers[0]);
		
		if (response.find("HTTP/1.1 200 OK") == std::string::npos) throw std::runtime_error("Response missing 200 OK");
		if (response.find("Content-Type: text/html") == std::string::npos) throw std::runtime_error("Response missing Content-Type");
		if (response.find("Hello, Browser.") == std::string::npos) throw std::runtime_error("Response missing file content");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_server_not_found_response(TestRunner& runner)
{
	runner.startTest("Server returns 404 for non-existent location (/echo)");
	try {
		wsv::ConfigParser parser("test/test.conf");
		parser.parse();
		TestServer server(parser);
		
		const std::vector<wsv::ServerConfig>& servers = parser.getServers();
		if (servers.empty()) throw std::runtime_error("No server config found");
		
		std::string request_raw = "GET /echo HTTP/1.1\r\nHost: localhost\r\n\r\n";
		wsv::HttpRequest request(request_raw);
		std::string response = server.process_request(request, servers[0]);
		
		// Should return 404 for non-existent path
		if (response.find("HTTP/1.1 404") == std::string::npos) throw std::runtime_error("Expected 404 response");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_autoindex_enabled(TestRunner& runner)
{
	runner.startTest("Autoindex generates directory listing for /errors/");
	try {
		wsv::ConfigParser parser("test/test.conf");
		parser.parse();
		TestServer server(parser);
		
		const std::vector<wsv::ServerConfig>& servers = parser.getServers();
		if (servers.empty()) throw std::runtime_error("No server config found");
		
		// Request /errors/ directory which has files but no index.html
		// Root location has autoindex on, so it should show directory listing
		std::string request_raw = "GET /errors/ HTTP/1.1\r\nHost: localhost\r\n\r\n";
		wsv::HttpRequest request(request_raw);
		std::string response = server.process_request(request, servers[0]);
		
		// Should return 200 with directory listing HTML
		if (response.find("HTTP/1.1 200 OK") == std::string::npos) 
			throw std::runtime_error("Expected 200 OK response");
		if (response.find("Index of") == std::string::npos) 
			throw std::runtime_error("Expected directory listing with 'Index of'");
		if (response.find("404.html") == std::string::npos) 
			throw std::runtime_error("Expected 404.html in directory listing");
		if (response.find("Content-Type: text/html") == std::string::npos) 
			throw std::runtime_error("Expected HTML content type");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_autoindex_disabled_uploads(TestRunner& runner)
{
	runner.startTest("Autoindex disabled returns 403 for /uploads/");
	try {
		wsv::ConfigParser parser("test/test.conf");
		parser.parse();
		TestServer server(parser);
		
		const std::vector<wsv::ServerConfig>& servers = parser.getServers();
		if (servers.empty()) throw std::runtime_error("No server config found");
		
		// /uploads has autoindex off and no index file
		std::string request_raw = "GET /uploads/ HTTP/1.1\r\nHost: localhost\r\n\r\n";
		wsv::HttpRequest request(request_raw);
		std::string response = server.process_request(request, servers[0]);
		
		// Should return 403 when autoindex is off and no index file exists
		if (response.find("HTTP/1.1 403") == std::string::npos) 
			throw std::runtime_error("Expected 403 Forbidden when autoindex is off");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Main Test Runner ====================

int main()
{
	std::cout << BOLD << "========================================" << RESET << std::endl;
	std::cout << BOLD << "  Server Test Suite" << RESET << std::endl;
	std::cout << BOLD << "========================================" << RESET << std::endl << std::endl;

	TestRunner runner;

	// Configuration Tests
	std::cout << BOLD << "--- Configuration Logic ---" << RESET << std::endl;
	test_config_parser_values(runner);
	test_location_matching_logic(runner);
	std::cout << std::endl;

	// Server Logic Tests
	std::cout << BOLD << "--- Server Request Processing ---" << RESET << std::endl;
	test_server_static_file_response(runner);
	test_server_not_found_response(runner);
	test_autoindex_enabled(runner);
	test_autoindex_disabled_uploads(runner);
	std::cout << std::endl;
	
	runner.summary();

	return runner.allPassed() ? 0 : 1;
}
