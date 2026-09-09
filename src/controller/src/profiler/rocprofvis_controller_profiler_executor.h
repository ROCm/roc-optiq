// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <vector>
#include <utility>

namespace RocProfVis
{
namespace Controller
{

class ProfilerConfig;

/**
 * Outcome of asking an executor to stop its process. A bool cannot express the
 * third case, and conflating it with the second is what makes a run that is
 * still going look like one that ended.
 */
enum class CancelOutcome
{
    // Signalled and reaped; the exit code is final.
    kStopped,
    // Nothing to stop: the process had already exited, and any status it left
    // has been collected.
    kNotRunning,
    // The OS refused (EPERM, a sandbox policy, ERROR_ACCESS_DENIED). The
    // process is still running and the executor still tracks it, so the caller
    // must not report it as having ended.
    kRefused,
};

/**
 * Abstract interface for profiler process execution.
 * LocalProfilerExecutor is the only implementation in this phase.
 * SshProfilerExecutor will be added in a future phase.
 */
class IProfilerExecutor
{
public:
    virtual ~IProfilerExecutor() = default;

    virtual bool Start(const ProfilerConfig& config) = 0;

    virtual bool IsRunning() = 0;

    virtual std::string ReadOutput() = 0;

    virtual int GetExitCode() const = 0;

    virtual CancelOutcome Cancel() = 0;

    /**
     * Whether the run's future must stay unresolved until this executor is
     * finished - not "is a process still alive", but "would tearing down now
     * pull something out from under it".
     *
     * Defaults to IsRunning() so an executor whose worker thread borrows
     * caller-owned state is safe without opting in; one that owns everything it
     * touches overrides this to false.
     */
    virtual bool HasPendingTeardown() { return IsRunning(); }

    /**
     * Fetch a remote artifact to a local path.
     * No-op for local execution; future SshProfilerExecutor will scp/rsync.
     */
    virtual bool FetchArtifact(const std::string& /*remote_path*/,
                               const std::string& /*local_path*/)
    {
        return true;
    }
};

} // namespace Controller
} // namespace RocProfVis
