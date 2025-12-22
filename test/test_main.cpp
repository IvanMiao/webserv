#include "TestUtils.hpp"
#include <iostream>

// 声明全局计数器
int tests_total = 0;
int tests_passed = 0;
int tests_failed = 0;

// 外部声明所有测试运行函数
void run_all_cgi_tests();
// void run_all_request_handler_tests(); // 如果有 RequestHandlerTest.cpp
void run_config_parser_tests(const std::string& config_file);
// ----------------------------------------------------
// 主测试函数
// ----------------------------------------------------

int main() {
    std::cout << GREEN << "--- WebServ Testing Framework (Standard C++) ---" << RESET << std::endl;

    // 调用所有测试套件的运行函数
    run_all_cgi_tests();
    run_config_parser_tests("test/test.conf"); // Config parser tests
    // run_all_request_handler_tests(); 

    // ----------------------------------------------------
    // 最终报告
    // ----------------------------------------------------
    std::cout << "\n" << YELLOW << "===========================================" << RESET << std::endl;
    std::cout << YELLOW << " Test Summary" << RESET << std::endl;
    std::cout << YELLOW << "===========================================" << RESET << std::endl;

    std::cout << "Total Tests Run: " << tests_total << std::endl;
    std::cout << GREEN << "Passed: " << tests_passed << RESET << std::endl;
    std::cout << RED << "Failed: " << tests_failed << RESET << std::endl;

    if (tests_failed > 0) {
        std::cout << RED << "\n❌ BUILD FAILED: Some tests did not pass." << RESET << std::endl;
        return 1;
    } else {
        std::cout << GREEN << "\n✅ SUCCESS: All tests passed!" << RESET << std::endl;
        return 0;
    }
}