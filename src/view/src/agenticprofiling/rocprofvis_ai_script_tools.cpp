// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The tool that hands a Python script to the interpreter and waits for what it
// worked out. The model writes the analysis instead of asking for rows, so what
// comes back is a conclusion rather than a table to format: use it for what has
// to be computed over many rows, and leave small lookups to the data tools.
//
// With scripting off the handler table is empty and the schema never registers
// the tool, so the model is not offered something that would always fail.
#include "rocprofvis_ai_tools_internal.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "json.h"

#include "rocprofvis_data_provider.h"
#include "rocprofvis_json_utils.h"

#ifdef ROCPROFVIS_ENABLE_SCRIPTING
#    include "rocprofvis_ai_actions.h"
#    include "widgets/rocprofvis_script_editor.h"
#endif

namespace RocProfVis
{
namespace View
{

#ifdef ROCPROFVIS_ENABLE_SCRIPTING

namespace
{

// Longer than any analysis worth writing, and short enough that a model
// looping on itself cannot post a novel to the interpreter.
constexpr size_t ASSISTANT_MAX_SCRIPT_CHARS = 20000;

// The wait here is on a person reading code, not on a query, so it gets its
// own budget rather than the deadline a fetch runs under. Long enough to read
// a screenful and think about it; short enough that a turn left open does not
// sit there for the rest of the session.
constexpr uint32_t ASSISTANT_SCRIPT_APPROVAL_TIMEOUT_SECONDS = 300;

// A script answers with conclusions, so anything this long means it printed
// its working out instead of its result.
constexpr size_t ASSISTANT_MAX_SCRIPT_OUTPUT_CHARS = 8000;

AssistantToolStartResult
ToolRunAnalysisScript(const AssistantToolContext& context, const jt::Json& args,
                      const std::string&)
{
    const std::string source = JsonUtils::GetString(args, "script", "");
    if(source.empty())
    {
        return DoneResult("run_analysis_script needs a script argument holding the "
                          "Python source to run.",
                          "No script");
    }
    if(source.size() > ASSISTANT_MAX_SCRIPT_CHARS)
    {
        return DoneResult("That script is too long. Keep it under " +
                              std::to_string(ASSISTANT_MAX_SCRIPT_CHARS) +
                              " characters.",
                          "Script too long");
    }

    // optiq.table() and Track.events() read a system trace, so a script on a
    // compute tab would fail inside Python after a round trip. Say so now.
    if(context.is_compute)
    {
        return DoneResult("Scripts read system traces. This tab is a compute "
                          "trace, so use kernel_metrics and the compute tools "
                          "instead.",
                          "Not a system trace");
    }

    // One script at a time per trace, and the user's own run owns the slot just
    // as much as this one does.
    if(context.data_provider->IsRequestPending(DataProvider::EXECUTE_SCRIPT_REQUEST_ID))
    {
        return DoneResult("A script is already running on this trace. Wait for it "
                          "to finish before starting another.",
                          "Script busy");
    }

    // Offer it, do not run it. Writing code is the one thing the model does
    // that executes, so the user reads it and decides. The Script tab drives
    // the run from there, through the path a hand-written script takes, and it
    // owns the trace and the selection the run needs.
    OptiqActions actions(context.data_provider, context.timeline_selection,
                         context.compute_selection, context.trace_view);
    if(!actions.ProposeScript(source))
    {
        return DoneResult("There is no script editor on this trace, so the "
                          "script could not be offered.",
                          "No script tab");
    }

    AssistantToolStartResult result;
    result.pending = true;
    // There is no request to poll yet - the wait is on a person - so the panel
    // asks AssistantScriptFetchPending instead of watching a request id.
    result.started_fetch    = true;
    result.fetch.kind       = AssistantFetchKind::kScript;
    result.status_line      = "Waiting for you to review a script...";
    result.timeout_seconds  = ASSISTANT_SCRIPT_APPROVAL_TIMEOUT_SECONDS;
    return result;
}

const AssistantToolEntry k_script_tool_handlers[] = {
    { "run_analysis_script", ToolRunAnalysisScript },
};

}  // namespace

// The script half of the tool set, for StartAssistantTool to search.
AssistantToolTable
GetAssistantScriptToolHandlers()
{
    AssistantToolTable table;
    table.entries = k_script_tool_handlers;
    table.count   = sizeof(k_script_tool_handlers) / sizeof(k_script_tool_handlers[0]);
    return table;
}

// True while the offer is still with the user, or the script they approved is
// still running. Both are waits with no request id behind them, which is why
// the panel asks here rather than polling the data provider.
bool
AssistantScriptFetchPending(const AssistantToolContext& context)
{
    const OptiqActions actions(context.data_provider, context.timeline_selection,
                               context.compute_selection, context.trace_view);
    const ScriptApproval state = actions.ScriptProposalState();
    return state == ScriptApproval::kPending || state == ScriptApproval::kRunning;
}

// Reads back what the user decided, and what the script produced if they ran it.
std::string
FinishAssistantScriptFetch(const AssistantToolContext& context)
{
    OptiqActions actions(context.data_provider, context.timeline_selection,
                         context.compute_selection, context.trace_view);
    const ScriptApproval state = actions.ScriptProposalState();
    actions.ClearScriptProposal();

    if(state == ScriptApproval::kRejected)
    {
        // Their call, and not a failure. Said plainly so the model moves on
        // rather than offering the same script again.
        return "The user read the script and chose not to run it. Do not offer "
               "it again unless they ask. Answer with what you have, or read "
               "the data with the other tools.";
    }
    if(state == ScriptApproval::kFailedToStart)
    {
        return "The user approved the script but it could not be started. The "
               "trace may still be loading, or another script may be running.";
    }
    if(state == ScriptApproval::kPending)
    {
        return "The user did not answer, so the script was not run. Carry on "
               "without it.";
    }
    if(state == ScriptApproval::kRunning)
    {
        return "The script was still running when the wait ran out. Answer with "
               "what you have; its output will be in the script editor.";
    }

    if(context.data_provider == nullptr)
    {
        return "The trace closed while the script was running.";
    }

    std::string text;
    std::string error;
    if(!context.data_provider->GetLastScriptResult(text, error))
    {
        // The traceback names the failing line, so the next attempt can fix it
        // rather than guess at what went wrong.
        return "The script failed:\n" +
               (error.empty() ? std::string("no error reported") : error) +
               "\nFix the script and offer it again, or read the data with the "
               "other tools instead.";
    }
    if(text.empty())
    {
        return "The script ran but reported nothing. Call optiq.result.text(...) "
               "with the numbers it worked out.";
    }
    return TrimAssistantText(
        text, ASSISTANT_MAX_SCRIPT_OUTPUT_CHARS,
        "\n... truncated. Have the script report totals rather than rows ...");
}

#else

// Scripting is not built in, so the tool never reaches the model. The
// accessors still exist because the dispatcher searches every table.
AssistantToolTable
GetAssistantScriptToolHandlers()
{
    AssistantToolTable table;
    return table;
}

bool
AssistantScriptFetchPending(const AssistantToolContext&)
{
    return false;
}

std::string
FinishAssistantScriptFetch(const AssistantToolContext&)
{
    return "This build has no script support.";
}

#endif  // ROCPROFVIS_ENABLE_SCRIPTING

}  // namespace View
}  // namespace RocProfVis
