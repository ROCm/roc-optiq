// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_server_trace.h"
#include "model/rocprofvis_common_defs.h"
#include "protocol/rocprofvis_protocol.h"
#include "rocprofvis_controller.h"

#include "spdlog/spdlog.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <memory>
#include <vector>

namespace RocProfVis
{
namespace Server
{

namespace
{

std::string
get_controller_string(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                      uint64_t index)
{
    std::string value;
    uint32_t    length = 0;
    if(rocprofvis_controller_get_string(handle, property, index, nullptr, &length) ==
           kRocProfVisResultSuccess &&
       length > 0)
    {
        value.resize(length);
        if(rocprofvis_controller_get_string(handle, property, index, value.data(),
                                            &length) != kRocProfVisResultSuccess)
        {
            value.clear();
        }
    }
    return value;
}

}  // namespace

ServerTrace::ServerTrace(SendMessageFunction send_message, SendChunkFunction send_chunk)
: m_send_message(std::move(send_message))
, m_send_chunk(std::move(send_chunk))
, m_controller(nullptr)
, m_load_request_id(0)
, m_load_progress_percent(0)
, m_allow_scripts(false)
{
    m_data_provider.SetRequestCompletedCallback(
        [this](const View::RequestInfo& req) { OnRequestCompleted(req); });
    m_data_provider.SetRequestProgressUpdateCallback(
        [this](const View::RequestInfo& req, uint64_t percent, const std::string& message) {
            OnRequestProgress(req, percent, message);
        });
}

ServerTrace::~ServerTrace()
{
    m_data_provider.SetRequestCompletedCallback(nullptr);
    m_data_provider.SetRequestProgressUpdateCallback(nullptr);
    m_data_provider.CloseController();
    for(const std::pair<const uint64_t, std::string>& file : m_output_files)
    {
        std::error_code error;
        std::filesystem::remove(file.second, error);
    }
}

bool
ServerTrace::Open(const std::vector<std::string>& paths, const std::string& config_path,
                  bool& is_compute, std::string& error)
{
    for(const std::string& path : paths)
    {
        std::error_code fs_error;
        if(!std::filesystem::is_regular_file(path, fs_error))
        {
            error = "No such file on the server: " + path;
            return false;
        }
    }
    if(paths.empty())
    {
        error = "No trace given";
        return false;
    }
    const std::string& path = paths.front();

    rocprofvis_controller_t* controller = nullptr;
    if(paths.size() > 1)
    {
#ifdef ROCPROFVIS_ENABLE_TRACE_COMPARE
        std::vector<const char*> file_ptrs;
        for(const std::string& compare_path : paths)
        {
            file_ptrs.push_back(compare_path.c_str());
        }
        controller = rocprofvis_controller_alloc_compare(file_ptrs.data(), file_ptrs.size());
#else
        error = "This server was built without trace comparison.";
        return false;
#endif
    }
    else
    {
        controller = rocprofvis_controller_alloc(path.c_str(), config_path.c_str());
    }
    if(!controller)
    {
        error = "Not a trace the server can open: " + path;
        return false;
    }
    rocprofvis_controller_object_type_t type = kRPVControllerObjectTypeControllerSystem;
    if(rocprofvis_controller_get_object_type(controller, &type) != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_free(controller);
        error = "Not a trace the server can open: " + path;
        return false;
    }
    is_compute = type == kRPVControllerObjectTypeControllerCompute;

    // FetchTrace takes the controller, and frees it if the load cannot start.
    if(!m_data_provider.FetchTrace(controller, path))
    {
        error = "Could not start loading " + path;
        return false;
    }
    if(paths.size() > 1)
    {
        // Before the load finishes: it tags each track with its source.
        m_data_provider.DataModel().SetCompareSources(View::make_compare_sources(paths));
    }
    m_controller      = controller;
    m_path            = path;
    m_load_request_id = is_compute ? View::DataProvider::FETCH_COMPUTE_TRACE_REQUEST_ID
                                   : View::DataProvider::FETCH_SYSTEM_TRACE_REQUEST_ID;
    spdlog::info("Loading {} ({} trace)", path, is_compute ? "compute" : "system");
    return true;
}

void
ServerTrace::HandleRequest(uint64_t request_id, const std::string& operation,
                           jt::Json& params)
{
    bool accepted = false;
    if(operation == View::OP_GRAPH)
    {
        accepted = m_data_provider.FetchTrack(View::decode_track_request(params)).first;
    }
    else if(operation == View::OP_TRACK)
    {
        View::TrackRequestParams track = View::decode_track_request(params);
        accepted = m_data_provider.FetchWholeTrack(
            track.m_track_id, track.m_start_ts, track.m_end_ts, track.m_horz_pixel_range,
            track.m_data_group_id, track.m_chunk_index, track.m_chunk_count);
    }
    else if(operation == View::OP_TRACK_TABLE)
    {
        View::TrackTableRequestParams table = View::decode_track_table_request(params);
        if(View::json_get_bool(params, "export"))
        {
            table.m_export_to_file_path = MakeTemporaryPath(".csv");
            m_output_files[request_id]  = table.m_export_to_file_path;
        }
        accepted = m_data_provider.FetchTable(table);
    }
    else if(operation == View::OP_EVENT_SEARCH)
    {
        View::EventSearchRequestParams table = View::decode_event_search_request(params);
        if(View::json_get_bool(params, "export"))
        {
            table.m_export_to_file_path = MakeTemporaryPath(".csv");
            m_output_files[request_id]  = table.m_export_to_file_path;
        }
        accepted = m_data_provider.FetchTable(table);
    }
    else if(operation == View::OP_SUMMARY)
    {
        accepted = m_data_provider.FetchSummary();
    }
    else if(operation == View::OP_TRACK_STATISTICS)
    {
        View::AnalysisTrackStatisticsRequestParams statistics =
            View::decode_track_statistics_request(params);
        // The provider writes the result into the analysis model, which only
        // takes tracks that were registered with it, as the UI does first.
        const View::TrackInfo* track =
            m_data_provider.DataModel().GetTimeline().GetTrack(statistics.m_track_id);
        if(track)
        {
            m_data_provider.DataModel().GetAnalysis().RegisterTrack(*track);
            accepted = m_data_provider.FetchAnalysisTrackStatistics(statistics);
        }
    }
    else if(operation == View::OP_EVENT_EXT_DATA)
    {
        // The client already seeded the event from its own track data; here the
        // event only has to exist so the controller's rows have somewhere to go.
        accepted = m_data_provider.FetchEvent(View::INVALID_UINT64_INDEX,
                                              View::json_get_uint64(params, "event_id"));
    }
    else if(operation == View::OP_EVENT_FLOW)
    {
        accepted =
            m_data_provider.FetchEventFlowDetails(View::json_get_uint64(params, "event_id"));
    }
    else if(operation == View::OP_EVENT_CALL_STACK)
    {
        accepted = m_data_provider.FetchEventCallStackData(
            View::json_get_uint64(params, "event_id"));
    }
    else if(operation == View::OP_SAVE_TRIMMED)
    {
        std::string output         = MakeTemporaryPath(std::filesystem::path(m_path).extension().string());
        m_output_files[request_id] = output;
        accepted                   = m_data_provider.SaveTrimmedTrace(
            output, View::json_get_double(params, "start"), View::json_get_double(params, "end"));
    }
    else if(operation == View::OP_CLEANUP_DATABASE)
    {
        accepted = m_data_provider.CleanupDatabase(View::json_get_bool(params, "rebuild"));
    }
    else if(operation == View::OP_METRICS)
    {
        accepted = m_data_provider.FetchMetrics(View::decode_metrics_request(params));
    }
    else if(operation == View::OP_METRIC_PIVOT)
    {
        accepted =
            m_data_provider.FetchMetricPivotTable(View::decode_compute_table_request(params));
    }
    else if(operation == View::OP_PC_SAMPLING)
    {
        accepted = m_data_provider.FetchPcSampling(View::decode_pc_sampling_request(params));
    }
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    else if(operation == View::OP_EXECUTE_SCRIPT)
    {
        if(!m_allow_scripts)
        {
            Reject(request_id, operation,
                   "This server does not run scripts. Start roc-optiq-server with "
                   "--allow-scripts to allow it.");
            return;
        }
        std::vector<uint64_t> track_ids;
        for(jt::Json& track_id : View::json_get_array(params, "tracks"))
        {
            track_ids.push_back(View::json_to_uint64(track_id));
        }
        accepted = m_data_provider.ExecuteScript(View::json_get_string(params, "source"),
                                                 track_ids, View::json_get_double(params, "start"),
                                                 View::json_get_double(params, "end"));
    }
#endif
    else
    {
        spdlog::warn("Unknown request operation '{}'", operation);
    }

    if(!accepted)
    {
        m_output_files.erase(request_id);
        Reject(request_id, operation, "The server could not start this request.");
    }
    else if(!m_data_provider.IsRequestPending(request_id))
    {
        // Both sides derive the id from the same parameters, so this means the
        // client and server builds disagree on the request bookkeeping.
        spdlog::error("Request {} ({}) is pending under a different id on the server; "
                      "the client will not receive its response",
                      request_id, operation);
    }
}

void
ServerTrace::Cancel(uint64_t request_id)
{
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
    if(request_id == View::DataProvider::EXECUTE_SCRIPT_REQUEST_ID)
    {
        // A script is not a JobSystem job; only its interpreter can stop it.
        m_data_provider.CancelScript();
        return;
    }
#endif
    m_data_provider.CancelRequest(request_id);
}

void
ServerTrace::SetAllowScripts(bool allow)
{
    m_allow_scripts = allow;
}

void
ServerTrace::SetSelected()
{
    m_data_provider.SetSelectedState(m_data_provider.GetTraceFilePath());
}

bool
ServerTrace::Update()
{
    m_data_provider.Update();
    if(m_data_provider.GetState() == View::ProviderState::kLoading)
    {
        uint64_t    percent = m_data_provider.GetProgressPercent();
        std::string message = m_data_provider.GetProgressMessage();
        if(percent != m_load_progress_percent || message != m_load_progress_message)
        {
            m_load_progress_percent = percent;
            m_load_progress_message = message;
            SendProgress(m_load_request_id, percent, message);
        }
    }
    return m_data_provider.GetPendingRequestCount() > 0;
}

void
ServerTrace::OnRequestProgress(const View::RequestInfo& req, uint64_t percent,
                               const std::string& message)
{
    SendProgress(req.request_id, percent, message);
}

void
ServerTrace::SendProgress(uint64_t request_id, uint64_t percent, const std::string& message)
{
    jt::Json json;
    json["type"]    = View::MSG_PROGRESS;
    json["id"]      = View::json_from_uint64(request_id);
    json["percent"] = View::json_from_uint64(percent);
    json["message"] = message;
    m_send_message(json);
}

void
ServerTrace::OnRequestCompleted(const View::RequestInfo& req)
{
    const bool success = req.response_code == kRocProfVisResultSuccess;
    jt::Json   data;
    data.setObject();
    bool has_file = false;
    bool file_ok  = false;

    switch(req.request_type)
    {
        case View::RequestType::kFetchSystemTrace:
        {
            if(success)
            {
                data = View::encode_system_trace(m_data_provider.DataModel());
                spdlog::info("Loaded {}: {} tracks", m_path,
                             m_data_provider.DataModel().GetTimeline().GetTrackCount());
            }
            break;
        }
        case View::RequestType::kFetchComputeTrace:
        {
            if(success)
            {
                data = View::encode_compute_trace(m_data_provider.ComputeModel(),
                                                  ReadMemoryChartLayouts());
                spdlog::info("Loaded {}: {} workloads", m_path,
                             m_data_provider.ComputeModel().GetWorkloadList().size());
            }
            break;
        }
        case View::RequestType::kFetchGraph:
        case View::RequestType::kFetchTrack:
        {
            std::shared_ptr<View::TrackDataResult> result =
                std::dynamic_pointer_cast<View::TrackDataResult>(req.result);
            if(result)
            {
                data = View::encode_track_data(*result);
            }
            // The client keeps the merged track data; the copy here only grew
            // as a side effect of applying the chunk.
            std::shared_ptr<View::TrackRequestParams> params =
                std::dynamic_pointer_cast<View::TrackRequestParams>(req.custom_params);
            if(params)
            {
                m_data_provider.DataModel().GetTimeline().FreeTrackData(params->m_track_id,
                                                                         true);
            }
            break;
        }
        case View::RequestType::kFetchTrackEventTable:
        case View::RequestType::kFetchTrackSampleTable:
        case View::RequestType::kFetchEventSearchTable:
        case View::RequestType::kFetchSummaryKernelInstanceTable:
        case View::RequestType::kFetchSummaryMemoryInstanceTable:
        case View::RequestType::kFetchSummaryThreadInstanceTable:
        case View::RequestType::kFetchAnalysisTopEventsTable:
        case View::RequestType::kFetchAnalysisTopDispatchEventsTable:
        case View::RequestType::kFetchAnalysisTopMemoryAllocationEventsTable:
        case View::RequestType::kFetchAnalysisTopMemoryCopyEventsTable:
        case View::RequestType::kFetchAnalysisTopLaunchSampleEventsTable:
        {
            rocprofvis_controller_table_type_t table_type;
            if(success && View::DataProvider::GetRequestTableType(req.request_type, table_type))
            {
                std::shared_ptr<View::TableRequestParams> params =
                    std::dynamic_pointer_cast<View::TableRequestParams>(req.custom_params);
                std::pair<View::TablesModel*, View::TableType> slot =
                    m_data_provider.GetTableSlot(table_type, params.get());
                if(slot.first && slot.second != View::TableType::__kTableTypeCount)
                {
                    data = View::encode_table(slot.first->GetTable(slot.second));
                }
            }
            break;
        }
        case View::RequestType::kTableExport:
        case View::RequestType::kSaveTrimmedTrace:
        {
            auto output = m_output_files.find(req.request_id);
            if(output != m_output_files.end())
            {
                has_file = true;
                file_ok  = success && SendFile(req.request_id, output->second);
                std::error_code error;
                std::filesystem::remove(output->second, error);
                m_output_files.erase(output);
            }
            break;
        }
        case View::RequestType::kFetchEventExtendedData:
        case View::RequestType::kFetchEventFlowDetails:
        case View::RequestType::kFetchEventCallStack:
        {
            if(std::shared_ptr<View::EventExtDataResult> result =
                   std::dynamic_pointer_cast<View::EventExtDataResult>(req.result))
            {
                data = View::encode_event_ext_data(*result);
            }
            else if(std::shared_ptr<View::EventFlowResult> flow =
                        std::dynamic_pointer_cast<View::EventFlowResult>(req.result))
            {
                data = View::encode_event_flow(*flow);
            }
            else if(std::shared_ptr<View::EventCallStackResult> call_stack =
                        std::dynamic_pointer_cast<View::EventCallStackResult>(req.result))
            {
                data = View::encode_event_call_stack(*call_stack);
            }
            std::shared_ptr<View::EventRequestParams> params =
                std::dynamic_pointer_cast<View::EventRequestParams>(req.custom_params);
            if(params && req.request_type == View::RequestType::kFetchEventExtendedData)
            {
                m_data_provider.DataModel().GetEvents().RemoveEvent(params->m_event_id);
            }
            break;
        }
        case View::RequestType::kFetchSummary:
        {
            if(success)
            {
                data = View::encode_summary(m_data_provider.DataModel().GetSummary());
            }
            break;
        }
        case View::RequestType::kFetchAnalysisTrackStatistics:
        {
            std::shared_ptr<View::AnalysisTrackStatisticsRequestParams> params =
                std::dynamic_pointer_cast<View::AnalysisTrackStatisticsRequestParams>(
                    req.custom_params);
            if(params && success)
            {
                data = View::encode_track_statistics(*params);
            }
            break;
        }
        case View::RequestType::kFetchMetrics:
        {
            if(std::shared_ptr<View::MetricsResult> result =
                   std::dynamic_pointer_cast<View::MetricsResult>(req.result))
            {
                data = View::encode_metrics(*result);
            }
            break;
        }
        case View::RequestType::kFetchMetricPivotTable:
        {
            if(success)
            {
                data = View::encode_kernel_selection_table(
                    m_data_provider.ComputeModel().GetKernelSelectionTable().GetTableInfo());
            }
            break;
        }
        case View::RequestType::kFetchPcSampling:
        {
            std::shared_ptr<View::PcSamplingRequestParams> params =
                std::dynamic_pointer_cast<View::PcSamplingRequestParams>(req.custom_params);
            if(params)
            {
                const View::KernelInfo* kernel = m_data_provider.ComputeModel().GetKernelInfo(
                    params->m_workload_id, params->m_kernel_id);
                if(kernel)
                {
                    data = View::encode_pc_sampling(kernel->pc_sampling_data, params->m_layer);
                }
            }
            break;
        }
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
        case View::RequestType::kExecuteScript:
        {
            std::string text;
            std::string error;
            data["ok"]    = m_data_provider.GetLastScriptResult(text, error);
            data["text"]  = text;
            data["error"] = error;
            break;
        }
#endif
        default: break;
    }

    jt::Json message;
    message["type"]         = View::MSG_RESPONSE;
    message["id"]           = View::json_from_uint64(req.request_id);
    message["request_type"] = static_cast<long long>(req.request_type);
    message["code"]         = View::json_from_uint64(req.response_code);
    message["error"]        = req.error_message;
    if(has_file)
    {
        message["file_ok"] = file_ok;
    }
    message["data"] = std::move(data);
    m_send_message(message);
}

void
ServerTrace::Reject(uint64_t request_id, const std::string& operation, const std::string& error)
{
    spdlog::warn("Rejected {} request {}: {}", operation, request_id, error);
    jt::Json message;
    message["type"]  = View::MSG_RESPONSE;
    message["id"]    = View::json_from_uint64(request_id);
    message["code"]  = View::json_from_uint64(kRocProfVisResultUnknownError);
    message["error"] = error;
    message["data"].setObject();
    m_send_message(message);
}

bool
ServerTrace::SendFile(uint64_t request_id, const std::string& path)
{
    std::error_code error;
    const uint64_t  size = std::filesystem::file_size(path, error);
    std::ifstream   file(path, std::ios::binary);
    bool            result = !error && file.good();

    jt::Json begin;
    begin["type"] = View::MSG_FILE_BEGIN;
    begin["id"]   = View::json_from_uint64(request_id);
    begin["size"] = View::json_from_uint64(result ? size : 0);
    m_send_message(begin);

    std::vector<char> buffer(View::FILE_CHUNK_SIZE);
    uint64_t          sent = 0;
    while(result && sent < size)
    {
        file.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        const std::streamsize count = file.gcount();
        if(count <= 0)
        {
            result = false;
            break;
        }
        result = m_send_chunk(request_id, buffer.data(), static_cast<size_t>(count));
        sent += static_cast<uint64_t>(count);
    }
    result = result && sent == size;

    jt::Json end;
    end["type"] = View::MSG_FILE_END;
    end["id"]   = View::json_from_uint64(request_id);
    end["ok"]   = result;
    m_send_message(end);
    return result;
}

std::string
ServerTrace::MakeTemporaryPath(const std::string& extension)
{
    static std::atomic<uint64_t> s_next_file(0);
    std::error_code              error;
    std::filesystem::path        directory = std::filesystem::temp_directory_path(error);
    if(error)
    {
        directory = std::filesystem::path(m_path).parent_path();
    }
    const uint64_t stamp = static_cast<uint64_t>(
        std::chrono::steady_clock::now().time_since_epoch().count());
    return (directory / ("roc-optiq-server-" + std::to_string(stamp) + "-" +
                         std::to_string(s_next_file++) + extension))
        .string();
}

std::vector<std::pair<uint32_t, std::string>>
ServerTrace::ReadMemoryChartLayouts()
{
    // The provider keeps only the parsed layout; the client parses its own copy.
    std::vector<std::pair<uint32_t, std::string>> layouts;
    uint64_t                                      num_workloads = 0;
    if(!m_controller ||
       rocprofvis_controller_get_uint64(m_controller, kRPVControllerNumWorkloads, 0,
                                        &num_workloads) != kRocProfVisResultSuccess)
    {
        return layouts;
    }
    for(uint64_t i = 0; i < num_workloads; i++)
    {
        rocprofvis_handle_t* workload = nullptr;
        uint64_t             id       = 0;
        if(rocprofvis_controller_get_object(m_controller, kRPVControllerWorkloadIndexed, i,
                                            &workload) == kRocProfVisResultSuccess &&
           workload &&
           rocprofvis_controller_get_uint64(workload, kRPVControllerWorkloadId, 0, &id) ==
               kRocProfVisResultSuccess)
        {
            layouts.emplace_back(
                static_cast<uint32_t>(id),
                get_controller_string(workload, kRPVControllerWorkloadMemoryChartLayout, 0));
        }
    }
    return layouts;
}

}  // namespace Server
}  // namespace RocProfVis
