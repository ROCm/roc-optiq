// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include "model/compute/rocprofvis_compute_model_types.h"
#include "rocprofvis_request_results.h"
#include "rocprofvis_requests.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

class ComputeDataModel;
class SummaryModel;
class TopologyTree;
class TraceDataModel;

/*
 * The roc-optiq client/server protocol. Every message is one JSON object sent
 * as a binary WebSocket frame (binary, because trace strings are not guaranteed
 * to be valid UTF-8 and text frames must be). rocprofvis_protocol_frame.h lists
 * the frame kinds: plain JSON, zlib-compressed JSON, and file/upload chunks.
 *
 * 64-bit ids travel as JSON integers holding the id's bit pattern as a signed
 * 64-bit value, because the parser has no unsigned type. Doubles that are not
 * finite travel as null (NaN) or 1e5000 (infinity).
 *
 * Messages, by "type" (C = client to server, S = server to client):
 *   C hello          {protocol, client, token?, session?, last_seq?}
 *   S welcome        {protocol, server, home, session, resumed, client_seq,
 *                     features: {upload, profiler, compare, scripts}, error?,
 *                     auth_required?}
 *   C open_trace     {trace, path, compare?: [path]}    trace: client-chosen id
 *   S trace_opened   {trace, ok, compute, error?}       the load starts at once
 *   C close_trace    {trace}
 *   C request        {trace, id, op, params}            id: DataProvider request id
 *   C cancel         {trace, id}
 *   C set_selected   {trace}
 *   S progress       {trace, id, percent, message}
 *   S response       {trace, id, request_type, code, error, data, file_ok?}
 *   S file_begin     {trace, id, size}                  then file chunks, then
 *   S file_end       {trace, id, ok}                    before that id's response
 *   C list_directory {listing, path}                    path "" is the home folder
 *   S directory      {listing, ok, path, parent, error?, entries: [{name, directory, size}]}
 *   C upload_begin   {upload, name, size}
 *   S upload_ready   {upload, ok, path?, error?}        then upload chunks
 *   C upload_end     {upload}
 *   C upload_cancel  {upload}
 *   S upload_done    {upload, ok, path?, error?}
 *   C profiler_launch {run, tool, tool_directory, working_directory, output_directory,
 *                      argv, env: [[name, value]]}
 *   C profiler_cancel {run}
 *   S profiler_output {run, text}
 *   S profiler_state  {run, state, exit_code, error?}
 *   C bye             {}     the session ends with this connection, not kept to resume
 * The load is answered under the id the local load uses: FETCH_SYSTEM_TRACE_REQUEST_ID
 * or FETCH_COMPUTE_TRACE_REQUEST_ID. The params of each op and the data of each
 * response are the encode_* functions below, except execute_script:
 * params {source, tracks: [id], start, end}, data {ok, text, error}.
 *
 * Resuming: every message after hello/welcome carries "seq", numbered per
 * direction from 1. A connection that drops reconnects with the session from
 * its welcome and the last seq it received; the server replays what came after
 * it, and its welcome's client_seq tells the client which of its own messages
 * to send again. Chunks are not numbered, so a transfer cut by a drop fails.
 */

// Bumped whenever a message changes shape; client and server must agree.
constexpr int64_t PROTOCOL_VERSION = 2;

constexpr const char* PROTOCOL_WEBSOCKET_PATH = "/optiq";
constexpr int         DEFAULT_SERVER_PORT     = 9750;
constexpr const char* SERVER_TOKEN_ENV = "ROCPROFVIS_SERVER_TOKEN";
constexpr const char* SERVER_CA_ENV    = "ROCPROFVIS_SERVER_CA";

// The server saves its access token under this name in its config directory,
// readable by its user only, so a client running as the same user on the same
// machine finds it without being told.
std::string server_token_file_name(int port);

// "type" of each message.
constexpr const char* MSG_HELLO           = "hello";
constexpr const char* MSG_WELCOME         = "welcome";
constexpr const char* MSG_OPEN_TRACE      = "open_trace";
constexpr const char* MSG_TRACE_OPENED    = "trace_opened";
constexpr const char* MSG_CLOSE_TRACE     = "close_trace";
constexpr const char* MSG_REQUEST         = "request";
constexpr const char* MSG_CANCEL          = "cancel";
constexpr const char* MSG_SET_SELECTED    = "set_selected";
constexpr const char* MSG_PROGRESS        = "progress";
constexpr const char* MSG_RESPONSE        = "response";
constexpr const char* MSG_LIST_DIRECTORY  = "list_directory";
constexpr const char* MSG_DIRECTORY       = "directory";
constexpr const char* MSG_FILE_BEGIN      = "file_begin";
constexpr const char* MSG_FILE_END        = "file_end";
constexpr const char* MSG_UPLOAD_BEGIN    = "upload_begin";
constexpr const char* MSG_UPLOAD_READY    = "upload_ready";
constexpr const char* MSG_UPLOAD_END      = "upload_end";
constexpr const char* MSG_UPLOAD_CANCEL   = "upload_cancel";
constexpr const char* MSG_UPLOAD_DONE     = "upload_done";
constexpr const char* MSG_PROFILER_LAUNCH = "profiler_launch";
constexpr const char* MSG_PROFILER_CANCEL = "profiler_cancel";
constexpr const char* MSG_PROFILER_OUTPUT = "profiler_output";
constexpr const char* MSG_PROFILER_STATE  = "profiler_state";
constexpr const char* MSG_BYE             = "bye";
constexpr const char* MSG_ERROR           = "error";
// Never sent; a channel delivers it when its server session is gone for good.
constexpr const char* MSG_DISCONNECTED = "disconnected";

// "op" of a MSG_REQUEST.
constexpr const char* OP_GRAPH            = "graph";
constexpr const char* OP_TRACK            = "track";
constexpr const char* OP_TRACK_TABLE      = "track_table";
constexpr const char* OP_EVENT_SEARCH     = "event_search";
constexpr const char* OP_SUMMARY          = "summary";
constexpr const char* OP_TRACK_STATISTICS = "track_statistics";
constexpr const char* OP_EVENT_EXT_DATA   = "event_ext_data";
constexpr const char* OP_EVENT_FLOW       = "event_flow";
constexpr const char* OP_EVENT_CALL_STACK = "event_call_stack";
constexpr const char* OP_SAVE_TRIMMED     = "save_trimmed";
constexpr const char* OP_CLEANUP_DATABASE = "cleanup_database";
constexpr const char* OP_METRICS          = "metrics";
constexpr const char* OP_METRIC_PIVOT     = "metric_pivot";
constexpr const char* OP_PC_SAMPLING      = "pc_sampling";
constexpr const char* OP_EXECUTE_SCRIPT   = "execute_script";

// Bytes of file data per file or upload chunk frame.
constexpr uint64_t FILE_CHUNK_SIZE = 1024 * 1024;

// Accessors that never abort on a missing field or a type mismatch: the
// parser's own getters do, and the input comes from the network.
jt::Json    json_from_uint64(uint64_t value);
jt::Json    json_from_double(double value);
uint64_t    json_get_uint64(jt::Json& object, const char* key);
int64_t     json_get_int64(jt::Json& object, const char* key);
double      json_get_double(jt::Json& object, const char* key);
bool        json_get_bool(jt::Json& object, const char* key);
std::string json_get_string(jt::Json& object, const char* key);
uint64_t    json_to_uint64(jt::Json& value);
double      json_to_double(jt::Json& value);
std::string json_to_string(jt::Json& value);
// The array at key, or an empty one when it is missing.
std::vector<jt::Json>& json_get_array(jt::Json& object, const char* key);

std::string encode_message(const jt::Json& message);
bool        decode_message(const std::string& payload, jt::Json& message);

// Request parameters.
jt::Json           encode_track_request(const TrackRequestParams& params);
TrackRequestParams decode_track_request(jt::Json& params);
jt::Json           encode_track_table_request(const TrackTableRequestParams& params);
TrackTableRequestParams decode_track_table_request(jt::Json& params);
jt::Json encode_event_search_request(const EventSearchRequestParams& params);
EventSearchRequestParams decode_event_search_request(jt::Json& params);
jt::Json encode_track_statistics_request(const AnalysisTrackStatisticsRequestParams& params);
AnalysisTrackStatisticsRequestParams decode_track_statistics_request(jt::Json& params);
jt::Json             encode_metrics_request(const MetricsRequestParams& params);
MetricsRequestParams decode_metrics_request(jt::Json& params);
jt::Json encode_compute_table_request(const ComputeTableRequestParams& params);
ComputeTableRequestParams decode_compute_table_request(jt::Json& params);
jt::Json                encode_pc_sampling_request(const PcSamplingRequestParams& params);
PcSamplingRequestParams decode_pc_sampling_request(jt::Json& params);

// Results. Each decode_* fills what the matching DataProvider::Process* step
// would have, from what the server's own DataProvider produced.
jt::Json encode_system_trace(const TraceDataModel& model);
bool     decode_system_trace(jt::Json& data, TraceDataModel& model);
jt::Json encode_track_data(const TrackDataResult& result);
void     decode_track_data(jt::Json& data, TrackDataResult& result);
jt::Json encode_table(const TableInfo& table);
void     decode_table(jt::Json& data, std::vector<std::string>& header,
                      std::vector<rocprofvis_controller_primitive_type_t>& column_types,
                      std::vector<std::vector<std::string>>& rows, uint64_t& total_rows);
jt::Json encode_event_ext_data(const EventExtDataResult& result);
void     decode_event_ext_data(jt::Json& data, EventExtDataResult& result);
jt::Json encode_event_flow(const EventFlowResult& result);
void     decode_event_flow(jt::Json& data, EventFlowResult& result);
jt::Json encode_event_call_stack(const EventCallStackResult& result);
void     decode_event_call_stack(jt::Json& data, EventCallStackResult& result);
jt::Json encode_summary(const SummaryModel& summary);
// Rebuilds the summary tree under the already cleared model, resolving each level
// against topology and registering every value for formatting.
bool     decode_summary(jt::Json& data, SummaryModel& summary, const TopologyTree& topology);
jt::Json encode_track_statistics(const AnalysisTrackStatisticsRequestParams& params);
// The output is a union; is_counter says which member the track's type fills.
void decode_track_statistics(jt::Json& data, AnalysisTrackStatisticsRequestParams& params,
                             bool is_counter);
// memory_chart_layouts: the raw layout JSON of each workload, by workload id.
jt::Json encode_compute_trace(
    const ComputeDataModel&                             model,
    const std::vector<std::pair<uint32_t, std::string>>& memory_chart_layouts);
bool     decode_compute_trace(jt::Json& data, ComputeDataModel& model);
jt::Json encode_metrics(const MetricsResult& result);
void     decode_metrics(jt::Json& data, MetricsResult& result);
jt::Json encode_kernel_selection_table(const ComputeTableInfo& table);
void     decode_kernel_selection_table(jt::Json& data, ComputeTableInfo& table);
jt::Json encode_pc_sampling(const PcSamplingData& data, PcSamplingLayer layer);
void     decode_pc_sampling(jt::Json& json, PcSamplingData& data, PcSamplingLayer layer);

}  // namespace View
}  // namespace RocProfVis
