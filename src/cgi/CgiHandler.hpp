#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include <string>
#include <map>
#include <vector>
#include <stdexcept>
#include <sys/types.h>

namespace wsv
{

/**
 * CgiHandler - Executes CGI scripts following CGI/1.1 specification
 * Refactored for non-blocking I/O
 */
class CgiHandler
{
    friend class CgiHandlerTestable;  // JUST for test, no use in runtime

public:
    // Type Definitions
    typedef std::map<std::string, std::string> HeaderMap;

    // Constants
    static const size_t BUFFER_SIZE = 4096;
    static const size_t MAX_OUTPUT_SIZE = 10485760;
    static const unsigned int DEFAULT_TIMEOUT = 30;
    static const int EXIT_CGI_FAILED = 42;


    // Exception Classes
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
        WriteFailed() : std::runtime_error("Failed to write to CGI stdin") {}
    };
    
    class ReadingFailed : public std::runtime_error 
    {
    public:
        ReadingFailed() : std::runtime_error("Failed to read from CGI stdout") {}
    };
    
    class OutputTooLarge : public std::runtime_error 
    {
    public:
        OutputTooLarge() : std::runtime_error("CGI output exceeds maximum size") {}
    };


    // Constructors & Destructor
    CgiHandler();
    CgiHandler(const std::string& cgi_bin, const std::string& script_path);
    ~CgiHandler();


    // Setters
    void setCGIBin(const std::string& path);
    void setScriptPath(const std::string& path);
    void setEnvironmentVariable(const std::string& key, const std::string& value);
    void setInput(const std::string& input);
    void setTimeout(unsigned int seconds);

    // Getters
    std::string getCGIBin() const { return _cgi_bin; }
    std::string getScriptPath() const { return _script_path; }
    HeaderMap getEnvironment() const { return _environment; }
    std::string getInput() const { return _input; }

    int getStdinWriteFd() const { return _pipes.input_pipe[1]; }
    int getStdoutReadFd() const { return _pipes.output_pipe[0]; }
    pid_t getChildPid() const { return _child_pid; }


    // Helpers for parsing output after read is done
    static void parseCgiOutput(const std::string& raw_output, HeaderMap& headers, std::string& body);


    // Non-Blocking Execution API
    /**
     * @brief Start the CGI process
     * @return PID of the child process
     */
    pid_t start();

    void closeStdin();      // Call when finished writing input
    void closePipes();      // Call when finished everything or error

private:
    // Helper Structures
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


    // Member Variables
    std::string _cgi_bin;
    std::string _script_path;
    HeaderMap _environment;
    std::string _input;
    unsigned int _timeout;
    
    _PipeSet _pipes;
    pid_t _child_pid;


    // Internal Methods
    void _executeInChild(const _PipeSet& pipes, _EnvironmentBuilder& env);
    void _redirectChildIO(const _PipeSet& pipes);
    void _executeCGIScript(_EnvironmentBuilder& env);
};

} // namespace wsv

#endif
