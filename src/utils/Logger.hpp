#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <iostream>
#include <ctime>
#include <sstream>

#define RESET	"\033[0m"
#define GREEN	"\033[32m"
#define YELLOW	"\033[33m"
#define RED		"\033[31m"
#define BLUE	"\033[34m"

namespace wsv
{

class Logger
{
public:
	static void info(const std::string& message);
	static void warning(const std::string& message);
	static void error(const std::string& message);
	static void debug(const std::string& message);

	template <typename T1>
	static void info(const std::string &format, T1 a1);

	template <typename T1, typename T2>
	static void info(const std::string &format, T1 a1, T2 a2);

	template <typename T1, typename T2, typename T3>
	static void info(const std::string &format, T1 a1, T2 a2, T3 a3);

	template <typename T1>
	static void error(const std::string &format, T1 a1);

	template <typename T1, typename T2>
	static void error(const std::string &format, T1 a1, T2 a2);

	template <typename T1>
	static void debug(const std::string &format, T1 a1);

	template <typename T1, typename T2>
	static void debug(const std::string &format, T1 a1, T2 a2);

private:
	static std::string _getTimestamp();
	static std::string _formatMessage(const std::string &format, const std::string args[], size_t count);
};


template <typename T1>
void Logger::info(const std::string &format, T1 a1)
{
	std::ostringstream oss;
	oss << a1;
	std::string args[] = {oss.str()};
	info(_formatMessage(format, args, 1));
}

template <typename T1, typename T2>
void Logger::info(const std::string &format, T1 a1, T2 a2)
{
	std::ostringstream oss1, oss2;
	oss1 << a1;
	oss2 << a2;
	std::string args[] = {oss1.str(), oss2.str()};
	info(_formatMessage(format, args, 2));
}

template <typename T1, typename T2, typename T3>
void Logger::info(const std::string &format, T1 a1, T2 a2, T3 a3)
{
	std::ostringstream oss1, oss2, oss3;
	oss1 << a1;
	oss2 << a2;
	oss3 << a3;
	std::string args[] = {oss1.str(), oss2.str(), oss3.str()};
	info(formatMessage(format, args, 3));
}

template <typename T1>
void Logger::error(const std::string &format, T1 a1)
{
	std::ostringstream oss;
	oss << a1;
	std::string args[] = {oss.str()};
	error(_formatMessage(format, args, 1));
}

template <typename T1, typename T2>
void Logger::error(const std::string &format, T1 a1, T2 a2)
{
	std::ostringstream oss1, oss2;
	oss1 << a1;
	oss2 << a2;
	std::string args[] = {oss1.str(), oss2.str()};
	error(_formatMessage(format, args, 2));
}

template <typename T1>
void Logger::debug(const std::string &format, T1 a1)
{
	std::ostringstream oss;
	oss << a1;
	std::string args[] = {oss.str()};
	debug(_formatMessage(format, args, 1));
}

template <typename T1, typename T2>
void Logger::debug(const std::string &format, T1 a1, T2 a2)
{
	std::ostringstream oss1, oss2;
	oss1 << a1;
	oss2 << a2;
	std::string args[] = {oss1.str(), oss2.str()};
	debug(_formatMessage(format, args, 2));
}


} // namespace wsv

#endif
