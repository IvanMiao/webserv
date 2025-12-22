#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <sys/types.h>

/**
 * CgiHandler - Executes CGI scripts following CGI/1.1 specification
 */
class CgiHandler
{
    friend class CgiHandlerTestable;

public:
    // ========================================
    // Type Definitions
    // ========================================
    
    typedef std::map<std::string, std::string> HeaderMap;
    
    // ========================================
    // Constants
    // ========================================
    
    static const size_t BUFFER_SIZE = 4096;
    static const size_t MAX_OUTPUT_SIZE = 10485760;
    static const unsigned int DEFAULT_TIMEOUT = 30;
    static const int EXIT_CGI_FAILED = 42;
    
    // ========================================
    // Exception Classes
    // ========================================
    
    class PipeFailed : public std::runtime_error 
    {
    public:
        PipeFailed() : std::runtime_error("Failed to create pipe") {}
    };
    
    class ForkFailed : public std::runtime_error 
    {
    public:
        ForkFailed() : std::runtime_error("Failed to fork process") {}
    };
    
    class Timeout : public std::runtime_error 
    {
    public:
        Timeout() : std::runtime_error("CGI script execution timeout") {}
    };
    
    class WriteFailed : public std::runtime_error 
    {
    public:
        WriteFailed(const std::string& msg = "Failed to write to CGI stdin") 
            : std::runtime_error(msg) {}
    };
    
    class ReadingFailed : public std::runtime_error 
    {
    public:
        ReadingFailed(const std::string& msg = "Failed to read from CGI stdout") 
            : std::runtime_error(msg) {}
    };
    
    class OutputTooLarge : public std::runtime_error 
    {
    public:
        OutputTooLarge() : std::runtime_error("CGI output exceeds maximum size") {}
    };
    
    // ========================================
    // Constructors & Destructor
    // ========================================
    
    CgiHandler();
    CgiHandler(const std::string& cgi_bin, const std::string& script_path);
    ~CgiHandler();
    
    // ========================================
    // Configuration Methods
    // ========================================
    
    void setCGIBin(const std::string& path);
    void setScriptPath(const std::string& path);
    void setEnvironmentVariable(const std::string& key, const std::string& value);
    void setInput(const std::string& input);
    void setTimeout(unsigned int seconds);
    
    // ========================================
    // Getters
    // ========================================
    
    std::string getCGIBin() const;
    std::string getScriptPath() const;
    HeaderMap getEnvironment() const;
    std::string getInput() const;
    std::string getOutput() const;
    HeaderMap getResponseHeaders() const;
    
    // ========================================
    // Core Execution
    // ========================================
    
    void execute();

private:
    // ========================================
    // Helper Structures
    // ========================================
    
    struct _PipeSet
    {
        int input_pipe[2];
        int output_pipe[2];
        
        _PipeSet();
        
        void _createPipes();
        void _closeAll();
        void _setupForChild();
        void _setupForParent();
    };
    
    struct _EnvironmentBuilder
    {
        std::vector<std::string> env_strings;
        std::vector<char*> env_pointers;
        
        void _build(const HeaderMap& env_map);
        char** _getEnvironmentArray();
    };
    
    // ========================================
    // Member Variables
    // ========================================
    
    std::string _cgi_bin;
    std::string _script_path;
    HeaderMap _environment;
    std::string _input;
    std::string _output;
    HeaderMap _response_headers;
    unsigned int _timeout;
    
    // ========================================
    // Core Execution Steps
    // ========================================
    
    void _executeInChild(const _PipeSet& pipes, _EnvironmentBuilder& env);
    void _executeInParent(pid_t child_pid, _PipeSet& pipes);
    
    // ========================================
    // Parent Process Operations
    // ========================================
    
    void _writeInputToChild(int write_fd);
    void _readOutputFromChild(int read_fd, pid_t child_pid);
    void _waitForChildAndCheckStatus(pid_t child_pid);
    
    // ========================================
    // Child Process Operations
    // ========================================
    
    void _redirectChildIO(const _PipeSet& pipes);
    void _executeCGIScript(_EnvironmentBuilder& env);
    
    // ========================================
    // Output Parsing
    // ========================================
    
    void _parseOutput();
    void _parseHeaderLines(const std::string& header_str);
    
    // ========================================
    // Utility Methods
    // ========================================
    
    void _cleanupFileDescriptors(int* fds, size_t count);
    void _killAndWaitForChild(pid_t child_pid);
};

#endif


/* #ifndef CGI_HANDLER_HPP
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

#endif // CGI_HANDLER_HPP */