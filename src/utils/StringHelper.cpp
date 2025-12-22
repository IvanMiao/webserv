#include "StringHelper.hpp"
#include <sstream>
#include <algorithm>
#include <cctype>

namespace wsv {

// ============================================================================
// Convert integer to string (used for HTTP status codes and timestamps)
// ============================================================================
std::string StringHelper::toString(int value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// ============================================================================
// Convert time_t to string (used for generating unique filenames)
// ============================================================================
std::string StringHelper::toString(std::time_t value) {
    std::ostringstream oss;
    oss << value;
    return oss.str();
}

// ============================================================================
// URL decode function (converts %20 to space, %2F to /, etc.)
// ============================================================================
std::string StringHelper::urlDecode(const std::string& str) {
    std::string result;
    result.reserve(str.size());
    
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '%' && i + 2 < str.size()) {
            // Convert hex digits to char
            int hex_value;
            std::istringstream hex_stream(str.substr(i + 1, 2));
            if (hex_stream >> std::hex >> hex_value) {
                result += static_cast<char>(hex_value);
                i += 2;  // Skip the two hex digits
            } else {
                result += str[i];  // Invalid hex, keep the %
            }
        } else if (str[i] == '+') {
            result += ' ';  // + is often used for space in URLs
        } else {
            result += str[i];
        }
    }
    
    return result;
}

// ============================================================================
// Check if path contains directory traversal attempts
// Security check to prevent accessing files outside allowed directories
// ============================================================================
bool StringHelper::hasPathTraversal(const std::string& path) {
    // Check for .. in path (parent directory reference)
    if (path.find("..") != std::string::npos)
        return true;
    
    // Check for absolute paths on Unix (starting with /)
    if (!path.empty() && path[0] == '/')
        return true;
    
    // Check for Windows absolute paths (C:, D:, etc.)
    if (path.size() >= 2 && path[1] == ':')
        return true;
    
    return false;
}

// ============================================================================
// Trim whitespace from both ends of string
// ============================================================================
std::string StringHelper::trim(const std::string& str) {
    // Find first non-whitespace character
    size_t start = 0;
    while (start < str.size() && std::isspace(static_cast<unsigned char>(str[start]))) {
        ++start;
    }
    
    // Find last non-whitespace character
    size_t end = str.size();
    while (end > start && std::isspace(static_cast<unsigned char>(str[end - 1]))) {
        --end;
    }
    
    return str.substr(start, end - start);
}

// ============================================================================
// Convert string to lowercase
// ============================================================================
std::string StringHelper::toLower(const std::string& str) {
    std::string result = str;
    for (std::string::iterator it = result.begin(); it != result.end(); ++it) {
        *it = std::tolower(static_cast<unsigned char>(*it));
    }
    return result;
}

} // namespace wsv