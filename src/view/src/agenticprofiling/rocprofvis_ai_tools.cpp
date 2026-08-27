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

#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "model/rocprofvis_timeline_model.h"
#include "rocprofvis_ai_tool_schema.h"
#include "rocprofvis_core_string_utils.h"
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

// Narrows a JSON number to an id. A double that is negative, not finite, or
// too large to represent is rejected rather than cast: those conversions are
// undefined, not merely wrong.
uint64_t
JsonU64FromDouble(double value, uint64_t default_value)
{
    constexpr double MAX_EXACT_U64 = 18446744073709549568.0;
    if(!std::isfinite(value) || value < 0.0 || value > MAX_EXACT_U64)
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

// The workload a compute tool should read: whichever one is selected, or the
// first loaded when nothing is. Null when the trace has none at all.
const WorkloadInfo*
SelectedComputeWorkload(const AssistantToolContext& context)
{
    ComputeDataModel& model       = context.data_provider->ComputeModel();
    uint32_t          workload_id = ComputeSelection::INVALID_SELECTION_ID;
    if(context.compute_selection != nullptr)
    {
        workload_id = context.compute_selection->GetSelectedWorkload();
    }
    const WorkloadInfo* workload = model.GetWorkload(workload_id);
    if(workload == nullptr && !model.GetWorkloadList().empty())
    {
        workload = model.GetWorkloadList().front();
    }
    return workload;
}

// Finds a kernel by id, then exact name, then a case-insensitive substring.
const KernelInfo*
FindComputeKernel(const WorkloadInfo& workload, const std::string& name, uint32_t id)
{
    if(id != ComputeSelection::INVALID_SELECTION_ID)
    {
        const auto it = workload.kernels.find(id);
        if(it != workload.kernels.end())
        {
            return &it->second;
        }
    }
    if(name.empty())
    {
        return nullptr;
    }
    for(const KernelInfo* kernel : workload.ordered_kernels)
    {
        if(kernel != nullptr && kernel->name == name)
        {
            return kernel;
        }
    }
    const std::string lowered = Core::String::to_lower_copy(name);
    for(const KernelInfo* kernel : workload.ordered_kernels)
    {
        if(kernel == nullptr)
        {
            continue;
        }
        if(Core::String::to_lower_copy(kernel->name).find(lowered) != std::string::npos)
        {
            return kernel;
        }
    }
    return nullptr;
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
        if(context.data_provider->GetState() != ProviderState::kReady)
        {
            return DoneResult("The trace is still loading. Wait and try again.",
                              "Trace not ready");
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
