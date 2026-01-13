#include "RequestHandler.hpp"
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
// Public Main Method: handleRequest
// ============================================================================

/**
 * Main entry point to process HTTP request
 * Routes request to appropriate handler based on method and location config
 */
HttpResponse RequestHandler::handleRequest(Client& client)
{
    const HttpRequest& request = client.request;
    std::string method = request.getMethod();
    std::string decoded_path = StringUtils::urlDecode(request.getPath());

    // Basic validation for CGI check
    // Note: We duplicate some checks here to find the LocationConfig and FilePath
    // This is necessary to determine if it is a CGI request safely.
    
    if (decoded_path.find("..") != std::string::npos)
        return ErrorHandler::get_error_page(403, _config);

    const LocationConfig* location_config = _config.findLocation(decoded_path);
    if (!location_config)
        return ErrorHandler::get_error_page(404, _config);

    if (!location_config->isMethodAllowed(method))
        return ErrorHandler::get_error_page(405, _config);

    // Check CGI
    std::string file_path = _buildFilePath(decoded_path, *location_config);
    if (_isCgiRequest(file_path, *location_config))
    {
        // For GET/HEAD requests, the CGI script file must exist
        // For POST requests, the target file doesn't need to exist (upload/creation scenario)
        if ((method == "GET" || method == "HEAD") && !FileHandler::file_exists(file_path))
            return ErrorHandler::get_error_page(404, _config);

        // Start Async CGI
        CgiRequestHandler::startCgi(client, file_path, *location_config, _config);
        
        // Return placeholder. Server will check client.state
        return HttpResponse(); 
    }

    // Fallback to standard synchronous processing
    return handleRequest(request);
}

    
// ============================================================================
// Main Entry Point for Request Processing
// ============================================================================
// Routes requests to appropriate handlers based on:
// 1. Security checks (path traversal)
// 2. Location matching
// 3. Method permissions
// 4. Redirect rules
// 5. Directory auto-redirect
// 6. Request size limits
// ============================================================================
HttpResponse RequestHandler::handleRequest(const HttpRequest& request)
{
    Logger::debug("============================================");
    Logger::debug("START handleRequest");
    Logger::debug("============================================");
    
    std::string method = request.getMethod();
    std::string raw_uri = request.getPath();
    
    Logger::debug("Raw URI: {}", raw_uri);
    Logger::debug("Method: {}", method);
    
    // STEP 1: URL Decode (SECURITY)
    // Decode URL BEFORE path traversal check
    // This prevents bypass via encoded sequences like %2e%2e%2f (../)
    std::string decoded_path = StringUtils::urlDecode(raw_uri);
    Logger::debug("Decoded path: {}", decoded_path);
    
    // STEP 2: Path Traversal Check (SECURITY)
    // Check for path traversal attacks BEFORE other validations
    // This ensures 403 is returned for traversal attempts, not 405
    if (decoded_path.find("..") != std::string::npos)
    {
        Logger::debug("SECURITY: Path traversal detected, returning 403");
        return ErrorHandler::get_error_page(403, _config);
    }
    
    // // ========================================================================
    // // STEP 3: Directory Auto-Redirect Check
    // // ========================================================================
    // // If requesting a directory without trailing slash, redirect to add it
    // // This must happen BEFORE findLocation() to avoid 405 errors
    
    // // Build potential file path to check if it's a directory
    // // We need to do a preliminary check with ANY location to see if path exists
    // std::string test_file_path = _buildFilePathForCheck(decoded_path);
    
    // Logger::debug("Testing path: {}", test_file_path);
    
    // if (FileHandler::file_exists(test_file_path) && 
    //     FileHandler::is_directory(test_file_path))
    // {
    //     // It's a directory - check if URI has trailing slash
    //     if (!decoded_path.empty() && decoded_path[decoded_path.length() - 1] != '/')
    //     {
    //         // Missing trailing slash - redirect to add it
    //         std::string redirect_uri = decoded_path + "/";
            
    //         // Preserve query string if present
    //         if (!request.getQuery().empty())
    //         {
    //             redirect_uri += "?" + request.getQuery();
    //         }
            
    //         Logger::debug("REDIRECT: Directory without trailing slash");
    //         Logger::debug("  From: {}", decoded_path);
    //         Logger::debug("  To: {}", redirect_uri);
            
    //         return HttpResponse::createRedirectResponse(301, redirect_uri);
    //     }
    // }
    
    // STEP 4: Find Matching Location
    const LocationConfig* location_config = _config.findLocation(decoded_path);
    
    if (!location_config)
    {
        Logger::debug("ERROR: No location matched for path: {}", decoded_path);
        return ErrorHandler::get_error_page(404, _config);
    }
    
    Logger::debug("Matched location: {}", location_config->path);
    Logger::debug("Location root: {}", location_config->root);
    Logger::debug("Allowed methods: [{}]", _formatMethodList(location_config->allow_methods));
    
    // STEP 5: Method Permission Check
    if (!location_config->isMethodAllowed(method))
    {
        Logger::debug("ERROR: Method {} not allowed for location {}", 
                     method, location_config->path);
        return ErrorHandler::get_error_page(405, _config);
    }
    
    // STEP 6: Redirect Rule Check
    if (location_config->hasRedirect())
    {
        Logger::debug("REDIRECT: Location has redirect rule");
        Logger::debug("  Code: {}", location_config->redirect_code);
        Logger::debug("  URL: {}", location_config->redirect_url);
        
        return HttpResponse::createRedirectResponse(
            location_config->redirect_code,
            location_config->redirect_url
        );
    }
    
    // STEP 7: Request Body Size Check
    size_t content_length = request.getContentLength();
    
    // For chunked requests, Content-Length header is not present/valid
    // We must check the actual received body size
    if (request.isChunked())
        content_length = request.getBody().length();

    if (content_length > location_config->client_max_body_size)
    {
        Logger::debug("ERROR: Request body too large");
        Logger::debug("  Size: {} bytes", content_length);
        Logger::debug("  Limit: {} bytes", location_config->client_max_body_size);
        return ErrorHandler::get_error_page(413, _config);
    }
    
    // STEP 8: Route to Method Handler
    Logger::debug("Routing to method handler: {}", method);
    
    if (method == "GET" || method == "HEAD")
        return _handleGet(request, *location_config, decoded_path);
    else if (method == "POST")
        return _handlePost(request, *location_config, decoded_path);
    else if (method == "DELETE")
        return _handleDelete(request, *location_config, decoded_path);
    
    // Method not implemented
    Logger::debug("ERROR: Method not implemented: {}", method);
    return ErrorHandler::get_error_page(501, _config);
}

// ============================================================================
// Helper: Build file path for preliminary directory check
// ============================================================================
// This is used before findLocation() to check if path is a directory
// We need to guess the root directory - try common patterns
std::string RequestHandler::_buildFilePathForCheck(const std::string& uri_path)
{
    // Strategy 1: Try to match against all locations to find best root
    const LocationConfig* best_location = NULL;
    size_t best_match_length = 0;
    
    for (size_t i = 0; i < _config.locations.size(); ++i)
    {
        const LocationConfig& loc = _config.locations[i];
        
        // Check if this location matches the URI
        if (uri_path.find(loc.path) == 0)
        {
            if (loc.path.length() > best_match_length)
            {
                best_location = &loc;
                best_match_length = loc.path.length();
            }
        }
    }
    
    // Strategy 2: If no match, use root location
    if (!best_location)
    {
        for (size_t i = 0; i < _config.locations.size(); ++i)
        {
            if (_config.locations[i].path == "/")
            {
                best_location = &_config.locations[i];
                break;
            }
        }
    }
    
    // Strategy 3: If still no match, use first location (fallback)
    if (!best_location && !_config.locations.empty())
    {
        best_location = &_config.locations[0];
    }
    
    // If we found a location, build the path
    if (best_location)
    {
        std::string relative_path = uri_path;
        
        // Remove location prefix
        if (uri_path.find(best_location->path) == 0 && best_location->path != "/")
        {
            relative_path = uri_path.substr(best_location->path.size());
        }
        
        // Ensure relative path starts with /
        if (relative_path.empty() || relative_path[0] != '/')
        {
            relative_path = "/" + relative_path;
        }
        
        // Build full path
        std::string result = best_location->root;
        if (!result.empty() && result[result.length() - 1] == '/')
        {
            result = result.substr(0, result.length() - 1);
        }
        result += relative_path;
        
        return result;
    }
    
    // Fallback: return URI as-is (should never happen)
    return uri_path;
}

// ============================================================================
// Helper: Format method list for logging
// ============================================================================
std::string RequestHandler::_formatMethodList(const std::vector<std::string>& methods)
{
    if (methods.empty())
        return "NONE";
    
    std::string result;
    for (size_t i = 0; i < methods.size(); ++i)
    {
        if (i > 0)
            result += ", ";
        result += methods[i];
    }
    return result;
}

// ============================================================================
// Private Methods: HTTP Method Handlers
// ============================================================================

/**
 * Handle GET and HEAD requests
 * Checks file/directory, CGI, and serves static content
 */
HttpResponse RequestHandler::_handleGet(const HttpRequest& request,
                                        const LocationConfig& location_config,
                                        const std::string& decoded_path)
{
    std::string file_path = _buildFilePath(decoded_path, location_config);

    if (!FileHandler::file_exists(file_path))
        return ErrorHandler::get_error_page(404, _config);

    // Directory Auto-Redirect
    if (FileHandler::is_directory(file_path))
    {
        // Check if request path ends with /
        std::string uri = request.getPath();
        if (!uri.empty() && uri[uri.length() - 1] != '/')
        {
            // If accessing directory but no trailing slash, redirect
            std::string redirect_uri = uri + "/";
            
            // Preserve query string
            if (!request.getQuery().empty())
                redirect_uri += "?" + request.getQuery();
            
            return HttpResponse::createRedirectResponse(301, redirect_uri);
        }
        
        // With trailing slash, handle directory normally
        HttpResponse response = _serve_directory(file_path, location_config);
        if (request.getMethod() == "HEAD")
            response.setBody("");
        return response;
    }

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
                                         const LocationConfig& location_config,
                                         const std::string& decoded_path)
{
    Logger::debug("Routing to _handlePost");
    Logger::debug("Method = {}, Path = {}", request.getMethod(), request.getPath());
    Logger::debug("upload_enable = {}", (location_config.upload_enable ? "true" : "false"));
    
    // Upload enabled -> call UploadHandler
    if (location_config.upload_enable)
    {
        Logger::debug("Calling UploadHandler");
        return UploadHandler::handle_upload(request, location_config);
    }

    // Build file path
    std::string file_path = _buildFilePath(decoded_path, location_config);

    // Non-upload, non-CGI POST: Return 200 OK (accepting the POST data)
    // This handles cases like /post_body which just needs to accept POST requests
    HttpResponse response;
    response.setStatus(200);
    response.setHeader("Content-Type", "text/plain");
    response.setBody("POST request received\n");
    return response;
}

/**
 * Handle DELETE requests
 * Validates file existence, prevents directory deletion, and deletes file
 */
HttpResponse RequestHandler::_handleDelete(const HttpRequest& request,
                                           const LocationConfig& location_config,
                                           const std::string& decoded_path)
{
    (void)request;  // Unused but kept for the same interface
    // Path traversal check is now done in handleRequest() before method validation
    std::string file_path = _buildFilePath(decoded_path, location_config);

    Logger::debug("Full file path: {}", file_path);

    if (!FileHandler::file_exists(file_path))
    {
        Logger::debug("File does not exist, returning 404");
        return ErrorHandler::get_error_page(404, _config);
    }

    if (FileHandler::is_directory(file_path))
    {
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
 * 
 * Two modes:
 * 1. If 'alias' is defined: Use alias semantics (remove location prefix from URI)
 *    Example: location /uploads { alias ./www/site_8080/uploads; }
 *    Request: /uploads/file.txt -> ./www/site_8080/uploads/file.txt
 * 
 * 2. If only 'root' is defined: Use root semantics (append full URI to root)
 *    Example: location / { root ./www; }
 *    Request: /index.html -> ./www/index.html
 * 
 * Note: uri_path is expected to already be URL-decoded by handleRequest()
 */
std::string RequestHandler::_buildFilePath(const std::string& uri_path,
                                           const LocationConfig& location_config)
{
    std::string final_path;

    // Print initial information
    Logger::debug("--- Building File Path ---");
    Logger::debug("URI Path: '{}'", uri_path);
    Logger::debug("Location Path: '{}'", location_config.path);
    Logger::debug("Location Root: '{}'", location_config.root);
    Logger::debug("Location Alias: '{}'", location_config.alias);

    // Prefer alias over root if both are defined
    if (!location_config.alias.empty())
    {
        std::string relative_path = uri_path;
        const std::string& location_path = location_config.path;
        
        if (uri_path.find(location_path) == 0)
        {
            relative_path = uri_path.substr(location_path.length());
            
            if (relative_path.empty())
                relative_path = "/";
            else if (relative_path[0] != '/')
                relative_path = "/" + relative_path;
        }
        
        final_path = location_config.alias + relative_path;
        Logger::debug("Using ALIAS logic. Relative: '{}' -> Final: '{}'", relative_path, final_path);
    }
    else
    {
        // Root semantics: append full URI to root
        final_path = location_config.root + uri_path;
        Logger::debug("Using ROOT logic. Final: '{}'", final_path);
    }

    Logger::debug("Resulting Path: '{}'", final_path);
    Logger::debug("--------------------------");

    return final_path;
}

// Check if file is a CGI script based on configured extension
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

// Serve static file
HttpResponse RequestHandler::_serve_file(const std::string& file_path)
{
    HttpResponse response = FileHandler::serve_file(file_path);
    if (response.getStatus() >= 400)
        return ErrorHandler::get_error_page(response.getStatus(), _config);
    return response;
}

// Serve directory (index file or autoindex)
HttpResponse RequestHandler::_serve_directory(const std::string& dir_path,
                                              const LocationConfig& location_config)
{
    HttpResponse response = FileHandler::serve_directory(dir_path, location_config);
    if (response.getStatus() >= 400)
        return ErrorHandler::get_error_page(response.getStatus(), _config);
    return response;
}

} // namespace wsv
