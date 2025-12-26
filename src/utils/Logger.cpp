#include "Logger.hpp"

namespace wsv
{

std::string Logger::getTimestamp()
{
	char buffer[80];
	std::time_t rawtime;
	std::tm* timeinfo;

	std::time(&rawtime);
	timeinfo = std::localtime(&rawtime);

	// Format: YYYY-MM-DD HH:MM:SS
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
	return std::string(buffer);
}

std::string Logger::formatMessage(const std::string& format, const std::string args[], size_t count)
{
	std::string result = format;
	for (size_t i = 0; i < count; ++i)
	{
		size_t pos = result.find("{}");
		if (pos != std::string::npos)
			result.replace(pos, 2, args[i]);
	}
	return result;
}

void Logger::info(const std::string& message)
{
	std::cout << "[" << getTimestamp() << "] "
			  << GREEN << "INFO" << RESET << ": "
			  << message << std::endl;
}

void Logger::warning(const std::string& message)
{
	std::cout << "[" << getTimestamp() << "] "
			  << YELLOW << "WARNING: " << RESET << ": "
			  << message << std::endl;
}

void Logger::error(const std::string& message)
{
	std::cerr << "[" << getTimestamp() << "] "
			  << RED << "ERROR" << RESET << ": "
			  << message << std::endl;
}

void Logger::debug(const std::string& message)
{
#ifdef DEBUG
	std::cout << "[" << getTimestamp() << "] "
			  << BLUE << "DEBUG" << RESET << ": "
			  << message << std::endl;
#else
	(void) message;
	return;
#endif
}

} // namespace wsv
