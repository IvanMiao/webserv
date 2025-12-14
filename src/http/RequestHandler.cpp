#include "RequestHandler.hpp"
#include "ConfigParser.hpp" 
#include "../cgi/CgiHandler.hpp" // NEW: Include CgiHandler
#include "HttpResponse.hpp" // Assuming this is needed for helper function access

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <iostream>
#include <stdexcept>

// ========================================
// Fictional Utility Functions (Need to be defined globally or in another utility file)
// ========================================

// Placeholder implementation for toString (int)
std::string toString(int code) {
    std::ostringstream oss;
    oss << code;
    return oss.str();
}

// Placeholder implementation for toString (time_t)
std::string toString(std::time_t t) {
    std::ostringstream oss;
    oss << t;
    return oss.str();
}

// Placeholder implementation for urlDecode
std::string urlDecode(const std::string& str) {
    // In a real project, implement URL decoding here
    return str;
}

// ========================================
// Constructor/Destructor
// ========================================

RequestHandler::RequestHandler(const ServerConfig& config)
    : _config(config) 
{
}

RequestHandler::~RequestHandler()
{
}

// ========================================
// Main Handler
// ========================================

HttpResponse RequestHandler::handleRequest(const HttpRequest& request)
{
    const LocationConfig* location_config = _config.findLocation(request.getPath()); // Local variable (snake_case)
    
    if (!location_config)
    {
        return _getErrorPage(404);
    }
    
    if (!location_config->isMethodAllowed(request.getMethod()))
    {
        return _getErrorPage(405);
    }
    
    if (location_config->hasRedirect())
    {
        return HttpResponse::createRedirectResponse(
            location_config->redirect_code,
            location_config->redirect_url
        );
    }
    
    if (request.getContentLength() > _config.client_max_body_size)
    {
        return _getErrorPage(413);
    }
    
    if (request.getMethod() == "GET" || request.getMethod() == "HEAD")
    {
        return _handleGet(request, *location_config);
    }
    else if (request.getMethod() == "POST")
    {
        return _handlePost(request, *location_config);
    }
    else if (request.getMethod() == "DELETE")
    {
        return _handleDelete(request, *location_config);
    }
    
    return _getErrorPage(501);
}

// ========================================
// Private HTTP Method Handlers
// ========================================

HttpResponse RequestHandler::_handleGet(const HttpRequest& request, 
                                        const LocationConfig& location_config)
{
    std::string file_path = _buildFilePath(request.getPath(), location_config); // Local variable (snake_case)
    
    if (file_path.find("..") != std::string::npos)
    {
        return _getErrorPage(403);
    }
    
    if (!_fileExists(file_path))
    {
        return _getErrorPage(404);
    }
    
    if (_isDirectory(file_path))
    {
        return _serveDirectory(file_path, location_config);
    }
    
    size_t ext_pos = file_path.find_last_of('.'); // Local variable (snake_case)
    if (!location_config.cgi_extension.empty() && ext_pos != std::string::npos)
    {
        std::string ext = file_path.substr(ext_pos); // Local variable (snake_case)
        if (ext == location_config.cgi_extension)
        {
            return _executeCgi(request, file_path, location_config);
        }
    }
    
    HttpResponse response = _serveFile(file_path); // Local variable (snake_case)

    if (request.getMethod() == "HEAD") {
        response.setBody("");
    }
    return response;
}

HttpResponse RequestHandler::_handlePost(const HttpRequest& request, 
                                        const LocationConfig& location_config)
{
    if (location_config.upload_enable)
    {
        return _handleUpload(request, location_config);
    }
    
    std::string file_path = _buildFilePath(request.getPath(), location_config); // Local variable (snake_case)
    
    size_t ext_pos = file_path.find_last_of('.'); // Local variable (snake_case)
    if (!location_config.cgi_extension.empty() && ext_pos != std::string::npos)
    {
        std::string ext = file_path.substr(ext_pos); // Local variable (snake_case)
        if (ext == location_config.cgi_extension)
        {
            return _executeCgi(request, file_path, location_config);
        }
    }
    
    return _getErrorPage(405);
}

HttpResponse RequestHandler::_handleDelete(const HttpRequest& request, 
                                          const LocationConfig& location_config)
{
    std::string file_path = _buildFilePath(request.getPath(), location_config); // Local variable (snake_case)
    
    if (file_path.find("..") != std::string::npos)
    {
        return _getErrorPage(403);
    }
    
    if (!_fileExists(file_path))
    {
        return _getErrorPage(404);
    }
    
    if (_isDirectory(file_path))
    {
        return _getErrorPage(403);
    }
    
    if (std::remove(file_path.c_str()) == 0)
    {
        HttpResponse response; // Local variable (snake_case)
        response.setStatus(204);
        return response;
    }
    
    return _getErrorPage(500);
}

// ========================================
// Private File/Directory Handlers
// ========================================

HttpResponse RequestHandler::_serveFile(const std::string& file_path)
{
    std::string file_content = _readFile(file_path); // Local variable (snake_case)
    
    if (file_content.empty())
    {
        return _getErrorPage(500);
    }
    
    // Assuming HttpResponse has a static method createOkResponse
    return HttpResponse::createOkResponse(file_content, _getMimeType(file_path));
}

HttpResponse RequestHandler::_serveDirectory(const std::string& dir_path,
                                             const LocationConfig& location_config)
{
    std::string index_path = dir_path; // Local variable (snake_case)
    if (!index_path.empty() && index_path[index_path.size() - 1] != '/')
    {
        index_path += "/";
    }
    index_path += location_config.index;
    
    if (_fileExists(index_path) && !_isDirectory(index_path))
    {
        return _serveFile(index_path);
    }
    
    if (location_config.autoindex)
    {
        return _generateDirectoryListing(dir_path, location_config.path); 
    }
    
    return _getErrorPage(403);
}

HttpResponse RequestHandler::_generateDirectoryListing(const std::string& dir_path,
                                                       const std::string& uri_path)
{
    DIR* dir_handle = opendir(dir_path.c_str()); // Local variable (snake_case)
    if (!dir_handle)
    {
        return _getErrorPage(500);
    }
    
    std::ostringstream html_stream; // Local variable (snake_case)
    html_stream << "<html><body><h1>Index of " << uri_path << "</h1>";
    
    // TODO: Loop through directory entries and add links here
    
    closedir(dir_handle);

    // Assuming HttpResponse has a static method createOkResponse
    return HttpResponse::createOkResponse(html_stream.str(), "text/html");
}

// ========================================
// Private Upload Handler
// ========================================

HttpResponse RequestHandler::_handleUpload(const HttpRequest& request,
                                          const LocationConfig& location_config)
{
    std::string file_name = "uploaded_file_" + toString(std::time(NULL)); // Local variable (snake_case)
    
    if (file_name.find("..") != std::string::npos || file_name.find("/") != std::string::npos)
    {
        return _getErrorPage(400);
    }
    
    std::string save_path = location_config.upload_path; // Local variable (snake_case)
    if (!save_path.empty() && save_path[save_path.size() - 1] != '/')
        save_path += "/";
    save_path += file_name;

    std::ofstream output_file(save_path.c_str(), std::ios::binary); // Local variable (snake_case)
    if (!output_file.is_open())
    {
        return _getErrorPage(500);
    }
    
    output_file.write(request.getBody().c_str(), request.getBody().size());
    output_file.close();
    
    HttpResponse response; // Local variable (snake_case)
    response.setStatus(201);
    response.setHeader("Location", request.getPath() + "/" + file_name);
    return response;
}

// ========================================
// Private CGI Executor (NOW USING CGI HANDLER)
// ========================================

HttpResponse RequestHandler::_executeCgi(const HttpRequest& request,
                                         const std::string& script_path,
                                         const LocationConfig& location_config)
{
    CgiHandler cgi_executor(location_config.cgi_path, script_path); // Local variable (snake_case)
    
    // 1. Setup Environment Variables
    std::map<std::string, std::string> env_vars = _buildCGIEnvironment(request, location_config); // Local variable (snake_case)
    for (std::map<std::string, std::string>::const_iterator it = env_vars.begin(); it != env_vars.end(); ++it)
    {
        cgi_executor.setEnv(it->first, it->second);
    }
    
    // 2. Set Input Body for POST
    if (request.getMethod() == "POST")
    {
        cgi_executor.setInput(request.getBody());
    }
    
    // 3. Execute and Handle Exceptions
    try
    {
        cgi_executor.execute();
    }
    catch (const Timeout& e)
    {
        std::cerr << "CGI Timeout: " << e.what() << std::endl;
        return _getErrorPage(504); // Gateway Timeout
    }
    catch (const CgiException& e)
    {
        std::cerr << "CGI Internal Error: " << e.what() << std::endl;
        return _getErrorPage(500);
    }
    catch (const std::runtime_error& e)
    {
        std::cerr << "CGI Script Error: " << e.what() << std::endl;
        return _getErrorPage(500);
    }
    
    // 4. Convert CGI Output to HttpResponse
    HttpResponse response; // Local variable (snake_case)
    std::map<std::string, std::string> cgi_headers = cgi_executor.getResponseHd(); // Local variable (snake_case)
    int status_code = 200; // Local variable (snake_case)
    
    // Process CGI Headers
    for (std::map<std::string, std::string>::const_iterator it = cgi_headers.begin(); it != cgi_headers.end(); ++it)
    {
        if (it->first == "Status")
        {
            std::istringstream iss(it->second); // Local variable (snake_case)
            iss >> status_code;
        }
        else
        {
            response.setHeader(it->first, it->second);
        }
    }
    
    response.setStatus(status_code);
    response.setBody(cgi_executor.getOutput());
    
    return response;
}

// ========================================
// CGI Environment Builder
// ========================================

std::map<std::string, std::string> RequestHandler::_buildCGIEnvironment(
    const HttpRequest& request,
    const LocationConfig& location_config) const
{
    std::map<std::string, std::string> environment_variables; // Local variable (snake_case)
    
    // Essential CGI Variables
    environment_variables["REQUEST_METHOD"] = request.getMethod();
    {
        // Compute SCRIPT_FILENAME here instead of calling _buildFilePath (avoids calling a non-const member)
        std::string relative_path = request.getPath();
        if (request.getPath().find(location_config.path) == 0)
            relative_path = request.getPath().substr(location_config.path.size());
        if (relative_path.empty() || relative_path[0] != '/')
            relative_path = "/" + relative_path;
        relative_path = urlDecode(relative_path);
        environment_variables["SCRIPT_FILENAME"] = location_config.root + relative_path;
    }
    environment_variables["SCRIPT_NAME"] = request.getPath();
    environment_variables["QUERY_STRING"] = request.getQuery();
    environment_variables["SERVER_PROTOCOL"] = request.getVersion();
    environment_variables["SERVER_NAME"] = _config.host;
    environment_variables["SERVER_PORT"] = toString(_config.listen_port);
    
    // Content Headers (for POST/PUT)
    if (request.hasHeader("Content-Length"))
        environment_variables["CONTENT_LENGTH"] = request.getHeader("Content-Length");
    if (request.hasHeader("Content-Type"))
        environment_variables["CONTENT_TYPE"] = request.getHeader("Content-Type");
    
    // Path Translation (Simplified)
    environment_variables["PATH_INFO"] = request.getPath();
    
    // Pass ALL HTTP Headers (prefixed with HTTP_ and converted to UPPER_CASE)
    // TODO: Replace with actual header retrieval method once HttpRequest provides access to headers
    // std::map<std::string, std::string> request_headers = request.getHeaders();
    // for (std::map<std::string, std::string>::const_iterator it = request_headers.begin(); it != request_headers.end(); ++it)
    // {
    //     std::string env_key = "HTTP_";
    //     std::string header_key = it->first;
    //     
    //     for (size_t i = 0; i < header_key.length(); ++i)
    //     {
    //         if (header_key[i] == '-')
    //             env_key += '_';
    //         else
    //             env_key += std::toupper(header_key[i]);
    //     }
    //     environment_variables[env_key] = it->second;
    // }
    
    return environment_variables;
}


// ========================================
// Private Utility Functions (Implemented)
// ========================================

std::string RequestHandler::_buildFilePath(const std::string& uri_path,
                                           const LocationConfig& location_config)
{
    std::string relative_path = uri_path; // Local variable (snake_case)
    
    if (uri_path.find(location_config.path) == 0)
    {
        relative_path = uri_path.substr(location_config.path.size());
    }
    
    if (relative_path.empty() || relative_path[0] != '/')
    {
        relative_path = "/" + relative_path;
    }
    
    relative_path = urlDecode(relative_path);
    
    return location_config.root + relative_path;
}

bool RequestHandler::_fileExists(const std::string& path)
{
    struct stat file_status; // Local variable (snake_case)
    return (stat(path.c_str(), &file_status) == 0);
}

bool RequestHandler::_isDirectory(const std::string& path)
{
    struct stat file_status; // Local variable (snake_case)
    if (stat(path.c_str(), &file_status) != 0)
        return false;
    
    return S_ISDIR(file_status.st_mode);
}

std::string RequestHandler::_readFile(const std::string& path)
{
    std::ifstream file(path.c_str()); // Local variable (snake_case)
    if (!file.is_open())
        return "";
    
    std::ostringstream buffer; // Local variable (snake_case)
    buffer << file.rdbuf();
    return buffer.str();
}

std::string RequestHandler::_getMimeType(const std::string& file_path)
{
    // A simplified mime-type implementation based on extension
    size_t pos = file_path.find_last_of('.'); // Local variable (snake_case)
    if (pos == std::string::npos)
        return "application/octet-stream";

    std::string ext = file_path.substr(pos); // Local variable (snake_case)

    if (ext == ".html" || ext == ".htm") return "text/html";
    if (ext == ".css") return "text/css";
    if (ext == ".js") return "application/javascript";
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".ico") return "image/x-icon";
    if (ext == ".txt") return "text/plain";
    
    return "application/octet-stream";
}

HttpResponse RequestHandler::_getErrorPage(int code)
{
    HttpResponse response; // Local variable (snake_case)
    response.setStatus(code);
    
    std::map<int, std::string>::const_iterator it = _config.error_pages.find(code); // Local variable (snake_case)
    
    std::string body_content; // Local variable (snake_case)

    if (it != _config.error_pages.end())
    {
        // Attempt to read custom error page
        body_content = _readFile(it->second);
        if (!body_content.empty())
        {
            response.setBody(body_content);
            response.setHeader("Content-Type", _getMimeType(it->second));
            return response;
        }
    }
    
    // Default error page if custom page is missing or not configured
    body_content = "<html><body><h1>Error " + toString(code) + "</h1><p>Web Server</p></body></html>";
    response.setBody(body_content);
    response.setHeader("Content-Type", "text/html");

    return response;
}