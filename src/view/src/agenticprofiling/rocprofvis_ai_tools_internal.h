// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

/**
 * @brief Private wiring between the three parts of the executor. The public
 * surface is rocprofvis_ai_tools.h; nothing outside the agenticprofiling
 * directory should include this header.
 *
 * The tool bodies are split by what they touch. The ones in
 * rocprofvis_ai_ui_tools.cpp drive Optiq through OptiqActions and answer in the
 * same call; the ones in rocprofvis_ai_data_tools.cpp read the data model and
 * usually park a fetch for the panel to poll. That is the line worth drawing,
 * because it is also the line between a tool that can run on any trace and one
 * that has to reason about request ids and pending rows.
 * rocprofvis_ai_script_tools.cpp is a third case: the model writes Python that
 * computes the answer, so what comes back is a conclusion rather than rows.
 *
 * Each body file owns its own handler table and hands it to the dispatcher in
 * rocprofvis_ai_tools.cpp, so a tool body stays next to the entry that
 * registers it and the handlers themselves keep internal linkage. The handful
 * of helpers both halves need are declared here and defined beside the
 * dispatcher.
 */

#include "rocprofvis_ai_tools.h"

#include "json.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

struct KernelInfo;
struct WorkloadInfo;

// One tool body. Each takes the context for the trace in front and the
// already-parsed arguments; the name it was called by is passed too, but only
// the shared track_events / track_samples body has any use for it.
using AssistantToolHandler = AssistantToolStartResult (*)(
    const AssistantToolContext& context, const jt::Json& args,
    const std::string& tool_name);

// Maps the name the model used to the body that implements it. Kept beside the
// bodies so adding a tool is one entry and one function, and so a name that is
// registered in the schema but missing here fails loudly.
struct AssistantToolEntry
{
    const char*          name;
    AssistantToolHandler handler;
};

// One body file's tool table, as handed to the dispatcher.
struct AssistantToolTable
{
    const AssistantToolEntry* entries = nullptr;
    size_t                    count   = 0;
};

// The tables StartAssistantTool searches. Defined by the body files, so none of
// them has to know the others exist.
AssistantToolTable GetAssistantUiToolHandlers();
AssistantToolTable GetAssistantDataToolHandlers();
// Empty when scripting is not built in, which is how the tool disappears
// without the dispatcher knowing anything about the option.
AssistantToolTable GetAssistantScriptToolHandlers();

// Formats a finished script run, or the decision that stopped it. Lives beside
// the tool that started it, and is reached through FinishAssistantFetch like
// every other fetch kind. AssistantScriptFetchPending is the panel-facing half
// and is declared in rocprofvis_ai_tools.h.
std::string FinishAssistantScriptFetch(const AssistantToolContext& context);

// --- Helpers both halves need, defined beside the dispatcher ---------------

// Builds the result of a tool that finished without waiting on a fetch.
AssistantToolStartResult DoneResult(const std::string& content,
                                    const std::string& status);

// Reads an unsigned id from JSON, however the model chose to encode it.
uint64_t JsonU64(const jt::Json& json, const std::string& key,
                 uint64_t default_value);

// Rejects a model-supplied array that is longer than anything a tool can use.
// Returns true when the array is fine to walk.
bool CheckArrayLength(const std::vector<jt::Json>& entries,
                      const char* argument_name, std::string& error_out);

// Yields the selected time range, or the whole trace if nothing is selected.
// A UI tool needs this as much as a query does: annotate stores the window the
// user was looking at alongside the note.
void SelectedOrFullTimeRange(const AssistantToolContext& context, double& start_ns,
                             double& end_ns);

// Finds a kernel by id, then exact name, then a case-insensitive substring.
// Shared because goto selects a kernel by the same loose name the model used to
// ask kernel_metrics about it.
const KernelInfo* FindComputeKernel(const WorkloadInfo& workload,
                                    const std::string& name, uint32_t id);

}  // namespace View
}  // namespace RocProfVis
