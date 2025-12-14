#ifndef REQUEST_HANDLER_HPP
#define REQUEST_HANDLER_HPP

#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ConfigParser.hpp"
#include "../cgi/CgiHandler.hpp" // NEW: Dependency on CgiHandler

#include <string>
#include <map>

class RequestHandler
{
public:
    RequestHandler(const ServerConfig& config);
    ~RequestHandler();

    // 主处理函数
    HttpResponse handleRequest(const HttpRequest& request);

private:
    const ServerConfig& _config;

    // ========== HTTP 方法处理 ==========

    HttpResponse _handleGet(const HttpRequest& request,
                            const LocationConfig& location);
    HttpResponse _handlePost(const HttpRequest& request,
                             const LocationConfig& location);
    HttpResponse _handleDelete(const HttpRequest& request,
                               const LocationConfig& location);

    // ========== 文件操作 ==========

    HttpResponse _serveFile(const std::string& file_path);
    HttpResponse _serveDirectory(const std::string& dir_path,
                                 const LocationConfig& location);
    HttpResponse _generateDirectoryListing(const std::string& dir_path,
                                           const std::string& uri_path);

    // ========== 文件上传 ==========

    HttpResponse _handleUpload(const HttpRequest& request,
                               const LocationConfig& location);

    // ========== CGI ==========

    HttpResponse _executeCgi(const HttpRequest& request,
                             const std::string& script_path,
                             const LocationConfig& location);
    
    // NEW: Helper function to build the environment map for CgiHandler
    std::map<std::string, std::string> _buildCGIEnvironment(
        const HttpRequest& request,
        const LocationConfig& location_config) const;


    // ========== 工具函数 ==========

    std::string _getMimeType(const std::string& file_path);
    std::string _buildFilePath(const std::string& uri_path,
                               const LocationConfig& location);

    bool _fileExists(const std::string& path);
    bool _isDirectory(const std::string& path);
    std::string _readFile(const std::string& path);

    // ========== 错误处理 ==========

    HttpResponse _getErrorPage(int code);
};

#endif // REQUEST_HANDLER_HPP