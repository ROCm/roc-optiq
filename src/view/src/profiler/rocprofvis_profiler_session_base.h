// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_profiler.h"

#include <cstdint>
#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Everything needed to configure one profiler process launch. These are grouped
// into a struct rather than passed positionally because most of them are
// strings: a transposed pair would compile cleanly and silently launch the wrong
// command.
struct ProfilerLaunchSpec
{
    // Which binary to execute. Named rather than pathed so that argv[0] is
    // decided by the controller's tool table, not by any string this struct
    // carries; the controller resolves it to an absolute path at launch.
    rocprofvis_profiler_tool_t tool = kRPVProfilerToolNone;

    // Directory to find that tool in (LaunchConfig::tool_directory), for a ROCm
    // install in a non-standard location. Empty uses the default search.
    std::string tool_directory;

    // Where the profiler is expected to write its output. This does not put
    // anything on the command line - the output path already appears in
    // profiler_argv wherever the profiler's CLI expects it - and the controller
    // has no reader for it yet; see
    // rocprofvis_profiler_config_set_output_directory for why it is passed.
    std::string output_directory;

    // Directory to run the profiler process in. Empty inherits Optiq's own
    // working directory. Only the child is affected.
    std::string working_directory;

    // The complete argument list following argv[0], as emitted by
    // IProfilerBackend::FlattenToExecution. Each entry becomes one argv entry
    // verbatim - no whitespace splitting, no shell interpretation.
    std::vector<std::string> profiler_argv;

    std::vector<std::pair<std::string, std::string>> env_vars;
};

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
    virtual bool Launch(const ProfilerLaunchSpec& spec) = 0;

    virtual rocprofvis_profiler_state_t GetState() const;
    virtual std::string                 GetOutput();
    void                                ClearOutput();
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
    bool BuildConfig(const ProfilerLaunchSpec& spec);

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

    // Optional teardown for a subclass-owned resource that MUST outlive an
    // in-flight profiler worker (e.g. a borrowed SSH connection the remote exec
    // streams over). FreeProfilerObjects runs it AFTER the profiler is freed
    // (i.e. after the worker is joined), in both the immediate and deferred
    // paths. Must be copyable (stored in std::function): capture owned resources
    // via shared_ptr, not unique_ptr.
    std::function<void()>           m_extra_teardown;

    rocprofvis_profiler_config_t*   m_config;
    rocprofvis_profiler_t*          m_profiler;
    rocprofvis_controller_future_t* m_future;
    uint64_t                        m_profiler_op_id;
};

}  // namespace View
}  // namespace RocProfVis
