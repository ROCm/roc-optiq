// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include <float.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "json.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_analysis.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_mw_enums.h"
#include "rocprofvis_mw_json.h"
#include "rocprofvis_mw_serialize.h"
#include "rocprofvis_mw_session.h"

namespace RocProfVis
{
namespace Middleware
{

/*
 * Entry count a fetch result array starts with. This is a count and not a
 * capacity hint: anything non-zero makes the array report that many entries
 * before the fetch has written any, so a decoder would read off the end.
 */
static constexpr uint32_t INITIAL_ARRAY_ENTRIES = 0;

/* Default horizontal resolution when a graph fetch omits one. */
static constexpr uint64_t DEFAULT_X_RESOLUTION = 1024;

static void
SetInvalidArgument(method_error_t& error, const std::string& message)
{
    error.m_code    = "invalid_argument";
    error.m_message = message;
}

static void
SetControllerError(method_error_t& error, rocprofvis_result_t result,
                   const std::string& what)
{
    error.m_code    = Enums::ResultToString(result);
    error.m_message = what + " failed: " + Enums::ResultToString(result);
}

bool
Session::MethodSessionInfo(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    (void) params;
    (void) error;

    result["protocol_version"] = Json::MakeUInt(PROTOCOL_VERSION);
    result["trace_state"]      = jt::Json(std::string(TraceStateToString(m_state)));

    jt::Json methods = Json::MakeArray();
    for(size_t i = 0; i < METHOD_COUNT; i++)
    {
        Json::Append(methods, jt::Json(std::string(METHODS[i].m_name)));
    }
    result["methods"] = methods;

    /* Scope of this build, so a client can feature-detect rather than guess. */
    jt::Json capabilities        = Json::MakeObject();
    capabilities["system_trace"] = jt::Json(true);
    capabilities["compute_trace"] = jt::Json(false);
    capabilities["remote"]        = jt::Json(false);
    capabilities["profiler"]      = jt::Json(false);
    result["capabilities"]        = capabilities;
    return true;
}

bool
Session::MethodTraceOpen(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    if(m_controller != nullptr)
    {
        error.m_code    = "trace_already_open";
        error.m_message = "a trace is already open; call trace.close first";
        return false;
    }

    /*
     * A single "path" and a "paths" array are both accepted; multiple paths
     * open a combined comparison trace where each file's tracks carry a source
     * instance id.
     */
    std::vector<std::string> paths = Json::GetStringArray(params, "paths");
    std::string              path  = Json::GetString(params, "path", "");
    if(!path.empty())
    {
        paths.insert(paths.begin(), path);
    }
    if(paths.empty())
    {
        SetInvalidArgument(error, "trace.open requires 'path' or a non-empty 'paths'");
        return false;
    }

    if(paths.size() == 1)
    {
        m_controller = rocprofvis_controller_alloc(paths[0].c_str());
    }
    else
    {
        std::vector<char const*> raw_paths;
        raw_paths.reserve(paths.size());
        for(size_t i = 0; i < paths.size(); i++)
        {
            raw_paths.push_back(paths[i].c_str());
        }
        m_controller = rocprofvis_controller_alloc_compare(raw_paths.data(),
                                                           raw_paths.size());
    }

    if(m_controller == nullptr)
    {
        error.m_code    = "open_failed";
        error.m_message = "could not open trace: " + paths[0];
        return false;
    }

    /*
     * Compute traces use a different controller and a different property
     * vocabulary; rejecting here gives a clear error instead of a stream of
     * unsupported-property failures later.
     */
    rocprofvis_controller_object_type_t object_type =
        kRPVControllerObjectTypeControllerSystem;
    rocprofvis_controller_get_object_type(m_controller, &object_type);
    if(object_type != kRPVControllerObjectTypeControllerSystem)
    {
        rocprofvis_controller_free(m_controller);
        m_controller    = nullptr;
        error.m_code    = "unsupported_trace";
        error.m_message = std::string("trace is a '") +
                          Enums::ObjectTypeToString(object_type) +
                          "'; this build serves system traces only";
        return false;
    }

    m_trace_paths = paths;
    m_state       = TraceState::kLoading;

    pending_request_t& request = CreateRequest(RequestKind::kTraceOpen, params);
    request.m_future           = rocprofvis_controller_future_alloc();
    if(request.m_future == nullptr)
    {
        m_requests.erase(request.m_id);
        rocprofvis_controller_free(m_controller);
        m_controller    = nullptr;
        m_state         = TraceState::kError;
        error.m_code    = "memory_alloc_error";
        error.m_message = "could not allocate a future";
        return false;
    }

    rocprofvis_result_t load_result =
        rocprofvis_controller_load_async(m_controller, request.m_future);
    if(load_result != kRocProfVisResultSuccess)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        rocprofvis_controller_free(m_controller);
        m_controller  = nullptr;
        m_state       = TraceState::kError;
        m_load_result = load_result;
        SetControllerError(error, load_result, "trace.open");
        return false;
    }

    ApplyWaitBudget(request, params);
    if(request.m_status == RequestStatus::kError ||
       request.m_status == RequestStatus::kCancelled)
    {
        m_state = TraceState::kError;
    }
    result = DeliverRequest(request);
    return true;
}

bool
Session::MethodTraceClose(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    (void) params;
    (void) error;

    ReleaseAllRequests();
    if(m_controller != nullptr)
    {
        rocprofvis_analysis_free_trace_data(m_controller);
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
    }
    m_timeline = nullptr;
    m_state    = TraceState::kEmpty;
    m_trace_paths.clear();
    m_load_message.clear();
    m_load_result = kRocProfVisResultSuccess;

    result["closed"] = jt::Json(true);
    return true;
}

bool
Session::MethodTraceStatus(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    (void) params;
    (void) error;

    result["state"] = jt::Json(std::string(TraceStateToString(m_state)));

    jt::Json paths = Json::MakeArray();
    for(size_t i = 0; i < m_trace_paths.size(); i++)
    {
        Json::Append(paths, jt::Json(m_trace_paths[i]));
    }
    result["paths"]            = paths;
    result["pending_requests"] = Json::MakeUInt(m_requests.size());
    return true;
}

bool
Session::MethodTraceTopology(const jt::Json& params, jt::Json& result,
                             method_error_t& error)
{
    (void) params;
    if(!RequireTrace(error))
    {
        return false;
    }

    /*
     * The topology is emitted as flat, id-keyed collections rather than a
     * nested tree. Objects cross-reference each other (a queue belongs to a
     * processor and a process), so a single tree would have to duplicate nodes
     * or drop edges.
     */
    jt::Json nodes      = Json::MakeArray();
    jt::Json processors = Json::MakeArray();
    jt::Json processes  = Json::MakeArray();
    jt::Json threads    = Json::MakeArray();
    jt::Json queues     = Json::MakeArray();
    jt::Json streams    = Json::MakeArray();
    jt::Json counters   = Json::MakeArray();

    uint64_t num_nodes = 0;
    rocprofvis_controller_get_uint64(m_controller, kRPVControllerSystemNumNodes, 0,
                                     &num_nodes);

    for(uint64_t node_index = 0; node_index < num_nodes; node_index++)
    {
        rocprofvis_handle_t* node = nullptr;
        if(rocprofvis_controller_get_object(m_controller, kRPVControllerSystemNodeIndexed,
                                            node_index, &node) != kRocProfVisResultSuccess ||
           node == nullptr)
        {
            continue;
        }
        Json::Append(nodes, Serialize::Node(node));

        uint64_t num_processors = 0;
        rocprofvis_controller_get_uint64(node, kRPVControllerNodeNumProcessors, 0,
                                         &num_processors);
        for(uint64_t i = 0; i < num_processors; i++)
        {
            rocprofvis_handle_t* processor = nullptr;
            if(rocprofvis_controller_get_object(node, kRPVControllerNodeProcessorIndexed, i,
                                                &processor) != kRocProfVisResultSuccess ||
               processor == nullptr)
            {
                continue;
            }
            Json::Append(processors, Serialize::Processor(processor));

            uint64_t num_queues = 0;
            rocprofvis_controller_get_uint64(processor, kRPVControllerProcessorNumQueues, 0,
                                             &num_queues);
            for(uint64_t j = 0; j < num_queues; j++)
            {
                rocprofvis_handle_t* queue = nullptr;
                if(rocprofvis_controller_get_object(processor,
                                                    kRPVControllerProcessorQueueIndexed, j,
                                                    &queue) == kRocProfVisResultSuccess &&
                   queue != nullptr)
                {
                    Json::Append(queues, Serialize::Queue(queue));
                }
            }

            uint64_t num_counters = 0;
            rocprofvis_controller_get_uint64(processor, kRPVControllerProcessorNumCounters,
                                             0, &num_counters);
            for(uint64_t j = 0; j < num_counters; j++)
            {
                rocprofvis_handle_t* counter = nullptr;
                if(rocprofvis_controller_get_object(processor,
                                                    kRPVControllerProcessorCounterIndexed,
                                                    j, &counter) ==
                       kRocProfVisResultSuccess &&
                   counter != nullptr)
                {
                    Json::Append(counters, Serialize::Counter(counter));
                }
            }
        }

        uint64_t num_processes = 0;
        rocprofvis_controller_get_uint64(node, kRPVControllerNodeNumProcesses, 0,
                                         &num_processes);
        for(uint64_t i = 0; i < num_processes; i++)
        {
            rocprofvis_handle_t* process = nullptr;
            if(rocprofvis_controller_get_object(node, kRPVControllerNodeProcessIndexed, i,
                                                &process) != kRocProfVisResultSuccess ||
               process == nullptr)
            {
                continue;
            }
            Json::Append(processes, Serialize::Process(process));

            uint64_t num_threads = 0;
            rocprofvis_controller_get_uint64(process, kRPVControllerProcessNumThreads, 0,
                                             &num_threads);
            for(uint64_t j = 0; j < num_threads; j++)
            {
                rocprofvis_handle_t* thread = nullptr;
                if(rocprofvis_controller_get_object(process,
                                                    kRPVControllerProcessThreadIndexed, j,
                                                    &thread) == kRocProfVisResultSuccess &&
                   thread != nullptr)
                {
                    Json::Append(threads, Serialize::Thread(thread));
                }
            }

            uint64_t num_streams = 0;
            rocprofvis_controller_get_uint64(process, kRPVControllerProcessNumStreams, 0,
                                             &num_streams);
            for(uint64_t j = 0; j < num_streams; j++)
            {
                rocprofvis_handle_t* stream = nullptr;
                if(rocprofvis_controller_get_object(process,
                                                    kRPVControllerProcessStreamIndexed, j,
                                                    &stream) == kRocProfVisResultSuccess &&
                   stream != nullptr)
                {
                    Json::Append(streams, Serialize::Stream(stream));
                }
            }
        }
    }

    result["nodes"]      = nodes;
    result["processors"] = processors;
    result["processes"]  = processes;
    result["threads"]    = threads;
    result["queues"]     = queues;
    result["streams"]    = streams;
    result["counters"]   = counters;
    return true;
}

bool
Session::MethodTimelineInfo(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    (void) params;
    if(!RequireTrace(error))
    {
        return false;
    }
    if(m_timeline == nullptr)
    {
        error.m_code    = "no_timeline";
        error.m_message = "trace has no timeline";
        return false;
    }

    double   min_timestamp = 0.0;
    double   max_timestamp = 0.0;
    uint64_t num_tracks    = 0;
    rocprofvis_controller_get_double(m_timeline, kRPVControllerTimelineMinTimestamp, 0,
                                     &min_timestamp);
    rocprofvis_controller_get_double(m_timeline, kRPVControllerTimelineMaxTimestamp, 0,
                                     &max_timestamp);
    rocprofvis_controller_get_uint64(m_timeline, kRPVControllerTimelineNumGraphs, 0,
                                     &num_tracks);

    jt::Json tracks = Json::MakeArray();
    for(uint64_t i = 0; i < num_tracks; i++)
    {
        rocprofvis_handle_t* track = nullptr;
        if(rocprofvis_controller_get_object(m_controller, kRPVControllerSystemTrackIndexed,
                                            i, &track) == kRocProfVisResultSuccess &&
           track != nullptr)
        {
            Json::Append(tracks, Serialize::Track(track));
        }
    }

    result["min_timestamp"] = Json::MakeDouble(min_timestamp);
    result["max_timestamp"] = Json::MakeDouble(max_timestamp);
    result["num_tracks"]    = Json::MakeUInt(num_tracks);
    result["tracks"]        = tracks;
    return true;
}

bool
Session::MethodTraceHistogram(const jt::Json& params, jt::Json& result,
                              method_error_t& error)
{
    if(!RequireTrace(error))
    {
        return false;
    }

    uint64_t num_buckets = 0;
    rocprofvis_controller_get_uint64(
        m_controller, kRPVControllerSystemGetHistogramBucketsNumber, 0, &num_buckets);

    uint64_t num_tracks = 0;
    if(m_timeline != nullptr)
    {
        rocprofvis_controller_get_uint64(m_timeline, kRPVControllerTimelineNumGraphs, 0,
                                         &num_tracks);
    }

    bool include_per_track = Json::GetBool(params, "per_track", true);

    /*
     * The aggregate histogram counts event density only. Sample tracks report
     * a value per bucket rather than a count, so summing them into the same
     * series would mix units.
     */
    std::vector<double> aggregate(static_cast<size_t>(num_buckets), 0.0);
    jt::Json            per_track = Json::MakeArray();

    for(uint64_t track_index = 0; track_index < num_tracks; track_index++)
    {
        rocprofvis_handle_t* track = nullptr;
        if(rocprofvis_controller_get_object(m_controller, kRPVControllerSystemTrackIndexed,
                                            track_index, &track) !=
               kRocProfVisResultSuccess ||
           track == nullptr)
        {
            continue;
        }

        uint64_t track_id   = 0;
        uint64_t track_type = kRPVControllerTrackTypeSamples;
        rocprofvis_controller_get_uint64(track, kRPVControllerTrackId, 0, &track_id);
        rocprofvis_controller_get_uint64(track, kRPVControllerTrackType, 0, &track_type);

        jt::Json buckets = Json::MakeArray();
        for(uint64_t bucket = 0; bucket < num_buckets; bucket++)
        {
            double value = 0.0;
            if(track_type == kRPVControllerTrackTypeSamples)
            {
                rocprofvis_controller_get_double(
                    track, kRPVControllerTrackHistogramBucketValueIndexed, bucket, &value);
            }
            else
            {
                uint64_t density = 0;
                rocprofvis_controller_get_uint64(
                    track, kRPVControllerTrackHistogramBucketDensityIndexed, bucket,
                    &density);
                value = static_cast<double>(density);
                aggregate[static_cast<size_t>(bucket)] += value;
            }
            if(include_per_track)
            {
                Json::Append(buckets, Json::MakeDouble(value));
            }
        }

        if(include_per_track)
        {
            jt::Json entry     = Json::MakeObject();
            entry["track_id"]  = Json::MakeUInt(track_id);
            entry["type"]      = jt::Json(std::string(Enums::TrackTypeToString(track_type)));
            entry["buckets"]   = buckets;
            Json::Append(per_track, entry);
        }
    }

    jt::Json aggregate_json = Json::MakeArray();
    for(size_t i = 0; i < aggregate.size(); i++)
    {
        Json::Append(aggregate_json, Json::MakeDouble(aggregate[i]));
    }

    result["num_buckets"] = Json::MakeUInt(num_buckets);
    result["aggregate"]   = aggregate_json;
    if(include_per_track)
    {
        result["tracks"] = per_track;
    }
    return true;
}

bool
Session::MethodTrackFetch(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    if(!RequireTrace(error))
    {
        return false;
    }

    uint64_t             track_id = Json::GetUInt(params, "track_id", 0);
    rocprofvis_handle_t* track    = nullptr;
    if(!ResolveTrack(track_id, &track, error))
    {
        return false;
    }

    pending_request_t& request = CreateRequest(RequestKind::kTrackFetch, params);
    request.m_future           = rocprofvis_controller_future_alloc();
    request.m_array            = rocprofvis_controller_array_alloc(INITIAL_ARRAY_ENTRIES);
    request.m_subject          = track;

    if(request.m_future == nullptr || request.m_array == nullptr)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        error.m_code    = "memory_alloc_error";
        error.m_message = "could not allocate request containers";
        return false;
    }

    rocprofvis_result_t fetch_result = rocprofvis_controller_track_fetch_async(
        m_controller, track, Json::GetDouble(params, "start_time", 0.0),
        Json::GetDouble(params, "end_time", 0.0), request.m_future, request.m_array);

    if(fetch_result != kRocProfVisResultSuccess)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        SetControllerError(error, fetch_result, "track.fetch");
        return false;
    }

    ApplyWaitBudget(request, params);
    result = DeliverRequest(request);
    return true;
}

bool
Session::MethodGraphFetch(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    if(!RequireTrace(error))
    {
        return false;
    }

    uint64_t             track_id = Json::GetUInt(params, "track_id", 0);
    rocprofvis_handle_t* graph    = nullptr;
    if(!ResolveGraph(track_id, &graph, error))
    {
        return false;
    }

    uint64_t x_resolution = Json::GetUInt(params, "x_resolution", DEFAULT_X_RESOLUTION);
    if(x_resolution == 0)
    {
        SetInvalidArgument(error, "x_resolution must be greater than zero");
        return false;
    }

    pending_request_t& request = CreateRequest(RequestKind::kGraphFetch, params);
    request.m_future           = rocprofvis_controller_future_alloc();
    request.m_array            = rocprofvis_controller_array_alloc(INITIAL_ARRAY_ENTRIES);
    request.m_subject          = graph;

    if(request.m_future == nullptr || request.m_array == nullptr)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        error.m_code    = "memory_alloc_error";
        error.m_message = "could not allocate request containers";
        return false;
    }

    rocprofvis_result_t fetch_result = rocprofvis_controller_graph_fetch_async(
        m_controller, graph, Json::GetDouble(params, "start_time", 0.0),
        Json::GetDouble(params, "end_time", 0.0), static_cast<uint32_t>(x_resolution),
        request.m_future, request.m_array);

    if(fetch_result != kRocProfVisResultSuccess)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        SetControllerError(error, fetch_result, "graph.fetch");
        return false;
    }

    ApplyWaitBudget(request, params);
    result = DeliverRequest(request);
    return true;
}

bool
Session::MethodTableFetch(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    return FetchOrExportTable(params, result, error, false);
}

bool
Session::MethodTableExport(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    return FetchOrExportTable(params, result, error, true);
}

bool
Session::FetchOrExportTable(const jt::Json& params, jt::Json& result,
                            method_error_t& error, bool export_csv)
{
    if(!RequireTrace(error))
    {
        return false;
    }

    std::string table_type_name = Json::GetString(params, "table_type", "");
    rocprofvis_controller_table_type_t table_type = kRPVControllerTableTypeEvents;
    if(!Enums::TableTypeFromString(table_type_name, table_type))
    {
        SetInvalidArgument(error, "unknown table_type '" + table_type_name + "'");
        return false;
    }

    std::string export_path = Json::GetString(params, "path", "");
    if(export_csv && export_path.empty())
    {
        SetInvalidArgument(error, "table.export_csv requires 'path'");
        return false;
    }

    rocprofvis_handle_t* table = nullptr;
    if(!ResolveTable(table_type, &table, error))
    {
        return false;
    }

    pending_request_t& request = CreateRequest(
        export_csv ? RequestKind::kTableExport : RequestKind::kTableFetch, params);
    request.m_future  = rocprofvis_controller_future_alloc();
    request.m_args    = rocprofvis_controller_arguments_alloc();
    request.m_subject = table;
    if(!export_csv)
    {
        request.m_array = rocprofvis_controller_array_alloc(INITIAL_ARRAY_ENTRIES);
    }

    if(request.m_future == nullptr || request.m_args == nullptr ||
       (!export_csv && request.m_array == nullptr))
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        error.m_code    = "memory_alloc_error";
        error.m_message = "could not allocate request containers";
        return false;
    }

    if(!BuildTableArguments(request.m_args, table_type, params, error))
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        return false;
    }

    /*
     * The analysis tables are served by a separate entry point from the three
     * built-in ones, even though the argument shape is identical.
     */
    bool is_analysis_table = (table_type == kRPVControllerTableTypeInstrumentedEvents) ||
                             (table_type == kRPVControllerTableTypeDispatchEvents) ||
                             (table_type == kRPVControllerTableTypeMemoryAllocationEvents) ||
                             (table_type == kRPVControllerTableTypeMemoryCopyEvents) ||
                             (table_type == kRPVControllerTableTypeSampledEvents);

    rocprofvis_result_t fetch_result = kRocProfVisResultNotSupported;
    if(export_csv)
    {
        fetch_result =
            is_analysis_table
                ? rocprofvis_analysis_table_export_csv(m_controller, table, request.m_args,
                                                       request.m_future,
                                                       export_path.c_str())
                : rocprofvis_controller_table_export_csv(m_controller, table,
                                                         request.m_args, request.m_future,
                                                         export_path.c_str());
    }
    else
    {
        fetch_result =
            is_analysis_table
                ? rocprofvis_analysis_fetch_table(m_controller, table, request.m_args,
                                                  request.m_future, request.m_array)
                : rocprofvis_controller_table_fetch_async(m_controller, table,
                                                          request.m_args, request.m_future,
                                                          request.m_array);
    }

    if(fetch_result != kRocProfVisResultSuccess)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        SetControllerError(error, fetch_result, export_csv ? "table.export_csv"
                                                           : "table.fetch");
        return false;
    }

    ApplyWaitBudget(request, params);
    result = DeliverRequest(request);
    return true;
}

bool
Session::MethodSummaryFetch(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    if(!RequireTrace(error))
    {
        return false;
    }

    rocprofvis_handle_t* summary = nullptr;
    rocprofvis_result_t  lookup  = rocprofvis_controller_get_object(
        m_controller, kRPVControllerSystemSummary, 0, &summary);
    if(lookup != kRocProfVisResultSuccess || summary == nullptr)
    {
        SetControllerError(error, lookup, "summary.fetch");
        return false;
    }

    pending_request_t& request = CreateRequest(RequestKind::kSummaryFetch, params);
    request.m_future           = rocprofvis_controller_future_alloc();
    request.m_args             = rocprofvis_controller_arguments_alloc();
    request.m_summary_metrics  = rocprofvis_controller_summary_metrics_alloc();
    request.m_subject          = summary;

    if(request.m_future == nullptr || request.m_args == nullptr ||
       request.m_summary_metrics == nullptr)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        error.m_code    = "memory_alloc_error";
        error.m_message = "could not allocate request containers";
        return false;
    }

    rocprofvis_controller_set_double(request.m_args,
                                     kRPVControllerSummaryArgsStartTimestamp, 0,
                                     Json::GetDouble(params, "start_time", 0.0));
    rocprofvis_controller_set_double(request.m_args, kRPVControllerSummaryArgsEndTimestamp,
                                     0, Json::GetDouble(params, "end_time", 0.0));

    rocprofvis_result_t fetch_result = rocprofvis_controller_summary_fetch_async(
        m_controller, summary, request.m_args, request.m_future,
        request.m_summary_metrics);

    if(fetch_result != kRocProfVisResultSuccess)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        SetControllerError(error, fetch_result, "summary.fetch");
        return false;
    }

    ApplyWaitBudget(request, params);
    result = DeliverRequest(request);
    return true;
}

bool
Session::MethodEventExtData(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    return FetchEventDetail(params, result, error, RequestKind::kEventExtData,
                            kRPVControllerSystemEventDataExtDataIndexed);
}

bool
Session::MethodEventFlow(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    return FetchEventDetail(params, result, error, RequestKind::kEventFlow,
                            kRPVControllerSystemEventDataFlowControlIndexed);
}

bool
Session::MethodEventCallstack(const jt::Json& params, jt::Json& result,
                              method_error_t& error)
{
    return FetchEventDetail(params, result, error, RequestKind::kEventCallstack,
                            kRPVControllerSystemEventDataCallStackIndexed);
}

bool
Session::FetchEventDetail(const jt::Json& params, jt::Json& result, method_error_t& error,
                          RequestKind kind, rocprofvis_property_t property)
{
    if(!RequireTrace(error))
    {
        return false;
    }
    uint64_t        event_id = 0;
    Json::IdStatus  id_status = Json::GetId(params, "event_id", event_id);
    if(id_status != Json::IdStatus::kValid)
    {
        SetInvalidArgument(error, std::string("'event_id' ") +
                                      Json::IdStatusToString(id_status));
        return false;
    }

    pending_request_t& request = CreateRequest(kind, params);
    request.m_future           = rocprofvis_controller_future_alloc();
    request.m_array            = rocprofvis_controller_array_alloc(INITIAL_ARRAY_ENTRIES);

    if(request.m_future == nullptr || request.m_array == nullptr)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        error.m_code    = "memory_alloc_error";
        error.m_message = "could not allocate request containers";
        return false;
    }

    /*
     * Event details are addressed by id against the controller itself; the
     * event id is passed as the index and the count is always one event.
     */
    rocprofvis_result_t fetch_result = rocprofvis_controller_get_indexed_property_async(
        m_controller, m_controller, property, event_id, 1, request.m_future,
        request.m_array);

    if(fetch_result != kRocProfVisResultSuccess)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        SetControllerError(error, fetch_result, "event detail fetch");
        return false;
    }

    ApplyWaitBudget(request, params);
    result = DeliverRequest(request);
    return true;
}

bool
Session::MethodAnalysisTrackStatistics(const jt::Json& params, jt::Json& result,
                                       method_error_t& error)
{
    if(!RequireTrace(error))
    {
        return false;
    }

    uint64_t             track_id = Json::GetUInt(params, "track_id", 0);
    rocprofvis_handle_t* track    = nullptr;
    if(!ResolveTrack(track_id, &track, error))
    {
        return false;
    }

    std::string metric = Json::GetString(params, "metric", "");
    bool        wants_counter_statistics = (metric == "counter_statistics");
    if(!wants_counter_statistics && metric != "queue_utilization")
    {
        SetInvalidArgument(
            error, "metric must be 'queue_utilization' or 'counter_statistics'");
        return false;
    }

    pending_request_t& request =
        CreateRequest(wants_counter_statistics ? RequestKind::kCounterStatistics
                                               : RequestKind::kQueueUtilization,
                      params);
    request.m_future  = rocprofvis_controller_future_alloc();
    request.m_subject = track;
    if(request.m_future == nullptr)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        error.m_code    = "memory_alloc_error";
        error.m_message = "could not allocate a future";
        return false;
    }

    double start_time = Json::GetDouble(params, "start_time", 0.0);
    double end_time   = Json::GetDouble(params, "end_time", 0.0);

    /*
     * The analysis entry points write straight into caller storage from a
     * worker thread, so the destination must be the registry-owned request
     * rather than a local: the request outlives this call, a local would not.
     */
    rocprofvis_result_t fetch_result =
        wants_counter_statistics
            ? rocprofvis_analysis_fetch_counter_statistics(m_controller, track, start_time,
                                                           end_time, request.m_future,
                                                           &request.m_counter_statistics)
            : rocprofvis_analysis_fetch_queue_utilization(m_controller, track, start_time,
                                                          end_time, request.m_future,
                                                          &request.m_queue_utilization);

    if(fetch_result != kRocProfVisResultSuccess)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        SetControllerError(error, fetch_result, "analysis.track_statistics");
        return false;
    }

    ApplyWaitBudget(request, params);
    result = DeliverRequest(request);
    return true;
}

bool
Session::MethodTraceSaveTrimmed(const jt::Json& params, jt::Json& result,
                                method_error_t& error)
{
    if(!RequireTrace(error))
    {
        return false;
    }

    std::string path = Json::GetString(params, "path", "");
    if(path.empty())
    {
        SetInvalidArgument(error, "trace.save_trimmed requires 'path'");
        return false;
    }

    pending_request_t& request = CreateRequest(RequestKind::kSaveTrimmedTrace, params);
    request.m_future           = rocprofvis_controller_future_alloc();
    if(request.m_future == nullptr)
    {
        m_requests.erase(request.m_id);
        error.m_code    = "memory_alloc_error";
        error.m_message = "could not allocate a future";
        return false;
    }

    rocprofvis_result_t save_result = rocprofvis_controller_save_trimmed_trace(
        m_controller, Json::GetDouble(params, "start_time", 0.0),
        Json::GetDouble(params, "end_time", 0.0), path.c_str(), request.m_future);

    if(save_result != kRocProfVisResultSuccess)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        SetControllerError(error, save_result, "trace.save_trimmed");
        return false;
    }

    ApplyWaitBudget(request, params);
    result = DeliverRequest(request);
    return true;
}

bool
Session::MethodTraceCleanup(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    if(!RequireTrace(error))
    {
        return false;
    }

    pending_request_t& request = CreateRequest(RequestKind::kCleanupDatabase, params);
    request.m_future           = rocprofvis_controller_future_alloc();
    if(request.m_future == nullptr)
    {
        m_requests.erase(request.m_id);
        error.m_code    = "memory_alloc_error";
        error.m_message = "could not allocate a future";
        return false;
    }

    rocprofvis_result_t cleanup_result = rocprofvis_controller_cleanup_trace_database(
        m_controller, Json::GetBool(params, "rebuild", false), request.m_future);

    if(cleanup_result != kRocProfVisResultSuccess)
    {
        ReleaseRequest(request);
        m_requests.erase(request.m_id);
        SetControllerError(error, cleanup_result, "trace.cleanup");
        return false;
    }

    ApplyWaitBudget(request, params);
    result = DeliverRequest(request);
    return true;
}

bool
Session::MethodRequestPoll(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    uint64_t request_id = Json::GetUInt(params, "request_id", 0);
    std::map<uint64_t, pending_request_t>::iterator entry = m_requests.find(request_id);
    if(entry == m_requests.end())
    {
        error.m_code    = "unknown_request";
        error.m_message = "no request with id " + std::to_string(request_id);
        return false;
    }

    ApplyWaitBudget(entry->second, params);
    result = DeliverRequest(entry->second);
    return true;
}

bool
Session::MethodRequestCancel(const jt::Json& params, jt::Json& result,
                             method_error_t& error)
{
    uint64_t request_id = Json::GetUInt(params, "request_id", 0);
    std::map<uint64_t, pending_request_t>::iterator entry = m_requests.find(request_id);
    if(entry == m_requests.end())
    {
        error.m_code    = "unknown_request";
        error.m_message = "no request with id " + std::to_string(request_id);
        return false;
    }

    bool cancelled = false;
    if(entry->second.m_status == RequestStatus::kPending &&
       entry->second.m_future != nullptr)
    {
        cancelled = rocprofvis_controller_future_cancel(entry->second.m_future) ==
                    kRocProfVisResultSuccess;

        /*
         * Cancellation is a request, not a guarantee. Either way the worker
         * must stop touching the output containers before they are freed.
         */
        rocprofvis_controller_future_wait(entry->second.m_future, FLT_MAX);
        entry->second.m_status = RequestStatus::kCancelled;
        ReleaseRequest(entry->second);
    }

    result["request_id"] = Json::MakeUInt(request_id);
    result["cancelled"]  = jt::Json(cancelled);
    m_requests.erase(entry);
    (void) error;
    return true;
}

bool
Session::MethodRequestList(const jt::Json& params, jt::Json& result, method_error_t& error)
{
    (void) params;
    (void) error;

    jt::Json requests = Json::MakeArray();
    for(std::map<uint64_t, pending_request_t>::const_iterator entry = m_requests.begin();
        entry != m_requests.end(); ++entry)
    {
        jt::Json described       = Json::MakeObject();
        described["request_id"]  = Json::MakeUInt(entry->second.m_id);
        described["status"]      = jt::Json(std::string(
            entry->second.m_status == RequestStatus::kPending ? "pending" : "complete"));
        described["progress"]    = Json::MakeUInt(entry->second.m_progress);
        Json::Append(requests, described);
    }
    result["requests"] = requests;
    return true;
}

}  // namespace Middleware
}  // namespace RocProfVis
