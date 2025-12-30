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
    std::string body = request.getBody();
    std::string content_type = request.hasHeader("Content-Type") ? request.getHeader("Content-Type") : "";
    std::string boundary = _extract_boundary(content_type);

    if (boundary.empty())
    {
        // Fallback to old behavior if not multipart
        size_t pos = body.find("Content-Disposition:");
        if (pos != std::string::npos)
        {
            size_t end = body.find("\r\n", pos);
            if (end == std::string::npos) end = body.find("\n", pos);
            if (end != std::string::npos)
            {
                std::string filename = _extract_multipart_filename(body.substr(pos, end - pos));
                if (!filename.empty()) return filename;
            }
        }
        return _generate_default_filename();
    }

    std::string part_boundary = "--" + boundary;
    size_t pos = body.find(part_boundary);

    while (pos != std::string::npos)
    {
        size_t next_pos = body.find(part_boundary, pos + part_boundary.length());
        std::string part = body.substr(pos, next_pos - pos);

        size_t disp_pos = part.find("Content-Disposition:");
        if (disp_pos != std::string::npos)
        {
            size_t end = part.find("\r\n", disp_pos);
            if (end == std::string::npos) end = part.find("\n", disp_pos);
            if (end != std::string::npos)
            {
                std::string filename = _extract_multipart_filename(part.substr(disp_pos, end - disp_pos));
                if (!filename.empty())
                {
                    Logger::debug("Successfully extracted filename from part: '" + filename + "'");
                    return filename;
                }
            }
        }
        pos = next_pos;
        if (pos != std::string::npos && pos + part_boundary.length() + 2 <= body.length() && 
            body.substr(pos + part_boundary.length(), 2) == "--")
            break; // End boundary
    }

    return _generate_default_filename();
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
 * SECURITY: Also checks for ".." sequences after sanitization
 */
std::string UploadHandler::_sanitize_filename(const std::string& filename)
{
    size_t last_slash = filename.find_last_of("/\\");
    std::string sanitized = (last_slash != std::string::npos) 
        ? filename.substr(last_slash + 1) 
        : filename;
    
    // SECURITY: Reject filenames containing ".." even after removing path components
    // This prevents attacks like uploading a file named "..malicious.txt"
    if (sanitized.find("..") != std::string::npos)
    {
        Logger::warning("Filename contains '..' after sanitization, using default name");
        return _generate_default_filename();
    }
    
    return sanitized;
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
    
    std::string part_boundary = "--" + boundary;
    size_t pos = body.find(part_boundary);

    while (pos != std::string::npos)
    {
        size_t next_pos = body.find(part_boundary, pos + part_boundary.length());
        
        // Headers of this part end at \r\n\r\n
        size_t header_end = body.find("\r\n\r\n", pos);
        if (header_end != std::string::npos && (next_pos == std::string::npos || header_end < next_pos))
        {
            // Check if this part has a filename
            std::string headers = body.substr(pos, header_end - pos);
            if (headers.find("Content-Disposition:") != std::string::npos && 
                headers.find("filename=") != std::string::npos)
            {
                size_t content_start = header_end + 4;
                size_t content_end = next_pos;
                
                if (content_end == std::string::npos)
                {
                    // Should not happen in valid multipart, but handle it
                    content_end = body.length();
                }

                // Backtrack to remove \r\n before the boundary
                if (content_end >= 2 && body[content_end - 2] == '\r' && body[content_end - 1] == '\n')
                    content_end -= 2;
                else if (content_end >= 1 && body[content_end - 1] == '\n')
                    content_end -= 1;

                Logger::debug("Found file part. Content starts at " + StringUtils::toString(content_start) + 
                             ", ends at " + StringUtils::toString(content_end));
                return body.substr(content_start, content_end - content_start);
            }
        }

        pos = next_pos;
        if (pos != std::string::npos && pos + part_boundary.length() + 2 <= body.length() && 
            body.substr(pos + part_boundary.length(), 2) == "--")
            break; // End boundary
    }

    Logger::warning("Could not find a part with filename in multipart body");
    return "";
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
    
    // Check if write operation succeeded
    if (output.fail())
    {
        output.close();
        Logger::error("Failed to write file (possibly disk full): " + file_path);
        HttpResponse response;
        response.setStatus(507);  // 507 Insufficient Storage
        response.setBody("{\"error\": \"Insufficient storage space\"}");
        response.setHeader("Content-Type", "application/json");
        return response;
    }
    
    output.close();
    
    // Check if close operation succeeded (flushes buffers)
    if (output.fail())
    {
        Logger::error("Failed to close file (possibly disk full): " + file_path);
        HttpResponse response;
        response.setStatus(507);  // 507 Insufficient Storage
        response.setBody("{\"error\": \"Insufficient storage space\"}");
        response.setHeader("Content-Type", "application/json");
        return response;
    }
    
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
    
    // Build Location header: ensure single slash between path and filename
    std::string location = request_path;
    if (!location.empty() && location[location.length() - 1] == '/')
        location = location.substr(0, location.length() - 1);
    location += "/" + filename;
    
    response.setHeader("Location", location);
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
