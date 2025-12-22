#ifndef STRING_HELPER_HPP
#define STRING_HELPER_HPP

#include <string>
#include <ctime>

namespace wsv {

class StringHelper {
public:
    // Convert integer to string
    static std::string toString(int value);
    
    // Convert time_t to string
    static std::string toString(std::time_t value);
    
    // URL decode function (converts %20 to space, etc.)
    static std::string urlDecode(const std::string& str);
    
    // Check if path contains directory traversal attempts
    static bool hasPathTraversal(const std::string& path);
    
    // Trim whitespace from both ends of string
    static std::string trim(const std::string& str);
    
    // Convert string to lowercase
    static std::string toLower(const std::string& str);
};

} // namespace wsv

#endif // STRING_HELPER_HPP