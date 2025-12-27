#include "UploadHandler.hpp"
#include "FileHandler.hpp"
#include "../utils/StringHelper.hpp"
#include <sys/stat.h>
#include <fstream>
#include <ctime>
#include <iostream>

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
        std::cerr << "[DEBUG UPLOAD] Directory validation failed with status: " << dir_validation.getStatus() << std::endl;
        return dir_validation;
    }
    
    // Step 2: Extract filename (raw, unsanitized)
    std::string raw_filename = _extract_filename(request);
    std::cerr << "[DEBUG UPLOAD] Extracted raw filename: '" << raw_filename << "'" << std::endl;
    
    // Step 3: Validate filename for security issues BEFORE sanitizing
    HttpResponse filename_validation = _validate_filename(raw_filename);
    if (filename_validation.getStatus() != 200) {
        std::cerr << "[DEBUG UPLOAD] Filename validation failed for: '" << raw_filename << "'" << std::endl;
        return filename_validation;
    }
    
    // Step 4: Sanitize filename (remove path components)
    std::string filename = _sanitize_filename(raw_filename);
    std::cerr << "[DEBUG UPLOAD] Sanitized filename: '" << filename << "'" << std::endl;
    
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
    std::cerr << "[DEBUG UPLOAD] Validating upload directory: '" << upload_path << "'" << std::endl;
    if (!_ensure_directory_exists(upload_path))
    {
        std::cerr << "[DEBUG UPLOAD] Directory validation failed for: '" << upload_path << "'" << std::endl;
        HttpResponse response;
        response.setStatus(500);
        response.setBody("{\"error\": \"Failed to create upload directory\"}");
        response.setHeader("Content-Type", "application/json");
        return response;
    }
    std::cerr << "[DEBUG UPLOAD] Directory validation passed" << std::endl;
    
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
        StringHelper::hasPathTraversal(filename) ||
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
    std::cerr << "[DEBUG UPLOAD] Body size: " << body.length() << std::endl;
    
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
            std::cerr << "[DEBUG UPLOAD] Found Content-Disposition in body: '" << disposition_line << "'" << std::endl;
            filename = _extract_multipart_filename(disposition_line);
            
            if (!filename.empty())
            {
                std::cerr << "[DEBUG UPLOAD] Successfully extracted filename: '" << filename << "'" << std::endl;
                return filename;  // Return WITHOUT sanitizing - let caller validate first
            }
        }
    }
    else
    {
        std::cerr << "[DEBUG UPLOAD] Content-Disposition not found in body" << std::endl;
    }
    
    // Only generate default if we couldn't extract a filename
    std::cerr << "[DEBUG UPLOAD] Generating default filename" << std::endl;
    filename = _generate_default_filename();
    std::cerr << "[DEBUG UPLOAD] Final filename before return: '" << filename << "'" << std::endl;
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
        std::cerr << "[DEBUG UPLOAD] No filename= found in: '" << content_disposition << "'" << std::endl;
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
    std::cerr << "[DEBUG UPLOAD] Extracted filename from disposition: '" << filename << "'" << std::endl;
    return filename;
}

/**
 * Generate default filename
 */
std::string UploadHandler::_generate_default_filename()
{
    return "uploaded_file_" + StringHelper::toString(std::time(NULL));
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
        return content;
    
    std::string content_type = request.getHeader("Content-Type");
    if (content_type.find("multipart/form-data") == std::string::npos)
        return content;
    
    std::string boundary = _extract_boundary(content_type);
    if (boundary.empty())
        return content;
    
    return _extract_multipart_content(content, boundary);
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
    std::cerr << "=== extractMultipartContent ===" << std::endl;
    std::cerr << "Boundary: " << boundary << std::endl;
    std::cerr << "Body size: " << body.size() << " bytes" << std::endl;
    
    // 查找 Content-Type 或 Content-Transfer-Encoding 之后的空行
    // 这是文件内容开始的位置
    
    // 方法：查找 "\r\n\r\n" (双换行)，这标志着 headers 结束，content 开始
    size_t content_start = body.find("\r\n\r\n");
    
    if (content_start == std::string::npos)
    {
        // 尝试只用 \n\n (某些客户端可能不用 \r\n)
        content_start = body.find("\n\n");
        if (content_start != std::string::npos)
        {
            content_start += 2;  // 跳过 \n\n
        }
        else
        {
            std::cerr << "Warning: Could not find content separator" << std::endl;
            return body;  // 找不到分隔符，返回原始 body
        }
    }
    else
    {
        content_start += 4;  // 跳过 \r\n\r\n
    }
    
    std::cerr << "Content starts at position: " << content_start << std::endl;
    
    // 查找结束边界: \r\n--boundary 或 \n--boundary
    std::string end_boundary1 = "\r\n--" + boundary;
    std::string end_boundary2 = "\n--" + boundary;
    
    size_t content_end = body.find(end_boundary1, content_start);
    
    if (content_end == std::string::npos)
    {
        content_end = body.find(end_boundary2, content_start);
        if (content_end == std::string::npos)
        {
            std::cerr << "Warning: Could not find end boundary" << std::endl;
            // 尝试找到最后的边界标记
            std::string final_boundary = "--" + boundary + "--";
            content_end = body.find(final_boundary, content_start);
            if (content_end != std::string::npos)
            {
                // 往回找换行符
                while (content_end > content_start && 
                       (body[content_end-1] == '\r' || body[content_end-1] == '\n'))
                {
                    content_end--;
                }
            }
            else
            {
                content_end = body.size();
            }
        }
    }
    
    std::cerr << "Content ends at position: " << content_end << std::endl;
    
    // 提取实际文件内容
    std::string content = body.substr(content_start, content_end - content_start);
    std::cerr << "Extracted content size: " << content.size() << " bytes" << std::endl;
    //std::cerr << "Content preview (first 100 chars): " << std::endl;
    //std::cerr << content.substr(0, std::min(size_t(100), content.size())) << std::endl;
    std::cerr << "===============================" << std::endl;
    
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
    
    std::cerr << "Failed to create directory: " << dir_path << std::endl;
    return false;
}


} // namespace wsv
