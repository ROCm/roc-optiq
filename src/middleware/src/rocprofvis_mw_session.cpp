// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_mw_session.h"

#include <float.h>
#include <stdint.h>

#include <map>
#include <string>
#include <utility>
#include <vector>

#include "json.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_analysis.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_mw_enums.h"
#include "rocprofvis_mw_json.h"
#include "rocprofvis_mw_serialize.h"

namespace RocProfVis
{
namespace Middleware
{

/*
 * Upper bound on an inline "wait_ms" budget. A client that wants to block
 * longer should poll; capping keeps one slow request from monopolising a
 * single-threaded transport such as the stdio adapter.
 */
static constexpr uint32_t MAX_WAIT_MS = 30000;

/* Seconds-per-millisecond, for the controller's float timeout. */
static constexpr float MS_TO_SECONDS = 0.001f;

static char const*
StatusToString(RequestStatus status)
{
    char const* name = "pending";
    switch(status)
    {
        case RequestStatus::kReady: name = "ready"; break;
        case RequestStatus::kError: name = "error"; break;
        case RequestStatus::kCancelled: name = "cancelled"; break;
        case RequestStatus::kPending: break;
    }
    return name;
}

char const*
TraceStateToString(TraceState state)
{
    char const* name = "empty";
    switch(state)
    {
        case TraceState::kLoading: name = "loading"; break;
        case TraceState::kReady: name = "ready"; break;
        case TraceState::kError: name = "error"; break;
        case TraceState::kEmpty: break;
    }
    return name;
}

const Session::method_entry_t Session::METHODS[] = {
    { "session.info", &Session::MethodSessionInfo },
    { "trace.open", &Session::MethodTraceOpen },
    { "trace.close", &Session::MethodTraceClose },
    { "trace.status", &Session::MethodTraceStatus },
    { "trace.topology", &Session::MethodTraceTopology },
    { "trace.histogram", &Session::MethodTraceHistogram },
    { "trace.save_trimmed", &Session::MethodTraceSaveTrimmed },
    { "trace.cleanup", &Session::MethodTraceCleanup },
    { "timeline.info", &Session::MethodTimelineInfo },
    { "track.fetch", &Session::MethodTrackFetch },
    { "graph.fetch", &Session::MethodGraphFetch },
    { "table.fetch", &Session::MethodTableFetch },
    { "table.export_csv", &Session::MethodTableExport },
    { "summary.fetch", &Session::MethodSummaryFetch },
    { "event.ext_data", &Session::MethodEventExtData },
    { "event.flow", &Session::MethodEventFlow },
    { "event.callstack", &Session::MethodEventCallstack },
    { "analysis.track_statistics", &Session::MethodAnalysisTrackStatistics },
    { "request.poll", &Session::MethodRequestPoll },
    { "request.cancel", &Session::MethodRequestCancel },
    { "request.list", &Session::MethodRequestList },
};

const size_t Session::METHOD_COUNT = sizeof(Session::METHODS) / sizeof(Session::METHODS[0]);

Session::Session()
: m_controller(nullptr)
, m_timeline(nullptr)
, m_state(TraceState::kEmpty)
, m_load_result(kRocProfVisResultSuccess)
, m_next_request_id(1)
{}

Session::~Session()
{
    ReleaseAllRequests();
    if(m_controller != nullptr)
    {
        rocprofvis_analysis_free_trace_data(m_controller);
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
    }
}

std::string
Session::Execute(const std::string& request_json)
{
    jt::Json    request;
    std::string parse_error;
    jt::Json    response = Json::MakeObject();

    if(!Json::Parse(request_json, request, parse_error))
    {
        jt::Json error   = Json::MakeObject();
        error["code"]    = jt::Json(std::string("parse_error"));
        error["message"] = jt::Json(parse_error);
        response["ok"]   = jt::Json(false);
        response["error"] = error;
        return response.toString();
    }

    /*
     * Echo the client's correlation id verbatim when present. It is opaque to
     * the middleware, which lets a client use a string, a number, or nothing.
     */
    if(Json::Has(request, "id"))
    {
        response["id"] = Json::GetValue(request, "id");
    }

    std::string method = Json::GetString(request, "method", "");
    response["method"] = jt::Json(method);

    /* Advance everything in flight before serving the request. */
    PumpRequests();

    method_fn_t handler = nullptr;
    for(size_t i = 0; i < METHOD_COUNT; i++)
    {
        if(method == METHODS[i].m_name)
        {
            handler = METHODS[i].m_function;
            break;
        }
    }

    if(handler == nullptr)
    {
        jt::Json error    = Json::MakeObject();
        error["code"]     = jt::Json(std::string("unknown_method"));
        error["message"]  = jt::Json("no such method: " + method);
        response["ok"]    = jt::Json(false);
        response["error"] = error;
        return response.toString();
    }

    jt::Json       result = Json::MakeObject();
    method_error_t error;
    if((this->*handler)(Json::GetObject(request, "params"), result, error))
    {
        response["ok"]     = jt::Json(true);
        response["result"] = result;
    }
    else
    {
        jt::Json error_json    = Json::MakeObject();
        error_json["code"]     = jt::Json(error.m_code);
        error_json["message"]  = jt::Json(error.m_message);
        response["ok"]         = jt::Json(false);
        response["error"]      = error_json;
    }
    return response.toString();
}

uint64_t
Session::AllocateRequestId(void)
{
    return m_next_request_id++;
}

pending_request_t&
Session::CreateRequest(RequestKind kind, const jt::Json& params)
{
    pending_request_t request{};
    request.m_id                = AllocateRequestId();
    request.m_kind              = kind;
    request.m_status            = RequestStatus::kPending;
    request.m_future            = nullptr;
    request.m_array             = nullptr;
    request.m_args              = nullptr;
    request.m_summary_metrics   = nullptr;
    request.m_subject           = nullptr;
    request.m_num_columns       = 0;
    request.m_queue_utilization = 0.0;
    request.m_counter_statistics = rocprofvis_analysis_counter_statistics_t{};
    request.m_params            = params;
    request.m_result            = Json::MakeObject();
    request.m_progress          = 0;
    request.m_result_code       = kRocProfVisResultPending;

    std::pair<std::map<uint64_t, pending_request_t>::iterator, bool> inserted =
        m_requests.emplace(request.m_id, std::move(request));
    return inserted.first->second;
}

void
Session::ReleaseRequest(pending_request_t& request)
{
    if(request.m_array != nullptr)
    {
        rocprofvis_controller_array_free(request.m_array);
        request.m_array = nullptr;
    }
    if(request.m_args != nullptr)
    {
        rocprofvis_controller_arguments_free(request.m_args);
        request.m_args = nullptr;
    }
    if(request.m_summary_metrics != nullptr)
    {
        rocprofvis_controller_summary_metric_free(request.m_summary_metrics);
        request.m_summary_metrics = nullptr;
    }
    if(request.m_future != nullptr)
    {
        rocprofvis_controller_future_free(request.m_future);
        request.m_future = nullptr;
    }
}

void
Session::ReleaseAllRequests(void)
{
    for(std::map<uint64_t, pending_request_t>::iterator entry = m_requests.begin();
        entry != m_requests.end(); ++entry)
    {
        /*
         * A future that is still running owns memory the worker thread may be
         * writing into, so it must be cancelled and drained before its
         * containers are freed.
         */
        if(entry->second.m_status == RequestStatus::kPending &&
           entry->second.m_future != nullptr)
        {
            rocprofvis_controller_future_cancel(entry->second.m_future);
            rocprofvis_controller_future_wait(entry->second.m_future, FLT_MAX);
        }
        ReleaseRequest(entry->second);
    }
    m_requests.clear();
}

void
Session::PumpRequests(void)
{
    for(std::map<uint64_t, pending_request_t>::iterator entry = m_requests.begin();
        entry != m_requests.end(); ++entry)
    {
        if(entry->second.m_status == RequestStatus::kPending)
        {
            AdvanceRequest(entry->second, 0);
        }
    }
}

bool
Session::AdvanceRequest(pending_request_t& request, uint32_t timeout_ms)
{
    if(request.m_status != RequestStatus::kPending)
    {
        return true;
    }
    if(request.m_future == nullptr)
    {
        request.m_status      = RequestStatus::kError;
        request.m_result_code = kRocProfVisResultUnknownError;
        request.m_message     = "request has no future";
        return true;
    }

    rocprofvis_result_t wait_result = rocprofvis_controller_future_wait(
        request.m_future, static_cast<float>(timeout_ms) * MS_TO_SECONDS);

    /* Progress is meaningful whether or not the job finished. */
    uint64_t progress = 0;
    if(rocprofvis_controller_get_uint64(request.m_future,
                                        kRPVControllerFutureProgressPercentage, 0,
                                        &progress) == kRocProfVisResultSuccess)
    {
        request.m_progress = progress;
    }
    std::string message;
    if(Serialize::GetString(request.m_future, kRPVControllerFutureProgressMessage, 0,
                            message))
    {
        request.m_message = message;
    }

    if(wait_result == kRocProfVisResultTimeout)
    {
        return false;
    }

    /*
     * future_wait reporting success only means the job stopped running; the
     * operation's own outcome lives in kRPVControllerFutureResult.
     */
    uint64_t job_result = kRocProfVisResultUnknownError;
    if(rocprofvis_controller_get_uint64(request.m_future, kRPVControllerFutureResult, 0,
                                        &job_result) != kRocProfVisResultSuccess)
    {
        job_result = static_cast<uint64_t>(wait_result);
    }

    if(job_result == kRocProfVisResultPending)
    {
        return false;
    }

    request.m_result_code = static_cast<rocprofvis_result_t>(job_result);
    if(job_result == kRocProfVisResultSuccess)
    {
        DecodeRequest(request);
        request.m_status = RequestStatus::kReady;
    }
    else if(job_result == kRocProfVisResultCancelled)
    {
        request.m_status = RequestStatus::kCancelled;
    }
    else
    {
        request.m_status = RequestStatus::kError;
    }

    /*
     * Decoding is done, so the controller-side containers can go back now
     * rather than at session teardown. m_result holds everything the client
     * still needs.
     */
    ReleaseRequest(request);
    return true;
}

void
Session::DecodeRequest(pending_request_t& request)
{
    switch(request.m_kind)
    {
        case RequestKind::kTraceOpen: DecodeTraceOpen(request); break;
        case RequestKind::kTrackFetch:
        case RequestKind::kGraphFetch: DecodeTrackOrGraph(request); break;
        case RequestKind::kTableFetch: DecodeTable(request); break;
        case RequestKind::kSummaryFetch: DecodeSummary(request); break;
        case RequestKind::kEventExtData:
        case RequestKind::kEventFlow:
        case RequestKind::kEventCallstack: DecodeHandleArray(request); break;
        case RequestKind::kQueueUtilization:
        {
            request.m_result["queue_utilization"] =
                Json::MakeDouble(request.m_queue_utilization);
            break;
        }
        case RequestKind::kCounterStatistics:
        {
            jt::Json statistics    = Json::MakeObject();
            statistics["min"]      = Json::MakeDouble(request.m_counter_statistics.min_value);
            statistics["max"]      = Json::MakeDouble(request.m_counter_statistics.max_value);
            statistics["mean"]     = Json::MakeDouble(request.m_counter_statistics.mean_value);
            statistics["std_dev"]  = Json::MakeDouble(request.m_counter_statistics.std_dev);
            request.m_result["counter_statistics"] = statistics;
            break;
        }
        case RequestKind::kTableExport:
        case RequestKind::kSaveTrimmedTrace:
        case RequestKind::kCleanupDatabase:
        {
            request.m_result["completed"] = jt::Json(true);
            break;
        }
    }
}

void
Session::DecodeTraceOpen(pending_request_t& request)
{
    m_state = TraceState::kReady;

    rocprofvis_handle_t* timeline = nullptr;
    if(rocprofvis_controller_get_object(m_controller, kRPVControllerSystemTimeline, 0,
                                        &timeline) == kRocProfVisResultSuccess)
    {
        m_timeline = timeline;
    }

    jt::Json paths = Json::MakeArray();
    for(size_t i = 0; i < m_trace_paths.size(); i++)
    {
        Json::Append(paths, jt::Json(m_trace_paths[i]));
    }
    request.m_result["paths"]  = paths;
    request.m_result["loaded"] = jt::Json(true);

    if(m_timeline != nullptr)
    {
        double   min_timestamp = 0.0;
        double   max_timestamp = 0.0;
        uint64_t num_tracks    = 0;
        rocprofvis_controller_get_double(m_timeline, kRPVControllerTimelineMinTimestamp, 0,
                                         &min_timestamp);
        rocprofvis_controller_get_double(m_timeline, kRPVControllerTimelineMaxTimestamp, 0,
                                         &max_timestamp);
        rocprofvis_controller_get_uint64(m_timeline, kRPVControllerTimelineNumGraphs, 0,
                                         &num_tracks);
        request.m_result["min_timestamp"] = Json::MakeDouble(min_timestamp);
        request.m_result["max_timestamp"] = Json::MakeDouble(max_timestamp);
        request.m_result["num_tracks"]    = Json::MakeUInt(num_tracks);
    }
}

void
Session::DecodeTrackOrGraph(pending_request_t& request)
{
    jt::Json entries   = Json::MakeArray();
    uint64_t num_entries = 0;
    if(request.m_array == nullptr ||
       rocprofvis_controller_get_uint64(request.m_array, kRPVControllerArrayNumEntries, 0,
                                        &num_entries) != kRocProfVisResultSuccess)
    {
        request.m_result["entries"] = entries;
        return;
    }

    /*
     * A graph fetch reports the shape it produced; a raw track fetch does not,
     * so fall back to the track's own type. Flame graphs carry events, line
     * graphs carry samples, and the two decode differently.
     */
    uint64_t is_event_data = 0;
    if(request.m_kind == RequestKind::kGraphFetch)
    {
        uint64_t graph_type = kRPVControllerGraphTypeLine;
        rocprofvis_controller_get_uint64(request.m_subject, kRPVControllerGraphType, 0,
                                         &graph_type);
        is_event_data                  = (graph_type == kRPVControllerGraphTypeFlame);
        request.m_result["graph_type"] = jt::Json(std::string(
            Enums::GraphTypeToString(graph_type)));
    }
    else
    {
        uint64_t track_type = kRPVControllerTrackTypeSamples;
        rocprofvis_controller_get_uint64(request.m_subject, kRPVControllerTrackType, 0,
                                         &track_type);
        is_event_data                  = (track_type == kRPVControllerTrackTypeEvents);
        request.m_result["track_type"] = jt::Json(std::string(
            Enums::TrackTypeToString(track_type)));
    }

    for(uint64_t i = 0; i < num_entries; i++)
    {
        rocprofvis_handle_t* entry = nullptr;
        if(rocprofvis_controller_get_object(request.m_array, kRPVControllerArrayEntryIndexed,
                                            i, &entry) != kRocProfVisResultSuccess ||
           entry == nullptr)
        {
            continue;
        }
        Json::Append(entries, is_event_data ? Serialize::Event(entry)
                                            : Serialize::Sample(entry));
    }

    request.m_result["kind"]    = jt::Json(std::string(is_event_data ? "events" : "samples"));
    request.m_result["entries"] = entries;
}

void
Session::DecodeTable(pending_request_t& request)
{
    if(request.m_subject == nullptr)
    {
        request.m_result["rows"] = Json::MakeArray();
        return;
    }

    jt::Json schema      = Serialize::TableSchema(request.m_subject);
    uint64_t num_columns = 0;
    rocprofvis_controller_get_uint64(request.m_subject, kRPVControllerTableNumColumns, 0,
                                     &num_columns);
    request.m_num_columns = num_columns;

    request.m_result["columns"]    = Json::GetValue(schema, "columns");
    request.m_result["total_rows"] = Json::GetValue(schema, "total_rows");
    request.m_result["start_row"] =
        Json::MakeUInt(Json::GetUInt(request.m_params, "start_row", 0));
    request.m_result["rows"] =
        (request.m_array != nullptr)
            ? Serialize::TableRows(request.m_array, request.m_subject, num_columns)
            : Json::MakeArray();
}

void
Session::DecodeSummary(pending_request_t& request)
{
    if(request.m_summary_metrics != nullptr)
    {
        request.m_result["summary"] =
            Serialize::SummaryMetrics(request.m_summary_metrics, SUMMARY_MAX_DEPTH);
    }
}

void
Session::DecodeHandleArray(pending_request_t& request)
{
    jt::Json entries    = Json::MakeArray();
    uint64_t num_entries = 0;
    if(request.m_array != nullptr &&
       rocprofvis_controller_get_uint64(request.m_array, kRPVControllerArrayNumEntries, 0,
                                        &num_entries) == kRocProfVisResultSuccess)
    {
        for(uint64_t i = 0; i < num_entries; i++)
        {
            rocprofvis_handle_t* entry = nullptr;
            if(rocprofvis_controller_get_object(request.m_array,
                                                kRPVControllerArrayEntryIndexed, i,
                                                &entry) != kRocProfVisResultSuccess ||
               entry == nullptr)
            {
                continue;
            }

            switch(request.m_kind)
            {
                case RequestKind::kEventExtData:
                    Json::Append(entries, Serialize::ExtDataEntry(entry));
                    break;
                case RequestKind::kEventFlow:
                    Json::Append(entries, Serialize::FlowControlEntry(entry));
                    break;
                case RequestKind::kEventCallstack:
                    Json::Append(entries, Serialize::CallstackFrame(entry));
                    break;
                default: break;
            }
        }
    }
    request.m_result["entries"] = entries;
}

jt::Json
Session::DescribeRequest(const pending_request_t& request)
{
    jt::Json described        = Json::MakeObject();
    described["request_id"]   = Json::MakeUInt(request.m_id);
    described["status"]       = jt::Json(std::string(StatusToString(request.m_status)));
    described["progress"]     = Json::MakeUInt(request.m_progress);
    if(!request.m_message.empty())
    {
        described["message"] = jt::Json(request.m_message);
    }

    if(request.m_status == RequestStatus::kReady)
    {
        described["result"] = request.m_result;
    }
    else if(request.m_status == RequestStatus::kError)
    {
        described["error_code"] =
            jt::Json(std::string(Enums::ResultToString(request.m_result_code)));
    }
    return described;
}

jt::Json
Session::DeliverRequest(pending_request_t& request)
{
    jt::Json described = DescribeRequest(request);

    /*
     * A terminal result is handed over exactly once, whether it was waited on
     * inline or polled for. The registry tracks work still in flight; keeping
     * finished entries would grow it for the life of the session, and a client
     * that needs the data again can re-issue the fetch.
     */
    if(request.m_status != RequestStatus::kPending)
    {
        m_requests.erase(request.m_id);
    }
    return described;
}

void
Session::ApplyWaitBudget(pending_request_t& request, const jt::Json& params)
{
    uint64_t wait_ms = Json::GetUInt(params, "wait_ms", 0);
    if(wait_ms > 0)
    {
        if(wait_ms > MAX_WAIT_MS)
        {
            wait_ms = MAX_WAIT_MS;
        }
        AdvanceRequest(request, static_cast<uint32_t>(wait_ms));
    }
}

bool
Session::RequireTrace(method_error_t& error)
{
    bool ready = true;
    if(m_controller == nullptr)
    {
        error.m_code    = "no_trace";
        error.m_message = "no trace is open; call trace.open first";
        ready           = false;
    }
    else if(m_state == TraceState::kLoading)
    {
        error.m_code    = "trace_loading";
        error.m_message = "trace is still loading";
        ready           = false;
    }
    else if(m_state != TraceState::kReady)
    {
        error.m_code    = "trace_not_ready";
        error.m_message = "trace failed to load";
        ready           = false;
    }
    return ready;
}

bool
Session::ResolveTrack(uint64_t track_id, rocprofvis_handle_t** track,
                      method_error_t& error)
{
    bool resolved = rocprofvis_controller_get_object(m_controller,
                                                     kRPVControllerSystemTrackById,
                                                     track_id, track) ==
                        kRocProfVisResultSuccess &&
                    *track != nullptr;
    if(!resolved)
    {
        error.m_code    = "unknown_track";
        error.m_message = "no track with id " + std::to_string(track_id);
    }
    return resolved;
}

bool
Session::ResolveGraph(uint64_t track_id, rocprofvis_handle_t** graph,
                      method_error_t& error)
{
    bool resolved = false;
    if(m_timeline == nullptr)
    {
        error.m_code    = "no_timeline";
        error.m_message = "trace has no timeline";
    }
    else
    {
        resolved = rocprofvis_controller_get_object(m_timeline,
                                                    kRPVControllerTimelineGraphById,
                                                    track_id, graph) ==
                       kRocProfVisResultSuccess &&
                   *graph != nullptr;
        if(!resolved)
        {
            error.m_code    = "unknown_track";
            error.m_message = "no graph for track id " + std::to_string(track_id);
        }
    }
    return resolved;
}

bool
Session::ResolveTable(rocprofvis_controller_table_type_t table_type,
                      rocprofvis_handle_t** table, method_error_t& error)
{
    rocprofvis_result_t result = kRocProfVisResultNotSupported;
    switch(table_type)
    {
        case kRPVControllerTableTypeEvents:
            result = rocprofvis_controller_get_object(m_controller,
                                                      kRPVControllerSystemEventTable, 0,
                                                      table);
            break;
        case kRPVControllerTableTypeSamples:
            result = rocprofvis_controller_get_object(m_controller,
                                                      kRPVControllerSystemSampleTable, 0,
                                                      table);
            break;
        case kRPVControllerTableTypeSearchResults:
            result = rocprofvis_controller_get_object(
                m_controller, kRPVControllerSystemSearchResultsTable, 0, table);
            break;
        case kRPVControllerTableTypeSummaryKernelInstances:
        {
            rocprofvis_handle_t* summary = nullptr;
            result = rocprofvis_controller_get_object(m_controller,
                                                      kRPVControllerSystemSummary, 0,
                                                      &summary);
            if(result == kRocProfVisResultSuccess && summary != nullptr)
            {
                result = rocprofvis_controller_get_object(
                    summary, kRPVControllerSummaryPropertyKernelInstanceTable, 0, table);
            }
            break;
        }
        case kRPVControllerTableTypeInstrumentedEvents:
            result = rocprofvis_analysis_get_instrumented_events_table(m_controller, table);
            break;
        case kRPVControllerTableTypeDispatchEvents:
            result = rocprofvis_analysis_get_dispatch_events_table(m_controller, table);
            break;
        case kRPVControllerTableTypeMemoryAllocationEvents:
            result = rocprofvis_analysis_get_memory_allocation_events_table(m_controller,
                                                                            table);
            break;
        case kRPVControllerTableTypeMemoryCopyEvents:
            result = rocprofvis_analysis_get_memory_copy_events_table(m_controller, table);
            break;
        case kRPVControllerTableTypeSampledEvents:
            result = rocprofvis_analysis_get_sampled_events_table(m_controller, table);
            break;
        default: break;
    }

    bool resolved = (result == kRocProfVisResultSuccess) && (*table != nullptr);
    if(!resolved)
    {
        error.m_code    = "table_unavailable";
        error.m_message = std::string("could not resolve table '") +
                          Enums::TableTypeToString(table_type) + "': " +
                          Enums::ResultToString(result);
    }
    return resolved;
}

bool
Session::BuildTableArguments(rocprofvis_controller_arguments_t* args,
                             rocprofvis_controller_table_type_t table_type,
                             const jt::Json& params, method_error_t& error)
{
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsType, 0, table_type);

    /*
     * The controller clips a table to a time window, so defaulting the bounds
     * to zero would ask for the empty range and answer with no rows at all.
     * A caller that named no window means the whole trace.
     */
    double start_time = 0.0;
    double end_time   = 0.0;
    if(m_timeline != nullptr)
    {
        rocprofvis_controller_get_double(m_timeline, kRPVControllerTimelineMinTimestamp,
                                         0, &start_time);
        rocprofvis_controller_get_double(m_timeline, kRPVControllerTimelineMaxTimestamp,
                                         0, &end_time);
    }
    rocprofvis_controller_set_double(args, kRPVControllerTableArgsStartTime, 0,
                                     Json::GetDouble(params, "start_time", start_time));
    rocprofvis_controller_set_double(args, kRPVControllerTableArgsEndTime, 0,
                                     Json::GetDouble(params, "end_time", end_time));
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortColumn, 0,
                                     Json::GetUInt(params, "sort_column", 0));

    rocprofvis_controller_sort_order_t sort_order = kRPVControllerSortOrderAscending;
    std::string sort_order_name = Json::GetString(params, "sort_order", "asc");
    if(!Enums::SortOrderFromString(sort_order_name, sort_order))
    {
        error.m_code    = "invalid_argument";
        error.m_message = "unknown sort_order '" + sort_order_name + "'";
        return false;
    }
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortOrder, 0, sort_order);

    /*
     * The controller expects these four to always be set, using the empty
     * string to mean "no clause".
     */
    std::string where         = Json::GetString(params, "where", "");
    std::string filter        = Json::GetString(params, "filter", "");
    std::string group         = Json::GetString(params, "group", "");
    std::string group_columns = Json::GetString(params, "group_columns", "");
    rocprofvis_controller_set_string(args, kRPVControllerTableArgsWhere, 0, where.c_str());
    rocprofvis_controller_set_string(args, kRPVControllerTableArgsFilter, 0,
                                     filter.c_str());
    rocprofvis_controller_set_string(args, kRPVControllerTableArgsGroup, 0, group.c_str());
    rocprofvis_controller_set_string(args, kRPVControllerTableArgsGroupColumns, 0,
                                     group_columns.c_str());

    /* Paging is optional; omitting either bound means "no bound". */
    if(Json::Has(params, "start_row"))
    {
        rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartIndex, 0,
                                         Json::GetUInt(params, "start_row", 0));
    }
    if(Json::Has(params, "row_count"))
    {
        rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartCount, 0,
                                         Json::GetUInt(params, "row_count", 0));
    }

    /*
     * The three collection counts below are read unconditionally by the
     * controller, so they have to be present even when the collection is
     * empty; leaving one out fails the whole request with an invalid
     * argument rather than being treated as zero.
     */
    std::vector<uint64_t> track_ids = Json::GetUIntArray(params, "track_ids");
    std::vector<uint64_t> op_types  = Json::GetUIntArray(params, "operation_types");

    /*
     * Every table query is scoped by a selection: the two summary-style tables
     * are scoped by operation type, the rest by track. Whichever applies, the
     * query builder reads its first element without first checking that one
     * exists, so an empty selection has to be refused here.
     */
    bool op_type_scoped = (table_type == kRPVControllerTableTypeSearchResults) ||
                          (table_type == kRPVControllerTableTypeSummaryKernelInstances);
    if(op_type_scoped && op_types.empty())
    {
        error.m_code    = "invalid_argument";
        error.m_message = std::string("table '") + Enums::TableTypeToString(table_type) +
                          "' requires a non-empty 'operation_types'";
        return false;
    }
    if(!op_type_scoped && track_ids.empty())
    {
        error.m_code    = "invalid_argument";
        error.m_message = std::string("table '") + Enums::TableTypeToString(table_type) +
                          "' requires a non-empty 'track_ids'";
        return false;
    }

    /*
     * A track of the wrong kind is dropped while packing rather than reported,
     * which empties the selection again further down, so the kinds are checked
     * up front where the offending id can still be named.
     */
    rocprofvis_controller_track_type_t wanted_track_type =
        (table_type == kRPVControllerTableTypeSamples) ? kRPVControllerTrackTypeSamples
                                                       : kRPVControllerTrackTypeEvents;

    for(size_t i = 0; i < track_ids.size(); i++)
    {
        rocprofvis_handle_t* track = nullptr;
        if(!ResolveTrack(track_ids[i], &track, error))
        {
            return false;
        }

        uint64_t track_type = 0;
        rocprofvis_controller_get_uint64(track, kRPVControllerTrackType, 0, &track_type);
        if(!op_type_scoped && track_type != static_cast<uint64_t>(wanted_track_type))
        {
            error.m_code = "invalid_argument";
            error.m_message = "track " + std::to_string(track_ids[i]) + " is a '" +
                              Enums::TrackTypeToString(track_type) + "' track, but table '" +
                              Enums::TableTypeToString(table_type) + "' needs a '" +
                              Enums::TrackTypeToString(wanted_track_type) + "' track";
            return false;
        }

        rocprofvis_controller_set_object(args, kRPVControllerTableArgsTracksIndexed, i,
                                         track);
    }
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumTracks, 0,
                                     track_ids.size());

    for(size_t i = 0; i < op_types.size(); i++)
    {
        rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsOpTypesIndexed, i,
                                         op_types[i]);
    }
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumOpTypes, 0,
                                     op_types.size());

    std::vector<std::string> string_filters =
        Json::GetStringArray(params, "string_table_filters");
    for(size_t i = 0; i < string_filters.size(); i++)
    {
        rocprofvis_controller_set_string(
            args, kRPVControllerTableArgsStringTableFiltersIndexed, i,
            string_filters[i].c_str());
    }
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumStringTableFilters, 0,
                                     string_filters.size());

    return true;
}

}  // namespace Middleware
}  // namespace RocProfVis
