// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The dispatcher, plus the few helpers both halves of the tool set need. The
// bodies themselves live next door: rocprofvis_ai_ui_tools.cpp for the tools
// that change Optiq, rocprofvis_ai_data_tools.cpp for the ones that read the
// trace. Each of those owns its own handler table, so this file never has to be
// edited to add a tool - only the table in the file the body went into, and the
// matching schema entry in rocprofvis_ai_tool_schema.cpp.
#include "rocprofvis_ai_tools_internal.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <sstream>
#include <string>
#include <vector>

#include "json.h"

#include "model/rocprofvis_timeline_model.h"
#include "model/rocprofvis_trace_data_model.h"
#include "rocprofvis_ai_tool_schema.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_core_string_utils.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_project.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_trace_view.h"
#include "rocprofvis_utils.h"

namespace RocProfVis
{
namespace View
{

namespace
{

// Longest JSON array a tool argument may be. The entries past a tool's own cap
// are ignored anyway, so refusing outright is both cheaper and more honest than
// silently reading the first few of a million.
constexpr size_t ASSISTANT_MAX_ARRAY_ENTRIES = 256;

// The one tool that runs without a trace, since it only puts buttons under the
// chat.
constexpr const char* ASSISTANT_NEXT_STEPS_TOOL = "offer_next_steps";

// The read tool that names its second trace with its own "with" argument, so a
// trace argument on it would be ambiguous.
constexpr const char* ASSISTANT_COMPARE_TRACES_TOOL = "compare_traces";

// Which handler table a tool came from, which decides what it may be pointed at.
enum class ToolFamily : uint8_t
{
    kUi,
    kData,
    kScript
};

// Reads a tool call's arguments. Clears ok_out on anything that is not a JSON
// object, so the caller can say that rather than report every field missing.
jt::Json
ParseArgsObject(const std::string& arguments_json, bool& ok_out)
{
    jt::Json empty;
    empty.setObject();
    if(arguments_json.empty())
    {
        return empty;
    }

    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(arguments_json);
    if(parsed.first != jt::Json::success || !parsed.second.isObject())
    {
        ok_out = false;
        return empty;
    }
    return parsed.second;
}

// Looks a tool up in the three handler tables. Null when none has it. The UI
// tools are searched first: they are the cheap ones, and none of them shares a
// name with a query.
const AssistantToolEntry*
FindTool(const std::string& tool_name, ToolFamily& family_out)
{
    const AssistantToolTable tables[]   = { GetAssistantUiToolHandlers(),
                                            GetAssistantDataToolHandlers(),
                                            GetAssistantScriptToolHandlers() };
    const ToolFamily         families[] = { ToolFamily::kUi, ToolFamily::kData,
                                            ToolFamily::kScript };
    for(size_t t = 0; t < sizeof(tables) / sizeof(tables[0]); ++t)
    {
        for(size_t i = 0; i < tables[t].count; ++i)
        {
            if(tool_name == tables[t].entries[i].name)
            {
                family_out = families[t];
                return &tables[t].entries[i];
            }
        }
    }
    return nullptr;
}

// Every link of a trace, as the refs a model can name them by.
std::string
LinkRefList(const std::vector<TraceLink>& links)
{
    std::ostringstream out;
    for(size_t i = 0; i < links.size(); ++i)
    {
        out << (i == 0 ? "" : ", ") << links[i].group_id << " (" << links[i].other.label
            << ", the " << links[i].other.role << ")";
    }
    return out.str();
}

// How a linked trace stands right now, in the words the link block uses.
std::string
LinkStatus(const AssistantToolContext& context, const TraceLink& link,
           const AssistantToolContext& linked)
{
    if(linked.data_provider == nullptr)
    {
        return "not open";
    }
    if(linked.is_compute)
    {
        return "compute trace";
    }
    if(linked.data_provider->GetState() != ProviderState::kReady)
    {
        return "loading";
    }
    const bool self_changed =
        !link.self.fingerprint.Matches(AssistantTraceFingerprint(context));
    const bool other_changed =
        !link.other.fingerprint.Matches(AssistantTraceFingerprint(linked));
    if(self_changed && other_changed)
    {
        return "open, both traces changed since they were linked";
    }
    if(self_changed)
    {
        return "open, this trace changed since it was linked";
    }
    if(other_changed)
    {
        return "open, linked trace changed since it was linked";
    }
    return "open";
}

// Runs a read tool against a linked trace instead of the conversation's own.
// Every result says which trace it came from, so rows from two runs can never
// blur together in the history.
AssistantToolStartResult
StartLinkedTool(const AssistantToolContext& context, const AssistantToolEntry& entry,
                ToolFamily family, const jt::Json& args, const std::string& tool_name,
                const std::string& trace_ref)
{
    if(family != ToolFamily::kData || tool_name == ASSISTANT_COMPARE_TRACES_TOOL)
    {
        std::string reason = "trace only applies to the tools that read data. " +
                             tool_name + " acts on the trace this conversation is about";
        if(tool_name == ASSISTANT_COMPARE_TRACES_TOOL)
        {
            reason += "; name the other trace with its with argument instead";
        }
        return DoneResult(reason + ".", "Not a read tool");
    }

    TraceLink   link;
    std::string error;
    if(!ResolveAssistantLink(context, trace_ref, link, error))
    {
        return DoneResult(error, "Unknown link");
    }
    const AssistantToolContext linked  = MakeAssistantToolContext(link.other.path);
    const std::string          problem = LinkedTraceProblem(linked, link);
    if(!problem.empty())
    {
        return DoneResult(problem, "Linked trace unavailable");
    }

    AssistantToolStartResult result = entry.handler(linked, args, tool_name);
    if(result.fetch.project_id.empty())
    {
        result.fetch.project_id = linked.project_id;
    }
    std::string label = "[read from linked trace " + link.group_id + ": " +
                        link.other.label + ", the " + link.other.role + "]\n";
    if(!link.self.fingerprint.Matches(AssistantTraceFingerprint(context)))
    {
        label += "note: this trace changed since it was linked, so it may not be the "
                 "capture the user linked.\n";
    }
    if(!link.other.fingerprint.Matches(AssistantTraceFingerprint(linked)))
    {
        label += "note: the linked trace changed since it was linked, so it may not be "
                 "the capture the user linked.\n";
    }
    result.content = label + result.content;
    return result;
}

}  // namespace

// Builds the result of a tool that finished without waiting on a fetch.
AssistantToolStartResult
DoneResult(const std::string& content, const std::string& status)
{
    AssistantToolStartResult result;
    result.content     = content;
    result.status_line = status;
    return result;
}

/*
 * Narrows a JSON number to an id, refusing anything a double cannot hold
 * exactly.
 *
 * A double carries 53 bits of mantissa, so every integer above 2^53 is stored
 * as the nearest representable one. An event uuid packs an id, a node and an
 * operation into 64 bits and lands around 2^61, where the gap between
 * representable doubles is 512 - so a uuid that arrives as a JSON number comes
 * back rounded to the nearest multiple of 512. That is not a near miss: it
 * names a different event, and the tools then answered confidently about it.
 * goto selected an event the timeline had never heard of, and event_details
 * returned another kernel's arguments and flow links.
 *
 * Rejecting here turns that into a tool error the model can see and correct,
 * rather than a wrong answer nothing downstream can detect. Ids should arrive
 * as strings - see the string branch of JsonU64, which is exact - and the
 * schema asks for them that way.
 */
uint64_t
JsonU64FromDouble(double value, uint64_t default_value)
{
    constexpr double MAX_EXACT_INTEGER = 9007199254740992.0;  // 2^53
    if(!std::isfinite(value) || value < 0.0 || value > MAX_EXACT_INTEGER)
    {
        return default_value;
    }
    return static_cast<uint64_t>(value);
}

// Reads an unsigned id from JSON, however the model chose to encode it.
uint64_t
JsonU64(const jt::Json& json, const std::string& key, uint64_t default_value)
{
    jt::Json& mutable_json = const_cast<jt::Json&>(json);
    if(!mutable_json.contains(key))
    {
        return default_value;
    }
    jt::Json& value = mutable_json[key];
    if(value.isLong())
    {
        const long long raw = value.getLong();
        return raw < 0 ? default_value : static_cast<uint64_t>(raw);
    }
    if(value.isDouble())
    {
        return JsonU64FromDouble(value.getDouble(), default_value);
    }
    if(value.isString())
    {
        // Whole string or nothing. strtoull alone would read "12abc" as 12 and
        // "-1" as UINT64_MAX, and an id the model half-meant is worse than one
        // it did not give: the tool would answer confidently about the wrong
        // row instead of saying it could not read the argument.
        const std::string& text = value.getString();
        size_t             first = text.find_first_not_of(" \t\n\r");
        if(first == std::string::npos || text[first] == '-' || text[first] == '+')
        {
            return default_value;
        }
        errno                     = 0;
        char*              end    = nullptr;
        unsigned long long parsed = std::strtoull(text.c_str() + first, &end, 10);
        if(end == nullptr || end == text.c_str() + first || errno == ERANGE)
        {
            return default_value;
        }
        while(*end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
        {
            ++end;
        }
        if(*end != '\0')
        {
            return default_value;
        }
        return static_cast<uint64_t>(parsed);
    }
    return default_value;
}

// Caps a tool result so one big answer cannot fill the model's context. The
// head is what is kept: a result that runs long leads with its summary and
// puts the rows after it.
std::string
TrimAssistantText(std::string text, size_t max_chars, const char* truncation_note)
{
    if(text.size() > max_chars)
    {
        text.resize(max_chars);
        text += truncation_note;
    }
    return text;
}

// Rejects a model-supplied array that is longer than anything a tool can use.
// Returns true when the array is fine to walk.
bool
CheckArrayLength(const std::vector<jt::Json>& entries, const char* argument_name,
                 std::string& error_out)
{
    if(entries.size() <= ASSISTANT_MAX_ARRAY_ENTRIES)
    {
        return true;
    }
    error_out = std::string(argument_name) + " has too many entries. Use at most " +
                std::to_string(ASSISTANT_MAX_ARRAY_ENTRIES) + ".";
    return false;
}

// Yields the selected time range, or the whole trace if nothing is selected.
void
SelectedOrFullTimeRange(const AssistantToolContext& context, double& start_ns,
                        double& end_ns)
{
    const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
    start_ns                      = timeline.GetStartTime();
    end_ns                        = timeline.GetEndTime();
    if(context.timeline_selection != nullptr &&
       context.timeline_selection->HasValidTimeRangeSelection())
    {
        context.timeline_selection->GetSelectedTimeRange(start_ns, end_ns);
    }
}

// Looked up by id on every call rather than held, so a closed tab cannot leave
// a tool holding a dead provider: it simply comes back empty.
AssistantToolContext
MakeAssistantToolContext(const std::string& project_id)
{
    AssistantToolContext context;
    context.project_id = project_id;

    AppWindow* app = AppWindow::GetInstance();
    if(app == nullptr || project_id.empty())
    {
        return context;
    }
    Project* project = app->GetProject(project_id);
    if(project == nullptr)
    {
        return context;
    }

    context.trace_name  = project->GetName();
    context.is_compute  = project->GetTraceType() == Project::Compute;
    RootView* root_view = dynamic_cast<RootView*>(project->GetView().get());
    if(root_view != nullptr)
    {
        context.data_provider = root_view->GetDataProvider();
    }
    if(project->GetTraceType() == Project::System)
    {
        TraceView* trace_view = dynamic_cast<TraceView*>(project->GetView().get());
        context.trace_view    = trace_view;
        if(trace_view != nullptr && trace_view->GetTimelineSelection())
        {
            context.timeline_selection = trace_view->GetTimelineSelection().get();
        }
        context.is_compare = context.data_provider != nullptr &&
                             context.data_provider->DataModel().HasCompareSources();
    }
    const Project* in_front = app->GetCurrentProject();
    context.in_front        = in_front != nullptr && in_front->GetID() == project_id;
    return context;
}

TraceLinkStore&
AssistantTraceLinks()
{
    static TraceLinkStore store(
        (std::filesystem::path(get_application_config_path(true)) / TRACE_LINKS_FILE_NAME)
            .string());
    return store;
}

TraceFingerprint
AssistantTraceFingerprint(const AssistantToolContext& context)
{
    TraceFingerprint fingerprint;
    if(context.data_provider == nullptr || context.is_compute ||
       context.data_provider->GetState() != ProviderState::kReady)
    {
        return fingerprint;
    }
    const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
    fingerprint.start_ns          = timeline.GetStartTime();
    fingerprint.end_ns            = timeline.GetEndTime();
    fingerprint.track_count       = timeline.GetTrackList().size();
    return fingerprint;
}

std::string
BuildAssistantLinkBlock(const AssistantToolContext& context)
{
    const std::vector<TraceLink> links =
        AssistantTraceLinks().LinksOf(context.project_id);
    if(links.empty())
    {
        return std::string();
    }

    std::ostringstream out;
    out << "linked_traces:\n";
    for(const TraceLink& link : links)
    {
        const AssistantToolContext linked = MakeAssistantToolContext(link.other.path);
        out << "  ref=" << link.group_id << " name=\"" << link.other.label
            << "\" role=" << link.other.role << " (this trace is the " << link.self.role
            << ") status=" << LinkStatus(context, link, linked) << "\n";
    }
    out << "note: the user linked these runs to this trace. Call compare_traces to diff "
           "this trace against one, or pass trace=<ref> to a read tool to read it "
           "directly.\n";
    return out.str();
}

bool
ResolveAssistantLink(const AssistantToolContext& context, const std::string& ref,
                     TraceLink& link_out, std::string& error_out)
{
    const std::vector<TraceLink> links =
        AssistantTraceLinks().LinksOf(context.project_id);
    if(links.empty())
    {
        error_out = "No other trace is linked to this one. The user links runs with the "
                    "Link runs button in the Ask Optiq header.";
        return false;
    }
    if(ref.empty())
    {
        if(links.size() == 1)
        {
            link_out = links.front();
            return true;
        }
        error_out = "Several traces are linked to this one, so name one by ref: " +
                    LinkRefList(links) + ".";
        return false;
    }

    for(const TraceLink& link : links)
    {
        if(link.group_id == ref)
        {
            link_out = link;
            return true;
        }
    }

    // A file name or a role is how the user would say it, so accept either
    // when it picks out exactly one link.
    const std::string lowered   = Core::String::to_lower_copy(ref);
    const TraceLink*  by_name   = nullptr;
    const TraceLink*  by_role   = nullptr;
    size_t            name_hits = 0;
    size_t            role_hits = 0;
    for(const TraceLink& link : links)
    {
        if(Core::String::to_lower_copy(link.other.label) == lowered)
        {
            by_name = &link;
            ++name_hits;
        }
        if(Core::String::to_lower_copy(link.other.role) == lowered)
        {
            by_role = &link;
            ++role_hits;
        }
    }
    if(name_hits == 1)
    {
        link_out = *by_name;
        return true;
    }
    if(role_hits == 1)
    {
        link_out = *by_role;
        return true;
    }
    error_out =
        "No linked trace matches \"" + ref + "\". Linked: " + LinkRefList(links) + ".";
    return false;
}

std::string
LinkedTraceProblem(const AssistantToolContext& linked, const TraceLink& link)
{
    if(linked.data_provider == nullptr)
    {
        return link.other.label + " is linked but not open, so it cannot be read. Ask "
                                  "the user to open it.";
    }
    if(linked.is_compute)
    {
        return link.other.label + " is a compute trace, and Ask Optiq reads system "
                                  "traces.";
    }
    if(linked.is_compare)
    {
        return link.other.label + " is a compare tab, which every tool would read as one "
                                  "trace.";
    }
    // Refused rather than parked: a background tab that is still opening could
    // hold the turn for minutes with nothing on screen to explain the wait.
    if(linked.data_provider->GetState() != ProviderState::kReady)
    {
        return link.other.label + " is still loading. Try again once it has opened.";
    }
    return std::string();
}

// Dispatches one named tool call, returning either a finished result or a
// pending fetch the panel polls until the rows land.
AssistantToolStartResult
StartAssistantTool(const AssistantToolContext& context, const std::string& tool_name,
                   const std::string& arguments_json)
{
    bool           args_ok = true;
    const jt::Json args    = ParseArgsObject(arguments_json, args_ok);
    if(!args_ok)
    {
        return DoneResult("Those arguments were not a JSON object, so none of them "
                          "were read. Send arguments as one JSON object.",
                          "Bad arguments");
    }

    ToolFamily                family = ToolFamily::kData;
    const AssistantToolEntry* entry  = FindTool(tool_name, family);
    if(entry == nullptr)
    {
        return DoneResult("Unknown tool \"" + tool_name +
                              "\". Available tools: " + AssistantToolNameList() + ".",
                          "Unknown tool");
    }

    // Next-step buttons are a UI side-effect, so they are the one tool that
    // does not need an open trace.
    if(tool_name == ASSISTANT_NEXT_STEPS_TOOL)
    {
        return entry->handler(context, args, tool_name);
    }

    const std::string trace_ref = JsonUtils::GetString(args, "trace", "");
    if(!trace_ref.empty())
    {
        return StartLinkedTool(context, *entry, family, args, tool_name, trace_ref);
    }

    if(context.data_provider == nullptr)
    {
        return DoneResult("No trace is open.", "No trace");
    }
    // Ask Optiq reads system traces. Refused once here rather than in every
    // tool: the panel is a singleton, so a compute tab can be in front even
    // when the panel was opened on a system trace.
    if(context.is_compute)
    {
        return DoneResult(
            "This is a compute trace, and Ask Optiq reads system traces. Tell "
            "the user to open a system trace to ask about it.",
            "Compute trace");
    }
    // A compare tab holds two runs in one provider, and every tool reads a
    // provider as one trace, so its totals would add the runs together.
    if(context.is_compare)
    {
        return DoneResult(
            "This tab compares two traces side by side, and Ask Optiq reads one trace "
            "at a time, so its numbers would add both runs together. Tell the user to "
            "open the two files on their own and link them with the Link runs button "
            "in the Ask Optiq header; compare_traces then diffs them.",
            "Compare tab");
    }
    // Park rather than fail. Loading a large trace takes far longer than a
    // round trip to the model, so answering "not ready" straight away only
    // teaches it to retry into the same wall, or to answer without the data
    // it asked for.
    if(context.data_provider->GetState() != ProviderState::kReady)
    {
        AssistantToolStartResult waiting;
        waiting.pending          = true;
        waiting.fetch.kind       = AssistantFetchKind::kTraceLoading;
        waiting.fetch.project_id = context.project_id;
        waiting.status_line      = "Waiting for the trace to finish loading...";
        waiting.timeout_seconds  = ASSISTANT_TRACE_LOADING_TIMEOUT_SECONDS;
        return waiting;
    }
    // UI and script tools change, or wait on, what the user sees. A turn keeps
    // reading its own trace after the user switches tabs, but it must not
    // rearrange a tab they are no longer looking at.
    if(family != ToolFamily::kData && !context.in_front)
    {
        return DoneResult("The user switched to another tab, so " + tool_name +
                              " was not applied to " + context.trace_name +
                              ". Say in the answer where to look instead.",
                          "Not in front");
    }

    AssistantToolStartResult result = entry->handler(context, args, tool_name);
    if(result.fetch.project_id.empty())
    {
        result.fetch.project_id = context.project_id;
    }
    return result;
}

}  // namespace View
}  // namespace RocProfVis
