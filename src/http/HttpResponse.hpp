#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <map>
#include <vector>

namespace wsv
{

class HttpResponse
{
public:
    HttpResponse();
    ~HttpResponse();

    // 设置响应
    void setStatus(int code);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    void appendBody(const std::string& data);

    // 生成完整的 HTTP 响应
    std::string serialize() const;

    // 便捷方法
    void setContentType(const std::string& type);
    void setContentLength(size_t length);

    // 预设响应
    static HttpResponse createErrorResponse(int code,
                                           const std::string& message = "");
    static HttpResponse createRedirectResponse(int code,
                                               const std::string& location);
    static HttpResponse createOkResponse(
        const std::string& body,
        const std::string& content_type = "text/html"
    );

    // 状态码描述
    static std::string getStatusMessage(int code);

private:
    int _status_code;
    std::string _version;
    std::map<std::string, std::string> _headers;
    std::string _body;

    // 默认 Header
    void _setDefaultHeaders();
};

} // namespace wsv

#endif // HTTP_RESPONSE_HPP


