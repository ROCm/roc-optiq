// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The DataProvider half of running a Python analysis script: build the context
// arguments, issue the async request, and turn the finished result into an
// event. Split out of rocprofvis_data_provider.cpp so the scripting feature is
// one conditionally compiled file rather than a block in the middle of the
// request pipeline. The small hooks that have to sit inside existing switches
// (request dispatch, progress, cleanup) stay there.
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_script.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"

#include "spdlog/spdlog.h"

#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

rocprofvis_controller_arguments_t*
DataProvider::BuildScriptContext(const std::vector<uint64_t>& track_ids, double start_ts,
                                 double end_ts)
{
    rocprofvis_controller_arguments_t* context = rocprofvis_controller_arguments_alloc();
    if(!context)
    {
        return nullptr;
    }
    rocprofvis_result_t result = rocprofvis_controller_set_double(
        context, kRPVControllerScriptContextTimeRangeStart, 0, start_ts);
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_double(
            context, kRPVControllerScriptContextTimeRangeEnd, 0, end_ts);
    }
    uint64_t written = 0;
    for(uint64_t track_id : track_ids)
    {
        rocprofvis_handle_t* track        = nullptr;
        rocprofvis_result_t  track_result = rocprofvis_controller_get_object(
            m_trace_controller, kRPVControllerSystemTrackById, track_id, &track);
        if(track_result != kRocProfVisResultSuccess || !track)
        {
            continue;
        }
        track_result = rocprofvis_controller_set_object(
            context, kRPVControllerScriptContextTracksIndexed, written, track);
        if(track_result == kRocProfVisResultSuccess)
        {
            written++;
        }
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = rocprofvis_controller_set_uint64(
            context, kRPVControllerScriptContextNumTracks, 0, written);
    }
    if(result != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_arguments_free(context);
        context = nullptr;
    }
    return context;
}

bool
DataProvider::ExecuteScript(const std::string& source, const std::vector<uint64_t>& track_ids,
                            double start_ts, double end_ts)
{
    if(m_state != ProviderState::kReady || !m_trace_controller)
    {
        spdlog::debug("Cannot execute script, provider not ready");
        return false;
    }
    if(m_requests.find(EXECUTE_SCRIPT_REQUEST_ID) != m_requests.end())
    {
        spdlog::debug("Script already running");
        return false;
    }

    // A caller that polls would otherwise read the previous run's text as
    // though this script had already answered.
    m_script_result_text.clear();
    m_script_result_error.clear();
    m_script_result_ok = false;

    rocprofvis_controller_arguments_t* context =
        BuildScriptContext(track_ids, start_ts, end_ts);
    if(!context)
    {
        spdlog::error("Could not build the script context");
        return false;
    }
    rocprofvis_controller_future_t*        future        = rocprofvis_controller_future_alloc();
    rocprofvis_controller_script_result_t* script_result = nullptr;
    if(!future)
    {
        rocprofvis_controller_arguments_free(context);
        return false;
    }

    rocprofvis_result_t result = rocprofvis_script_execute_async(
        m_trace_controller, source.c_str(), context, future, &script_result);
    if(result != kRocProfVisResultSuccess || !script_result)
    {
        spdlog::error("script_execute_async failed ({})", static_cast<int>(result));
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_arguments_free(context);
        if(script_result)
        {
            rocprofvis_script_result_free(script_result);
        }
        return false;
    }

    RequestInfo request_info;
    request_info.request_array      = nullptr;
    request_info.request_future     = future;
    request_info.request_obj_handle = script_result;
    request_info.request_args       = context;
    request_info.request_id         = EXECUTE_SCRIPT_REQUEST_ID;
    request_info.loading_state      = RequestState::kLoading;
    request_info.request_type       = RequestType::kExecuteScript;
    request_info.request_progress   = 0;
    m_requests.emplace(request_info.request_id, request_info);
    return true;
}

bool
DataProvider::CancelScript()
{
    return CancelRequest(EXECUTE_SCRIPT_REQUEST_ID);
}

void
DataProvider::ProcessExecuteScriptRequest(RequestInfo& req)
{
    std::string text;
    std::string error;
    bool        success = req.response_code == kRocProfVisResultSuccess;
    if(req.request_obj_handle)
    {
        text = GetString(req.request_obj_handle, kRPVControllerScriptResultText, 0);
        error =
            GetString(req.request_obj_handle, kRPVControllerScriptResultErrorMessage, 0);
        rocprofvis_script_result_free(
            reinterpret_cast<rocprofvis_controller_script_result_t*>(
                req.request_obj_handle));
        req.request_obj_handle = nullptr;
    }
    if(req.request_args)
    {
        rocprofvis_controller_arguments_free(req.request_args);
        req.request_args = nullptr;
    }
    m_script_result_text  = text;
    m_script_result_error = error;
    m_script_result_ok    = success;
    EventManager::GetInstance()->AddEvent(std::make_shared<ScriptExecuteCompleteEvent>(
        success, text, error, GetTraceFilePath()));
}

bool
DataProvider::GetLastScriptResult(std::string& text_out, std::string& error_out) const
{
    text_out  = m_script_result_text;
    error_out = m_script_result_error;
    return m_script_result_ok;
}

}  // namespace View
}  // namespace RocProfVis
