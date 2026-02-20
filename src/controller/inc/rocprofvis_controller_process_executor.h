// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <map>
#include <optional>
#include <functional>
#include <memory>

namespace RocProfVis
{
namespace Controller
{

class ProcessExecutor
{
public:
    struct ExecutionOptions
    {
        std::string working_directory;
        std::map<std::string, std::string> environment;
        bool capture_output = true;
        int timeout_seconds = 0;  // 0 = no timeout
        std::function<void(const std::string&)> output_callback;  // Real-time output streaming
    };

    struct ExecutionResult
    {
        int exit_code = -1;
        std::string stdout_output;
        std::string stderr_output;
        bool timed_out = false;
        bool cancelled = false;
        std::string error_message;
    };

    // Async execution context (holds result pointer for checking status)
    struct AsyncContext
    {
        std::shared_ptr<ExecutionResult> result;
        std::function<void(const ExecutionResult&)> completion_callback;
        void* internal_future = nullptr;  // Opaque pointer to internal Future object

        // Cancel the async execution
        void Cancel();
    };

    // Local execution - returns context with Future and result pointer
    static std::shared_ptr<AsyncContext> ExecuteAsync(
        const std::string& command,
        const ExecutionOptions& options = {},
        std::function<void(const ExecutionResult&)> completion_callback = nullptr);

    // Remote execution via SSH - returns context with Future and result pointer
    static std::shared_ptr<AsyncContext> ExecuteRemoteAsync(
        const std::string& ssh_host,
        const std::string& command,
        const ExecutionOptions& options = {},
        int ssh_port = 22,
        std::function<void(const ExecutionResult&)> completion_callback = nullptr,
        const std::string& ssh_key_path = "",
        const std::string& ssh_proxy_jump = "",
        const std::string& ssh_options = "");

    // Synchronous local execution (for simple cases)
    static ExecutionResult ExecuteSync(
        const std::string& command,
        const ExecutionOptions& options = {});

    // Find ROCm tool path (searches PATH and /opt/rocm)
    static std::optional<std::string> FindROCmTool(const std::string& tool_name);

    // Find executable in PATH
    static std::optional<std::string> FindExecutableInPath(const std::string& exe_name);

private:
    static ExecutionResult ExecuteInternal(
        const std::string& command,
        const ExecutionOptions& options);

    static std::string BuildSSHCommand(
        const std::string& ssh_host,
        const std::string& command,
        int ssh_port,
        const std::string& ssh_key_path = "",
        const std::string& ssh_proxy_jump = "",
        const std::string& ssh_options = "");

    static std::string EscapeShellArgument(const std::string& arg);
};

}
}
