#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include <string>
#include <map>
#include <vector>
#include <stdexcept>

// 假设我们有一个 MAX_OUTPUT_SIZE 常量
#define MAX_OUTPUT_SIZE 10 * 1024 * 1024 // 10MB limit for CGI output


// 前向声明，for testing purposes
class CgiHandlerTestable;


// 定义异常类
class CgiException : public std::runtime_error {
public:
    CgiException(const std::string& msg) : std::runtime_error(msg) {}
};

// ... (PipeFailed, ForkFailed, WriteFailed, ReadingFailed, Timeout 异常定义保持不变)
class PipeFailed : public CgiException {
public:
    PipeFailed() : CgiException("CGI Pipe failed") {}
};

class ForkFailed : public CgiException {
public:
    ForkFailed() : CgiException("CGI Fork failed") {}
};

class WriteFailed : public CgiException {
public:
    WriteFailed() : CgiException("CGI Write failed") {}
};

class ReadingFailed : public CgiException {
public:
    ReadingFailed() : CgiException("CGI Read failed") {}
};

class Timeout : public CgiException {
public:
    Timeout() : CgiException("CGI Timeout (SIGALRM)") {}
};


class CgiHandler
{
// ------------------------------------------------------------------
// 重点：在这里添加友元声明，解决测试代码中访问私有成员的问题
    friend class CgiHandlerTestable;
// ------------------------------------------------------------------
private:
    std::string     _cgi_bin;
    std::string     _script_path;
    std::map<std::string, std::string> _env;
    std::string     _input;
    std::string     _output;
    std::map<std::string, std::string> _response_hd;
    unsigned int    _timeout;

    void            cleanFds(int *fd, size_t count);
    void            parseOutput();

public:
    CgiHandler();
    CgiHandler(const std::string& cgi_bin, const std::string& script_path);
    ~CgiHandler();

    // Setter
    void            setCGI(const std::string& path);
    void            setScript(const std::string& path);
    void            setEnv(const std::string& key, const std::string& value);
    void            setInput(const std::string& str);
    void            setTimeout(unsigned int seconds);

    // Getter
    std::string     getCGI() const;
    std::string     getScript() const;
    std::map<std::string, std::string> getEnv() const;
    std::string     getInput() const;
    std::string     getOutput() const;
    std::map<std::string, std::string> getResponseHd() const;
    
    // 核心执行函数
    void            execute();
};

#endif // CGI_HANDLER_HPP