#include "TestUtils.hpp"
#include "../src/cgi/CgiHandler.hpp"

// ----------------------------------------------------
// CGI 逻辑测试：Header 解析 (不需要执行 fork/execve)
// ----------------------------------------------------

// 测试用派生类（假设 CgiHandler 已对测试类开放访问）
class CgiHandlerTestable : public CgiHandler {
public:
    CgiHandlerTestable(const std::string& cgi_bin, const std::string& script_path)
        : CgiHandler(cgi_bin, script_path) {}

    // 暴露私有方法
    void runParseOutput() { _parseOutput(); }

    // 直接操作私有成员
    void setRawOutput(const std::string& raw) { _output = raw; }

    std::string getRawOutput() const { return _output; }
    std::map<std::string, std::string> getHeaders() const { return _response_headers; }
};


// ----------------------------------------------------
// Header Parsing Tests
// ----------------------------------------------------

TEST_CASE(CgiParsing, BasicHeaders) {
    CgiHandlerTestable handler("bin", "script");

    std::string raw_output =
        "Content-Type: text/html\r\nStatus: 200 OK\r\n\r\n<html>Body</html>";

    handler.setRawOutput(raw_output);
    handler.runParseOutput();

    std::map<std::string, std::string> headers = handler.getHeaders();

    ASSERT_EQ(headers.size(), 2, "Should have exactly 2 headers parsed.");
    ASSERT_STREQ(headers.at("Content-Type").c_str(), "text/html", "Content-Type mismatch.");
    ASSERT_STREQ(headers.at("Status").c_str(), "200 OK", "Status mismatch.");
    ASSERT_STREQ(handler.getRawOutput().c_str(), "<html>Body</html>", "Body extraction failed.");
}

TEST_CASE(CgiParsing, MissingCRLF) {
    CgiHandlerTestable handler("bin", "script");

    std::string raw_output =
        "Location: /new_uri\nContent-Length: 100\n\nPure Body";

    handler.setRawOutput(raw_output);
    handler.runParseOutput();

    std::map<std::string, std::string> headers = handler.getHeaders();

    ASSERT_EQ(headers.size(), 2, "Should handle \\n\\n separator.");
    ASSERT_STREQ(headers.at("Location").c_str(), "/new_uri", "Location mismatch.");
    ASSERT_STREQ(handler.getRawOutput().c_str(), "Pure Body", "Body extraction failed.");
}


// ----------------------------------------------------
// CGI Execution Tests
// ----------------------------------------------------

#define PYTHON_PATH "/usr/bin/python3"
#define TEST_SCRIPT_PATH "./test/temp_scripts/"

TEST_CASE(CgiExecution, SimpleGetEnvironment) {
    CgiHandler handler(PYTHON_PATH, TEST_SCRIPT_PATH "echo_cgi.py");

    handler.setEnvironmentVariable("REQUEST_METHOD", "GET");
    handler.setEnvironmentVariable("QUERY_STRING", "id=123");

    handler.execute();

    ASSERT_STREQ(
        handler.getOutput().c_str(),
        "Query: id=123",
        "CGI Environment Echo failed."
    );
}

TEST_CASE(CgiExecution, CheckTimeout) {
    CgiHandler handler(PYTHON_PATH, TEST_SCRIPT_PATH "sleep_cgi.py");
    handler.setTimeout(1);

    ASSERT_THROW(handler.execute(), CgiHandler::Timeout);
}


// ----------------------------------------------------
// Test Runner
// ----------------------------------------------------

void run_all_cgi_tests() {
    std::cout << "\n" << YELLOW << "===========================================" << RESET << std::endl;
    std::cout << YELLOW << " Running CgiHandler Tests" << RESET << std::endl;
    std::cout << YELLOW << "===========================================" << RESET << std::endl;

    RUN_TEST(CgiParsing, BasicHeaders);
    RUN_TEST(CgiParsing, MissingCRLF);
    RUN_TEST(CgiExecution, SimpleGetEnvironment);
    RUN_TEST(CgiExecution, CheckTimeout);
}
