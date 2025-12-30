#ifndef FILE_HANDLER_HPP
#define FILE_HANDLER_HPP

#include <string>
#include "HttpResponse.hpp"
#include "ConfigParser.hpp"

namespace wsv
{

/**
 * FileHandler - Handles static file and directory requests
 * 
 * Features:
 * - Serve static files with correct MIME types
 * - Handle directory requests (index file or autoindex)
 * - Check file existence and directory status
 * - Read files into memory
 */
class FileHandler
{
public:
    /**
     * Serve a static file with the appropriate MIME type
     * @param file_path Filesystem path to the file
     * @return HttpResponse containing file contents or error
     */
    static HttpResponse serve_file(const std::string& file_path);

    /**
     * Handle directory requests
     * Serves index file if available or generates directory listing
     * @param dir_path Filesystem path to the directory
     * @param location_config Location-specific configuration
     * @return HttpResponse containing directory listing or index file
     */
    static HttpResponse serve_directory(const std::string& dir_path,
                                        const LocationConfig& location_config);

    /**
     * Read entire file content into memory
     * @param path Filesystem path to the file
     * @return File content as string, empty if cannot read
     */
    static std::string read_file(const std::string& path);

    /**
     * Determine MIME type based on file extension
     * @param file_path Filesystem path or filename
     * @return MIME type as string (default: application/octet-stream)
     */
    static std::string get_mime_type(const std::string& file_path);

    /**
     * Check if a file or directory exists
     * @param path Filesystem path
     * @return true if exists, false otherwise
     */
    static bool file_exists(const std::string& path);

    /**
     * Check if a path is a directory
     * @param path Filesystem path
     * @return true if path is a directory, false otherwise
     */
    static bool is_directory(const std::string& path);

private:
    // Private Helper Method

    /**
     * Generate HTML directory listing (for autoindex)
     * @param dir_path Filesystem path to directory
     * @param uri_path URI path corresponding to the directory
     * @return HttpResponse containing HTML listing
     */
    static HttpResponse _generate_directory_listing(const std::string& dir_path,
                                                    const std::string& uri_path);
};

} // namespace wsv

#endif // FILE_HANDLER_HPP
