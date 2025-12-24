#include "StringUtils.hpp"
#include <cctype>
#include <algorithm>
#include <sstream>

namespace StringUtils {

std::string trim(const std::string& str)
{
	size_t start = 0;
	size_t end = str.size();
	
	while (start < end && std::isspace(static_cast<unsigned char>(str[start])))
		start++;
	
	while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1])))
		end--;
	
	return str.substr(start, end - start);
}

std::vector<std::string> split(const std::string& str, const std::string& delimiters)
{
	std::vector<std::string> result;
	size_t start = 0;
	size_t end = 0;
	
	while (end < str.size())
	{
		while (start < str.size() && delimiters.find(str[start]) != std::string::npos)
			start++;
		end = start;
		while (end < str.size() && delimiters.find(str[end]) == std::string::npos)
			end++;
		if (start < end)
			result.push_back(str.substr(start, end - start));
		start = end;
	}
	
	return result;
}

size_t parseSize(const std::string& str)
{
	if (str.empty())
		return 0;
	
	size_t value = 0;
	size_t i = 0;
	
	while (i < str.size() && std::isdigit(static_cast<unsigned char>(str[i])))
	{
		value = value * 10 + (str[i] - '0');
		i++;
	}
	
	while (i < str.size() && std::isspace(static_cast<unsigned char>(str[i])))
		i++;
	
	if (i < str.size())
	{
		char unit = std::toupper(static_cast<unsigned char>(str[i]));
		switch (unit)
		{
			case 'K':
				return value * 1024;
			case 'M':
				return value * 1024 * 1024;
			case 'G':
				return value * 1024 * 1024 * 1024;
			default:
				return value;
		}
	}
	
	return value;
}

bool startsWith(const std::string& str, const std::string& prefix)
{
	if (str.size() < prefix.size())
		return false;
	
	return str.compare(0, prefix.size(), prefix) == 0;
}

std::string removeSemicolon(const std::string& str)
{
	std::string result = str;
	
	while (!result.empty() && result[result.size() - 1] == ';')
		result.erase(result.size() - 1);
	
	return trim(result);
}

std::string	toString(int value)
{
	std::stringstream ss;
	ss << value;
	return ss.str();
}

std::string	toLower(const std::string& str)
{
	std::string result = str;

	for (size_t i = 0; i < result.size(); ++i)
		result[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(result[i])));
	
	return result;
}

} // namespace StringUtils
