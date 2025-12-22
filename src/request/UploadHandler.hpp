#ifndef UPLOAD_HANDLER_HPP
#define UPLOAD_HANDLER_HPP

#include <string>
#include "HttpRequest.hpp"
#include "HttpResponse.hpp"
#include "ConfigParser.hpp"

namespace wsv {

/**
 * UploadHandler - Handles file upload requests
 * Supports file uploads via POST/multipart requests and saves files to server filesystem.
 */
class UploadHandler {
public:
    /**
     * Handle a file upload request
     * @param request HTTP request containing uploaded file
     * @param config Location configuration (upload path, limits)
     * @return HttpResponse indicating success or failure
     */
    static HttpResponse handle_upload(const HttpRequest& request,
                                      const LocationConfig& config);

private:
    /**
     * Validate that the upload directory exists and is writable
     * @param upload_path Path to the upload directory
     * @return HttpResponse with error if invalid, otherwise empty
     */
    static HttpResponse _validate_upload_directory(const std::string& upload_path);

    /**
     * Validate the filename for safety and allowed characters
     * @param filename Filename to validate
     * @return HttpResponse with error if invalid, otherwise empty
     */
    static HttpResponse _validate_filename(const std::string& filename);

    /**
     * Extract filename from HTTP request
     * @param request HTTP request containing uploaded file
     * @return Extracted filename
     */
    static std::string _extract_filename(const HttpRequest& request);

    /**
     * Extract filename from multipart Content-Disposition header
     * @param content_disposition Value of Content-Disposition header
     * @return Extracted filename
     */
    static std::string _extract_multipart_filename(const std::string& content_disposition);

    /**
     * Generate a default filename if none is provided
     * @return Default filename string
     */
    static std::string _generate_default_filename();

    /**
     * Sanitize a filename to remove unsafe characters
     * @param filename Original filename
     * @return Sanitized filename
     */
    static std::string _sanitize_filename(const std::string& filename);

    /**
     * Extract raw file content from HTTP request body
     * @param request HTTP request
     * @return File content as string
     */
    static std::string _extract_file_content(const HttpRequest& request);

    /**
     * Extract file content from multipart body using boundary
     * @param body Full request body
     * @param boundary Multipart boundary string
     * @return File content as string
     */
    static std::string _extract_multipart_content(const std::string& body,
                                                  const std::string& boundary);

    /**
     * Extract multipart boundary from Content-Type header
     * @param content_type Value of Content-Type header
     * @return Boundary string
     */
    static std::string _extract_boundary(const std::string& content_type);

    /**
     * Save file content to server filesystem
     * @param file_path Full filesystem path
     * @param content File content to save
     * @return HttpResponse indicating success or failure
     */
    static HttpResponse _save_file(const std::string& file_path,
                                   const std::string& content);

    /**
     * Create HttpResponse indicating successful upload
     * @param filename Saved filename
     * @param request_path Original request path
     * @return HttpResponse with success message
     */
    static HttpResponse _create_success_response(const std::string& filename,
                                                 const std::string& request_path);

    /**
     * Ensure a directory exists, creating it if necessary
     * @param dir_path Directory path
     * @return true if directory exists or was created successfully
     */
    static bool _ensure_directory_exists(const std::string& dir_path);
};

} // namespace wsv

#endif // UPLOAD_HANDLER_HPP
