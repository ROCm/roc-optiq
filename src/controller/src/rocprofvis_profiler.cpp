// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_profiler.h"
#include "rocprofvis_controller_profiler_process.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_job_system.h"

#include <cstring>

namespace RocProfVis
{
namespace Controller
{
typedef Reference<rocprofvis_controller_future_t, Future, kRPVControllerObjectTypeFuture> FutureRef;
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
    if (config)
    {
        RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
        delete cfg;
    }
}

rocprofvis_result_t rocprofvis_profiler_config_set_type(rocprofvis_profiler_config_t* config, rocprofvis_profiler_type_t profiler_type)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetProfilerType(profiler_type);
}

rocprofvis_result_t rocprofvis_profiler_config_set_profiler_path(rocprofvis_profiler_config_t* config, char const* profiler_path)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetProfilerPath(profiler_path);
}

rocprofvis_result_t rocprofvis_profiler_config_set_target_executable(rocprofvis_profiler_config_t* config, char const* target_executable)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetTargetExecutable(target_executable);
}

rocprofvis_result_t rocprofvis_profiler_config_set_target_args(rocprofvis_profiler_config_t* config, char const* target_args)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetTargetArgs(target_args);
}

rocprofvis_result_t rocprofvis_profiler_config_set_profiler_args(rocprofvis_profiler_config_t* config, char const* profiler_args)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetProfilerArgs(profiler_args);
}

rocprofvis_result_t rocprofvis_profiler_config_set_output_directory(rocprofvis_profiler_config_t* config, char const* output_directory)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetOutputDirectory(output_directory);
}

rocprofvis_result_t rocprofvis_profiler_config_add_env_var(rocprofvis_profiler_config_t* config, char const* name, char const* value)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->AddEnvVar(name, value);
}

rocprofvis_result_t rocprofvis_profiler_config_add_profiler_arg(rocprofvis_profiler_config_t* config, char const* arg)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->AddProfilerArg(arg);
}

rocprofvis_result_t rocprofvis_profiler_config_set_connection_local(rocprofvis_profiler_config_t* config)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetConnectionLocal();
}

rocprofvis_result_t rocprofvis_profiler_config_set_connection_ssh(rocprofvis_profiler_config_t* config,
    char const* host, char const* user, int port, char const* identity_file, char const* remote_stage_dir)
{
    if (config == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;
    return cfg->SetConnectionSsh(host, user, port, identity_file, remote_stage_dir);
}

rocprofvis_result_t rocprofvis_profiler_launch_async(rocprofvis_profiler_config_t* config, rocprofvis_controller_future_t* future)
{
    if (config == nullptr || future == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::ProfilerConfig* cfg = (RocProfVis::Controller::ProfilerConfig*)config;

    RocProfVis::Controller::ProfilerProcessController* controller =
        new RocProfVis::Controller::ProfilerProcessController();

    rocprofvis_result_t result = controller->LaunchAsync(cfg);
    if (result != kRocProfVisResultSuccess)
    {
        delete controller;
        return result;
    }

    RocProfVis::Controller::FutureRef future_ref(future);
    if (future_ref.IsValid())
    {
        future_ref->SetUserData(controller, [](void* ptr)
        {
            delete static_cast<RocProfVis::Controller::ProfilerProcessController*>(ptr);
        });

        RocProfVis::Controller::Job* job = RocProfVis::Controller::JobSystem::Get().IssueJob(
            [controller](RocProfVis::Controller::Future* future) -> rocprofvis_result_t
            {
                RocProfVis::Controller::ProfilerProcessController::ExecuteJob(controller, future);
                return kRocProfVisResultSuccess;
            },
            future_ref.Get());
        future_ref->Set(job);
    }

    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_state(rocprofvis_controller_future_t* future, rocprofvis_profiler_state_t* state)
{
    if (future == nullptr || state == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;
    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller == nullptr)
    {
        *state = kRPVProfilerStateIdle;
        return kRocProfVisResultSuccess;
    }

    *state = controller->GetState();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_output(rocprofvis_controller_future_t* future, char* buffer, uint32_t* length)
{
    if (future == nullptr || length == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;
    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller == nullptr)
    {
        *length = 0;
        return kRocProfVisResultSuccess;
    }

    std::string output = controller->GetOutput();

    if (buffer == nullptr || *length == 0)
    {
        *length = static_cast<uint32_t>(output.size());
        return kRocProfVisResultSuccess;
    }

    uint32_t copy_len = std::min(*length, static_cast<uint32_t>(output.size() + 1));
    std::strncpy(buffer, output.c_str(), copy_len);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_trace_path(rocprofvis_controller_future_t* future, char* buffer, uint32_t* length)
{
    if (future == nullptr || length == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;
    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller == nullptr)
    {
        *length = 0;
        return kRocProfVisResultSuccess;
    }

    std::string trace_path = controller->GetTracePath();

    if (buffer == nullptr || *length == 0)
    {
        *length = static_cast<uint32_t>(trace_path.size());
        return kRocProfVisResultSuccess;
    }

    uint32_t copy_len = std::min(*length, static_cast<uint32_t>(trace_path.size() + 1));
    std::strncpy(buffer, trace_path.c_str(), copy_len);
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_get_exit_code(rocprofvis_controller_future_t* future, int32_t* exit_code)
{
    if (future == nullptr || exit_code == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;
    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller == nullptr)
    {
        *exit_code = -1;
        return kRocProfVisResultSuccess;
    }

    *exit_code = controller->GetExitCode();
    return kRocProfVisResultSuccess;
}

rocprofvis_result_t rocprofvis_profiler_cancel(rocprofvis_controller_future_t* future)
{
    if (future == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    RocProfVis::Controller::Future* fut = (RocProfVis::Controller::Future*)future;

    fut->Cancel();

    auto* controller = static_cast<RocProfVis::Controller::ProfilerProcessController*>(fut->GetUserData());
    if (controller)
    {
        controller->Cancel();
    }

    return kRocProfVisResultSuccess;
}

}
