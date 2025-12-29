#include "UploadHandler.hpp"
#include "FileHandler.hpp"
#include "utils/StringUtils.hpp"
#include "utils/Logger.hpp"
#include <sys/stat.h>
#include <fstream>
#include <ctime>

namespace wsv {

/**
 * Main upload handler - orchestrates the upload process
 */
HttpResponse UploadHandler::handle_upload(const HttpRequest& request,
                                          const LocationConfig& config)
{
    // Step 1: Validate upload directory
    HttpResponse dir_validation = _validate_upload_directory(config.upload_path);
    if (dir_validation.getStatus() != 200) {
        Logger::debug("Directory validation failed with status: " + StringUtils::toString(dir_validation.getStatus()));
        return dir_validation;
    }
    
    // Step 2: Extract filename (raw, unsanitized)
    std::string raw_filename = _extract_filename(request);
    Logger::debug("Extracted raw filename: '" + raw_filename + "'");
    
    // Step 3: Validate filename for security issues BEFORE sanitizing
    HttpResponse filename_validation = _validate_filename(raw_filename);
    if (filename_validation.getStatus() != 200) {
        Logger::debug("Filename validation failed for: '" + raw_filename + "'");
        return filename_validation;
    }
    
    // Step 4: Sanitize filename (remove path components)
    std::string filename = _sanitize_filename(raw_filename);
    Logger::debug("Sanitized filename: '" + filename + "'");
    
    // Step 5: Build save path
    std::string save_path = config.upload_path;
    if (!save_path.empty() && save_path[save_path.length() - 1] != '/')
        save_path += "/";
    save_path += filename;
    
    // Step 6: Extract file content
    std::string file_content = _extract_file_content(request);
    
    // Step 7: Save file to disk
    HttpResponse save_result = _save_file(save_path, file_content);
    if (save_result.getStatus() != 200) {
        return save_result;
    }
    
    // Step 8: Return success response
    return _create_success_response(filename, request.getPath());
}

/**
 * Validate that upload directory exists or can be created
 */
HttpResponse UploadHandler::_validate_upload_directory(const std::string& upload_path)
{
    Logger::debug("Validating upload directory: '" + upload_path + "'");
    if (!_ensure_directory_exists(upload_path))
    {
        Logger::debug("Directory validation failed for: '" + upload_path + "'");
        HttpResponse response;
        response.setStatus(500);
        response.setBody("{\"error\": \"Failed to create upload directory\"}");
        response.setHeader("Content-Type", "application/json");
        return response;
    }
    Logger::debug("Directory validation passed");
    
    HttpResponse response;
    response.setStatus(200);  // Explicitly set success status
    return response;
}

/**
 * Validate filename for security issues
 */
HttpResponse UploadHandler::_validate_filename(const std::string& filename)
{
    if (filename.empty() ||
        filename.find("..") != std::string::npos ||
        filename.find("/") != std::string::npos ||
        filename.find("\\") != std::string::npos)
    {
        HttpResponse response;
        response.setStatus(400);
        response.setBody("{\"error\": \"Invalid filename\"}");
        response.setHeader("Content-Type", "application/json");
        return response;
    }
    
    HttpResponse response;
    response.setStatus(200);  // Explicitly set success
    return response;
}

/**
 * Extract filename from request
 */
std::string UploadHandler::_extract_filename(const HttpRequest& request)
{
    std::string filename;
    
    // In multipart/form-data, Content-Disposition is in the body, not in headers
    // We need to extract it from the body
    std::string body = request.getBody();
    Logger::debug("Body size: " + StringUtils::toString(body.length()));
    
    // Look for Content-Disposition in the body
    size_t pos = body.find("Content-Disposition:");
    if (pos != std::string::npos)
    {
        // Find the end of this line
        size_t end = body.find("\r\n", pos);
        if (end == std::string::npos)
            end = body.find("\n", pos);
        
        if (end != std::string::npos)
        {
            std::string disposition_line = body.substr(pos, end - pos);
            Logger::debug("Found Content-Disposition in body: '" + disposition_line + "'");
            filename = _extract_multipart_filename(disposition_line);
            
            if (!filename.empty())
            {
                Logger::debug("Successfully extracted filename: '" + filename + "'");
                return filename;  // Return WITHOUT sanitizing - let caller validate first
            }
        }
    }
    else
    {
        Logger::debug("Content-Disposition not found in body");
    }
    
    // Only generate default if we couldn't extract a filename
    Logger::debug("Generating default filename");
    filename = _generate_default_filename();
    Logger::debug("Final filename before return: '" + filename + "'");
    return filename;  // Return WITHOUT sanitizing
}

/**
 * Extract filename from Content-Disposition header
 */
std::string UploadHandler::_extract_multipart_filename(const std::string& content_disposition)
{
    size_t pos = content_disposition.find("filename=");
    if (pos == std::string::npos)
    {
        Logger::debug("No filename= found in: '" + content_disposition + "'");
        return "";
    }
    
    pos += 9;  // Length of "filename="
    
    // Skip opening quote if present
    bool has_quotes = false;
    if (pos < content_disposition.size() && content_disposition[pos] == '"')
    {
        has_quotes = true;
        pos++;
    }
    
    size_t end_pos = pos;
    if (has_quotes)
    {
        // Find closing quote
        while (end_pos < content_disposition.size() && content_disposition[end_pos] != '"')
            end_pos++;
    }
    else
    {
        // Find semicolon or newline
        while (end_pos < content_disposition.size())
        {
            char c = content_disposition[end_pos];
            if (c == ';' || c == '\r' || c == '\n' || c == ' ')
                break;
            end_pos++;
        }
    }
    
    std::string filename = content_disposition.substr(pos, end_pos - pos);
    Logger::debug("Extracted filename from disposition: '" + filename + "'");
    return filename;
}

/**
 * Generate default filename
 */
std::string UploadHandler::_generate_default_filename()
{
    return "uploaded_file_" + StringUtils::toString(std::time(NULL));
}

/**
 * Sanitize filename by removing path separators
 */
std::string UploadHandler::_sanitize_filename(const std::string& filename)
{
    size_t last_slash = filename.find_last_of("/\\");
    if (last_slash != std::string::npos)
        return filename.substr(last_slash + 1);
    
    return filename;
}

/**
 * Extract file content from request body
 */
std::string UploadHandler::_extract_file_content(const HttpRequest& request)
{
    std::string content = request.getBody();
    
    if (!request.hasHeader("Content-Type"))
        return content;  // Handles raw binary uploads
    
    std::string content_type = request.getHeader("Content-Type");
    if (content_type.find("multipart/form-data") == std::string::npos)
        return content;  // Handles other content types (octet-streaming, etc.)
    
    std::string boundary = _extract_boundary(content_type);
    if (boundary.empty())
        return content;
    
    return _extract_multipart_content(content, boundary);  // Only for multipart
}

/**
 * Extract boundary from Content-Type header
 */
std::string UploadHandler::_extract_boundary(const std::string& content_type)
{
    size_t pos = content_type.find("boundary=");
    if (pos == std::string::npos)
        return "";
    
    std::string boundary = content_type.substr(pos + 9);
    if (!boundary.empty() && boundary[0] == '"')
        boundary = boundary.substr(1);
    if (!boundary.empty() && boundary[boundary.length() - 1] == '"')
        boundary = boundary.substr(0, boundary.length() - 1);
    
    return boundary;
}

/**
 * Extract multipart content using boundary
 */
std::string UploadHandler::_extract_multipart_content(const std::string& body,
                                                      const std::string& boundary)
{
    Logger::debug("=== extractMultipartContent ===");
    Logger::debug("Boundary: " + boundary);
    Logger::debug("Body size: " + StringUtils::toString(body.size()) + " bytes");
    
    // Find the empty line after Content-Type or Content-Transfer-Encoding
    // This marks where the file content begins
    
    // Method: Find "\r\n\r\n", which marks the end of headers and start of content
    size_t content_start = body.find("\r\n\r\n");
    
    if (content_start == std::string::npos)
    {
        Logger::warning("Could not find content separator (\\r\\n\\r\\n)");
        return "";  // Strictly follow HTTP specification
    }
    
    content_start += 4;  // Skip \r\n\r\n
    
    Logger::debug("Content starts at position: " + StringUtils::toString(content_start));
    
    // Find end boundary: "\r\n--"boundary
    std::string end_boundary = "\r\n--" + boundary;
    
    size_t content_end = body.find(end_boundary, content_start);
    
    if (content_end == std::string::npos)
    {
        Logger::warning("Could not find end boundary");
        // Try to find the final boundary marker
        std::string final_boundary = "--" + boundary + "--";
        content_end = body.find(final_boundary, content_start);
        if (content_end != std::string::npos)
        {
            // Backtrack to remove trailing newlines
            while (content_end > content_start && 
                   (body[content_end-1] == '\r' || body[content_end-1] == '\n'))
            {
                content_end--;
            }
        }
        else
        {
            Logger::error("Invalid multipart format: no end boundary found");
            return "";
        }
    }
    
    Logger::debug("Content ends at position: " + StringUtils::toString(content_end));
    
    // Extract actual file content
    std::string content = body.substr(content_start, content_end - content_start);
    Logger::debug("Extracted content size: " + StringUtils::toString(content.size()) + " bytes");
    Logger::debug("Content preview (first 100 chars): ");
    Logger::debug(content.substr(0, std::min(size_t(100), content.size())));
    Logger::debug("===============================");
    
    return content;
}

/**
 * Save file content to disk
 */
HttpResponse UploadHandler::_save_file(const std::string& file_path,
                                       const std::string& content)
{
    std::ofstream output(file_path.c_str(), std::ios::binary);
    if (!output.is_open())
    {
        HttpResponse response;
        response.setStatus(500);
        response.setBody("{\"error\": \"Failed to save file\"}");
        response.setHeader("Content-Type", "application/json");
        return response;
    }
    
    output.write(content.c_str(), content.size());
    output.close();
    
    HttpResponse response;
    response.setStatus(200);  // Success
    return response;
}

/**
 * Create success response
 */
HttpResponse UploadHandler::_create_success_response(const std::string& filename,
                                                     const std::string& request_path)
{
    HttpResponse response;
    response.setStatus(201);
    response.setHeader("Location", request_path + "/" + filename);
    response.setHeader("Content-Type", "application/json");
    response.setBody("{\"status\": \"uploaded\", \"filename\": \"" + filename + "\"}");
    return response;
}

/**
 * Ensure directory exists
 */
bool UploadHandler::_ensure_directory_exists(const std::string& dir_path)
{
    if (FileHandler::file_exists(dir_path) && FileHandler::is_directory(dir_path))
        return true;
    
    if (mkdir(dir_path.c_str(), 0755) == 0)
        return true;
    
    Logger::error("Failed to create directory: " + dir_path);
    return false;
}


} // namespace wsv
