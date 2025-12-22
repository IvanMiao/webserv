#include "ConfigParser.hpp"
#include "TestUtils.hpp"
#include <iostream>

using namespace wsv;

// ----------------------------------------------------
// ConfigParser Test Suite
// ----------------------------------------------------
void run_config_parser_tests(const std::string& config_file)
{
    tests_total++;

    try
    {
        wsv::ConfigParser parser(config_file);
        parser.parse();
        
        // Print configuration (假设这会打印所有 LocationConfig 详情，包括 redirect_code)
        parser.printConfig();
        
        // --- 核心验证区域 ---
        
        const std::vector<wsv::ServerConfig>& servers = parser.getServers();
        if (servers.empty()) {
            throw std::runtime_error("No servers parsed.");
        }
        
        const wsv::ServerConfig& server = servers[0];

        // ------------------------------------------------------------------
        // NEW TEST: Explicitly Verify Redirect Code for /old-page
        // ------------------------------------------------------------------
        const wsv::LocationConfig* redirect_loc = server.findLocation("/old-page");
        
        bool redirect_test_passed = false;
        
        std::cout << "\n=== Verification of Redirect Data ===" << std::endl;
        
        if (redirect_loc == NULL)
        {
            std::cout << RED << "FAILED: findLocation('/old-page') returned NULL." << RESET << std::endl;
        }
        else if (redirect_loc->redirect_code != 301)
        {
            std::cout << RED << "FAILED: Redirect code for /old-page is " << redirect_loc->redirect_code 
                      << ", expected 301." << RESET << std::endl;
            std::cout << "Location Path: " << redirect_loc->path << std::endl;
        }
        else
        {
            std::cout << GREEN << "PASSED: Redirect code for /old-page is correctly 301." << RESET << std::endl;
            std::cout << "Redirect URL: " << redirect_loc->redirect_url << std::endl;
            redirect_test_passed = true;
        }
        
        // ------------------------------------------------------------------
        // NEW TEST 2: Explicitly Verify Redirect Code for /old-path
        // ------------------------------------------------------------------
        const wsv::LocationConfig* redirect_loc2 = server.findLocation("/old-path");
        
        bool redirect_test2_passed = false;
        
        std::cout << "\n=== Verification of /old-path Redirect ===" << std::endl;
        
        if (redirect_loc2 == NULL)
        {
            std::cout << RED << "FAILED: findLocation('/old-path') returned NULL." << RESET << std::endl;
        }
        else if (redirect_loc2->redirect_code != 301)
        {
            std::cout << RED << "FAILED: Redirect code for /old-path is " << redirect_loc2->redirect_code 
                      << ", expected 301." << RESET << std::endl;
            std::cout << "Location Path: " << redirect_loc2->path << std::endl;
        }
        else
        {
            std::cout << GREEN << "PASSED: Redirect code for /old-path is correctly 301." << RESET << std::endl;
            std::cout << "Redirect URL: " << redirect_loc2->redirect_url << std::endl;
            redirect_test2_passed = true;
        }
        
        // ------------------------------------------------------------------
        // Existing Location Matching Test
        // ------------------------------------------------------------------
        std::cout << "\n=== Testing Location Matching ===" << std::endl;
        
        std::string test_paths[] = {
            "/",
            "/index.html",
            "/uploads",
            "/uploads/file.txt",
            "/old-page",
            "/old-path",
            "/cgi-bin/script.py",
            "/nonexistent"
        };
        for (size_t i = 0; i < 8; i++)
        for (size_t i = 0; i < 7; i++)
        {
            const std::string& path = test_paths[i];
            const wsv::LocationConfig* loc = server.findLocation(path);
            
            std::cout << "Path: " << path << " -> ";
            if (loc)
                std::cout << "Location: " << loc->path << std::endl;
            else
                std::cout << "No matching location" << std::endl;
        }

        std::cout << "\nConfig parsing successful!" << std::endl;
        
        // Only increment tests_passed if both parsing and our explicit tests passed
        if (redirect_test_passed && redirect_test2_passed) {
             tests_passed++;
        } else {
             tests_failed++;
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << RED << "ConfigParser Test Failed: " << e.what() << RESET << std::endl;
        tests_failed++;
    }
}
