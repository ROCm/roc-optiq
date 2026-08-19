// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

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
class ComputeSelection;
class TraceView;

constexpr size_t ASSISTANT_DEFAULT_ROW_LIMIT = 20;
constexpr size_t ASSISTANT_MAX_ROW_LIMIT     = 200;

enum class AssistantFetchKind
{
    kNone,
    kSummary,
    kTopEvents,
    kKernelInstances,
    kMetrics,
    // Any table read back out of TraceDataModel::GetTables(): track events,
    // counter samples, and search results.
    kDataTable,
    kEventDetails,
    kTrackStatistics
};

struct AssistantToolContext
{
    DataProvider*       data_provider       = nullptr;
    TimelineSelection*  timeline_selection  = nullptr;
    ComputeSelection*   compute_selection   = nullptr;
    TraceView*          trace_view          = nullptr;
    bool                is_compute          = false;
    std::string         trace_name;
    uint64_t            metrics_client_id   = 0;
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
};

// One track's row from the minimap, normalized to [0,1] within the row.
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

jt::Json BuildAssistantToolsJson();

std::string BuildAssistantBriefing(const AssistantToolContext& context);

std::string AssistantToolStatusLabel(const std::string& tool_name);

AssistantToolStartResult StartAssistantTool(const AssistantToolContext& context,
                                            const std::string&          tool_name,
                                            const std::string&          arguments_json);

std::string FinishAssistantFetch(const AssistantToolContext& context,
                                 const AssistantFetchState&  fetch);

}  // namespace View
}  // namespace RocProfVis
