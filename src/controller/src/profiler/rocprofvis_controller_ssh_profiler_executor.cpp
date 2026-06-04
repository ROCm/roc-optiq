// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Windows: NOMINMAX before any header that drags in windows.h (via winsock2.h).
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

// Include the SSH client header (which pulls winsock2.h) FIRST so the
// winsock2/windows include order is correct on Windows (profiler_process.h
// pulls windows.h, which would otherwise drag in the legacy winsock.h).
#include "remote/rocprofvis_controller_ssh_client.h"
#include "remote/rocprofvis_controller_ssh_bridge.h"

#include "rocprofvis_controller_ssh_profiler_executor.h"
#include "rocprofvis_controller_profiler_process.h"
#include "rocprofvis_controller_profiler_cmdline.h"

#include "spdlog/spdlog.h"

#include <vector>

namespace RocProfVis
{
namespace Controller
{

SshProfilerExecutor::SshProfilerExecutor(SshConnection* connection, Future* future)
    : m_connection(connection)
    , m_future(future)
    , m_is_running(false)
    , m_exit_code(-1)
{
}

SshProfilerExecutor::~SshProfilerExecutor()
{
    // The remote exec loop observes future cancellation; by the time we get
    // here the controller has already cancelled/joined the monitor job, but
    // join defensively so the worker thread never outlives this object.
    if (m_worker.joinable())
    {
        m_worker.join();
    }
}

bool SshProfilerExecutor::Start(const ProfilerConfig& config)
{
    if (m_connection == nullptr)
    {
        spdlog::error("SshProfilerExecutor::Start: null connection");
        return false;
    }

    // Compose the remote command from the canonical argv/env schema, serialized
    // for a POSIX shell (the remote host).
    std::vector<std::string> argv = Cmdline::BuildArgv(config);
    std::vector<std::pair<std::string, std::string>> env = Cmdline::BuildEnv(config);
    std::string remote_cmd = Cmdline::ToPosixShellCommand(argv, env);

    spdlog::info("SSH profiler launch: {}", Cmdline::ToDisplayString(argv, env));

    m_is_running = true;
    m_exit_code  = -1;

    SshConnection* connection = m_connection;
    Future*        future     = m_future;
    m_worker = std::thread([this, connection, future, remote_cmd]()
    {
        // ExecuteCommand streams stdout/stderr into the connection's bridge and
        // blocks until the remote channel closes (or the bound future is
        // cancelled - the exec loop polls future->IsCancelled()). On a clean
        // run it writes the remote process's exit status into `remote_exit`.
        int               remote_exit = -1;
        SshClient::Result result =
            SshClient::ExecuteCommand(connection, remote_cmd, future, &remote_exit);

        if (result == SshClient::Result::Success)
        {
            // SSH transport succeeded: the remote profiler's own exit code is
            // the success/failure key (0 == success). UpdateState() maps a
            // zero exit to Completed, non-zero to Failed.
            m_exit_code = remote_exit;
        }
        else
        {
            // SSH transport error or cancellation: surface as a failure with a
            // non-zero exit code (use the remote exit if it was captured).
            m_exit_code = (remote_exit != 0) ? remote_exit : 1;
        }
        m_is_running = false;
    });

    return true;
}

bool SshProfilerExecutor::IsRunning()
{
    return m_is_running.load();
}

std::string SshProfilerExecutor::DrainBridgeOutput()
{
    if (m_connection == nullptr)
    {
        return "";
    }

    SshBridge* bridge = m_connection->GetSshBridge();
    if (bridge == nullptr)
    {
        return "";
    }

    uint32_t length = 0;
    bridge->GetString(kRPVControllerRemoteExecuteStdOut, 0, nullptr, &length);
    if (length == 0)
    {
        return "";
    }

    std::vector<char> buffer(length + 1, '\0');
    rocprofvis_result_t result =
        bridge->GetString(kRPVControllerRemoteExecuteStdOut, 0, buffer.data(), &length);
    if (result != kRocProfVisResultSuccess)
    {
        return "";
    }

    buffer[length] = '\0';
    return std::string(buffer.data());
}

std::string SshProfilerExecutor::ReadOutput()
{
    std::lock_guard<std::mutex> lock(m_output_mutex);
    std::string chunk = DrainBridgeOutput();
    if (!chunk.empty())
    {
        m_output_buffer += chunk;
    }
    return chunk;
}

int SshProfilerExecutor::GetExitCode() const
{
    return m_exit_code.load();
}

bool SshProfilerExecutor::Cancel()
{
    if (m_connection == nullptr)
    {
        return false;
    }

    // Signal the exec loop (via the bridge) to stop; the worker thread will
    // unwind and clear m_is_running.
    if (SshBridge* bridge = m_connection->GetSshBridge())
    {
        bridge->Cancel();
    }
    m_is_running = false;
    m_exit_code  = 1;
    return true;
}

} // namespace Controller
} // namespace RocProfVis
