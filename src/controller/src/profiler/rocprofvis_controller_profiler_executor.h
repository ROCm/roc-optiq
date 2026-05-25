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

    virtual bool Cancel() = 0;

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
