// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_profiler.h"

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

// View-layer owner of a single profiler launch. Wraps the profiler C API
// (config / session handle / future) and registers itself with the AppMonitor
// so its state transitions are surfaced as ProfilerStatusEvents rather than
// polled directly. Lives independently of any open trace.
class ProfilerSession
{
public:
    ProfilerSession();
    ~ProfilerSession();

    ProfilerSession(const ProfilerSession&)            = delete;
    ProfilerSession& operator=(const ProfilerSession&) = delete;

    // Launches a profiler process asynchronously. On success the session is
    // registered with the AppMonitor and ProfilerStatusEvents are emitted as
    // the state changes. Any previous launch on this session is closed first.
    bool Launch(rocprofvis_profiler_type_t profiler_type,
                const std::string&         profiler_path,
                const std::string&         target_executable,
                const std::string&         target_args,
                const std::string&         output_directory,
                const std::string&         profiler_args,
                const std::vector<std::pair<std::string, std::string>>& env_vars = {});

    rocprofvis_profiler_state_t GetState() const;
    std::string                 GetOutput();
    void                        ClearOutput();
    std::string                 GetTracePath();
    int32_t                     GetExitCode() const;
    bool                        Cancel();

    // Terminates any running process, drains the future, and frees all
    // profiler objects. The session is reusable afterwards.
    void Close();

    // Monitor operation id for the in-flight launch, or 0 when not monitored.
    uint64_t GetOperationId() const { return m_operation_id; }

private:
    rocprofvis_profiler_config_t*   m_config;
    rocprofvis_profiler_t*          m_profiler;
    rocprofvis_controller_future_t* m_future;
    uint64_t                        m_operation_id;
};

}  // namespace View
}  // namespace RocProfVis
