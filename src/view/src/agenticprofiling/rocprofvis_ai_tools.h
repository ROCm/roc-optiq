// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

// The tool set is described in rocprofvis_ai_tool_schema.h and the query
// arguments are turned into SQL in rocprofvis_ai_tool_query.h; this header is
// the executor that sits between them.
#include "json.h"
#include "model/rocprofvis_common_defs.h"
#include "model/rocprofvis_tables_model.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class TimelineSelection;
class TraceView;

constexpr size_t ASSISTANT_DEFAULT_ROW_LIMIT = 20;
constexpr size_t ASSISTANT_MAX_ROW_LIMIT     = 200;

// How long a tool may wait for a trace to finish opening. Sized for a large
// file on a slow disk rather than for a query, because this wait is the load
// itself. Still bounded, so a load that never finishes cannot park the turn
// forever.
constexpr uint32_t ASSISTANT_TRACE_LOADING_TIMEOUT_SECONDS = 600;

// What a parked fetch is waiting on, which decides how its rows get formatted.
enum class AssistantFetchKind
{
    kNone,
    kSummary,
    kTopEvents,
    kKernelInstances,
    // Any table read back out of TraceDataModel::GetTables(): track events,
    // counter samples, and search results.
    kDataTable,
    kEventDetails,
    kTrackStatistics,
    // A Python analysis script, which answers with its own text rather than
    // rows to format.
    kScript,
    // The trace was still loading when the tool was called. Nothing has been
    // queried yet: the panel parks until the trace is ready and then runs the
    // tool for real, so a big file is waited out rather than reported as a
    // failure the model answers around.
    kTraceLoading
};

// What the tools may touch on the trace in front. Rebuilt for every call, so it
// is never held across a frame.
struct AssistantToolContext
{
    DataProvider*      data_provider      = nullptr;
    TimelineSelection* timeline_selection = nullptr;
    TraceView*         trace_view         = nullptr;
    bool               is_compute         = false;
    std::string        trace_name;
};

// Everything FinishAssistantFetch needs to format a completed fetch. Carried
// unchanged from StartAssistantTool through the panel's wait state.
struct AssistantFetchState
{
    AssistantFetchKind kind       = AssistantFetchKind::kNone;
    TableType          table_type = TableType::kSummaryKernelTable;
    uint32_t           kernel_id  = 0;
    uint64_t           event_id   = 0;
    uint64_t           track_id   = 0;
    size_t             row_limit  = ASSISTANT_DEFAULT_ROW_LIMIT;
};

// What one tool call produced: either finished content, or a set of requests
// for the panel to poll.
struct AssistantToolStartResult
{
    bool                  pending       = false;
    bool                  started_fetch = false;
    // A tool may issue several fetches at once (event details needs three);
    // the panel waits until none of them are pending.
    std::vector<uint64_t> request_ids;
    AssistantFetchState   fetch;
    std::string           content;
    std::string           status_line;
    // Ask the panel to draw the timeline activity strip under this result.
    bool                  chart          = false;
    uint64_t              chart_track_id = INVALID_UINT64_INDEX;
    // When true, replace the stacked follow-up buttons under the transcript.
    bool                     set_next_steps = false;
    std::vector<std::string> next_steps;
    // How long the panel waits before calling this a timeout. Zero takes the
    // default, which suits a query; a tool waiting on the user needs longer.
    uint32_t                 timeout_seconds = 0;
};

// One track's row from the minimap, in [0,1]. The rows of a set share one
// scale, so their brightness is comparable to each other.
struct AssistantActivityRow
{
    uint64_t            track_id = 0;
    std::string         name;
    std::vector<double> bins;
};

// Normalized [0,1] activity over time for the whole trace, or for one track when
// track_id is not INVALID_UINT64_INDEX. Empty when no overview is loaded.
std::vector<double> GetAssistantActivityBins(const AssistantToolContext& context,
                                             uint64_t track_id, size_t bin_count);

// The busiest event tracks, for drawing the minimap under the histogram.
std::vector<AssistantActivityRow> GetAssistantActivityRows(
    const AssistantToolContext& context, size_t bin_count, size_t max_rows);

// The short trace description the model sees before it asks anything.
std::string BuildAssistantBriefing(const AssistantToolContext& context);

// Runs one named tool. Returns finished content, or the requests to wait on.
AssistantToolStartResult StartAssistantTool(const AssistantToolContext& context,
                                            const std::string&          tool_name,
                                            const std::string&          arguments_json);

// Whether a parked kScript fetch is still waiting - on the user reading the
// script, or on the run they approved. That wait has no request id behind it,
// so the panel asks here rather than polling the data provider. Always false
// when scripting is not built in.
bool AssistantScriptFetchPending(const AssistantToolContext& context);

// Formats the rows of a fetch that has landed.
std::string FinishAssistantFetch(const AssistantToolContext& context,
                                 const AssistantFetchState&  fetch);

}  // namespace View
}  // namespace RocProfVis
