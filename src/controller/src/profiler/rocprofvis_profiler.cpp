// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Windows: ensure NOMINMAX is defined before any header drags in windows.h
// (via winsock2.h in the SSH client header), so std::min/std::max are not
// shadowed by the min/max macros.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "rocprofvis_profiler.h"
// Include the SSH client header (which pulls winsock2.h) BEFORE the profiler
// process header (which pulls windows.h) so the winsock/windows include order
// is correct on Windows.
#include "remote/rocprofvis_controller_ssh_client.h"
#include "rocprofvis_controller_profiler_process.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_job_system.h"

#include <algorithm>
#include <cstring>

namespace RocProfVis
{
namespace Controller
{
typedef Reference<rocprofvis_controller_future_t, Future, kRPVControllerObjectTypeFuture> FutureRef;
typedef Reference<rocprofvis_profiler_config_t, ProfilerConfig, kRPVProfilerConfig> ProfilerConfigRef;
typedef Reference<rocprofvis_profiler_t, ProfilerSession, kRPVProfiler> ProfilerSessionRef;
typedef Reference<rocprofvis_controller_connection_t, SshConnection, kRPVControllerObjectTypeRemoteConnection> ConnectionRef;

// Copies a std::string into the caller's buffer using the standard
// "pass null buffer to query length" idiom shared by every string getter
// in the profiler C API.
//   - If buffer is null or *length is 0: writes the required size (excluding
//     the null terminator) into *length and returns success.
//   - Otherwise copies up to *length bytes (including a null terminator when
//     it fits) and returns success.
static rocprofvis_result_t copy_string_to_buffer(std::string const& src, char* buffer, uint32_t* length)
{
    if (length == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    if (buffer == nullptr || *length == 0)
    {
        *length = static_cast<uint32_t>(src.size());
        return kRocProfVisResultSuccess;
    }

    uint32_t copy_len = std::min(*length, static_cast<uint32_t>(src.size() + 1));
    std::strncpy(buffer, src.c_str(), copy_len);
    return kRocProfVisResultSuccess;
}

}
}

extern "C"
{

rocprofvis_profiler_config_t* rocprofvis_profiler_config_alloc(void)
{
    RocProfVis::Controller::ProfilerConfig* config = new RocProfVis::Controller::ProfilerConfig();
    return (rocprofvis_profiler_config_t*)config;
}

void rocprofvis_profiler_config_free(rocprofvis_profiler_config_t* config)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (config_ref.IsValid())
    {
        delete config_ref.Get();
    }
}

rocprofvis_result_t rocprofvis_profiler_config_set_type(rocprofvis_profiler_config_t* config, rocprofvis_profiler_type_t profiler_type)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->SetProfilerType(profiler_type);
}

rocprofvis_result_t rocprofvis_profiler_config_set_profiler_path(rocprofvis_profiler_config_t* config, char const* profiler_path)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->SetProfilerPath(profiler_path);
}

rocprofvis_result_t rocprofvis_profiler_config_set_target_executable(rocprofvis_profiler_config_t* config, char const* target_executable)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->SetTargetExecutable(target_executable);
}

rocprofvis_result_t rocprofvis_profiler_config_set_target_args(rocprofvis_profiler_config_t* config, char const* target_args)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->SetTargetArgs(target_args);
}

rocprofvis_result_t rocprofvis_profiler_config_set_profiler_args(rocprofvis_profiler_config_t* config, char const* profiler_args)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->SetProfilerArgs(profiler_args);
}

rocprofvis_result_t rocprofvis_profiler_config_set_output_directory(rocprofvis_profiler_config_t* config, char const* output_directory)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->SetOutputDirectory(output_directory);
}

rocprofvis_result_t rocprofvis_profiler_config_add_env_var(rocprofvis_profiler_config_t* config, char const* name, char const* value)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->AddEnvVar(name, value);
}

rocprofvis_result_t rocprofvis_profiler_config_add_profiler_arg(rocprofvis_profiler_config_t* config, char const* arg)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->AddProfilerArg(arg);
}

rocprofvis_result_t rocprofvis_profiler_config_set_connection_local(rocprofvis_profiler_config_t* config)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->SetConnectionLocal();
}

rocprofvis_result_t rocprofvis_profiler_config_set_connection_ssh(rocprofvis_profiler_config_t* config,
    char const* host, char const* user, int port, char const* identity_file, char const* remote_stage_dir)
{
    RocProfVis::Controller::ProfilerConfigRef config_ref(config);
    if (!config_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    return config_ref->SetConnectionSsh(host, user, port, identity_file, remote_stage_dir);
}

rocprofvis_profiler_t* rocprofvis_profiler_alloc(void)
{
    RocProfVis::Controller::ProfilerSession* session = new RocProfVis::Controller::ProfilerSession();
    return (rocprofvis_profiler_t*)session;
}

void rocprofvis_profiler_free(rocprofvis_profiler_t* profiler)
{
    RocProfVis::Controller::ProfilerSessionRef session_ref(profiler);
    if (session_ref.IsValid())
    {
        delete session_ref.Get();
    }
}

rocprofvis_result_t rocprofvis_profiler_launch_async(rocprofvis_profiler_t* profiler, rocprofvis_profiler_config_t* config, rocprofvis_controller_future_t* future)
{
    RocProfVis::Controller::ProfilerSessionRef session_ref(profiler);
    RocProfVis::Controller::ProfilerConfigRef  config_ref(config);
    RocProfVis::Controller::FutureRef          future_ref(future);
    if (!session_ref.IsValid() || !config_ref.IsValid() || !future_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerProcessController& controller = session_ref->GetController();

    rocprofvis_result_t result = controller.LaunchAsync(config_ref.Get());
    if (result != kRocProfVisResultSuccess)
    {
        return result;
    }

    session_ref->SetBoundFuture(future_ref.Get());

    RocProfVis::Controller::ProfilerProcessController* controller_ptr = &controller;
    RocProfVis::Controller::Job* job = RocProfVis::Controller::JobSystem::Get().IssueJob(
        [controller_ptr](RocProfVis::Controller::Future* job_future) -> rocprofvis_result_t
        {
            RocProfVis::Controller::ProfilerProcessController::ExecuteJob(controller_ptr, job_future);
            return kRocProfVisResultSuccess;
        },
        future_ref.Get());
    future_ref->Set(job);

    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_launch_remote_async(rocprofvis_profiler_t* profiler, rocprofvis_profiler_config_t* config, rocprofvis_controller_connection_t* connection, rocprofvis_controller_future_t* future)
{
    RocProfVis::Controller::ProfilerSessionRef session_ref(profiler);
    RocProfVis::Controller::ProfilerConfigRef  config_ref(config);
    RocProfVis::Controller::ConnectionRef       connection_ref(connection);
    RocProfVis::Controller::FutureRef           future_ref(future);
    if (!session_ref.IsValid() || !config_ref.IsValid() || !connection_ref.IsValid() || !future_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerProcessController& controller = session_ref->GetController();

    // Pass the bound future so the remote exec loop can observe cancellation.
    rocprofvis_result_t result = controller.LaunchAsyncRemote(config_ref.Get(), connection_ref.Get(), future_ref.Get());
    if (result != kRocProfVisResultSuccess)
    {
        return result;
    }

    session_ref->SetBoundFuture(future_ref.Get());

    RocProfVis::Controller::ProfilerProcessController* controller_ptr = &controller;
    RocProfVis::Controller::Job* job = RocProfVis::Controller::JobSystem::Get().IssueJob(
        [controller_ptr](RocProfVis::Controller::Future* job_future) -> rocprofvis_result_t
        {
            RocProfVis::Controller::ProfilerProcessController::ExecuteJob(controller_ptr, job_future);
            return kRocProfVisResultSuccess;
        },
        future_ref.Get());
    future_ref->Set(job);

    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_state(rocprofvis_profiler_t* profiler, rocprofvis_profiler_state_t* state)
{
    RocProfVis::Controller::ProfilerSessionRef session_ref(profiler);
    if (!session_ref.IsValid() || state == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    *state = session_ref->GetController().GetState();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_output(rocprofvis_profiler_t* profiler, char* buffer, uint32_t* length)
{
    RocProfVis::Controller::ProfilerSessionRef session_ref(profiler);
    if (!session_ref.IsValid() || length == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    return RocProfVis::Controller::copy_string_to_buffer(
        session_ref->GetController().GetOutput(), buffer, length);
}

rocprofvis_result_t rocprofvis_profiler_clear_output(rocprofvis_profiler_t* profiler)
{
    RocProfVis::Controller::ProfilerSessionRef session_ref(profiler);
    if (!session_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    session_ref->GetController().ClearOutput();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_trace_path(rocprofvis_profiler_t* profiler, char* buffer, uint32_t* length)
{
    RocProfVis::Controller::ProfilerSessionRef session_ref(profiler);
    if (!session_ref.IsValid() || length == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    return RocProfVis::Controller::copy_string_to_buffer(
        session_ref->GetController().GetTracePath(), buffer, length);
}

rocprofvis_result_t rocprofvis_profiler_get_exit_code(rocprofvis_profiler_t* profiler, int32_t* exit_code)
{
    RocProfVis::Controller::ProfilerSessionRef session_ref(profiler);
    if (!session_ref.IsValid() || exit_code == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    *exit_code = session_ref->GetController().GetExitCode();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_cancel(rocprofvis_profiler_t* profiler)
{
    RocProfVis::Controller::ProfilerSessionRef session_ref(profiler);
    if (!session_ref.IsValid())
    {
        return kRocProfVisResultInvalidArgument;
    }

    session_ref->GetController().Cancel();

    // Forward cancellation to the bound future so any waiter (e.g. job system
    // wait, view-side future_wait) unblocks promptly.
    if (RocProfVis::Controller::Future* bound = session_ref->GetBoundFuture())
    {
        bound->Cancel();
    }

    return kRocProfVisResultSuccess;
}

}
