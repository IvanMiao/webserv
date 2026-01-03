#include "cgi/CgiHandler.hpp"
#include "TestRunner.hpp"
#include <iostream>
#include <vector>
#include <cstring>
#include <sys/stat.h>

namespace wsv
{
// Friend class to access private members of CgiHandler
class CgiHandlerTestable {
public:
    CgiHandler& _handler;

    CgiHandlerTestable(CgiHandler& handler) : _handler(handler) {}

    // Accessors for private members
    std::string getCgiBin() const { return _handler._cgi_bin; }
    std::string getScriptPath() const { return _handler._script_path; }
    CgiHandler::HeaderMap getEnvironment() const { return _handler._environment; }
    
    // Check pipe state
    bool arePipesClosed() const {
        return _handler._pipes.input_pipe[0] == -1 && 
               _handler._pipes.input_pipe[1] == -1 &&
               _handler._pipes.output_pipe[0] == -1 &&
               _handler._pipes.output_pipe[1] == -1;
    }

    // Verify environment builder logic
    void testEnvironmentBuilder() {
        CgiHandler::_EnvironmentBuilder builder;
        builder._build(_handler._environment);
        
        char** env = builder._getEnvironmentArray();
        if (!env) throw std::runtime_error("Environment array is NULL");
        
        // Verify key variables exist in the array
        bool found_method = false;
        for (int i = 0; env[i] != NULL; ++i) {
            std::string line(env[i]);
            if (line.find("REQUEST_METHOD=") == 0) found_method = true;
        }
        
        if (!found_method && !_handler._environment.empty()) 
            throw std::runtime_error("REQUEST_METHOD not found in built environment");
    }
};

void test_constructors(TestRunner& runner) {
    runner.startTest("CgiHandler Constructors");
    try {
        CgiHandler h1("/usr/bin/python3", "/var/www/script.py");
        CgiHandlerTestable t1(h1);
        
        if (t1.getCgiBin() != "/usr/bin/python3") runner.fail("CGI Bin mismatch");
        if (t1.getScriptPath() != "/var/www/script.py") runner.fail("Script path mismatch");
        
        runner.pass();
    } catch (const std::exception& e) {
        runner.fail(e.what());
    }
}

void test_environment_vars(TestRunner& runner) {
    runner.startTest("CgiHandler Environment Variables");
    try {
        CgiHandler h;
        h.setEnvironmentVariable("REQUEST_METHOD", "POST");
        h.setEnvironmentVariable("CONTENT_LENGTH", "42");
        
        CgiHandlerTestable t(h);
        CgiHandler::HeaderMap env = t.getEnvironment();
        
        if (env["REQUEST_METHOD"] != "POST") runner.fail("REQUEST_METHOD mismatch");
        if (env["CONTENT_LENGTH"] != "42") runner.fail("CONTENT_LENGTH mismatch");
        
        // Test builder logic
        t.testEnvironmentBuilder();
        
        runner.pass();
    } catch (const std::exception& e) {
        runner.fail(e.what());
    }
}

void test_pipe_management(TestRunner& runner) {
    runner.startTest("CgiHandler Pipe Management");
    try {
        CgiHandler h("/bin/echo", "test");
        CgiHandlerTestable t(h);
        
        // Initially pipes should be closed/invalid
        if (!t.arePipesClosed()) runner.fail("Pipes should be closed initially");
        
        // for pipe logic check.
        // We'll catch fork errors if system is overloaded
        try {
            pid_t pid = h.start();
            if (pid <= 0) runner.fail("Invalid PID returned");
            
            // In parent, verify we have valid FDs
            if (h.getStdinWriteFd() < 0) runner.fail("Stdin Write FD invalid");
            if (h.getStdoutReadFd() < 0) runner.fail("Stdout Read FD invalid");
            
            // Clean up
            h.closePipes();
        } catch (const std::exception& e) {
            runner.fail(std::string("Start failed: ") + e.what());
        }

        runner.pass();
    } catch (const std::exception& e) {
        runner.fail(e.what());
    }
}

void test_empty_input(TestRunner& runner) {
    runner.startTest("CgiHandler Empty Input");
    try {
        CgiHandler h("/bin/echo", "test");
        h.setInput("");
        pid_t pid = h.start();
        if (pid <= 0) runner.fail("Failed to start with empty input");
        h.closePipes();
        runner.pass();
    } catch (const std::exception& e) {
        runner.fail(e.what());
    }
}

void test_timeout_config(TestRunner& runner) {
    runner.startTest("CgiHandler Timeout Configuration");
    try {
        CgiHandler h("/bin/echo", "test");
        h.setTimeout(5);
        CgiHandler h2;
        h2.setTimeout(10);
        runner.pass();
    } catch (const std::exception& e) {
        runner.fail(e.what());
    }
}

void test_large_input(TestRunner& runner) {
    runner.startTest("CgiHandler Large Input (5MB)");
    try {
        CgiHandler h("/bin/cat", "test");
        // 5MB input - within limit, reflects real POST body uploads
        std::string large_input(5 * 1024 * 1024, 'A');
        h.setInput(large_input);
        pid_t pid = h.start();
        if (pid <= 0) runner.fail("Failed to start with large input");
        h.closePipes();
        runner.pass();
    } catch (const std::exception& e) {
        runner.fail(e.what());
    }
}

} // namespace wsv

int main() {
    std::cout << BOLD << "========================================" << RESET << std::endl;
    std::cout << BOLD << "  CgiHandler Unit Tests" << RESET << std::endl;
    std::cout << BOLD << "========================================" << RESET << std::endl << std::endl;

    TestRunner runner;

    wsv::test_constructors(runner);
    wsv::test_environment_vars(runner);
    wsv::test_pipe_management(runner);
    wsv::test_empty_input(runner);
    wsv::test_timeout_config(runner);
    wsv::test_large_input(runner);

    runner.summary();
    return runner.allPassed() ? 0 : 1;
}
