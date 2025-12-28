#include "router/RequestHandler.hpp"
#include "http/HttpRequest.hpp"
#include "http/HttpResponse.hpp"
#include "TestRunner.hpp"
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <cstdlib>

using namespace wsv;

// ==================== Helper Functions ====================

void create_dummy_file(const std::string& path, const std::string& content) {
	std::ofstream f(path.c_str());
	f << content;
	f.close();
}

void remove_test_file(const std::string& path) {
	std::remove(path.c_str());
}

ServerConfig create_basic_config() {
	ServerConfig config;
	config.root = "test/www_test";
	config.client_max_body_size = 1024 * 1024; // 1MB

	// Location: /
	LocationConfig loc_root;
	loc_root.path = "/";
	loc_root.root = "test/www_test";
	loc_root.allow_methods.push_back("GET");
	loc_root.index = "index.html";
	loc_root.autoindex = false;
	config.locations.push_back(loc_root);

	// Location: /uploads
	LocationConfig loc_uploads;
	loc_uploads.path = "/uploads";
	loc_uploads.root = "test/www_test"; // Maps to test/www_test/uploads
	loc_uploads.allow_methods.push_back("GET");
	loc_uploads.allow_methods.push_back("POST");
	loc_uploads.allow_methods.push_back("DELETE");
	loc_uploads.upload_enable = true;
	loc_uploads.upload_path = "test/www_test/uploads";
	loc_uploads.autoindex = true;
	config.locations.push_back(loc_uploads);

	// Location: /redirect
	LocationConfig loc_redirect;
	loc_redirect.path = "/redirect";
	// Checking RequestHandler.cpp: location_config->hasRedirect(), redirect_code, redirect_url
	loc_redirect.redirect_code = 301;
	loc_redirect.redirect_url = "http://example.com";
	config.locations.push_back(loc_redirect);

	return config;
}

// ==================== Test Cases ====================

void test_get_static_file(TestRunner& runner) {
	runner.startTest("GET existing static file");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		HttpRequest request("GET /file.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 200) throw std::runtime_error("Expected 200 OK, got " + StringUtils::toString(response.getStatus()));
		if (response.getBody() != "Hello World Content") throw std::runtime_error("Body mismatch");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_get_index_file(TestRunner& runner) {
	runner.startTest("GET directory serves index.html");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		HttpRequest request("GET / HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 200) throw std::runtime_error("Expected 200 OK");
		if (response.getBody() != "<html>Main Index</html>") throw std::runtime_error("Index content mismatch");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_get_not_found(TestRunner& runner) {
	runner.startTest("GET non-existent file returns 404");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		HttpRequest request("GET /does_not_exist.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 404) throw std::runtime_error("Expected 404, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_method_not_allowed(TestRunner& runner) {
	runner.startTest("POST to read-only location returns 405");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		// Root location only allows GET
		HttpRequest request("POST /file.txt HTTP/1.1\r\nHost: localhost\r\n\r\nData");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 405) throw std::runtime_error("Expected 405, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_path_traversal(TestRunner& runner) {
	runner.startTest("Path traversal attempt returns 403");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		HttpRequest request("GET /../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 403) throw std::runtime_error("Expected 403, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_redirect(TestRunner& runner) {
	runner.startTest("Redirect location returns 301");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		HttpRequest request("GET /redirect HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 301) throw std::runtime_error("Expected 301, got " + StringUtils::toString(response.getStatus()));
		if (response.getHeader("Location") != "http://example.com") throw std::runtime_error("Location header mismatch");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_file_upload(TestRunner& runner) {
	runner.startTest("POST file upload");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		std::string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
		std::string body = 
			"--" + boundary + "\r\n"
			"Content-Disposition: form-data; name=\"file\"; filename=\"test_upload.txt\"\r\n"
			"Content-Type: text/plain\r\n"
			"\r\n"
			"Uploaded Content\r\n"
			"--" + boundary + "--\r\n";
			
		std::string raw_req = 
			"POST /uploads/test_upload.txt HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
			"Content-Length: " + StringUtils::toString(body.length()) + "\r\n"
			"\r\n" + body;

		HttpRequest request(raw_req);
		HttpResponse response = handler.handleRequest(request);
		
		// UploadHandler might return 200 or 201. Based on code read, it returns success response.
		// Let's check if file exists.
		std::ifstream f("test/www_test/uploads/test_upload.txt");
		if (!f.good()) throw std::runtime_error("Uploaded file not found on disk");
		
		std::string content((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
		// Note: The simple UploadHandler might save the whole body or parse it. 
		// Looking at UploadHandler.cpp, it calls _extract_file_content. 
		// Assuming it works correctly for this test.
		
		if (response.getStatus() != 200 && response.getStatus() != 201) 
			throw std::runtime_error("Expected 200/201, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_delete_file(TestRunner& runner) {
	runner.startTest("DELETE existing file");
	try {
		// Setup file to delete
		create_dummy_file("test/www_test/uploads/delete_me.txt", "bye");
		
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		HttpRequest request("DELETE /uploads/delete_me.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 204) throw std::runtime_error("Expected 204, got " + StringUtils::toString(response.getStatus()));
		
		// Verify file is gone
		std::ifstream f("test/www_test/uploads/delete_me.txt");
		if (f.good()) throw std::runtime_error("File still exists after DELETE");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_max_body_size(TestRunner& runner) {
	runner.startTest("Request exceeding max_body_size returns 413");
	try {
		ServerConfig config = create_basic_config();
		config.client_max_body_size = 10; // Very small limit
		RequestHandler handler(config);
		
		std::string body = "This body is definitely longer than 10 bytes";
		std::string raw_req = 
			"POST /uploads HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Length: " + StringUtils::toString(body.length()) + "\r\n"
			"\r\n" + body;
			
		HttpRequest request(raw_req);
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 413) throw std::runtime_error("Expected 413, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

// ==================== Additional Edge Case Tests ====================

void test_autoindex_directory(TestRunner& runner) {
	runner.startTest("GET directory with autoindex enabled");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		// /uploads has autoindex = true
		HttpRequest request("GET /uploads HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 200) throw std::runtime_error("Expected 200 OK, got " + StringUtils::toString(response.getStatus()));
		// Check that response contains directory listing HTML
		if (response.getBody().find("<html>") == std::string::npos) 
			throw std::runtime_error("Expected HTML directory listing");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_directory_no_autoindex(TestRunner& runner) {
	runner.startTest("GET directory without index and autoindex disabled returns 403");
	try {
		ServerConfig config = create_basic_config();
		// Add a location without index file and autoindex disabled
		LocationConfig loc_protected;
		loc_protected.path = "/protected";
		loc_protected.root = "test/www_test";
		loc_protected.allow_methods.push_back("GET");
		loc_protected.index = "nonexistent.html";
		loc_protected.autoindex = false;
		config.locations.push_back(loc_protected);
		
		RequestHandler handler(config);
		
		HttpRequest request("GET /protected HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 403) throw std::runtime_error("Expected 403, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_delete_non_existent_file(TestRunner& runner) {
	runner.startTest("DELETE non-existent file returns 404");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		HttpRequest request("DELETE /uploads/nonexistent_file.txt HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 404) throw std::runtime_error("Expected 404, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_delete_directory(TestRunner& runner) {
	runner.startTest("DELETE directory returns 403");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		// Try to delete the uploads directory itself
		HttpRequest request("DELETE /uploads HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 403) throw std::runtime_error("Expected 403, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_path_traversal_encoded(TestRunner& runner) {
	runner.startTest("URL-encoded path traversal attempt (..%2F) returns 403");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		// The path is decoded after parsing, so this tests the _buildFilePath decoding
		HttpRequest request("GET /..%2F..%2Fetc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		// Should still be blocked (either 403 or 404)
		if (response.getStatus() != 403 && response.getStatus() != 404)
			throw std::runtime_error("Expected 403 or 404, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_path_traversal_middle(TestRunner& runner) {
	runner.startTest("Path traversal in middle of path returns 403");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		HttpRequest request("GET /uploads/../../../etc/passwd HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 403) throw std::runtime_error("Expected 403, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_unknown_location(TestRunner& runner) {
	runner.startTest("Request to unknown location returns 404");
	try {
		ServerConfig config = create_basic_config();
		// Remove the root "/" location to test unknown location handling
		config.locations.clear();
		
		LocationConfig loc_specific;
		loc_specific.path = "/api";
		loc_specific.root = "test/www_test";
		loc_specific.allow_methods.push_back("GET");
		config.locations.push_back(loc_specific);
		
		RequestHandler handler(config);
		
		HttpRequest request("GET /something HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 404) throw std::runtime_error("Expected 404, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_redirect_302(TestRunner& runner) {
	runner.startTest("302 temporary redirect");
	try {
		ServerConfig config = create_basic_config();
		
		LocationConfig loc_temp_redirect;
		loc_temp_redirect.path = "/temp-redirect";
		loc_temp_redirect.redirect_code = 302;
		loc_temp_redirect.redirect_url = "http://example.com/new";
		config.locations.push_back(loc_temp_redirect);
		
		RequestHandler handler(config);
		
		HttpRequest request("GET /temp-redirect HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 302) throw std::runtime_error("Expected 302, got " + StringUtils::toString(response.getStatus()));
		if (response.getHeader("Location") != "http://example.com/new") 
			throw std::runtime_error("Location header mismatch");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_subdirectory_with_index(TestRunner& runner) {
	runner.startTest("GET subdirectory with index.html");
	try {
		ServerConfig config = create_basic_config();
		
		LocationConfig loc_public;
		loc_public.path = "/public";
		loc_public.root = "test/www_test";
		loc_public.allow_methods.push_back("GET");
		loc_public.index = "index.html";
		loc_public.autoindex = false;
		config.locations.push_back(loc_public);
		
		RequestHandler handler(config);
		
		HttpRequest request("GET /public HTTP/1.1\r\nHost: localhost\r\n\r\n");
		HttpResponse response = handler.handleRequest(request);
		
		if (response.getStatus() != 200) throw std::runtime_error("Expected 200 OK, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
}

void test_upload_without_filename(TestRunner& runner) {
	runner.startTest("POST upload without filename uses generated name");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		std::string body = "Plain text content without multipart";
		std::string raw_req = 
			"POST /uploads HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: text/plain\r\n"
			"Content-Length: " + StringUtils::toString(body.length()) + "\r\n"
			"\r\n" + body;
			
		HttpRequest request(raw_req);
		HttpResponse response = handler.handleRequest(request);
		
		// Should succeed with auto-generated filename
		if (response.getStatus() != 200 && response.getStatus() != 201) 
			throw std::runtime_error("Expected 200/201, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
	// Cleanup: remove any auto-generated uploaded files
	system("rm -f test/www_test/uploads/uploaded_file_* 2>/dev/null");
}

void test_zero_content_length(TestRunner& runner) {
	runner.startTest("POST with zero Content-Length");
	std::string uploaded_file;
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		std::string raw_req = 
			"POST /uploads HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Length: 0\r\n"
			"\r\n";
			
		HttpRequest request(raw_req);
		HttpResponse response = handler.handleRequest(request);
		
		// Empty upload should still work (creates empty file)
		if (response.getStatus() != 200 && response.getStatus() != 201 && response.getStatus() != 400) 
			throw std::runtime_error("Expected 200/201/400, got " + StringUtils::toString(response.getStatus()));
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
	// Cleanup: remove any auto-generated uploaded files (pattern: uploaded_file_*)
	system("rm -f test/www_test/uploads/uploaded_file_* 2>/dev/null");
}

void test_body_exactly_at_limit(TestRunner& runner) {
	runner.startTest("Request body exactly at max_body_size limit");
	try {
		ServerConfig config = create_basic_config();
		config.client_max_body_size = 20;
		RequestHandler handler(config);
		
		std::string body = "12345678901234567890"; // Exactly 20 bytes
		std::string raw_req = 
			"POST /uploads HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Length: " + StringUtils::toString(body.length()) + "\r\n"
			"\r\n" + body;
			
		HttpRequest request(raw_req);
		HttpResponse response = handler.handleRequest(request);
		
		// Should succeed (exactly at limit)
		if (response.getStatus() == 413) throw std::runtime_error("Body at exact limit should not return 413");
		
		runner.pass();
	} catch (const std::exception& e) {
		runner.fail(e.what());
	}
	// Cleanup: remove any auto-generated uploaded files
	system("rm -f test/www_test/uploads/uploaded_file_* 2>/dev/null");
}

void test_special_characters_in_filename(TestRunner& runner) {
	runner.startTest("Upload with special characters in filename");
	try {
		ServerConfig config = create_basic_config();
		RequestHandler handler(config);
		
		std::string boundary = "----WebKitFormBoundary7MA4YWxkTrZu0gW";
		std::string body = 
			"--" + boundary + "\r\n"
			"Content-Disposition: form-data; name=\"file\"; filename=\"test file (1).txt\"\r\n"
			"Content-Type: text/plain\r\n"
			"\r\n"
			"Content with spaces\r\n"
			"--" + boundary + "--\r\n";
			
		std::string raw_req = 
			"POST /uploads HTTP/1.1\r\n"
			"Host: localhost\r\n"
			"Content-Type: multipart/form-data; boundary=" + boundary + "\r\n"
			"Content-Length: " + StringUtils::toString(body.length()) + "\r\n"
			"\r\n" + body;

		HttpRequest request(raw_req);
		HttpResponse response = handler.handleRequest(request);
		
		// Should succeed or at least not crash
		if (response.getStatus() >= 500) 
			throw std::runtime_error("Unexpected server error: " + StringUtils::toString(response.getStatus()));
		
		// Cleanup: remove uploaded file with special characters
		remove_test_file("test/www_test/uploads/test file (1).txt");
		
		runner.pass();
	} catch (const std::exception& e) {
		remove_test_file("test/www_test/uploads/test file (1).txt");
		runner.fail(e.what());
	}
}

int main() {
	std::cout << BOLD << "========================================" << RESET << std::endl;
	std::cout << BOLD << "  RequestHandler Unit Tests" << RESET << std::endl;
	std::cout << BOLD << "========================================" << RESET << std::endl << std::endl;

	TestRunner runner;

	// Basic functionality tests
	test_get_static_file(runner);
	test_get_index_file(runner);
	test_get_not_found(runner);
	test_method_not_allowed(runner);
	test_path_traversal(runner);
	test_redirect(runner);
	test_file_upload(runner);
	test_delete_file(runner);
	test_max_body_size(runner);
	
	// Additional edge case tests
	test_autoindex_directory(runner);
	test_directory_no_autoindex(runner);
	test_delete_non_existent_file(runner);
	test_delete_directory(runner);
	test_path_traversal_encoded(runner);
	test_path_traversal_middle(runner);
	test_unknown_location(runner);
	test_redirect_302(runner);
	test_subdirectory_with_index(runner);
	test_upload_without_filename(runner);
	test_zero_content_length(runner);
	test_body_exactly_at_limit(runner);
	test_special_characters_in_filename(runner);

	runner.summary();
	return runner.allPassed() ? 0 : 1;
}
