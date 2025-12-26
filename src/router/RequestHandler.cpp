#include "RequestHandler.hpp"
#include "FileHandler.hpp"
#include "UploadHandler.hpp"
#include "CgiRequestHandler.hpp"
#include "ErrorHandler.hpp"
#include <algorithm>
#include <iostream>

namespace wsv
{

RequestHandler::RequestHandler(const ServerConfig& config)
    : _config(config)
{ }

RequestHandler::~RequestHandler()
{ }

// ============================================================================
// Public Method: handleRequest
// ============================================================================

/**
 * Main entry point to process HTTP request
 * Routes request to appropriate handler based on method and location config
 */
HttpResponse RequestHandler::handleRequest(const HttpRequest& request)
{
    Logger::debug("START handleRequest");
    std::string method = request.getMethod();
    Logger::debug("URI = {}, Method = {}", request.getPath(), method);
    
    // Check for path traversal attacks BEFORE other validations
    // This ensures 403 is returned for traversal attempts, not 405
    if (request.getPath().find("..") != std::string::npos)
    {
        Logger::debug("Path traversal detected, returning 403");
        return ErrorHandler::get_error_page(403, _config);
    }
    
    const LocationConfig* location_config = _config.findLocation(request.getPath());

    if (!location_config)
        return ErrorHandler::get_error_page(404, _config);

    if (!location_config->isMethodAllowed(method))
    {
        Logger::debug("Method not allowed: {}", method);
        return ErrorHandler::get_error_page(405, _config);
    }

    if (location_config->hasRedirect())
    {
        return HttpResponse::createRedirectResponse(
            location_config->redirect_code,
            location_config->redirect_url
        );
    }

    if (request.getContentLength() > _config.client_max_body_size)
        return ErrorHandler::get_error_page(413, _config);

    if (method == "GET" || method == "HEAD")
        return _handleGet(request, *location_config);
    else if (method == "POST")
    {
        Logger::debug("Routing to _handlePost");
        return _handlePost(request, *location_config);
    }
    else if (method == "DELETE")
        return _handleDelete(request, *location_config);

    return ErrorHandler::get_error_page(501, _config);
}

// ============================================================================
// Private Methods: HTTP Method Handlers
// ============================================================================

/**
 * Handle GET and HEAD requests
 * Checks file/directory, CGI, and serves static content
 */
HttpResponse RequestHandler::_handleGet(const HttpRequest& request,
                                        const LocationConfig& location_config)
{

    std::string file_path = _buildFilePath(request.getPath(), location_config);

    if (!FileHandler::file_exists(file_path))
        return ErrorHandler::get_error_page(404, _config);

    if (FileHandler::is_directory(file_path))
    {
        HttpResponse response = _serve_directory(file_path, location_config);
        if (request.getMethod() == "HEAD")
            response.setBody("");
        return response;
    }

    if (_isCgiRequest(file_path, location_config))
        return _execute_cgi(request, file_path, location_config);

    HttpResponse response = _serve_file(file_path);
    if (request.getMethod() == "HEAD")
        response.setBody("");

    return response;
}

/**
 * Handle POST requests
 * Supports file upload or CGI execution
 * Process: Check for upload -> check file exists -> handle CGI scripts -> or return 405
 */
HttpResponse RequestHandler::_handlePost(const HttpRequest& request,
                                         const LocationConfig& location_config)
{
    Logger::debug("Method = {}, Path = {}", request.getMethod(), request.getPath());
    Logger::debug("upload_enable = {}", (location_config.upload_enable ? "true" : "false"));
    
    // 上传开启 → 调用 UploadHandler
    if (location_config.upload_enable) {
        Logger::debug("Calling UploadHandler");
        return UploadHandler::handle_upload(request, location_config);
    }

    // 构建文件路径
    std::string file_path = _buildFilePath(request.getPath(), location_config);

    // CGI POST 请求
    if (_isCgiRequest(file_path, location_config))
    {
        // Check if CGI script file exists before attempting execution
        if (!FileHandler::file_exists(file_path))
        {
            return ErrorHandler::get_error_page(404, _config);  // Not Found
        }
        return _execute_cgi(request, file_path, location_config);
    }

    // 其他情况 POST 不允许
    return ErrorHandler::get_error_page(405, _config);
}

// HttpResponse RequestHandler::_handlePost(const HttpRequest& request,
//                                          const LocationConfig& location_config)
// {
//     if (location_config.upload_enable)
//         return UploadHandler::handle_upload(request, location_config);

//     std::string file_path = _buildFilePath(request.getPath(), location_config);

//     if (!FileHandler::file_exists(file_path))
//         return ErrorHandler::get_error_page(404, _config);

//     if (_isCgiRequest(file_path, location_config))
//         return _execute_cgi(request, file_path, location_config);

//     return ErrorHandler::get_error_page(405, _config);
// }


/**
 * Handle DELETE requests
 * Validates file existence, prevents directory deletion, and deletes file
 */
HttpResponse RequestHandler::_handleDelete(const HttpRequest& request,
                                           const LocationConfig& location_config)
{
    // Path traversal check is now done in handleRequest() before method validation
    std::string file_path = _buildFilePath(request.getPath(), location_config);

    Logger::debug("Full file path: {}", file_path);

    if (!FileHandler::file_exists(file_path)) {
        Logger::debug("File does not exist, returning 404");
        return ErrorHandler::get_error_page(404, _config);
    }

    if (FileHandler::is_directory(file_path)) {
        Logger::debug("Path is a directory, returning 403");
        return ErrorHandler::get_error_page(403, _config);
    }

    Logger::debug("Attempting to remove file: {}", file_path);
    int remove_result = std::remove(file_path.c_str());
    Logger::debug("Remove returned: {}", remove_result);
    
    if (remove_result == 0)
    {
        Logger::debug("File deleted successfully, returning 204");
        HttpResponse response;
        response.setStatus(204); // No Content
        return response;
    }

    Logger::debug("Remove failed, returning 500");
    return ErrorHandler::get_error_page(500, _config);
}

// ============================================================================
// Private Methods: Utilities
// ============================================================================

/**
 * Convert URI path to filesystem path
 * Handles root, location path stripping, and URL decoding
 */
std::string RequestHandler::_buildFilePath(const std::string& uri_path,
                                           const LocationConfig& location_config)
{
    std::string relative_path = uri_path;

    if (uri_path.find(location_config.path) == 0)
        relative_path = uri_path.substr(location_config.path.size());

    if (relative_path.empty() || relative_path[0] != '/')
        relative_path = "/" + relative_path;

    relative_path = StringUtils::urlDecode(relative_path);

    return location_config.root + relative_path;
}

/**
 * Check if file is a CGI script based on configured extension
 */
bool RequestHandler::_isCgiRequest(const std::string& file_path,
                                   const LocationConfig& location_config) const
{
    if (location_config.cgi_extension.empty())
        return false;

    size_t ext_pos = file_path.find_last_of('.');
    if (ext_pos == std::string::npos)
        return false;

    std::string ext = file_path.substr(ext_pos);
    return (ext == location_config.cgi_extension);
}

/**
 * Serve static file
 */
HttpResponse RequestHandler::_serve_file(const std::string& file_path)
{
    HttpResponse response = FileHandler::serve_file(file_path);
    if (response.getStatus() >= 400)
        return ErrorHandler::get_error_page(response.getStatus(), _config);
    return response;
}

/**
 * Serve directory (index file or autoindex)
 */
HttpResponse RequestHandler::_serve_directory(const std::string& dir_path,
                                              const LocationConfig& location_config)
{
    HttpResponse response = FileHandler::serve_directory(dir_path, location_config);
    if (response.getStatus() >= 400)
        return ErrorHandler::get_error_page(response.getStatus(), _config);
    return response;
}

/**
 * Execute CGI script
 */
HttpResponse RequestHandler::_execute_cgi(const HttpRequest& request,
                                          const std::string& file_path,
                                          const LocationConfig& location_config)
{
    return CgiRequestHandler::execute_cgi(request, file_path, location_config, _config);
}

} // namespace wsv

