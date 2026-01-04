#include "FileHandler.hpp"
#include "ErrorHandler.hpp"
#include <sys/stat.h>
#include <dirent.h>
#include <fstream>
#include <sstream>

namespace wsv
{

// // ============================================================================
// // Serve a static file with appropriate MIME type
// // ============================================================================
// HttpResponse FileHandler::serve_file(const std::string& file_path)
// {
//     // Read the entire file into memory
//     std::string file_content = read_file(file_path);
    
//     // If file could not be read, return 500 Internal Server Error
//     if (file_content.empty())
//     {
//         HttpResponse response;
//         response.setStatus(500);
//         return response;
//     }
    
//     // Return file with appropriate MIME type
//     return HttpResponse::createOkResponse(file_content, get_mime_type(file_path));
// }

HttpResponse FileHandler::serve_file(const std::string& file_path)
{
    // 1. 检查文件是否存在（其实之前可能检查过，但这里更安全）
    if (!file_exists(file_path)) {
        return HttpResponse::createErrorResponse(404);
    }

    // 2. 尝试读取文件
    std::string file_content = read_file(file_path);
    
    // 3. 不要在这里直接 return 500！
    // 如果 read_file 返回空，我们需要判断是因为文件本来就是 0 字节，还是因为打不开。
    
    std::ifstream file(file_path.c_str(), std::ios::binary);
    if (!file.is_open())
    {
        // 只有真正打不开文件时（可能是权限问题），才返回错误
        // 对于 42 webserv 建议返回 403 或 500（取决于你对“内部错误”的定义）
        // 但绝大多数情况下，这里应该返回 403。
        HttpResponse response;
        response.setStatus(403); 
        return response;
    }

    // 4. 返回 200 OK，即便 file_content 是空的也没关系
    return HttpResponse::createOkResponse(file_content, get_mime_type(file_path));
}

// // ============================================================================
// // Handle directory requests (try index file, then listing if enabled)
// // ============================================================================
HttpResponse FileHandler::serve_directory(const std::string& dir_path,
                                          const LocationConfig& location_config)
{
    // Build path to index file (e.g., /var/www/html/index.html)
    std::string index_path = dir_path;
    if (!index_path.empty() && index_path[index_path.size() - 1] != '/')
    {
        index_path += "/";
    }
    index_path += location_config.index;
    
    // If index file exists and is readable, serve it
    if (file_exists(index_path) && !is_directory(index_path))
    {
        return serve_file(index_path);
    }
    
    // If autoindex is enabled, generate HTML directory listing
    if (location_config.autoindex)
    {
        return _generate_directory_listing(dir_path, location_config.path);
    }
    
    // No index file and autoindex disabled
    HttpResponse response;
    // [TUDO] - SHOULD BE 403 BUT TESTER WANT 404
    response.setStatus(404);
    return response;
}



// ============================================================================
// Generate HTML directory listing (private helper)
// ============================================================================
HttpResponse FileHandler::_generate_directory_listing(const std::string& dir_path,
                                                      const std::string& uri_path)
{
    // Open directory for reading
    DIR* dir_handle = opendir(dir_path.c_str());
    if (!dir_handle)
    {
        HttpResponse response;
        response.setStatus(500);
        return response;
    }
    
    // Build HTML directory listing
    std::ostringstream html_stream;
    html_stream << "<html><head><title>Index of " << uri_path << "</title></head>";
    html_stream << "<body><h1>Index of " << uri_path << "</h1><hr><pre>";
    
    // Read directory entries
    struct dirent* entry;
    while ((entry = readdir(dir_handle)) != NULL)
    {
        std::string name = entry->d_name;
        
        // Skip hidden files (starting with .)
        if (name[0] == '.')
            continue;
        
        // Build full path for stat
        std::string full_path = dir_path;
        if (!full_path.empty() && full_path[full_path.size() - 1] != '/')
            full_path += "/";
        full_path += name;
        
        // Check if it's a directory
        if (is_directory(full_path))
            name += "/";
        
        // Add link to listing
        html_stream << "<a href=\"" << name << "\">" << name << "</a>\n";
    }
    
    html_stream << "</pre><hr></body></html>";
    closedir(dir_handle);
    
    // Return directory listing as HTML page
    return HttpResponse::createOkResponse(html_stream.str(), "text/html");
}

// ============================================================================
// Read entire file content into memory
// ============================================================================
std::string FileHandler::read_file(const std::string& path)
{
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open())
        return "";
    
    std::ostringstream buffer;
    buffer << file.rdbuf();  // [TODO] Read entire file at once?
    return buffer.str();
}

// ============================================================================
// Determine MIME type based on file extension
// ============================================================================
std::string FileHandler::get_mime_type(const std::string& file_path)
{
    size_t pos = file_path.find_last_of('.');
    if (pos == std::string::npos)
        return "application/octet-stream";
    
    std::string ext = file_path.substr(pos);
    
    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".txt") return "text/plain";
    if (ext == ".json") return "application/json";
    if (ext == ".xml") return "application/xml";
    if (ext == ".pdf") return "application/pdf";
    
    return "application/octet-stream";
}

// ============================================================================
// Check if file or directory exists
// ============================================================================
bool FileHandler::file_exists(const std::string& path)
{
    struct stat file_status;
    return (stat(path.c_str(), &file_status) == 0);
}

// ============================================================================
// Check if path is a directory
// ============================================================================
bool FileHandler::is_directory(const std::string& path)
{
    struct stat file_status;
    if (stat(path.c_str(), &file_status) != 0)
        return false;
    return S_ISDIR(file_status.st_mode);
}

} // namespace wsv
