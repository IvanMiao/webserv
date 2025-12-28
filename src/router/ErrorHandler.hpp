#ifndef ERROR_HANDLER_HPP
#define ERROR_HANDLER_HPP

#include <string>
#include "HttpResponse.hpp"
#include "ConfigParser.hpp"

namespace wsv {

/**
 * ErrorHandler - Generates HTTP error responses
 * Supports custom error pages from server config, otherwise generates default HTML pages.
 */
class ErrorHandler {
public:
    /**
     * Generate HTTP error response
     * Uses custom error page from server config if available
     * @param status_code HTTP status code (e.g., 404, 500)
     * @param config Server configuration containing custom error page paths
     * @return HttpResponse object with status code and body content
     */
    static HttpResponse get_error_page(int status_code, const ServerConfig& config);

};

} // namespace wsv

#endif // ERROR_HANDLER_HPP
