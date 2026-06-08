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

// Shared base for view-layer profiler sessions. Owns the profiler C API
// objects (config / session handle / future) and the AppMonitor operation that
// surfaces profiler state transitions as ProfilerStatusEvents. Subclasses
// implement Launch() with their own mechanism (local launch, remote SSH launch,
// or future multi-stage flows) while reusing the common config build, state /
// output accessors, monitor registration, and deferred teardown.
class ProfilerSessionBase
{
public:
    ProfilerSessionBase();
    virtual ~ProfilerSessionBase();

    ProfilerSessionBase(const ProfilerSessionBase&)            = delete;
    ProfilerSessionBase& operator=(const ProfilerSessionBase&) = delete;

    // Launches a profiler workflow asynchronously. The mechanism (local /
    // remote / multi-stage) is defined by the concrete subclass.
    virtual bool Launch(rocprofvis_profiler_type_t profiler_type,
                        const std::string&         profiler_path,
                        const std::string&         target_executable,
                        const std::string&         target_args,
                        const std::string&         output_directory,
                        const std::string&         profiler_args,
                        const std::vector<std::pair<std::string, std::string>>& env_vars = {}) = 0;

    virtual rocprofvis_profiler_state_t GetState() const;
    virtual std::string                 GetOutput();
    void                                ClearOutput();
    virtual std::string                 GetTracePath();
    int32_t                             GetExitCode() const;
    virtual bool                        Cancel();

    // Stops the status poller and frees all profiler objects (deferred,
    // non-blocking, if a worker is still using them). The session is reusable
    // afterwards. Does not affect any subclass-owned resources (e.g. an SSH
    // session).
    virtual void Close();

    // Monitor operation id for the in-flight profiler op, or 0 when not
    // monitored.
    uint64_t GetOperationId() const { return m_profiler_op_id; }

protected:
    // Allocates m_config and applies the common profiler settings. Returns
    // false if allocation fails. Subclasses may apply additional settings
    // (e.g. SSH connection details) after a successful return.
    bool BuildConfig(rocprofvis_profiler_type_t profiler_type,
                     const std::string&         profiler_path,
                     const std::string&         target_executable,
                     const std::string&         target_args,
                     const std::string&         output_directory,
                     const std::string&         profiler_args,
                     const std::vector<std::pair<std::string, std::string>>& env_vars);

    // Registers the profiler op with the AppMonitor (status poller reads the
    // live controller state; the factory emits ProfilerStatusEvents). Stores
    // and returns the assigned operation id.
    uint64_t RegisterProfilerMonitor();

    // Removes the profiler monitor op and frees m_config / m_profiler /
    // m_future. If the future is still pending, ownership is transferred to the
    // monitor for deferred, non-blocking teardown.
    void FreeProfilerObjects();

    // Reads the live controller profiler state (kRPVProfilerStateIdle if no
    // profiler handle yet).
    uint64_t ReadProfilerState() const;

    static bool IsTerminalProfilerState(uint64_t state);

    rocprofvis_profiler_config_t*   m_config;
    rocprofvis_profiler_t*          m_profiler;
    rocprofvis_controller_future_t* m_future;
    uint64_t                        m_profiler_op_id;
};

}  // namespace View
}  // namespace RocProfVis
