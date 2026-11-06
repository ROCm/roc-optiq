// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// DataProvider's remote mode: the trace lives on a roc-optiq-server, which runs
// a DataProvider of its own against the controller. Requests keep the ids and
// bookkeeping of the local path, and responses are applied by the same steps,
// so everything above the DataProvider behaves the same either way.

#include "rocprofvis_data_provider.h"
#include "protocol/rocprofvis_data_channel.h"
#include "protocol/rocprofvis_protocol.h"
#include "rocprofvis_core_assert.h"

#include "spdlog/spdlog.h"

#include <chrono>
#include <memory>

namespace RocProfVis
{
namespace View
{

namespace
{

// Responses are applied on the UI thread, so a burst of them is spread over a
// few frames rather than stalling one.
constexpr size_t MAX_REMOTE_MESSAGES_PER_UPDATE = 64;

RequestInfo
make_remote_request(uint64_t request_id, RequestType request_type,
                    std::shared_ptr<RequestParamsBase> params)
{
    RequestInfo request_info   = {};
    request_info.request_id    = request_id;
    request_info.request_type  = request_type;
    request_info.custom_params = std::move(params);
    request_info.loading_state = RequestState::kLoading;
    return request_info;
}

}  // namespace

bool
DataProvider::FetchRemoteTrace(std::shared_ptr<IDataChannel> channel,
                               const std::string& file_path, bool is_compute)
{
    if(m_state == ProviderState::kLoading || m_state == ProviderState::kError)
    {
        spdlog::debug("Cannot fetch, provider busy or error, state: {}",
                      static_cast<int>(m_state));
        return false;
    }
    if(!channel)
    {
        spdlog::error("Cannot fetch, no server channel");
        return false;
    }

    CloseController();
    m_model.SetTraceFilePath(file_path);
    m_remote_channel = std::move(channel);

    // The server starts loading as soon as it opens the trace; its progress and
    // result arrive under the same request id the local load uses.
    RequestInfo request_info =
        is_compute
            ? make_remote_request(FETCH_COMPUTE_TRACE_REQUEST_ID,
                                  RequestType::kFetchComputeTrace, nullptr)
            : make_remote_request(FETCH_SYSTEM_TRACE_REQUEST_ID,
                                  RequestType::kFetchSystemTrace, nullptr);
    request_info.request_time = std::chrono::steady_clock::now();
    m_requests.emplace(request_info.request_id, std::move(request_info));
    m_progress_mesage = "Loading trace on server";
    m_state           = ProviderState::kLoading;
    return true;
}

bool
DataProvider::SubmitRemoteRequest(RequestInfo&& request_info, const char* operation,
                                  jt::Json& params)
{
    if(!m_remote_channel || !m_remote_channel->IsConnected())
    {
        spdlog::warn("Cannot send {} request, not connected to the server", operation);
        return false;
    }

    jt::Json message;
    message["type"]   = MSG_REQUEST;
    message["id"]     = json_from_uint64(request_info.request_id);
    message["op"]     = operation;
    message["params"] = std::move(params);
    if(!m_remote_channel->Send(message))
    {
        spdlog::warn("Failed to send {} request to the server", operation);
        return false;
    }

    request_info.request_time = std::chrono::steady_clock::now();
    const uint64_t request_id = request_info.request_id;
    m_requests.emplace(request_id, std::move(request_info));
    return true;
}

void
DataProvider::HandleRemoteMessages()
{
    jt::Json message;
    size_t   processed = 0;
    while(m_remote_channel && processed < MAX_REMOTE_MESSAGES_PER_UPDATE &&
          m_remote_channel->Receive(message))
    {
        processed++;
        std::string type = json_get_string(message, "type");
        if(type == MSG_PROGRESS)
        {
            ProcessRemoteProgress(message);
        }
        else if(type == MSG_RESPONSE)
        {
            const uint64_t request_id = json_get_uint64(message, "id");
            auto           it         = m_requests.find(request_id);
            if(it == m_requests.end())
            {
                spdlog::debug("Dropping response for request {} that is no longer pending",
                              request_id);
                continue;
            }
            const RequestType request_type = it->second.request_type;
            // As HandleRequests: the request stays pending while it is applied.
            ProcessRemoteResponse(it->second, message);
            m_requests.erase(request_id);

            if(request_type == RequestType::kFetchPcSampling)
            {
                auto replacement = m_pc_sampling_replacements.find(request_id);
                if(replacement != m_pc_sampling_replacements.end())
                {
                    PcSamplingRequestParams pending = std::move(replacement->second);
                    m_pc_sampling_replacements.erase(replacement);
                    if(!FetchPcSampling(pending) && m_pc_sampling_fetch_callback)
                    {
                        m_pc_sampling_fetch_callback(
                            m_model.GetTraceFilePath(), pending.m_layer,
                            pending.m_kernel_id, pending.m_source_file_uuid,
                            pending.m_generation, pending.m_request_token,
                            kRocProfVisResultUnknownError);
                    }
                }
            }
        }
        else if(type == MSG_DISCONNECTED)
        {
            spdlog::error("Lost the connection to the server for {}",
                          m_model.GetTraceFilePath());
            FailRemoteRequests(kRocProfVisResultUnknownError);
        }
        else if(type == MSG_ERROR)
        {
            spdlog::error("Server error for {}: {}", m_model.GetTraceFilePath(),
                          json_get_string(message, "message"));
        }
    }
}

void
DataProvider::FailRemoteRequests(uint64_t response_code)
{
    std::vector<uint64_t> request_ids;
    for(const std::pair<const int64_t, RequestInfo>& item : m_requests)
    {
        request_ids.push_back(item.first);
    }
    m_pc_sampling_replacements.clear();
    for(uint64_t request_id : request_ids)
    {
        auto it = m_requests.find(request_id);
        if(it != m_requests.end())
        {
            jt::Json message;
            message["code"] = json_from_uint64(response_code);
            message["data"].setObject();
            ProcessRemoteResponse(it->second, message);
            m_requests.erase(request_id);
        }
    }
}

void
DataProvider::ProcessRemoteProgress(jt::Json& message)
{
    auto it = m_requests.find(json_get_uint64(message, "id"));
    if(it == m_requests.end())
    {
        return;
    }
    RequestInfo&   req        = it->second;
    const uint64_t percentage = json_get_uint64(message, "percent");
    std::string    text       = json_get_string(message, "message");
    if(req.request_type == RequestType::kFetchSystemTrace ||
       req.request_type == RequestType::kFetchComputeTrace)
    {
        m_progress_percent = percentage;
        m_progress_mesage  = text;
    }
    else if(percentage != req.request_progress)
    {
        req.request_progress = percentage;
        if(m_request_progress_callback)
        {
            m_request_progress_callback(req, percentage, text);
        }
    }
}

void
DataProvider::ProcessRemoteResponse(RequestInfo& req, jt::Json& message)
{
    req.loading_state = RequestState::kReady;
    req.response_code = json_get_uint64(message, "code");
    req.error_message = json_get_string(message, "error");
    // A request that produced a file only succeeded if the file arrived too.
    if(message.contains("file_ok") && !json_get_bool(message, "file_ok") &&
       req.response_code == kRocProfVisResultSuccess)
    {
        req.response_code = kRocProfVisResultUnknownError;
    }
    jt::Json&  data    = message["data"];
    const bool success = req.response_code == kRocProfVisResultSuccess;

    switch(req.request_type)
    {
        case RequestType::kFetchSystemTrace:
        {
            ProcessRemoteSystemTrace(req, data);
            break;
        }
        case RequestType::kFetchComputeTrace:
        {
            ProcessRemoteComputeTrace(req, data);
            break;
        }
        case RequestType::kFetchGraph:
        case RequestType::kFetchTrack:
        {
            std::shared_ptr<TrackRequestParams> params =
                std::dynamic_pointer_cast<TrackRequestParams>(req.custom_params);
            ROCPROFVIS_ASSERT(params);
            TrackDataResult result;
            if(success)
            {
                decode_track_data(data, result);
            }
            else if(req.response_code != kRocProfVisResultOutOfRange)
            {
                spdlog::warn("Track data request failed with code {}", req.response_code);
            }
            const TrackInfo* metadata = m_model.GetTimeline().GetTrack(params->m_track_id);
            if(metadata && metadata->track_type == kRPVControllerTrackTypeEvents)
            {
                ApplyRawEventData(*params, req, std::move(result.events));
            }
            else if(metadata && metadata->track_type == kRPVControllerTrackTypeSamples)
            {
                ApplyRawSampleData(*params, req, std::move(result.samples));
            }
            else
            {
                spdlog::error("Track metadata not found for track id {}, cannot process data!",
                              params->m_track_id);
            }
            FireTrackDataReady(*params, req);
            break;
        }
        case RequestType::kFetchTrackEventTable:
        case RequestType::kFetchTrackSampleTable:
        case RequestType::kFetchEventSearchTable:
        case RequestType::kFetchSummaryKernelInstanceTable:
        case RequestType::kFetchSummaryMemoryInstanceTable:
        case RequestType::kFetchSummaryThreadInstanceTable:
        case RequestType::kFetchAnalysisTopEventsTable:
        case RequestType::kFetchAnalysisTopDispatchEventsTable:
        case RequestType::kFetchAnalysisTopMemoryAllocationEventsTable:
        case RequestType::kFetchAnalysisTopMemoryCopyEventsTable:
        case RequestType::kFetchAnalysisTopLaunchSampleEventsTable:
        {
            if(success)
            {
                rocprofvis_controller_table_type_t table_type;
                if(!GetRequestTableType(req.request_type, table_type))
                {
                    spdlog::error("Invalid table request type: {}",
                                  static_cast<int>(req.request_type));
                    break;
                }
                std::vector<std::string>                            header;
                std::vector<rocprofvis_controller_primitive_type_t> column_types;
                std::vector<std::vector<std::string>>               rows;
                uint64_t                                            total_rows = 0;
                decode_table(data, header, column_types, rows, total_rows);
                if(!ApplyTableData(req, table_type, std::move(header),
                                   std::move(column_types), std::move(rows), total_rows))
                {
                    break;
                }
            }
            else if(req.response_code != kRocProfVisResultCancelled)
            {
                spdlog::warn("Table request failed with code {}", req.response_code);
            }
            FireTableDataReady(req);
            break;
        }
        case RequestType::kTableExport:
        {
            ProcessTableExportRequest(req);
            break;
        }
        case RequestType::kFetchEventExtendedData:
        {
            EventExtDataResult result;
            decode_event_ext_data(data, result);
            ApplyEventExtData(req, result.rows);
            break;
        }
        case RequestType::kFetchEventFlowDetails:
        {
            EventFlowResult result;
            decode_event_flow(data, result);
            ApplyEventFlowData(req, result.rows);
            break;
        }
        case RequestType::kFetchEventCallStack:
        {
            EventCallStackResult result;
            decode_event_call_stack(data, result);
            ApplyEventCallStackData(req, result.rows);
            break;
        }
        case RequestType::kFetchSummary:
        {
            if(success)
            {
                SummaryModel& summary = m_model.GetSummary();
                summary.Clear();
                summary.SetRevision(m_model.GetTopology().GetRevision());
                if(!decode_summary(data, summary, m_model.GetTopology()))
                {
                    spdlog::warn("Summary refers to topology the trace does not have");
                }
            }
            else
            {
                spdlog::warn("Summary request failed with code {}", req.response_code);
            }
            if(m_summary_data_ready_callback)
            {
                m_summary_data_ready_callback();
            }
            break;
        }
        case RequestType::kFetchAnalysisTrackStatistics:
        {
            std::shared_ptr<AnalysisTrackStatisticsRequestParams> params =
                std::dynamic_pointer_cast<AnalysisTrackStatisticsRequestParams>(
                    req.custom_params);
            if(params && success)
            {
                const TrackInfo* metadata = m_model.GetTimeline().GetTrack(params->m_track_id);
                decode_track_statistics(data, *params,
                                        metadata && metadata->topology.type ==
                                                        TrackInfo::TrackType::Counter);
            }
            ProcessAnalysisTrackStatisticsRequest(req);
            break;
        }
        case RequestType::kSaveTrimmedTrace:
        {
            ProcessSaveTrimmedTraceRequest(req);
            break;
        }
        case RequestType::kCleanupDatabase:
        {
            ProcessCleanupDatabaseRequest(req);
            break;
        }
        case RequestType::kFetchMetrics:
        {
            MetricsResult result;
            if(success)
            {
                decode_metrics(data, result);
            }
            ApplyMetricsData(req, result.rows);
            break;
        }
        case RequestType::kFetchMetricPivotTable:
        {
            std::shared_ptr<ComputeTableRequestParams> params =
                std::dynamic_pointer_cast<ComputeTableRequestParams>(req.custom_params);
            if(params)
            {
                if(success)
                {
                    m_compute_model.GetKernelSelectionTable().Clear();
                    ComputeTableInfo& table =
                        m_compute_model.GetKernelSelectionTable().GetTableInfoMutable();
                    decode_kernel_selection_table(data, table);
                    table.table_params = params;
                }
                else
                {
                    spdlog::error("Metric pivot table request failed with code {}",
                                  req.response_code);
                }
                FireTableDataReady(req);
            }
            break;
        }
        case RequestType::kFetchPcSampling:
        {
            ProcessRemotePcSampling(req, data);
            break;
        }
#ifdef ROCPROFVIS_ENABLE_SCRIPTING
        case RequestType::kExecuteScript:
        {
            std::string error = json_get_string(data, "error");
            ApplyScriptResult(success && json_get_bool(data, "ok"), json_get_string(data, "text"),
                              error.empty() && !success ? req.error_message : error);
            break;
        }
#endif
        default:
        {
            spdlog::debug("Unknown request type {}", static_cast<int>(req.request_type));
            break;
        }
    }
}

void
DataProvider::ProcessRemoteSystemTrace(RequestInfo& req, jt::Json& data)
{
    if(req.response_code != kRocProfVisResultSuccess)
    {
        spdlog::error("Failed to load trace file: {}, error code: {}",
                      m_model.GetTraceFilePath(), req.response_code);
        m_state = ProviderState::kError;
        if(m_trace_data_ready_callback)
        {
            m_trace_data_ready_callback(m_model.GetTraceFilePath(), req.response_code);
        }
        return;
    }

    decode_system_trace(data, m_model);
    m_state = ProviderState::kReady;
    if(m_trace_data_ready_callback)
    {
        m_trace_data_ready_callback(m_model.GetTraceFilePath(), kRocProfVisResultSuccess);
    }
}

void
DataProvider::ProcessRemoteComputeTrace(RequestInfo& req, jt::Json& data)
{
    m_state = ProviderState::kError;
    if(req.response_code != kRocProfVisResultSuccess)
    {
        spdlog::error("Failed to load trace file: {}, error code: {}",
                      m_model.GetTraceFilePath(), req.response_code);
        if(m_trace_data_ready_callback)
        {
            m_trace_data_ready_callback(m_model.GetTraceFilePath(), req.response_code);
        }
        return;
    }

    decode_compute_trace(data, m_compute_model);
    m_state = ProviderState::kReady;
    if(m_trace_data_ready_callback)
    {
        m_trace_data_ready_callback(m_model.GetTraceFilePath(), kRocProfVisResultSuccess);
    }
}

void
DataProvider::ProcessRemotePcSampling(RequestInfo& req, jt::Json& data)
{
    std::shared_ptr<PcSamplingRequestParams> params =
        std::dynamic_pointer_cast<PcSamplingRequestParams>(req.custom_params);
    if(!params)
    {
        return;
    }
    if(m_pc_sampling_replacements.count(req.request_id))
    {
        spdlog::debug("Discarding superseded PC sampling {} request",
                      static_cast<uint32_t>(params->m_layer));
        return;
    }

    const bool success                    = (req.response_code == kRocProfVisResultSuccess);
    uint64_t   completed_source_file_uuid = params->m_source_file_uuid;
    if(req.response_code != kRocProfVisResultCancelled &&
       (success || params->m_layer == PcSamplingLayer::kStalls))
    {
        KernelInfo* kernel =
            m_compute_model.GetKernelInfoMutable(params->m_workload_id, params->m_kernel_id);
        if(kernel)
        {
            decode_pc_sampling(data, kernel->pc_sampling_data, params->m_layer);
            if(params->m_layer == PcSamplingLayer::kSource &&
               completed_source_file_uuid == 0 &&
               !kernel->pc_sampling_data.source_files.empty())
            {
                completed_source_file_uuid =
                    kernel->pc_sampling_data.source_files.front().source_file_uuid;
            }
        }
    }
    if(!success && req.response_code != kRocProfVisResultCancelled)
    {
        spdlog::warn("PC sampling request failed with code {}", req.response_code);
    }

    if(m_pc_sampling_fetch_callback)
    {
        m_pc_sampling_fetch_callback(m_model.GetTraceFilePath(), params->m_layer,
                                     params->m_kernel_id, completed_source_file_uuid,
                                     params->m_generation, params->m_request_token,
                                     static_cast<rocprofvis_result_t>(req.response_code));
    }
}

bool
DataProvider::RemoteFetchTrack(const TrackRequestParams& request_params, RequestType type,
                               uint64_t request_id)
{
    jt::Json params = encode_track_request(request_params);
    return SubmitRemoteRequest(
        make_remote_request(request_id, type,
                            std::make_shared<TrackRequestParams>(request_params)),
        type == RequestType::kFetchGraph ? OP_GRAPH : OP_TRACK, params);
}

bool
DataProvider::RemoteFetchTrackTable(const TrackTableRequestParams& table_params)
{
    uint64_t request_id = 0;
    if(!GetTrackTableRequestId(table_params, request_id))
    {
        return false;
    }
    std::vector<uint64_t> filtered_track_ids = FilterTableTrackIds(table_params);
    if(filtered_track_ids.empty())
    {
        spdlog::debug("No valid tracks found for table request, aborting");
        return false;
    }

    std::shared_ptr<TrackTableRequestParams> stored_params =
        std::make_shared<TrackTableRequestParams>(table_params);
    stored_params->m_track_ids = std::move(filtered_track_ids);
    jt::Json params            = encode_track_table_request(*stored_params);
    if(!table_params.m_export_to_file_path.empty())
    {
        m_remote_channel->ReceiveFile(request_id, table_params.m_export_to_file_path);
    }
    return SubmitRemoteRequest(make_remote_request(request_id,
                                                   GetTrackTableRequestType(table_params),
                                                   stored_params),
                               OP_TRACK_TABLE, params);
}

bool
DataProvider::RemoteFetchEventSearch(const EventSearchRequestParams& table_params)
{
    uint64_t request_id = 0;
    if(!GetEventSearchRequestId(table_params, request_id))
    {
        return false;
    }
    std::shared_ptr<EventSearchRequestParams> stored_params =
        std::make_shared<EventSearchRequestParams>(table_params);
    jt::Json params = encode_event_search_request(*stored_params);
    if(!table_params.m_export_to_file_path.empty())
    {
        m_remote_channel->ReceiveFile(request_id, table_params.m_export_to_file_path);
    }
    return SubmitRemoteRequest(make_remote_request(request_id,
                                                   GetEventSearchRequestType(table_params),
                                                   stored_params),
                               OP_EVENT_SEARCH, params);
}

bool
DataProvider::RemoteFetchEventData(uint64_t event_id, RequestType type, uint64_t request_id,
                                   const char* operation)
{
    if(m_requests.count(request_id))
    {
        spdlog::debug("Event request {} is already pending", request_id);
        return false;
    }
    jt::Json params;
    params["event_id"] = json_from_uint64(event_id);
    return SubmitRemoteRequest(
        make_remote_request(request_id, type, std::make_shared<EventRequestParams>(event_id)),
        operation, params);
}

bool
DataProvider::RemoteFetchSummary()
{
    jt::Json params;
    params.setObject();
    return SubmitRemoteRequest(
        make_remote_request(SUMMARY_REQUEST_ID, RequestType::kFetchSummary, nullptr),
        OP_SUMMARY, params);
}

bool
DataProvider::RemoteFetchAnalysisTrackStatistics(
    const AnalysisTrackStatisticsRequestParams& params)
{
    const uint64_t request_id = RequestIdBuilder::MakeTrackDataRequestId(
        static_cast<uint32_t>(params.m_track_id), 0, 0,
        RequestType::kFetchAnalysisTrackStatistics);
    jt::Json json = encode_track_statistics_request(params);
    return SubmitRemoteRequest(
        make_remote_request(request_id, RequestType::kFetchAnalysisTrackStatistics,
                            std::make_shared<AnalysisTrackStatisticsRequestParams>(params)),
        OP_TRACK_STATISTICS, json);
}

bool
DataProvider::RemoteSaveTrimmedTrace(const std::string& path, double start_ns, double end_ns)
{
    jt::Json params;
    params["start"] = json_from_double(start_ns);
    params["end"]   = json_from_double(end_ns);
    m_remote_channel->ReceiveFile(SAVE_TRIMMED_TRACE_REQUEST_ID, path);
    return SubmitRemoteRequest(make_remote_request(SAVE_TRIMMED_TRACE_REQUEST_ID,
                                                   RequestType::kSaveTrimmedTrace, nullptr),
                               OP_SAVE_TRIMMED, params);
}

bool
DataProvider::RemoteCleanupDatabase(bool rebuild)
{
    jt::Json params;
    params["rebuild"] = rebuild;
    return SubmitRemoteRequest(make_remote_request(CLEANUP_DATABASE_REQUEST_ID,
                                                   RequestType::kCleanupDatabase, nullptr),
                               OP_CLEANUP_DATABASE, params);
}

bool
DataProvider::RemoteFetchMetrics(const MetricsRequestParams& metrics_params)
{
    const uint64_t request_id = RequestIdBuilder::MakeClientRequestId(
        RequestType::kFetchMetrics, metrics_params.m_client_id);
    jt::Json params = encode_metrics_request(metrics_params);
    return SubmitRemoteRequest(
        make_remote_request(request_id, RequestType::kFetchMetrics,
                            std::make_shared<MetricsRequestParams>(metrics_params)),
        OP_METRICS, params);
}

bool
DataProvider::RemoteFetchMetricPivotTable(const ComputeTableRequestParams& table_params)
{
    jt::Json params = encode_compute_table_request(table_params);
    return SubmitRemoteRequest(
        make_remote_request(METRIC_PIVOT_TABLE_REQUEST_ID, RequestType::kFetchMetricPivotTable,
                            std::make_shared<ComputeTableRequestParams>(table_params)),
        OP_METRIC_PIVOT, params);
}

bool
DataProvider::RemoteFetchPcSampling(const PcSamplingRequestParams& params)
{
    const uint64_t request_id = RequestIdBuilder::MakeClientRequestId(
        RequestType::kFetchPcSampling, static_cast<uint64_t>(params.m_layer));

    if(m_requests.count(request_id))
    {
        // As FetchPcSampling: the new params replace the in-flight request,
        // which is submitted once the old one has answered.
        RemoteCancelRequest(request_id);
        m_pc_sampling_replacements.insert_or_assign(request_id, params);
        spdlog::debug("PC sampling {} request queued as replacement",
                      static_cast<uint32_t>(params.m_layer));
        return true;
    }

    if(!m_compute_model.GetKernelInfo(params.m_workload_id, params.m_kernel_id))
    {
        return false;
    }

    jt::Json json = encode_pc_sampling_request(params);
    return SubmitRemoteRequest(
        make_remote_request(request_id, RequestType::kFetchPcSampling,
                            std::make_shared<PcSamplingRequestParams>(params)),
        OP_PC_SAMPLING, json);
}

bool
DataProvider::RemoteCancelRequest(uint64_t request_id)
{
    if(!m_requests.count(request_id))
    {
        spdlog::debug("Cannot cancel request, invalid id: {}", request_id);
        return false;
    }
    jt::Json message;
    message["type"] = MSG_CANCEL;
    message["id"]   = json_from_uint64(request_id);
    return m_remote_channel->Send(message);
}

void
DataProvider::RemoteSetSelected()
{
    jt::Json message;
    message["type"] = MSG_SET_SELECTED;
    m_remote_channel->Send(message);
}

void
DataProvider::CloseRemote()
{
    if(m_remote_channel)
    {
        m_remote_channel->Close();
        m_remote_channel.reset();
    }
}

}  // namespace View
}  // namespace RocProfVis
