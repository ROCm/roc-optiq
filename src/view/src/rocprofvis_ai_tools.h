// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include "model/rocprofvis_tables_model.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class TimelineSelection;
class ComputeSelection;

enum class AssistantFetchKind
{
    kNone,
    kSummary,
    kTopEvents,
    kKernelInstances,
    kMetrics
};

struct AssistantToolContext
{
    DataProvider*       data_provider       = nullptr;
    TimelineSelection*  timeline_selection  = nullptr;
    ComputeSelection*   compute_selection   = nullptr;
    bool                is_compute          = false;
    std::string         trace_name;
    uint64_t            metrics_client_id   = 0;
};

struct AssistantToolStartResult
{
    bool               pending       = false;
    bool               started_fetch = false;
    uint64_t           request_id    = 0;
    AssistantFetchKind fetch_kind    = AssistantFetchKind::kNone;
    TableType          table_type    = TableType::kSummaryKernelTable;
    uint32_t           kernel_id     = 0;
    size_t             row_limit     = 10;
    std::string        content;
    std::string        status_line;
};

jt::Json BuildAssistantToolsJson();

std::string BuildAssistantBriefing(const AssistantToolContext& context);

std::string AssistantToolStatusLabel(const std::string& tool_name);

AssistantToolStartResult StartAssistantTool(const AssistantToolContext& context,
                                            const std::string&          tool_name,
                                            const std::string&          arguments_json);

std::string FinishAssistantFetch(const AssistantToolContext& context,
                                 AssistantFetchKind          kind, TableType table_type,
                                 uint32_t kernel_id, size_t row_limit);

}  // namespace View
}  // namespace RocProfVis
