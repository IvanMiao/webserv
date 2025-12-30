#include "http/HttpResponse.hpp"
#include "TestRunner.hpp"
#include <iostream>
#include <string>

void test_response_defaults(TestRunner& runner)
{
	runner.startTest("HttpResponse defaults");
	try {
		wsv::HttpResponse response;
		
		if (response.getStatus() != 200) throw std::runtime_error("Default status should be 200");
		if (response.getHeader("Server").empty()) throw std::runtime_error("Server header missing");
		if (response.getHeader("Date").empty()) throw std::runtime_error("Date header missing");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_response_setters(TestRunner& runner)
{
	runner.startTest("HttpResponse setters");
	try {
		wsv::HttpResponse response;
		
		response.setStatus(404);
		response.setHeader("Content-Type", "text/plain");
		response.setBody("Not Found");
		
		if (response.getStatus() != 404) throw std::runtime_error("Status mismatch");
		if (response.getHeader("Content-Type") != "text/plain") throw std::runtime_error("Content-Type mismatch");
		if (response.getBody() != "Not Found") throw std::runtime_error("Body mismatch");
		
		// Check if Content-Length was automatically set
		std::string cl = response.getHeader("Content-Length");
		if (cl != "9") throw std::runtime_error("Content-Length mismatch: " + cl);
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_response_serialization(TestRunner& runner)
{
	runner.startTest("HttpResponse serialization");
	try {
		wsv::HttpResponse response;
		response.setStatus(200);
		response.setHeader("Content-Type", "text/plain");
		response.setBody("Hello");
		
		std::string raw = response.serialize();
		
		if (raw.find("HTTP/1.1 200 OK") == std::string::npos) throw std::runtime_error("Status line missing");
		if (raw.find("Content-Type: text/plain") == std::string::npos) throw std::runtime_error("Content-Type missing");
		if (raw.find("Content-Length: 5") == std::string::npos) throw std::runtime_error("Content-Length missing");
		if (raw.find("\r\n\r\nHello") == std::string::npos) throw std::runtime_error("Body missing or malformed");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_factory_methods(TestRunner& runner)
{
	runner.startTest("HttpResponse factory methods");
	try {
		// Test createOkResponse
		wsv::HttpResponse ok = wsv::HttpResponse::createOkResponse("<h1>Hi</h1>", "text/html");
		if (ok.getStatus() != 200) throw std::runtime_error("createOkResponse status mismatch");
		if (ok.getHeader("Content-Type") != "text/html") throw std::runtime_error("createOkResponse Content-Type mismatch");
		if (ok.getBody() != "<h1>Hi</h1>") throw std::runtime_error("createOkResponse body mismatch");
		
		// Test createErrorResponse
		wsv::HttpResponse err = wsv::HttpResponse::createErrorResponse(403, "Forbidden Access");
		if (err.getStatus() != 403) throw std::runtime_error("createErrorResponse status mismatch");
		if (err.getBody().find("Forbidden Access") == std::string::npos) throw std::runtime_error("createErrorResponse body missing message");
		
		// Test createRedirectResponse
		wsv::HttpResponse red = wsv::HttpResponse::createRedirectResponse(301, "/new-location");
		if (red.getStatus() != 301) throw std::runtime_error("createRedirectResponse status mismatch");
		if (red.getHeader("Location") != "/new-location") throw std::runtime_error("createRedirectResponse Location mismatch");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Edge Cases ====================

void test_response_header_overwrite(TestRunner& runner)
{
	runner.startTest("HttpResponse: Header overwrite");
	try {
		wsv::HttpResponse res;
		res.setHeader("Content-Type", "text/plain");
		res.setHeader("Content-Type", "text/html");
		
		if (res.getHeader("Content-Type") != "text/html") throw std::runtime_error("Header should be overwritten");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_response_body_append(TestRunner& runner)
{
	runner.startTest("HttpResponse: Body append updates Content-Length");
	try {
		wsv::HttpResponse res;
		res.setBody("Hello");
		if (res.getHeader("Content-Length") != "5") throw std::runtime_error("Initial CL wrong");
		
		res.appendBody(" World");
		if (res.getHeader("Content-Length") != "11") throw std::runtime_error("Appended CL wrong");
		if (res.getBody() != "Hello World") throw std::runtime_error("Body content wrong");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

int main()
{
	std::cout << BOLD << "========================================" << RESET << std::endl;
	std::cout << BOLD << "  HttpResponse Test Suite" << RESET << std::endl;
	std::cout << BOLD << "========================================" << RESET << std::endl << std::endl;

	TestRunner runner;

	test_response_defaults(runner);
	test_response_setters(runner);
	test_response_serialization(runner);
	test_factory_methods(runner);

	// Edge Cases
	test_response_header_overwrite(runner);
	test_response_body_append(runner);

	runner.summary();

	return runner.allPassed() ? 0 : 1;
}
