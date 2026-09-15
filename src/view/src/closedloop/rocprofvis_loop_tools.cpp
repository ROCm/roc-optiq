// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The closed-loop half of the tool set: read the source behind a finding, offer
// a change, offer a run of a saved launch profile, and report where the loop has
// got to. Everything that acts goes through LoopBridge, which is to the loop
// what OptiqActions is to the trace view.
//
// Compiled whenever the assistant is, so the dispatcher always has a table to
// search. With the loop off that table is empty, the schema never registers the
// tools, and the prompt never names them - the same way scripting disappears.

#include "agenticprofiling/rocprofvis_ai_tools_internal.h"

#ifdef ROCPROFVIS_ENABLE_CLOSED_LOOP
#    include "closedloop/rocprofvis_loop_bridge.h"
#    include "rocprofvis_json_utils.h"
#endif

#include <string>

namespace RocProfVis
{
namespace View
{

#ifdef ROCPROFVIS_ENABLE_CLOSED_LOOP

namespace
{

// How long an offer may sit with the user. The wait is on a person reading a
// change or watching a run, not on a query, so both are far longer than a fetch
// deadline - and still bounded, so a turn nobody answers does not sit open for
// the rest of the session.
//
// The edit deadline has to stay above the bridge's own build timeout. Giving up
// first would leave the apply-and-build future running while the next proposal
// replaced it, and assigning over a live std::async future blocks on it - on
// the UI thread.
constexpr uint32_t LOOP_EDIT_TIMEOUT_SECONDS = 3600;
constexpr uint32_t LOOP_RUN_TIMEOUT_SECONDS  = 3600;
// Connect, authenticate, one command. The user may have a host-key or password
// prompt to answer in the middle of it, which is what this is really sized for.
constexpr uint32_t LOOP_ATTACH_TIMEOUT_SECONDS = 300;

// Parks the turn on an offer the user has to answer. There is no request id
// behind this wait, so the panel polls AssistantLoopFetchPending instead.
AssistantToolStartResult
ParkedResult(const char* status, uint32_t timeout_seconds)
{
    AssistantToolStartResult result;
    result.pending         = true;
    result.started_fetch   = true;
    result.fetch.kind      = AssistantFetchKind::kLoop;
    result.status_line     = status;
    result.timeout_seconds = timeout_seconds;
    return result;
}

AssistantToolStartResult
ToolFindSource(const AssistantToolContext&, const jt::Json& args, const std::string&)
{
    const std::string query = JsonUtils::GetString(args, "query", "");
    return DoneResult(LoopBridge::GetInstance().FindSource(query),
                      "Searching the workspace...");
}

AssistantToolStartResult
ToolReadSource(const AssistantToolContext&, const jt::Json& args, const std::string&)
{
    const std::string file = JsonUtils::GetString(args, "file", "");
    return DoneResult(LoopBridge::GetInstance().ReadSource(file), "Reading source...");
}

AssistantToolStartResult
ToolProposeCodeChange(const AssistantToolContext&, const jt::Json& args,
                      const std::string&)
{
    LoopBridge&       bridge = LoopBridge::GetInstance();
    const std::string refused =
        bridge.ProposeEdit(JsonUtils::GetString(args, "file", ""),
                           JsonUtils::GetString(args, "find", ""),
                           JsonUtils::GetString(args, "replace", ""),
                           JsonUtils::GetString(args, "why", ""));
    if(!refused.empty())
    {
        return DoneResult(refused, "Change not offered");
    }
    return ParkedResult("Waiting for you to approve a change...",
                        LOOP_EDIT_TIMEOUT_SECONDS);
}

AssistantToolStartResult
ToolListLaunchProfiles(const AssistantToolContext&, const jt::Json&, const std::string&)
{
    return DoneResult(LoopBridge::GetInstance().LaunchProfiles(), "Listing profiles...");
}

AssistantToolStartResult
ToolProposeProfileRun(const AssistantToolContext&, const jt::Json& args,
                      const std::string&)
{
    LoopBridge&       bridge = LoopBridge::GetInstance();
    const std::string refused =
        bridge.ProposeRun(JsonUtils::GetString(args, "profile", ""),
                          JsonUtils::GetString(args, "why", ""));
    if(!refused.empty())
    {
        return DoneResult(refused, "Run not offered");
    }
    return ParkedResult("Waiting for you to approve a run...",
                        LOOP_RUN_TIMEOUT_SECONDS);
}

// Also the way an editor is found. Looks on this machine first, then - because
// the workspace may be open over Remote-SSH on the box being profiled - asks
// that host over the SSH profile, which takes a connect and an authenticate.
AssistantToolStartResult
ToolLoopStatus(const AssistantToolContext&, const jt::Json&, const std::string&)
{
    LoopBridge& bridge = LoopBridge::GetInstance();
    if(bridge.Attached())
    {
        return DoneResult(bridge.Status(), "Checking the loop...");
    }

    const std::string refused = bridge.Attach();
    if(!refused.empty())
    {
        return DoneResult(refused + "\n" + bridge.Status(), "No editor");
    }
    return ParkedResult("Looking for the editor on the profiling host...",
                        LOOP_ATTACH_TIMEOUT_SECONDS);
}

const AssistantToolEntry k_loop_tool_handlers[] = {
    { "find_source", ToolFindSource },
    { "read_source", ToolReadSource },
    { "propose_code_change", ToolProposeCodeChange },
    { "list_launch_profiles", ToolListLaunchProfiles },
    { "propose_profile_run", ToolProposeProfileRun },
    { "loop_status", ToolLoopStatus },
};

}  // namespace

AssistantToolTable
GetAssistantLoopToolHandlers()
{
    AssistantToolTable table;
    table.entries = k_loop_tool_handlers;
    table.count   = sizeof(k_loop_tool_handlers) / sizeof(k_loop_tool_handlers[0]);
    return table;
}

bool
AssistantLoopFetchPending()
{
    return LoopBridge::GetInstance().Pending();
}

std::string
FinishAssistantLoopFetch()
{
    return LoopBridge::GetInstance().TakeResult();
}

#else

// The loop is not built in, so these tools never reach the model. The accessors
// still exist because the dispatcher searches every table.
AssistantToolTable
GetAssistantLoopToolHandlers()
{
    AssistantToolTable table;
    return table;
}

bool
AssistantLoopFetchPending()
{
    return false;
}

std::string
FinishAssistantLoopFetch()
{
    return std::string();
}

#endif  // ROCPROFVIS_ENABLE_CLOSED_LOOP

}  // namespace View
}  // namespace RocProfVis
