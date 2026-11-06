// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_server_profiler.h"
#include "protocol/rocprofvis_protocol.h"
#include "rocprofvis_controller.h"

#include "spdlog/spdlog.h"

#include <vector>

namespace RocProfVis
{
namespace Server
{

namespace
{

// How long a run being torn down may take to stop before it is abandoned.
constexpr float CANCEL_WAIT_SECONDS = 10.0f;

bool
is_terminal_state(rocprofvis_profiler_state_t state)
{
    return state == kRPVProfilerStateCompleted || state == kRPVProfilerStateFailed ||
           state == kRPVProfilerStateCancelled;
}

}  // namespace

ServerProfilerRun::ServerProfilerRun(uint64_t run_id, SendMessageFunction send)
: m_run_id(run_id)
, m_send(std::move(send))
, m_config(nullptr)
, m_profiler(nullptr)
, m_future(nullptr)
, m_state(kRPVProfilerStateIdle)
, m_sent_output(0)
{}

ServerProfilerRun::~ServerProfilerRun()
{
    bool abandoned = false;
    if(m_future && rocprofvis_controller_future_wait(m_future, 0) == kRocProfVisResultTimeout)
    {
        rocprofvis_profiler_cancel(m_profiler);
        abandoned = rocprofvis_controller_future_wait(m_future, CANCEL_WAIT_SECONDS) ==
                    kRocProfVisResultTimeout;
    }
    if(abandoned)
    {
        // The worker still writes through these; leaking them beats a crash.
        spdlog::error("Profiler run {} did not stop; leaving its handles allocated", m_run_id);
        return;
    }
    if(m_future)
    {
        rocprofvis_controller_future_free(m_future);
    }
    if(m_profiler)
    {
        rocprofvis_profiler_free(m_profiler);
    }
    if(m_config)
    {
        rocprofvis_profiler_config_free(m_config);
    }
}

bool
ServerProfilerRun::Launch(jt::Json& message, std::string& error)
{
    const uint64_t tool = View::json_get_uint64(message, "tool");
    if(tool == kRPVProfilerToolNone || tool >= __kRPVProfilerToolLast)
    {
        error = "Unknown profiler tool " + std::to_string(tool);
        return false;
    }

    m_config   = rocprofvis_profiler_config_alloc();
    m_profiler = rocprofvis_profiler_alloc();
    m_future   = rocprofvis_controller_future_alloc();
    if(!m_config || !m_profiler || !m_future ||
       rocprofvis_profiler_config_set_tool(
           m_config, static_cast<rocprofvis_profiler_tool_t>(tool)) != kRocProfVisResultSuccess ||
       rocprofvis_profiler_config_set_connection_local(m_config) != kRocProfVisResultSuccess)
    {
        error = "Could not set up the profiler";
        return false;
    }

    std::string tool_directory = View::json_get_string(message, "tool_directory");
    if(!tool_directory.empty())
    {
        rocprofvis_profiler_config_set_tool_directory(m_config, tool_directory.c_str());
    }
    rocprofvis_profiler_config_set_working_directory(
        m_config, View::json_get_string(message, "working_directory").c_str());
    std::string output_directory = View::json_get_string(message, "output_directory");
    if(!output_directory.empty())
    {
        rocprofvis_profiler_config_set_output_directory(m_config, output_directory.c_str());
    }
    // One call per argument, as the UI does: an argument with spaces stays one.
    for(jt::Json& argument : View::json_get_array(message, "argv"))
    {
        rocprofvis_profiler_config_add_profiler_arg(m_config,
                                                    View::json_to_string(argument).c_str());
    }
    for(jt::Json& variable : View::json_get_array(message, "env"))
    {
        if(variable.isArray() && variable.getArray().size() == 2)
        {
            rocprofvis_profiler_config_add_env_var(
                m_config, View::json_to_string(variable.getArray()[0]).c_str(),
                View::json_to_string(variable.getArray()[1]).c_str());
        }
    }

    if(rocprofvis_profiler_launch_async(m_profiler, m_config, m_future) !=
       kRocProfVisResultSuccess)
    {
        error = "The profiler could not be started; is the tool installed on the server?";
        return false;
    }
    spdlog::info("Profiler run {} started (tool {})", m_run_id, tool);
    return true;
}

void
ServerProfilerRun::Cancel()
{
    if(m_profiler)
    {
        rocprofvis_profiler_cancel(m_profiler);
    }
}

bool
ServerProfilerRun::Update()
{
    rocprofvis_profiler_state_t state = kRPVProfilerStateIdle;
    rocprofvis_profiler_get_state(m_profiler, &state);
    const bool finished = rocprofvis_controller_future_wait(m_future, 0) != kRocProfVisResultTimeout;
    SendOutput();
    if(state != m_state || (finished && !is_terminal_state(m_state)))
    {
        m_state = finished && !is_terminal_state(state) ? kRPVProfilerStateFailed : state;
        SendState("");
    }
    return !(finished && is_terminal_state(m_state));
}

void
ServerProfilerRun::SendOutput()
{
    uint32_t length = 0;
    rocprofvis_profiler_get_output(m_profiler, nullptr, &length);
    if(length < m_sent_output)
    {
        // The output was cleared or trimmed: start over from what is there.
        m_sent_output = 0;
    }
    if(length == m_sent_output)
    {
        return;
    }
    std::vector<char> buffer(static_cast<size_t>(length) + 1, '\0');
    if(rocprofvis_profiler_get_output(m_profiler, buffer.data(), &length) !=
       kRocProfVisResultSuccess)
    {
        return;
    }
    jt::Json message;
    message["type"] = View::MSG_PROFILER_OUTPUT;
    message["run"]  = View::json_from_uint64(m_run_id);
    message["text"] = std::string(buffer.data() + m_sent_output, length - m_sent_output);
    m_sent_output   = length;
    m_send(message);
}

void
ServerProfilerRun::SendState(const std::string& error)
{
    int32_t exit_code = -1;
    if(is_terminal_state(m_state))
    {
        rocprofvis_profiler_get_exit_code(m_profiler, &exit_code);
        spdlog::info("Profiler run {} ended in state {} with exit code {}", m_run_id,
                     static_cast<int>(m_state), exit_code);
    }
    jt::Json message;
    message["type"]      = View::MSG_PROFILER_STATE;
    message["run"]       = View::json_from_uint64(m_run_id);
    message["state"]     = static_cast<long long>(m_state);
    message["exit_code"] = static_cast<long long>(exit_code);
    if(!error.empty())
    {
        message["error"] = error;
    }
    m_send(message);
}

}  // namespace Server
}  // namespace RocProfVis
