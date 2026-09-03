// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

/**
 * @brief Private wiring between the three parts of the executor. The public
 * surface is rocprofvis_ai_tools.h; nothing outside the agenticprofiling
 * directory should include this header.
 *
 * Tool bodies are split by what they touch: rocprofvis_ai_ui_tools.cpp drives
 * Optiq through OptiqActions and answers in the same call,
 * rocprofvis_ai_data_tools.cpp reads the data model and usually parks a fetch
 * for the panel to poll, and rocprofvis_ai_script_tools.cpp has the model write
 * Python that computes the answer. Each body file owns its handler table and
 * hands it to the dispatcher, so the handlers keep internal linkage.
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

// One tool body. Each takes the context for the trace in front and the
// already-parsed arguments; the name it was called by is passed too, but only
// the shared track_events / track_samples body has any use for it.
typedef AssistantToolStartResult (*AssistantToolHandler)(
    const AssistantToolContext& context, const jt::Json& args,
    const std::string& tool_name);

// Maps the name the model used to the body that implements it.
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
// them has to know the others exist. The script table is empty when scripting
// is not built in, which is how that tool disappears without the dispatcher
// knowing anything about the option.
AssistantToolTable GetAssistantUiToolHandlers();
AssistantToolTable GetAssistantDataToolHandlers();
AssistantToolTable GetAssistantScriptToolHandlers();

// Formats a finished script run, or the decision that stopped it. Reached
// through FinishAssistantFetch like every other fetch kind.
std::string FinishAssistantScriptFetch(const AssistantToolContext& context);

// --- Helpers both halves need, defined beside the dispatcher ---------------

// Builds the result of a tool that finished without waiting on a fetch.
AssistantToolStartResult DoneResult(const std::string& content,
                                    const std::string& status);

// Reads an unsigned id from JSON, however the model chose to encode it.
uint64_t JsonU64(const jt::Json& json, const std::string& key,
                 uint64_t default_value);

// Narrows a JSON number to an id, rejecting anything a uint64_t cannot hold.
uint64_t JsonU64FromDouble(double value, uint64_t default_value);

// Caps a tool result at max_chars, appending truncation_note when it had to
// cut. Keeps the head, which is where a well-behaved result puts its summary.
std::string TrimAssistantText(std::string text, size_t max_chars,
                              const char* truncation_note);

// Rejects a model-supplied array that is longer than anything a tool can use.
// Returns true when the array is fine to walk.
bool CheckArrayLength(const std::vector<jt::Json>& entries,
                      const char* argument_name, std::string& error_out);

// Yields the selected time range, or the whole trace if nothing is selected.
void SelectedOrFullTimeRange(const AssistantToolContext& context, double& start_ns,
                             double& end_ns);

}  // namespace View
}  // namespace RocProfVis
