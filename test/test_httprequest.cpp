#include "http/HttpRequest.hpp"
#include "utils/StringUtils.hpp"
#include "TestRunner.hpp"
#include <iostream>
#include <string>
#include <vector>

void test_parse_simple_get(TestRunner& runner)
{
	runner.startTest("Parse simple GET request");
	try {
		std::string raw = "GET /index.html HTTP/1.1\r\n"
						  "Host: localhost\r\n"
						  "User-Agent: curl/7.64.1\r\n"
						  "\r\n";
		
		wsv::HttpRequest request(raw);
		
		if (!request.isComplete()) throw std::runtime_error("Request should be complete");
		if (request.getMethod() != "GET") throw std::runtime_error("Method mismatch: " + request.getMethod());
		if (request.getPath() != "/index.html") throw std::runtime_error("Path mismatch: " + request.getPath());
		if (request.getVersion() != "HTTP/1.1") throw std::runtime_error("Version mismatch: " + request.getVersion());
		if (request.getHeader("Host") != "localhost") throw std::runtime_error("Host header mismatch");
		if (request.getHeader("User-Agent") != "curl/7.64.1") throw std::runtime_error("User-Agent header mismatch");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_parse_post_with_body(TestRunner& runner)
{
	runner.startTest("Parse POST request with body");
	try {
		std::string body = "name=test&value=123";
		std::string raw = "POST /submit HTTP/1.1\r\n"
						  "Host: localhost\r\n"
						  "Content-Length: " + StringUtils::toString(body.length()) + "\r\n"
						  "Content-Type: application/x-www-form-urlencoded\r\n"
						  "\r\n" + body;
		
		wsv::HttpRequest request(raw);
		
		if (!request.isComplete()) throw std::runtime_error("Request should be complete");
		if (request.getMethod() != "POST") throw std::runtime_error("Method mismatch");
		if (request.getBody() != body) throw std::runtime_error("Body mismatch");
		if (request.getContentLength() != body.length()) throw std::runtime_error("Content-Length mismatch");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_parse_chunked_encoding(TestRunner& runner)
{
	runner.startTest("Parse chunked transfer encoding");
	try {
		// "Wiki" in hex is 4 bytes -> 4\r\nWiki\r\n
		// "pedia" in hex is 5 bytes -> 5\r\npedia\r\n
		// " in\r\n\r\nchunks." in hex is 14 bytes (0xE) -> E\r\n in\r\n\r\nchunks.\r\n
		// End -> 0\r\n\r\n
		std::string raw = "POST /chunked HTTP/1.1\r\n"
						  "Host: localhost\r\n"
						  "Transfer-Encoding: chunked\r\n"
						  "\r\n"
						  "4\r\nWiki\r\n"
						  "5\r\npedia\r\n"
						  "E\r\n in\r\n\r\nchunks.\r\n"
						  "0\r\n\r\n";
		
		wsv::HttpRequest request(raw);
		
		if (!request.isComplete()) throw std::runtime_error("Request should be complete");
		if (!request.isChunked()) throw std::runtime_error("Should be chunked");
		
		std::string expected_body = "Wikipedia in\r\n\r\nchunks.";
		if (request.getBody() != expected_body) {
			throw std::runtime_error("Body mismatch. Got: '" + request.getBody() + "'");
		}
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_progressive_parsing(TestRunner& runner)
{
	runner.startTest("Progressive parsing (partial data)");
	try {
		wsv::HttpRequest request;
		
		std::string part1 = "GET /partial ";
		std::string part2 = "HTTP/1.1\r\nHost: ";
		std::string part3 = "localhost\r\n\r\n";
		
		if (request.parse(part1.c_str(), part1.length()) != wsv::PARSING_REQUEST_LINE) 
			throw std::runtime_error("State should be PARSING_REQUEST_LINE after part1");
			
		if (request.parse(part2.c_str(), part2.length()) != wsv::PARSING_HEADERS)
			throw std::runtime_error("State should be PARSING_HEADERS after part2");
			
		if (request.parse(part3.c_str(), part3.length()) != wsv::PARSE_COMPLETE)
			throw std::runtime_error("State should be PARSE_COMPLETE after part3");
			
		if (request.getPath() != "/partial") throw std::runtime_error("Path mismatch");
		if (request.getHeader("Host") != "localhost") throw std::runtime_error("Host mismatch");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_query_string_parsing(TestRunner& runner)
{
	runner.startTest("Query string parsing");
	try {
		std::string raw = "GET /search?q=test&page=1 HTTP/1.1\r\nHost: localhost\r\n\r\n";
		wsv::HttpRequest request(raw);
		
		if (request.getPath() != "/search") throw std::runtime_error("Path mismatch: " + request.getPath());
		if (request.getQuery() != "q=test&page=1") throw std::runtime_error("Query mismatch: " + request.getQuery());
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Edge Cases ====================

void test_malformed_request_line(TestRunner& runner)
{
	runner.startTest("HttpRequest: Malformed request line");
	try {
		// Case 1: Missing version
		{
			wsv::HttpRequest req;
			std::string raw = "GET / \r\nHost: localhost\r\n\r\n";
			req.parse(raw.c_str(), raw.length());
			if (!req.hasError()) throw std::runtime_error("Should fail on missing version");
		}
		// Case 2: Invalid method
		{
			wsv::HttpRequest req;
			std::string raw = "INVALID / HTTP/1.1\r\nHost: localhost\r\n\r\n";
			req.parse(raw.c_str(), raw.length());
			if (!req.hasError()) throw std::runtime_error("Should fail on invalid method");
		}
		// Case 3: Invalid version
		{
			wsv::HttpRequest req;
			std::string raw = "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n";
			req.parse(raw.c_str(), raw.length());
			if (!req.hasError()) throw std::runtime_error("Should fail on unsupported version");
		}
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_header_parsing_edge_cases(TestRunner& runner)
{
	runner.startTest("HttpRequest: Header parsing edge cases");
	try {
		// Case 1: Case insensitivity
		{
			wsv::HttpRequest req("GET / HTTP/1.1\r\nHOST: localhost\r\ncontent-length: 0\r\n\r\n");
			if (req.getHeader("host") != "localhost") throw std::runtime_error("Host header case lookup failed");
			if (req.getHeader("Content-Length") != "0") throw std::runtime_error("Content-Length header case lookup failed");
		}
		// Case 2: Whitespace handling
		{
			wsv::HttpRequest req("GET / HTTP/1.1\r\nHost:   localhost   \r\n\r\n");
			if (req.getHeader("Host") != "localhost") throw std::runtime_error("Whitespace trimming failed");
		}
		// Case 3: Empty header value
		{
			wsv::HttpRequest req("GET / HTTP/1.1\r\nEmpty-Header:\r\nHost: localhost\r\n\r\n");
			if (!req.hasHeader("Empty-Header")) throw std::runtime_error("Empty header not found");
			if (!req.getHeader("Empty-Header").empty()) throw std::runtime_error("Empty header should have empty value");
		}
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_body_parsing_limits(TestRunner& runner)
{
	runner.startTest("HttpRequest: Body parsing limits");
	try {
		// Case 1: Content-Length > Actual Data (Incomplete)
		{
			wsv::HttpRequest req;
			std::string raw = "POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 10\r\n\r\n12345";
			req.parse(raw.c_str(), raw.length());
			if (req.isComplete()) throw std::runtime_error("Request should be incomplete");
		}
		// Case 2: Content-Length < Actual Data (Truncation)
		{
			wsv::HttpRequest req("POST / HTTP/1.1\r\nHost: localhost\r\nContent-Length: 3\r\n\r\n12345");
			if (!req.isComplete()) throw std::runtime_error("Request should be complete");
			if (req.getBody() != "123") throw std::runtime_error("Body should be truncated to Content-Length");
		}
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_chunked_split_parsing(TestRunner& runner)
{
	runner.startTest("HttpRequest: Split chunked parsing");
	try {
		wsv::HttpRequest req;
		
		// Split the chunk size and data across multiple parse calls
		std::string part1 = "POST / HTTP/1.1\r\nHost: localhost\r\nTransfer-Encoding: chunked\r\n\r\n";
		std::string part2 = "4\r"; // Split inside size line
		std::string part3 = "\nWiki\r\n";
		std::string part4 = "0\r\n\r\n";
		
		req.parse(part1.c_str(), part1.length());
		req.parse(part2.c_str(), part2.length());
		req.parse(part3.c_str(), part3.length());
		req.parse(part4.c_str(), part4.length());
		
		if (!req.isComplete()) throw std::runtime_error("Request should be complete");
		if (req.getBody() != "Wiki") throw std::runtime_error("Body mismatch: " + req.getBody());
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_size_limits(TestRunner& runner)
{
	runner.startTest("HttpRequest: Size limits");
	try {
		// Case 1: Request line too long
		{
			wsv::HttpRequest req;
			std::string long_url(8193, 'a');
			std::string raw = "GET /" + long_url + " HTTP/1.1\r\nHost: localhost\r\n\r\n";
			req.parse(raw.c_str(), raw.length());
			if (!req.hasError()) throw std::runtime_error("Should fail on request line too long");
		}
		// Case 2: Headers too long
		{
			wsv::HttpRequest req;
			std::string long_header(8193, 'a');
			std::string raw = "GET / HTTP/1.1\r\nHost: localhost\r\nX-Long: " + long_header + "\r\n\r\n";
			req.parse(raw.c_str(), raw.length());
			if (!req.hasError()) throw std::runtime_error("Should fail on headers too long");
		}
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

int main()
{
	std::cout << BOLD << "========================================" << RESET << std::endl;
	std::cout << BOLD << "  HttpRequest Test Suite" << RESET << std::endl;
	std::cout << BOLD << "========================================" << RESET << std::endl << std::endl;

	TestRunner runner;

	test_parse_simple_get(runner);
	test_parse_post_with_body(runner);
	test_parse_chunked_encoding(runner);
	test_progressive_parsing(runner);
	test_query_string_parsing(runner);

	// Edge Cases
	test_malformed_request_line(runner);
	test_header_parsing_edge_cases(runner);
	test_body_parsing_limits(runner);
	test_chunked_split_parsing(runner);
	test_size_limits(runner);

	runner.summary();

	return runner.allPassed() ? 0 : 1;
}
