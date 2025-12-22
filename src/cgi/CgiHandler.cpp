#include "CgiHandler.hpp"
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <cstring>
#include <cerrno>
#include <sstream>
#include <cstdlib>   // C++
#include <stdlib.h>  // C

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
}

void CgiHandler::_PipeSet::_setupForParent()
{
    close(input_pipe[0]);
    close(output_pipe[1]);
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
    : _timeout(DEFAULT_TIMEOUT)
{
}

CgiHandler::CgiHandler(const std::string& cgi_bin, const std::string& script_path)
    : _cgi_bin(cgi_bin)
    , _script_path(script_path)
    , _timeout(DEFAULT_TIMEOUT)
{
}

CgiHandler::~CgiHandler()
{
}

// ========================================
// Configuration Methods
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
// Getters
// ========================================

std::string CgiHandler::getCGIBin() const
{
    return _cgi_bin;
}

std::string CgiHandler::getScriptPath() const
{
    return _script_path;
}

CgiHandler::HeaderMap CgiHandler::getEnvironment() const
{
    return _environment;
}

std::string CgiHandler::getInput() const
{
    return _input;
}

std::string CgiHandler::getOutput() const
{
    return _output;
}

CgiHandler::HeaderMap CgiHandler::getResponseHeaders() const
{
    return _response_headers;
}

// ========================================
// Core Execution
// ========================================

void CgiHandler::execute()
{
    _EnvironmentBuilder env_builder;
    env_builder._build(_environment);

    _PipeSet pipes;
    pipes._createPipes();

    pid_t child_pid = fork();

    if (child_pid < 0)
    {
        pipes._closeAll();
        throw ForkFailed();
    }

    if (child_pid == 0)
    {
        _executeInChild(pipes, env_builder);
    }
    else
    {
        _executeInParent(child_pid, pipes);
    }
}

// ========================================
// Child Process Execution
// ========================================

void CgiHandler::_executeInChild(const _PipeSet& pipes, _EnvironmentBuilder& env)
{
    alarm(_timeout);
    _redirectChildIO(pipes);
    _executeCGIScript(env);
    exit(EXIT_CGI_FAILED);
}

void CgiHandler::_redirectChildIO(const _PipeSet& pipes)
{
    if (dup2(pipes.input_pipe[0], STDIN_FILENO) == -1)
        exit(EXIT_CGI_FAILED);

    if (dup2(pipes.output_pipe[1], STDOUT_FILENO) == -1)
        exit(EXIT_CGI_FAILED);

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

// ========================================
// Parent Process Execution
// ========================================

void CgiHandler::_executeInParent(pid_t child_pid, _PipeSet& pipes)
{
    pipes._setupForParent();

    _writeInputToChild(pipes.input_pipe[1]);
    close(pipes.input_pipe[1]);

    _readOutputFromChild(pipes.output_pipe[0], child_pid);
    close(pipes.output_pipe[0]);

    _waitForChildAndCheckStatus(child_pid);
    _parseOutput();
}

void CgiHandler::_writeInputToChild(int write_fd)
{
    if (_input.empty())
        return;

    ssize_t bytes_written = write(write_fd, _input.c_str(), _input.size());

    if (bytes_written < 0)
        throw WriteFailed(strerror(errno));

    if (static_cast<size_t>(bytes_written) != _input.size())
        throw WriteFailed("Incomplete write to CGI stdin");
}

void CgiHandler::_readOutputFromChild(int read_fd, pid_t child_pid)
{
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    _output.clear();

    while ((bytes_read = read(read_fd, buffer, BUFFER_SIZE)) > 0)
    {
        if (_output.size() + bytes_read > MAX_OUTPUT_SIZE)
        {
            _killAndWaitForChild(child_pid);
            throw OutputTooLarge();
        }
        _output.append(buffer, bytes_read);
    }

    if (bytes_read < 0)
    {
        _killAndWaitForChild(child_pid);
        throw ReadingFailed(strerror(errno));
    }
}

void CgiHandler::_waitForChildAndCheckStatus(pid_t child_pid)
{
    int status;
    waitpid(child_pid, &status, 0);

    if (WIFEXITED(status))
    {
        if (WEXITSTATUS(status) != 0)
            throw std::runtime_error("CGI script exited with non-zero status");
    }
    else if (WIFSIGNALED(status))
    {
        if (WTERMSIG(status) == SIGALRM)
            throw Timeout();

        throw std::runtime_error("CGI script killed by signal");
    }
}

// ========================================
// Output Parsing
// ========================================

void CgiHandler::_parseOutput()
{
    size_t header_end = _output.find("\r\n\r\n");
    size_t sep_len = 4;

    if (header_end == std::string::npos)
    {
        header_end = _output.find("\n\n");
        sep_len = 2;
    }

    if (header_end == std::string::npos)
        return;

    std::string headers = _output.substr(0, header_end);
    std::string body = _output.substr(header_end + sep_len);

    _parseHeaderLines(headers);
    _output = body;
}

void CgiHandler::_parseHeaderLines(const std::string& header_str)
{
    std::istringstream stream(header_str);
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

        _response_headers[key] = value;
    }
}

// ========================================
// Utility Methods
// ========================================

void CgiHandler::_cleanupFileDescriptors(int* fds, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (fds[i] != -1)
        {
            close(fds[i]);
            fds[i] = -1;
        }
    }
}

void CgiHandler::_killAndWaitForChild(pid_t child_pid)
{
    kill(child_pid, SIGKILL);
    waitpid(child_pid, NULL, 0);
}


/* #include "CgiHandler.hpp"
#include <iostream>
#include <vector>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sstream>
#include <cstdlib>
#include <signal.h>
#include <map>

// ========================================
// Constructor/Destructor/Setters/Getters
// ========================================

CgiHandler::CgiHandler() : _timeout(30) {}

CgiHandler::CgiHandler(const std::string& cgi_bin, const std::string& script_path) 
    : _cgi_bin(cgi_bin), _script_path(script_path), _timeout(30) {}

CgiHandler::~CgiHandler() {}

void CgiHandler::setCGI(const std::string& path) { _cgi_bin = path; }
void CgiHandler::setScript(const std::string& path) { _script_path = path; }
void CgiHandler::setEnv(const std::string& key, const std::string& value) { _env[key] = value; }
void CgiHandler::setInput(const std::string& str) { _input = str; }
void CgiHandler::setTimeout(unsigned int seconds) { _timeout = seconds; }

std::string CgiHandler::getCGI() const { return (_cgi_bin); }
std::string CgiHandler::getScript() const { return (_script_path); }
std::map<std::string, std::string> CgiHandler::getEnv() const { return (_env); }
std::string CgiHandler::getInput() const { return (_input); }
std::string CgiHandler::getOutput() const { return (_output); }
std::map<std::string, std::string> CgiHandler::getResponseHd() const { return (_response_hd); }


// ========================================
// Utility Methods
// ========================================

void CgiHandler::cleanFds(int *file_descriptors, size_t count)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (file_descriptors[i] != -1)
            close(file_descriptors[i]);
    }
}

void CgiHandler::parseOutput()
{
    size_t header_end = _output.find("\r\n\r\n");
    size_t sep_length = 4;

    if (header_end == std::string::npos)
    {
        header_end = _output.find("\n\n");
        sep_length = 2;
    }
    
    if (header_end != std::string::npos)
    {
        std::string hd_str = _output.substr(0, header_end);
        std::string body = _output.substr(header_end + sep_length);
        std::istringstream iss(hd_str);
        std::string line;
        
        while (std::getline(iss, line))
        {
            if (!line.empty() && line[line.length() - 1] == '\r')
                line.erase(line.length() -1);
            size_t two_points = line.find(':');
            if (two_points != std::string::npos)
            {
                std::string key = line.substr(0, two_points);
                std::string value = line.substr(two_points + 1);
                size_t start = value.find_first_not_of(" \t");
                if (start != std::string::npos)
                    value = value.substr(start);
                _response_hd[key] = value;
            }
        }
        _output = body;
    }
}


// ========================================
// Core Execution
// ========================================

void CgiHandler::execute()
{
    // 1. Prepare Environment Variables
    std::vector<std::string> env_str;
    for (std::map<std::string, std::string>::const_iterator ite = _env.begin(); ite != _env.end(); ++ite)
        env_str.push_back(ite->first + "=" + ite->second);

    std::vector<char *> envp;
    for (size_t i = 0; i < env_str.size(); ++i)
        envp.push_back(const_cast<char *>(env_str[i].c_str()));
    envp.push_back(NULL);

    // 2. Setup Pipes and Fork
    pid_t child_pid;
    int input_pipe[2];
    int output_pipe[2];
    char *argv[3];

    if (pipe(input_pipe) == -1)
        throw PipeFailed();
    if (pipe(output_pipe) == -1)
    {
        cleanFds(input_pipe, 2);
        throw PipeFailed();
    }
    
    child_pid = fork();
    if (child_pid < 0)
    {
        cleanFds(input_pipe, 2);
        cleanFds(output_pipe, 2);
        throw ForkFailed();
    }
    
    // 3. Child Process (Execution)
    if (child_pid == 0)
    {
        alarm(_timeout);
        
        if (dup2(input_pipe[0], STDIN_FILENO) == -1)
            exit(1);
        if (dup2(output_pipe[1], STDOUT_FILENO) == -1)
            exit(1);
        
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);

        argv[0] = const_cast<char *>(_cgi_bin.c_str());
        argv[1] = const_cast<char *>(_script_path.c_str());
        argv[2] = NULL;

        if (execve(_cgi_bin.c_str(), argv, &envp[0]) == -1)
            exit(1);
    }
    
    // 4. Parent Process (I/O Handling and Wait)
    else
    {
        close (input_pipe[0]);
        close (output_pipe[1]);

        // 4a. Write Input (POST Body)
        if (!_input.empty())
        {
            ssize_t written_bytes = write(input_pipe[1], _input.c_str(), _input.size());
            if (written_bytes < 0)
            {
                close (input_pipe[1]);
                close (output_pipe[0]);
                waitpid(child_pid, NULL, 0);
                throw WriteFailed();
            }
        }
        close(input_pipe[1]);

        // 4b. Read Output
        char temp_buffer[4096];
        ssize_t bytes_read;
        _output.clear();
        
        while ((bytes_read = read(output_pipe[0], temp_buffer, sizeof(temp_buffer))) > 0)
        {
            if (_output.size() + bytes_read > MAX_OUTPUT_SIZE)
            {
                close(output_pipe[0]);
                kill(child_pid, SIGKILL);
                waitpid(child_pid, NULL, 0);
                throw ReadingFailed();
            }
            _output.append(temp_buffer, bytes_read);
        }
        
        if (bytes_read < 0)
        {
            close(output_pipe[0]);
            kill(child_pid, SIGKILL);
            waitpid(child_pid, NULL, 0);
            throw ReadingFailed();
        }
        close (output_pipe[0]);

        // 4c. Wait for Child and Check Status
        int status;
        waitpid(child_pid, &status, 0);
        
        if (WIFEXITED(status))
        {
            int exit_code = WEXITSTATUS(status);
            if (exit_code != 0)
            {
                std::ostringstream oss;
                oss << "CGI exited with status " << exit_code;
                throw std::runtime_error(oss.str());
            }
        }
        else if (WIFSIGNALED(status))
        {
            int sig = WTERMSIG(status);
            if (sig == SIGALRM)
                throw Timeout();
            std::ostringstream oss;
            oss << "CGI killed by signal " << sig;
            throw std::runtime_error(oss.str());
        }
        
        // 5. Finalize Result
        parseOutput();
    }
} */