// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_profiler_executor.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace RocProfVis
{
namespace Controller
{

class ProfilerConfig;
class SshConnection;
class Future;

/*
 * SshProfilerExecutor - Runs the profiler command on a remote host over an
 * already-connected, authenticated SSH connection (owned by the View). Streams
 * stdout/stderr back through the connection's SshBridge and reports running
 * state + exit code. The connection is BORROWED; this executor never frees it.
 *
 * Only the remote exec phase lives here; downloading the produced trace back is
 * driven by the View (so it can show progress / prompt on overwrite).
 *
 * This class lives in its own translation unit so the libssh2 / winsock headers
 * pulled in by the SSH client stay isolated from the profiler process code.
 *
 * NOTE (remote OS): the remote host is assumed to be POSIX, which holds today
 * because the ROCm profilers ship on Linux only. If they gain Windows support,
 * driving a Windows remote takes more than selecting a different tool: the
 * command is serialized by Cmdline::ToPosixShellCommand, which single-quotes
 * every token and prefixes "cd '<dir>' &&" for /bin/sh, and cmd.exe or
 * PowerShell would not treat those quotes as quoting at all.
 * ProfilerConfig::ResolveToolPathRemote requires a leading '/' and joins with
 * '/', and the launcher's remote command preview reproduces that same
 * composition, so those three have to be reworked together or the preview and
 * the executed command stop matching.
 */
class SshProfilerExecutor : public IProfilerExecutor
{
public:
    // connection and future are both borrowed (owned by the View / ABI caller).
    // future is observed by the remote exec loop for cancellation; may be null.
    SshProfilerExecutor(SshConnection* connection, Future* future);
    ~SshProfilerExecutor() override;

    bool Start(const ProfilerConfig& config) override;
    bool IsRunning() override;
    std::string ReadOutput() override;
    int GetExitCode() const override;
    bool Cancel() override;

private:
    // Pulls any newly-streamed stdout out of the connection's SshBridge.
    std::string DrainBridgeOutput();

    SshConnection*    m_connection;   // borrowed; not owned
    Future*           m_future;       // borrowed; cancellation source (may be null)
    std::thread       m_worker;
    std::atomic<bool> m_is_running;
    std::atomic<int>  m_exit_code;
    std::mutex        m_output_mutex;
    std::string       m_output_buffer;
};

} // namespace Controller
} // namespace RocProfVis
