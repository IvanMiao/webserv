#ifndef TESTRUNNER_HPP
#define TESTRUNNER_HPP

#include <iostream>
#include <string>

#define GREEN "\033[32m"
#define RED "\033[31m"
#define YELLOW "\033[33m"
#define RESET "\033[0m"
#define BOLD "\033[1m"

class TestRunner
{
private:
	int _passed;
	int _failed;
	std::string _current_test;

public:
	TestRunner() : _passed(0), _failed(0) {}

	void startTest(const std::string& name)
	{
		_current_test = name;
		std::cout << YELLOW << "[TEST] " << RESET << name << " ... ";
		std::cout.flush();
	}

	void pass()
	{
		std::cout << GREEN << "PASS" << RESET << std::endl;
		_passed++;
	}

	void fail(const std::string& msg)
	{
		std::cout << RED << "FAIL" << RESET << std::endl;
		std::cout << "  Error: " << msg << std::endl;
		_failed++;
	}

	void summary()
	{
		std::cout << std::endl << BOLD << "=== Test Summary ===" << RESET << std::endl;
		std::cout << "Total: " << (_passed + _failed) << " tests" << std::endl;
		std::cout << GREEN << "Passed: " << _passed << RESET << std::endl;
		if (_failed > 0)
			std::cout << RED << "Failed: " << _failed << RESET << std::endl;
		else
			std::cout << "Failed: " << _failed << std::endl;
	}

	bool allPassed() const { return _failed == 0; }
};

#endif
