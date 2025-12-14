#include "TestUtils.hpp"
#include "../src/cgi/CgiHandler.hpp" // 包含您项目中的 CgiHandler.hpp

// ----------------------------------------------------
// CGI 逻辑测试：Header 解析 (不需要执行 fork/execve)
// ----------------------------------------------------

// 为了测试私有方法 parseOutput，我们需要一个特殊的 CgiHandler 子类
// 这种方法通常用于测试私有成员，或者修改您的 CgiHandler 接口以支持测试
class CgiHandlerTestable : public CgiHandler {
public:
    CgiHandlerTestable(const std::string& cgi_bin, const std::string& script_path) 
        : CgiHandler(cgi_bin, script_path) {}
    
    // 暴露私有的 parseOutput (现在可以访问，因为是友元)
    void runParseOutput() { parseOutput(); } 
    
    // 直接访问并设置私有成员 _output
    void setRawOutput(const std::string& raw) { _output = raw; }
    
    // 允许测试访问私有成员 _output 和 _response_hd
    std::string getRawOutput() const { return _output; }
    std::map<std::string, std::string> getHeaders() const { return _response_hd; }
};


// 定义测试套件 CgiParsing，测试名称 BasicHeaders
TEST_CASE(CgiParsing, BasicHeaders) {
    CgiHandlerTestable handler("bin", "script");
    
    std::string raw_output = "Content-Type: text/html\r\nStatus: 200 OK\r\n\r\n<html>Body</html>";
    handler.setRawOutput(raw_output);
    handler.runParseOutput(); // ** 【修复点 3：调用新名称的方法】 **

    std::map<std::string, std::string> headers = handler.getHeaders();
    
    ASSERT_EQ(headers.size(), 2, "Should have exactly 2 headers parsed.");
    ASSERT_STREQ(headers.at("Content-Type").c_str(), "text/html", "Content-Type mismatch.");
    ASSERT_STREQ(headers.at("Status").c_str(), "200 OK", "Status mismatch.");
    ASSERT_STREQ(handler.getRawOutput().c_str(), "<html>Body</html>", "Body extraction failed.");
}


// 定义测试套件 CgiParsing，测试名称 MissingCRLF
TEST_CASE(CgiParsing, MissingCRLF) {
    CgiHandlerTestable handler("bin", "script");
    
    // 模拟 CGI 脚本的标准输出，只包含 \n\n 分隔符
    std::string raw_output = "Location: /new_uri\nContent-Length: 100\n\nPure Body";
    handler.setRawOutput(raw_output);
    handler.runParseOutput();

    std::map<std::string, std::string> headers = handler.getHeaders();
    
    ASSERT_EQ(headers.size(), 2, "Should handle \\n\\n separator.");
    ASSERT_STREQ(headers.at("Location").c_str(), "/new_uri", "Location mismatch.");
    ASSERT_STREQ(handler.getRawOutput().c_str(), "Pure Body", "Body extraction failed.");
}


#define PYTHON_PATH "/usr/bin/python3" 
#define TEST_SCRIPT_PATH "./test/temp_scripts/"
// ----------------------------------------------------
// CGI 执行测试：需要运行真实的 CGI 脚本
// ----------------------------------------------------

// 您需要一个简单的可执行脚本 (例如一个 Python 文件) 来进行测试，
// 假设该脚本位于测试环境的 /tmp/echo_cgi.py
// 并且 cgi_path 是 /usr/bin/python3

TEST_CASE(CgiExecution, SimpleGetEnvironment) {
    CgiHandler handler(PYTHON_PATH, TEST_SCRIPT_PATH "echo_cgi.py");
    handler.setEnv("REQUEST_METHOD", "GET");
    handler.setEnv("QUERY_STRING", "id=123");
    
    handler.execute();
    
    // 假设 /tmp/echo_cgi.py 脚本会回显环境变量
    ASSERT_STREQ(handler.getOutput().c_str(), "Query: id=123", "CGI Environment Echo failed.");
}

TEST_CASE(CgiExecution, CheckTimeout) {
    // 假设 /tmp/sleep_cgi.py 脚本会 sleep 10 秒
    CgiHandler handler(PYTHON_PATH, TEST_SCRIPT_PATH "sleep_cgi.py");
    handler.setTimeout(1);
    
    // 期望抛出 Timeout 异常（CgiHandler.hpp 中定义的）
    ASSERT_THROW(handler.execute(), Timeout);
}

// ----------------------------------------------------
// 运行所有 CgiHandler 测试
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