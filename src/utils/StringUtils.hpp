#ifndef STRING_UTILS_HPP
#define STRING_UTILS_HPP

#include <string>
#include <vector>

namespace StringUtils
{

std::string	trim(const std::string& str);
size_t		parseSize(const std::string& str);
bool		startsWith(const std::string& str, const std::string& prefix);
std::string	removeSemicolon(const std::string& str);
std::vector<std::string>	split(const std::string& str, const std::string& delimiters);
std::string	toString(int value);
std::string	toLower(const std::string& str);

} // namespace StringUtils

#endif
