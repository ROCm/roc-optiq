// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "json.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_analysis.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"

namespace RocProfVis
{
namespace Middleware
{

/*
 * Protocol version reported by session.info. Bump on a breaking schema change.
 */
constexpr uint32_t PROTOCOL_VERSION = 1;

/*
 * Guards SummaryMetrics recursion. The real tree is trace -> node -> processor,
 * so this is generous; it exists only so a malformed tree cannot recurse away
 * the stack.
 */
constexpr uint32_t SUMMARY_MAX_DEPTH = 16;

/*
 * Lifecycle of the trace owned by a session.
 */
enum class TraceState
{
    kEmpty,
    kLoading,
    kReady,
    kError
};

/* Wire spelling of a trace state. Always returns a non-null literal. */
char const* TraceStateToString(TraceState state);

/*
 * Which controller operation a pending request wraps. Determines how the
 * request's output container is interpreted once its future completes.
 */
enum class RequestKind
{
    kTraceOpen,
    kTrackFetch,
    kGraphFetch,
    kTableFetch,
    kTableExport,
    kSummaryFetch,
    kEventExtData,
    kEventFlow,
    kEventCallstack,
    kQueueUtilization,
    kCounterStatistics,
    kSaveTrimmedTrace,
    kCleanupDatabase
};

/*
 * Observable state of a request, as reported to the client.
 */
enum class RequestStatus
{
    kPending,
    kReady,
    kError,
    kCancelled
};

/*
 * A controller operation in flight, plus everything needed to decode it.
 *
 * The middleware owns every container here (the controller's async API requires
 * caller-owned output), so a request is the unit of cleanup: ReleaseRequest
 * frees the future, array, arguments and metrics container together. The result
 * is decoded once on completion and cached in m_result so repeated polls are
 * cheap and so the containers can be released as early as possible.
 */
struct pending_request_t
{
    uint64_t                                 m_id;
    RequestKind                              m_kind;
    RequestStatus                            m_status;
    rocprofvis_controller_future_t*          m_future;
    rocprofvis_controller_array_t*           m_array;
    rocprofvis_controller_arguments_t*       m_args;
    rocprofvis_controller_summary_metrics_t* m_summary_metrics;
    rocprofvis_handle_t*                     m_subject;
    uint64_t                                 m_num_columns;
    double                                   m_queue_utilization;
    rocprofvis_analysis_counter_statistics_t m_counter_statistics;
    jt::Json                                 m_params;
    jt::Json                                 m_result;
    uint64_t                                 m_progress;
    std::string                              m_message;
    rocprofvis_result_t                      m_result_code;
};

/*
 * A failure to report back to the client. Codes are stable identifiers; the
 * message is human-facing and may change.
 */
struct method_error_t
{
    std::string m_code;
    std::string m_message;
};

/*
 * One controller instance and the requests outstanding against it, driven by
 * JSON messages.
 *
 * A session is single-threaded: Execute must not be called concurrently on the
 * same instance. The controller's own worker threads run underneath, and every
 * Execute call first advances all outstanding futures, so any request refreshes
 * the progress of all of them.
 */
class Session
{
public:
    Session();
    ~Session();

    Session(const Session&)            = delete;
    Session& operator=(const Session&) = delete;

    /*
     * Execute one JSON request and return the JSON response. Always returns a
     * well-formed response document, including for malformed input.
     */
    std::string Execute(const std::string& request_json);

private:
    typedef bool (Session::*method_fn_t)(const jt::Json& params, jt::Json& result,
                                         method_error_t& error);

    struct method_entry_t
    {
        char const* m_name;
        method_fn_t m_function;
    };

    /* Request methods. Each returns false with error filled on failure. */
    bool MethodSessionInfo(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodTraceOpen(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodTraceClose(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodTraceStatus(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodTraceTopology(const jt::Json& params, jt::Json& result,
                             method_error_t& error);
    bool MethodTraceHistogram(const jt::Json& params, jt::Json& result,
                              method_error_t& error);
    bool MethodTraceSaveTrimmed(const jt::Json& params, jt::Json& result,
                                method_error_t& error);
    bool MethodTraceCleanup(const jt::Json& params, jt::Json& result,
                            method_error_t& error);
    bool MethodTimelineInfo(const jt::Json& params, jt::Json& result,
                            method_error_t& error);
    bool MethodTrackFetch(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodGraphFetch(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodTableFetch(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodTableExport(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodSummaryFetch(const jt::Json& params, jt::Json& result,
                            method_error_t& error);
    bool MethodEventExtData(const jt::Json& params, jt::Json& result,
                            method_error_t& error);
    bool MethodEventFlow(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodEventCallstack(const jt::Json& params, jt::Json& result,
                              method_error_t& error);
    bool MethodAnalysisTrackStatistics(const jt::Json& params, jt::Json& result,
                                       method_error_t& error);
    bool MethodRequestPoll(const jt::Json& params, jt::Json& result, method_error_t& error);
    bool MethodRequestCancel(const jt::Json& params, jt::Json& result,
                             method_error_t& error);
    bool MethodRequestList(const jt::Json& params, jt::Json& result, method_error_t& error);

    /* Shared implementation for the event.* detail fetches. */
    bool FetchEventDetail(const jt::Json& params, jt::Json& result, method_error_t& error,
                          RequestKind kind, rocprofvis_property_t property);

    /* Shared implementation for table.fetch and table.export_csv. */
    bool FetchOrExportTable(const jt::Json& params, jt::Json& result,
                            method_error_t& error, bool export_csv);

    /* Request registry. */
    uint64_t           AllocateRequestId(void);
    pending_request_t& CreateRequest(RequestKind kind, const jt::Json& params);
    void               ReleaseRequest(pending_request_t& request);
    void               ReleaseAllRequests(void);

    /*
     * Advance every outstanding future by one non-blocking poll, decoding any
     * that have completed. Called at the start of every Execute.
     */
    void PumpRequests(void);

    /*
     * Poll one request, optionally blocking for up to timeout_ms.
     * @returns True when the request reached a terminal state.
     */
    bool AdvanceRequest(pending_request_t& request, uint32_t timeout_ms);

    /* Decode a completed request's output container into m_result. */
    void DecodeRequest(pending_request_t& request);
    void DecodeTrackOrGraph(pending_request_t& request);
    void DecodeTable(pending_request_t& request);
    void DecodeSummary(pending_request_t& request);
    void DecodeHandleArray(pending_request_t& request);
    void DecodeTraceOpen(pending_request_t& request);

    /*
     * Build the {request_id, status, ...} envelope a client gets back from an
     * async method, inlining the result when the request already completed.
     */
    jt::Json DescribeRequest(const pending_request_t& request);

    /*
     * Describe a request on its way to the client and retire it if it has
     * finished. The reference does not outlive the call for a finished
     * request.
     */
    jt::Json DeliverRequest(pending_request_t& request);

    /*
     * Honour an optional "wait_ms" budget on an async method before replying.
     */
    void ApplyWaitBudget(pending_request_t& request, const jt::Json& params);

    /* Resolve controller objects by id, reporting a precise error on failure. */
    bool RequireTrace(method_error_t& error);
    bool ResolveTrack(uint64_t track_id, rocprofvis_handle_t** track,
                      method_error_t& error);
    bool ResolveGraph(uint64_t track_id, rocprofvis_handle_t** graph,
                      method_error_t& error);
    bool ResolveTable(rocprofvis_controller_table_type_t table_type,
                      rocprofvis_handle_t** table, method_error_t& error);

    /* Populate a table arguments container from a request's params object. */
    bool BuildTableArguments(rocprofvis_controller_arguments_t* args,
                             rocprofvis_controller_table_type_t table_type,
                             const jt::Json& params, method_error_t& error);

    static const method_entry_t METHODS[];
    static const size_t         METHOD_COUNT;

    rocprofvis_controller_t* m_controller;
    rocprofvis_handle_t*     m_timeline;
    TraceState               m_state;
    std::vector<std::string> m_trace_paths;
    std::string              m_load_message;
    rocprofvis_result_t      m_load_result;

    std::map<uint64_t, pending_request_t> m_requests;
    uint64_t                              m_next_request_id;
};

}  // namespace Middleware
}  // namespace RocProfVis
