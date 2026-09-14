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
#include <string>
#include <vector>

#include "json.h"

#include "model/rocprofvis_timeline_model.h"
#include "rocprofvis_ai_tool_schema.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_timeline_selection.h"

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

// Dispatches one named tool call, returning either a finished result or a
// pending fetch the panel polls until the rows land. The UI tools are searched
// first: they are the cheap ones, and none of them share a name with a query.
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

    // Next-step buttons are a UI side-effect, so they are the one tool that
    // does not need an open trace.
    if(tool_name != "offer_next_steps")
    {
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
        // Park rather than fail. Loading a large trace takes far longer than a
        // round trip to the model, so answering "not ready" straight away only
        // teaches it to retry into the same wall, or to answer without the data
        // it asked for.
        if(context.data_provider->GetState() != ProviderState::kReady)
        {
            AssistantToolStartResult waiting;
            waiting.pending         = true;
            waiting.fetch.kind      = AssistantFetchKind::kTraceLoading;
            waiting.status_line     = "Waiting for the trace to finish loading...";
            waiting.timeout_seconds = ASSISTANT_TRACE_LOADING_TIMEOUT_SECONDS;
            return waiting;
        }
    }

    const AssistantToolTable tables[] = { GetAssistantUiToolHandlers(),
                                          GetAssistantDataToolHandlers(),
                                          GetAssistantScriptToolHandlers() };
    for(const AssistantToolTable& table : tables)
    {
        for(size_t i = 0; i < table.count; ++i)
        {
            if(tool_name == table.entries[i].name)
            {
                return table.entries[i].handler(context, args, tool_name);
            }
        }
    }

    return DoneResult("Unknown tool \"" + tool_name + "\". Available tools: " +
                          AssistantToolNameList() + ".",
                      "Unknown tool");
}

}  // namespace View
}  // namespace RocProfVis
