#include "CgiHandler.hpp"
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
}