// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_process_executor.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_job_system.h"
#include "rocprofvis_core_assert.h"

#include <spdlog/spdlog.h>

#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <sstream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#define popen _popen
#define pclose _pclose
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#endif

namespace RocProfVis
{
namespace Controller
{

namespace
{
    // Maximum output buffer size (10 MB)
    constexpr size_t kMaxOutputSize = 10 * 1024 * 1024;

    // Read buffer size for pipe reading
    constexpr size_t kReadBufferSize = 4096;
}

std::shared_ptr<ProcessExecutor::AsyncContext> ProcessExecutor::ExecuteAsync(
    const std::string& command,
    const ExecutionOptions& options,
    std::function<void(const ExecutionResult&)> completion_callback)
{
    auto context = std::make_shared<AsyncContext>();
    context->result = std::make_shared<ExecutionResult>();
    context->completion_callback = completion_callback;

    // Create Future and store as opaque pointer
    Future* future_ptr = new Future();
    context->internal_future = static_cast<void*>(future_ptr);

    // Capture context by value to keep it alive
    auto job_function = [command, options, context](Future* fut) -> rocprofvis_result_t
    {
        if(fut->IsCancelled())
        {
            context->result->cancelled = true;
            context->result->error_message = "Execution cancelled";
            return kRocProfVisResultCancelled;
        }

        // Execute command and store result
        *context->result = ExecuteInternal(command, options);

        // Call completion callback if provided
        if(context->completion_callback)
        {
            context->completion_callback(*context->result);
        }

        return context->result->exit_code == 0 ? kRocProfVisResultSuccess : kRocProfVisResultUnknownError;
    };

    Job* job = JobSystem::Get().IssueJob(job_function, future_ptr);
    future_ptr->Set(job);

    return context;
}

std::shared_ptr<ProcessExecutor::AsyncContext> ProcessExecutor::ExecuteRemoteAsync(
    const std::string& ssh_host,
    const std::string& command,
    const ExecutionOptions& options,
    int ssh_port,
    std::function<void(const ExecutionResult&)> completion_callback,
    const std::string& ssh_key_path,
    const std::string& ssh_proxy_jump,
    const std::string& ssh_options)
{
    std::string ssh_command = BuildSSHCommand(ssh_host, command, ssh_port, ssh_key_path, ssh_proxy_jump, ssh_options);
    return ExecuteAsync(ssh_command, options, completion_callback);
}

ProcessExecutor::ExecutionResult ProcessExecutor::ExecuteSync(
    const std::string& command,
    const ExecutionOptions& options)
{
    return ExecuteInternal(command, options);
}

ProcessExecutor::ExecutionResult ProcessExecutor::ExecuteInternal(
    const std::string& command,
    const ExecutionOptions& options)
{
    ExecutionResult result;

    try
    {
        spdlog::debug("Executing command: {}", command);

        // Build full command with environment and working directory
        std::string full_command = command;

        // Set working directory if specified (using cd for cross-platform compatibility)
        if(!options.working_directory.empty())
        {
#ifdef _WIN32
            full_command = "cd /d \"" + options.working_directory + "\" && " + full_command;
#else
            full_command = "cd \"" + options.working_directory + "\" && " + full_command;
#endif
        }

        // Add environment variables
        for(const auto& [key, value] : options.environment)
        {
#ifdef _WIN32
            full_command = "set " + key + "=" + value + " && " + full_command;
#else
            full_command = "export " + key + "=\"" + value + "\" && " + full_command;
#endif
        }

        // Redirect stderr to stdout to capture both
        if(options.capture_output)
        {
            full_command += " 2>&1";
        }

        // Execute command
        FILE* pipe = popen(full_command.c_str(), "r");
        if(!pipe)
        {
            result.error_message = "Failed to open pipe for command execution";
            spdlog::error("{}", result.error_message);
            return result;
        }

        // Read output
        std::array<char, kReadBufferSize> buffer;
        auto start_time = std::chrono::steady_clock::now();

        while(fgets(buffer.data(), buffer.size(), pipe) != nullptr)
        {
            std::string line(buffer.data());

            // Check output size limit
            if(result.stdout_output.size() + line.size() > kMaxOutputSize)
            {
                spdlog::warn("Command output exceeded maximum size, truncating");
                result.stdout_output += "\n[Output truncated - exceeded maximum size]\n";
                break;
            }

            result.stdout_output += line;

            // Call output callback if provided (for real-time streaming)
            if(options.output_callback)
            {
                options.output_callback(line);
            }

            // Check timeout
            if(options.timeout_seconds > 0)
            {
                auto elapsed = std::chrono::steady_clock::now() - start_time;
                auto elapsed_seconds = std::chrono::duration_cast<std::chrono::seconds>(elapsed).count();

                if(elapsed_seconds >= options.timeout_seconds)
                {
                    result.timed_out = true;
                    result.error_message = "Command execution timed out after " +
                                          std::to_string(options.timeout_seconds) + " seconds";
                    spdlog::warn("{}", result.error_message);

                    // Try to close the pipe
                    pclose(pipe);
                    return result;
                }
            }
        }

        // Get exit code
        int status = pclose(pipe);

#ifdef _WIN32
        result.exit_code = status;
#else
        if(WIFEXITED(status))
        {
            result.exit_code = WEXITSTATUS(status);
        }
        else if(WIFSIGNALED(status))
        {
            result.exit_code = -WTERMSIG(status);
            result.error_message = "Process terminated by signal " + std::to_string(WTERMSIG(status));
        }
        else
        {
            result.exit_code = -1;
            result.error_message = "Process terminated abnormally";
        }
#endif

        if(result.exit_code != 0 && result.error_message.empty())
        {
            result.error_message = "Command failed with exit code " + std::to_string(result.exit_code);
        }

        spdlog::debug("Command completed with exit code: {}", result.exit_code);
    }
    catch(const std::exception& e)
    {
        result.exit_code = -1;
        result.error_message = std::string("Exception during command execution: ") + e.what();
        spdlog::error("{}", result.error_message);
    }

    return result;
}

std::string ProcessExecutor::BuildSSHCommand(
    const std::string& ssh_host,
    const std::string& command,
    int ssh_port,
    const std::string& ssh_key_path,
    const std::string& ssh_proxy_jump,
    const std::string& ssh_options)
{
    std::ostringstream ssh_cmd;
    ssh_cmd << "ssh";

    // Add port if not default
    if(ssh_port != 22)
    {
        ssh_cmd << " -p " << ssh_port;
    }

    // Add SSH key if specified
    if(!ssh_key_path.empty())
    {
        ssh_cmd << " -i \"" << ssh_key_path << "\"";
    }

    // Add ProxyJump/bastion host if specified
    if(!ssh_proxy_jump.empty())
    {
        ssh_cmd << " -J \"" << ssh_proxy_jump << "\"";
    }

    // Add custom SSH options if specified
    if(!ssh_options.empty())
    {
        ssh_cmd << " " << ssh_options;
    }

    // Add common options
    ssh_cmd << " -o BatchMode=yes";  // Don't prompt for passwords
    ssh_cmd << " -o StrictHostKeyChecking=no";  // Don't prompt for host key verification (user should have keys set up)

    // Add host
    ssh_cmd << " " << ssh_host;

    // Wrap command to source user environment and ensure ROCm paths are in PATH and PYTHONPATH
    // This ensures non-interactive SSH sessions have access to ROCm tools and rocpd Python packages
    std::string wrapped_command = "bash -l -c '";  // -l makes it a login shell, sources .bash_profile/.profile
    wrapped_command += "export PATH=/opt/rocm/bin:/opt/rocm/libexec/rocprofiler:$PATH; ";

    // Set PYTHONPATH for rocpd (tries common Python versions)
    wrapped_command += "for pyver in 3.12 3.11 3.10 3.9 3.8; do "
                       "if [ -d /opt/rocm/lib/python$pyver/site-packages ]; then "
                       "export PYTHONPATH=/opt/rocm/lib/python$pyver/site-packages:$PYTHONPATH; "
                       "break; "
                       "fi; "
                       "done; ";

    wrapped_command += command;
    wrapped_command += "'";

    // Add command (properly escaped for SSH)
    ssh_cmd << " \"" << wrapped_command << "\"";

    return ssh_cmd.str();
}

std::string ProcessExecutor::EscapeShellArgument(const std::string& arg)
{
    std::string escaped;
    escaped.reserve(arg.size() * 2);

    for(char c : arg)
    {
        // Escape single quotes by ending the quote, adding escaped quote, and starting new quote
        if(c == '\'')
        {
            escaped += "'\\''";
        }
        else
        {
            escaped += c;
        }
    }

    return escaped;
}

std::optional<std::string> ProcessExecutor::FindROCmTool(const std::string& tool_name)
{
    // First try to find in PATH
    auto exe_path = FindExecutableInPath(tool_name);
    if(exe_path)
    {
        return exe_path;
    }

    // Try common ROCm installation paths
    std::vector<std::string> rocm_paths = {
        "/opt/rocm/bin",
        "/opt/rocm/libexec/rocprofiler",
        "/usr/local/rocm/bin",
        "/usr/local/rocm/libexec/rocprofiler"
    };

    for(const auto& base_path : rocm_paths)
    {
        std::filesystem::path full_path = std::filesystem::path(base_path) / tool_name;

        if(std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path))
        {
            spdlog::debug("Found ROCm tool {} at {}", tool_name, full_path.string());
            return full_path.string();
        }
    }

    spdlog::warn("ROCm tool {} not found in PATH or common ROCm directories", tool_name);
    return std::nullopt;
}

void ProcessExecutor::AsyncContext::Cancel()
{
    if(internal_future)
    {
        Future* future_ptr = static_cast<Future*>(internal_future);
        future_ptr->Cancel();
    }
}

std::optional<std::string> ProcessExecutor::FindExecutableInPath(const std::string& exe_name)
{
    // Get PATH environment variable
    const char* path_env = std::getenv("PATH");
    if(!path_env)
    {
        return std::nullopt;
    }

    std::string path_str(path_env);

#ifdef _WIN32
    const char path_separator = ';';
    std::string exe_with_ext = exe_name;

    // Add .exe extension if not present on Windows
    if(exe_name.find(".exe") == std::string::npos &&
       exe_name.find(".bat") == std::string::npos &&
       exe_name.find(".cmd") == std::string::npos)
    {
        exe_with_ext += ".exe";
    }
#else
    const char path_separator = ':';
    const std::string& exe_with_ext = exe_name;
#endif

    // Split PATH and search each directory
    std::istringstream path_stream(path_str);
    std::string path_entry;

    while(std::getline(path_stream, path_entry, path_separator))
    {
        if(path_entry.empty())
        {
            continue;
        }

        std::filesystem::path full_path = std::filesystem::path(path_entry) / exe_with_ext;

        if(std::filesystem::exists(full_path) && std::filesystem::is_regular_file(full_path))
        {
            spdlog::debug("Found executable {} at {}", exe_name, full_path.string());
            return full_path.string();
        }
    }

    return std::nullopt;
}

}
}
