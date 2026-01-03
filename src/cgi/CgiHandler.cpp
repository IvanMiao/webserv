#include "CgiHandler.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <cstdlib>
#include <vector>

namespace wsv
{

// ========================================
// _PipeSet Implementation
// ========================================

CgiHandler::_PipeSet::_PipeSet()
{
    input_pipe[0] = -1;
    input_pipe[1] = -1;
    output_pipe[0] = -1;
    output_pipe[1] = -1;
}

void CgiHandler::_PipeSet::_createPipes()
{
    if (pipe(input_pipe) == -1)
        throw PipeFailed();

    if (pipe(output_pipe) == -1)
    {
        close(input_pipe[0]);
        close(input_pipe[1]);
        throw PipeFailed();
    }
}

void CgiHandler::_PipeSet::_closeAll()
{
    for (int i = 0; i < 2; i++)
    {
        if (input_pipe[i] != -1)
        {
            close(input_pipe[i]);
            input_pipe[i] = -1;
        }
        if (output_pipe[i] != -1)
        {
            close(output_pipe[i]);
            output_pipe[i] = -1;
        }
    }
}

void CgiHandler::_PipeSet::_setupForChild()
{
    close(input_pipe[1]);
    close(output_pipe[0]);
    // Child keeps input_pipe[0] (read from parent) and output_pipe[1] (write to parent)
    input_pipe[1] = -1;
    output_pipe[0] = -1;
}

void CgiHandler::_PipeSet::_setupForParent()
{
    close(input_pipe[0]);
    close(output_pipe[1]);
    // Parent keeps input_pipe[1] (write to child) and output_pipe[0] (read from child)
    input_pipe[0] = -1;
    output_pipe[1] = -1;
}

// ========================================
// _EnvironmentBuilder Implementation
// ========================================

void CgiHandler::_EnvironmentBuilder::_build(const HeaderMap& env_map)
{
    for (HeaderMap::const_iterator it = env_map.begin(); it != env_map.end(); ++it)
        env_strings.push_back(it->first + "=" + it->second);

    for (size_t i = 0; i < env_strings.size(); ++i)
        env_pointers.push_back(const_cast<char*>(env_strings[i].c_str()));

    env_pointers.push_back(NULL);
}

char** CgiHandler::_EnvironmentBuilder::_getEnvironmentArray()
{
    return &env_pointers[0];
}

// ========================================
// Constructors & Destructor
// ========================================

CgiHandler::CgiHandler()
    : _timeout(DEFAULT_TIMEOUT), _child_pid(-1)
{
}

CgiHandler::CgiHandler(const std::string& cgi_bin, const std::string& script_path)
    : _cgi_bin(cgi_bin)
    , _script_path(script_path)
    , _timeout(DEFAULT_TIMEOUT)
    , _child_pid(-1)
{
}

CgiHandler::~CgiHandler()
{
    _pipes._closeAll();
    if (_child_pid > 0)
    {
        // Non-blocking cleanup for async I/O architecture
        // SIGKILL is sent immediately, and we use WNOHANG to avoid blocking
        // The actual waitpid will be called by the Server's epoll handler
        kill(_child_pid, SIGKILL);
        waitpid(_child_pid, NULL, WNOHANG);
    }
}

// ========================================
// Setters
// ========================================

void CgiHandler::setCGIBin(const std::string& path)
{
    _cgi_bin = path;
}

void CgiHandler::setScriptPath(const std::string& path)
{
    _script_path = path;
}

void CgiHandler::setEnvironmentVariable(const std::string& key, const std::string& value)
{
    _environment[key] = value;
}

void CgiHandler::setInput(const std::string& input)
{
    _input = input;
}

void CgiHandler::setTimeout(unsigned int seconds)
{
    _timeout = seconds;
}

// ========================================
// Non-Blocking Execution API
// ========================================

pid_t CgiHandler::start()
{
    _EnvironmentBuilder env_builder;
    env_builder._build(_environment);

    _pipes._createPipes();

    _child_pid = fork();

    if (_child_pid < 0)
    {
        _pipes._closeAll();
        throw ForkFailed();
    }

    if (_child_pid == 0)
    {
        _executeInChild(_pipes, env_builder);
    }
    else
    {
        _pipes._setupForParent();
        
        // Set parent pipes to non-blocking
        if (fcntl(_pipes.input_pipe[1], F_SETFL, O_NONBLOCK) == -1)
             throw PipeFailed();
        if (fcntl(_pipes.output_pipe[0], F_SETFL, O_NONBLOCK) == -1)
             throw PipeFailed();
    }
    
    return _child_pid;
}

void CgiHandler::closeStdin()
{
    if (_pipes.input_pipe[1] != -1)
    {
        close(_pipes.input_pipe[1]);
        _pipes.input_pipe[1] = -1;
    }
}

void CgiHandler::closePipes()
{
    _pipes._closeAll();
}

// ========================================
// Static Helper: Output Parsing
// ========================================

void CgiHandler::parseCgiOutput(const std::string& raw_output, HeaderMap& headers, std::string& body)
{
    size_t header_end = raw_output.find("\r\n\r\n");
    size_t sep_len = 4;

    if (header_end == std::string::npos)
    {
        header_end = raw_output.find("\n\n");
        sep_len = 2;
    }

    if (header_end == std::string::npos)
    {
        // No headers found, treat entire output as body? Or error?
        // RFC suggests there should be headers. But if missing, maybe just body.
        body = raw_output;
        return;
    }

    std::string headers_str = raw_output.substr(0, header_end);
    body = raw_output.substr(header_end + sep_len);

    std::istringstream stream(headers_str);
    std::string line;

    while (std::getline(stream, line))
    {
        if (!line.empty() && line[line.size() - 1] == '\r')
            line.erase(line.size() - 1);

        size_t pos = line.find(':');
        if (pos == std::string::npos)
            continue;

        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        size_t start = value.find_first_not_of(" \t");
        value = (start == std::string::npos) ? "" : value.substr(start);

        headers[key] = value;
    }
}

// ========================================
// Child Process Execution
// ========================================

void CgiHandler::_executeInChild(const _PipeSet& pipes, _EnvironmentBuilder& env)
{
    // Note: alarm might kill the process, but we should handle timeout in parent via epoll/timers ideally.
    // However, keeping alarm as a safety net for the child process itself is okay.
    alarm(_timeout); 
    _redirectChildIO(pipes);
    _executeCGIScript(env);
    std::exit(EXIT_CGI_FAILED);
}

void CgiHandler::_redirectChildIO(const _PipeSet& pipes)
{
    if (dup2(pipes.input_pipe[0], STDIN_FILENO) == -1)
        std::exit(EXIT_CGI_FAILED);

    if (dup2(pipes.output_pipe[1], STDOUT_FILENO) == -1)
        std::exit(EXIT_CGI_FAILED);

    // Close all original pipe FDs as they are now duplicated
    // Note: We need to close everything to avoid leaks in child
    // _PipeSet::closeAll won't work easily here because it's const, 
    
    close(pipes.input_pipe[0]);
    close(pipes.input_pipe[1]);
    close(pipes.output_pipe[0]);
    close(pipes.output_pipe[1]);
}

void CgiHandler::_executeCGIScript(_EnvironmentBuilder& env)
{
    std::vector<char*> argv;
    argv.push_back(const_cast<char*>(_cgi_bin.c_str()));
    argv.push_back(const_cast<char*>(_script_path.c_str()));
    argv.push_back(NULL);

    execve(_cgi_bin.c_str(), &argv[0], env._getEnvironmentArray());
}

} // namespace wsv