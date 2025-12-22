#ifndef TEST_UTILS_HPP
#define TEST_UTILS_HPP

#include <iostream>
#include <string>
#include <map>
#include <sstream>


// ANSI 颜色代码
#define RESET   "\033[0m"
#define RED     "\033[31m"
#define GREEN   "\033[32m"
#define YELLOW  "\033[33m"
#define BLUE    "\033[34m"

// 宏定义：用于声明和运行测试函数
#define TEST_CASE(suite, name) void test_##suite##_##name() 

// 宏定义：用于运行测试并捕获异常
#define RUN_TEST(suite, name) \
    { \
        std::cout << BLUE << "  [RUN] " << #suite << "." << #name << RESET << std::flush; \
        try { \
            test_##suite##_##name(); \
            std::cout << " " << GREEN << "[PASS]" << RESET << std::endl; \
            tests_passed++; \
        } catch (const std::exception& e) { \
            std::cout << " " << RED << "[FAIL]" << RESET << std::endl; \
            std::cerr << "      " << RED << "Exception: " << e.what() << RESET << std::endl; \
            tests_failed++; \
        } catch (...) { \
            std::cout << " " << RED << "[FAIL]" << RESET << std::endl; \
            std::cerr << "      " << RED << "Unknown error caught." << RESET << std::endl; \
            tests_failed++; \
        } \
        tests_total++; \
    }

// 简单的断言宏
#define ASSERT_EQ(expected, actual, message) \
    if (expected != actual) { \
        std::ostringstream oss; \
        oss << "Assertion Failed (" << __FILE__ << ":" << __LINE__ << "): " << message \
            << "\n        Expected: [" << expected << "]" \
            << "\n        Actual:   [" << actual << "]"; \
        throw std::runtime_error(oss.str()); \
    }

// 专门用于字符串比较的宏，避免 char* 和 string 的隐式转换问题
#define ASSERT_STREQ(expected, actual, message) \
    if (std::string(expected) != std::string(actual)) { \
        std::ostringstream oss; \
        oss << "Assertion Failed (" << __FILE__ << ":" << __LINE__ << "): " << message \
            << "\n        Expected: [" << expected << "]" \
            << "\n        Actual:   [" << actual << "]"; \
        throw std::runtime_error(oss.str()); \
    }

// 用于检查是否抛出特定异常的宏
#define ASSERT_THROW(statement, expected_exception) \
    { \
        bool thrown = false; \
        try { \
            statement; \
        } catch (const expected_exception& e) { \
            thrown = true; \
        } catch (...) {} \
        if (!thrown) { \
            std::ostringstream oss; \
            oss << "Assertion Failed (" << __FILE__ << ":" << __LINE__ << "): Expected exception " \
                << #expected_exception << " was not thrown."; \
            throw std::runtime_error(oss.str()); \
        } \
    }


extern int tests_total;
extern int tests_passed;
extern int tests_failed;

// Test function declarations
void run_all_cgi_tests();
void run_config_parser_tests(const std::string& config_file);

#endif // TEST_UTILS_HPP