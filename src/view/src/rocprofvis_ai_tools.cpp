// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_tools.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "json.h"
#include "spdlog/spdlog.h"

#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "model/rocprofvis_analysis_model.h"
#include "model/rocprofvis_summary_model.h"
#include "model/rocprofvis_tables_model.h"
#include "model/rocprofvis_timeline_model.h"
#include "model/rocprofvis_topology_model.h"
#include "rocprofvis_c_interface_types.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_json_utils.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_utils.h"

namespace RocProfVis
{
namespace View
{

namespace
{

constexpr size_t ASSISTANT_DEFAULT_ROW_LIMIT = 10;
constexpr size_t ASSISTANT_MAX_ROW_LIMIT     = 20;
constexpr size_t ASSISTANT_MAX_TRACKS        = 32;
constexpr size_t ASSISTANT_MAX_LIST_TRACKS   = 40;
constexpr size_t ASSISTANT_MAX_METRIC_NAMES  = 40;
constexpr size_t ASSISTANT_MAX_METRIC_FETCH  = 8;
constexpr size_t ASSISTANT_TOP_KERNEL_LIMIT  = 10;
constexpr size_t ASSISTANT_MAX_RESULT_CHARS  = 6000;

struct TopEventsSpec
{
    const char*                        key;
    rocprofvis_controller_table_type_t controller_type;
    TableType                          table_type;
    uint64_t                           request_id;
    rocprofvis_dm_event_operation_t    op;
};

const TopEventsSpec k_top_event_specs[] = {
    { "dispatch", kRPVControllerTableTypeDispatchEvents,
      TableType::kAnalysisTopDispatchEventsTable,
      DataProvider::ANALYSIS_TOP_DISPATCH_EVENTS_TABLE_REQUEST_ID,
      kRocProfVisDmOperationDispatch },
    { "memory_copy", kRPVControllerTableTypeMemoryCopyEvents,
      TableType::kAnalysisTopMemoryCopyEventsTable,
      DataProvider::ANALYSIS_TOP_MEMORY_COPY_EVENTS_TABLE_REQUEST_ID,
      kRocProfVisDmOperationMemoryCopy },
    { "memory_alloc", kRPVControllerTableTypeMemoryAllocationEvents,
      TableType::kAnalysisTopMemoryAllocationEventsTable,
      DataProvider::ANALYSIS_TOP_MEMORY_ALLOCATION_EVENTS_TABLE_REQUEST_ID,
      kRocProfVisDmOperationMemoryAllocate },
    { "instrumented", kRPVControllerTableTypeInstrumentedEvents,
      TableType::kAnalysisTopInstrumentedEventsTable,
      DataProvider::ANALYSIS_TOP_INSTRUMENTED_EVENTS_TABLE_REQUEST_ID,
      kRocProfVisDmOperationLaunch },
    { "sampled", kRPVControllerTableTypeSampledEvents,
      TableType::kAnalysisTopSampledEventsTable,
      DataProvider::ANALYSIS_TOP_LAUNCH_SAMPLED_TABLE_REQUEST_ID,
      kRocProfVisDmOperationLaunchSample },
};

std::string
ToLowerCopy(std::string value)
{
    for(char& c : value)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool
NameLooksInternal(const std::string& name)
{
    return name.size() >= 2 && name[0] == '_' && name[1] == '_';
}

bool
KeepInternalColumn(const std::string& name)
{
    return name == "__uuid" || name == "__trackId" || name == "__streamTrackId";
}

size_t
ClampRowLimit(int32_t requested)
{
    if(requested <= 0)
    {
        return ASSISTANT_DEFAULT_ROW_LIMIT;
    }
    return std::min(static_cast<size_t>(requested), ASSISTANT_MAX_ROW_LIMIT);
}

std::string
FormatPercent(float value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f", static_cast<double>(value));
    return std::string(buffer);
}

void
AppendGpuMetrics(std::ostringstream& out, const SummaryInfo::GPUMetrics& gpu)
{
    if(gpu.gfx_utilization.has_value())
    {
        out << "gfx_util_pct: " << FormatPercent(gpu.gfx_utilization.value()) << "\n";
    }
    if(gpu.mem_utilization.has_value())
    {
        out << "mem_util_pct: " << FormatPercent(gpu.mem_utilization.value()) << "\n";
    }
    out << "kernel_exec_time_total_ns: " << gpu.kernel_exec_time_total << "\n";
    out << "top_kernels:\n";
    const size_t count = std::min(gpu.top_kernels.size(), ASSISTANT_TOP_KERNEL_LIMIT);
    if(count == 0)
    {
        out << "  (none)\n";
        return;
    }
    for(size_t i = 0; i < count; ++i)
    {
        const SummaryInfo::KernelMetrics& kernel = gpu.top_kernels[i];
        out << "  " << (i + 1) << ". " << kernel.name << "  pct=" << kernel.exec_time_pct
            << "  calls=" << kernel.invocations << "  sum_ns=" << kernel.exec_time_sum
            << "\n";
    }
}

std::string
TrimResult(std::string text)
{
    if(text.size() > ASSISTANT_MAX_RESULT_CHARS)
    {
        text.resize(ASSISTANT_MAX_RESULT_CHARS);
        text += "\n... truncated ...\n";
    }
    return text;
}

jt::Json
ParseArgsObject(const std::string& arguments_json)
{
    if(arguments_json.empty())
    {
        jt::Json empty;
        empty.setObject();
        return empty;
    }
    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(arguments_json);
    if(parsed.first != jt::Json::success)
    {
        jt::Json empty;
        empty.setObject();
        return empty;
    }
    if(parsed.second.isObject())
    {
        return parsed.second;
    }
    jt::Json empty;
    empty.setObject();
    return empty;
}

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
        const double raw = value.getDouble();
        return raw < 0.0 ? default_value : static_cast<uint64_t>(raw);
    }
    if(value.isString())
    {
        char*              end  = nullptr;
        unsigned long long parsed = std::strtoull(value.getString().c_str(), &end, 10);
        if(end != nullptr && end != value.getString().c_str())
        {
            return static_cast<uint64_t>(parsed);
        }
    }
    return default_value;
}

const TopEventsSpec*
FindTopEventsSpec(const std::string& category)
{
    const std::string lowered = ToLowerCopy(category);
    std::string       key     = lowered;
    if(key == "kernel" || key == "kernels" || key == "dispatch_events")
    {
        key = "dispatch";
    }
    else if(key == "copy" || key == "memcpy" || key == "memory-copy")
    {
        key = "memory_copy";
    }
    else if(key == "alloc" || key == "allocate" || key == "memory-alloc")
    {
        key = "memory_alloc";
    }
    else if(key == "cpu" || key == "thread" || key == "launch")
    {
        key = "instrumented";
    }
    else if(key == "sample" || key == "sampling")
    {
        key = "sampled";
    }

    for(const TopEventsSpec& spec : k_top_event_specs)
    {
        if(key == spec.key)
        {
            return &spec;
        }
    }
    return nullptr;
}

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

std::vector<uint64_t>
TracksForOperation(const AssistantToolContext&     context,
                   rocprofvis_dm_event_operation_t op)
{
    std::vector<uint64_t> selected;
    if(context.timeline_selection != nullptr)
    {
        context.timeline_selection->GetSelectedTracks(selected);
    }

    std::vector<uint64_t> matching;
    matching.reserve(ASSISTANT_MAX_TRACKS);
    for(uint64_t track_id : selected)
    {
        const TrackInfo* track =
            context.data_provider->DataModel().GetTimeline().GetTrack(track_id);
        if(track != nullptr && track->operation_types.count(op) > 0 &&
           track->track_type == kRPVControllerTrackTypeEvents)
        {
            matching.push_back(track_id);
            if(matching.size() >= ASSISTANT_MAX_TRACKS)
            {
                return matching;
            }
        }
    }
    if(!matching.empty())
    {
        return matching;
    }

    const std::vector<const TrackInfo*> tracks =
        context.data_provider->DataModel().GetTimeline().GetTrackList();
    for(const TrackInfo* track : tracks)
    {
        if(track == nullptr)
        {
            continue;
        }
        if(track->operation_types.count(op) == 0)
        {
            continue;
        }
        if(track->track_type != kRPVControllerTrackTypeEvents)
        {
            continue;
        }
        matching.push_back(track->id);
        if(matching.size() >= ASSISTANT_MAX_TRACKS)
        {
            break;
        }
    }
    return matching;
}

std::string
FormatTableSnapshot(const TablesModel& tables, TableType type, size_t limit)
{
    const std::vector<std::string>&              header = tables.GetTableHeader(type);
    const std::vector<std::vector<std::string>>& data   = tables.GetTableData(type);
    if(header.empty() && data.empty())
    {
        return "No rows returned.";
    }

    std::vector<size_t> columns;
    std::ostringstream  out;
    out << "columns:";
    for(size_t i = 0; i < header.size(); ++i)
    {
        if(NameLooksInternal(header[i]) && !KeepInternalColumn(header[i]))
        {
            continue;
        }
        columns.push_back(i);
        out << " " << header[i];
    }
    out << "\n";

    const size_t row_count = std::min(data.size(), limit);
    for(size_t row = 0; row < row_count; ++row)
    {
        out << (row + 1) << ".";
        for(size_t column_index : columns)
        {
            if(column_index >= data[row].size())
            {
                continue;
            }
            out << " " << header[column_index] << "=" << data[row][column_index];
        }
        out << "\n";
    }

    const uint64_t total = tables.GetTableTotalRowCount(type);
    if(total > row_count)
    {
        out << "... " << (total - row_count) << " more rows not shown\n";
    }
    return TrimResult(out.str());
}

std::string
FormatSystemSummary(const AssistantToolContext& context)
{
    std::ostringstream out;
    const SummaryInfo::AggregateMetrics& summary =
        context.data_provider->DataModel().GetSummary().GetSummaryData();
    AppendGpuMetrics(out, summary.gpu);
    if(summary.gpu.top_kernels.empty())
    {
        out << "note: summary has no kernels. Call top_events category=dispatch.\n";
    }
    return out.str();
}

std::string
FormatComputeKernels(const AssistantToolContext& context, size_t limit)
{
    ComputeDataModel& model = context.data_provider->ComputeModel();
    uint32_t workload_id    = ComputeSelection::INVALID_SELECTION_ID;
    if(context.compute_selection != nullptr)
    {
        workload_id = context.compute_selection->GetSelectedWorkload();
    }
    const WorkloadInfo* workload = model.GetWorkload(workload_id);
    if(workload == nullptr && !model.GetWorkloadList().empty())
    {
        workload = model.GetWorkloadList().front();
    }
    if(workload == nullptr)
    {
        return "No compute workload is loaded.";
    }

    std::vector<const KernelInfo*> kernels = workload->ordered_kernels;
    std::sort(kernels.begin(), kernels.end(),
              [](const KernelInfo* a, const KernelInfo* b) {
                  if(a == nullptr || b == nullptr)
                  {
                      return b == nullptr;
                  }
                  return a->dispatch_metrics[KernelInfo::DurationTotal] >
                         b->dispatch_metrics[KernelInfo::DurationTotal];
              });

    std::ostringstream out;
    out << "workload: " << workload->name << "\n";
    out << "top_kernels:\n";
    const size_t count = std::min(kernels.size(), limit);
    if(count == 0)
    {
        out << "  (none)\n";
        return out.str();
    }
    for(size_t i = 0; i < count; ++i)
    {
        const KernelInfo* kernel = kernels[i];
        if(kernel == nullptr)
        {
            continue;
        }
        out << "  " << (i + 1) << ". id=" << kernel->id << " " << kernel->name
            << "  duration_total_ns=" << kernel->dispatch_metrics[KernelInfo::DurationTotal]
            << "  calls=" << kernel->dispatch_metrics[KernelInfo::InvocationCount]
            << "  mean_ns=" << kernel->dispatch_metrics[KernelInfo::DurationMean]
            << "\n";
    }
    return out.str();
}

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
    const std::string lowered = ToLowerCopy(name);
    for(const KernelInfo* kernel : workload.ordered_kernels)
    {
        if(kernel == nullptr)
        {
            continue;
        }
        if(ToLowerCopy(kernel->name).find(lowered) != std::string::npos)
        {
            return kernel;
        }
    }
    return nullptr;
}

const char*
TrackTypeName(TrackInfo::TrackType type)
{
    switch(type)
    {
        case TrackInfo::Queue:              return "Queue";
        case TrackInfo::Stream:             return "Stream";
        case TrackInfo::InstrumentedThread: return "InstrumentedThread";
        case TrackInfo::SampledThread:      return "SampledThread";
        case TrackInfo::Counter:            return "Counter";
        default:                            return "Unknown";
    }
}

void
AppendOpName(std::ostringstream& out, bool& first, rocprofvis_dm_event_operation_t op,
             const TrackInfo& track)
{
    if(track.operation_types.count(op) == 0)
    {
        return;
    }
    if(!first)
    {
        out << ",";
    }
    first = false;
    switch(op)
    {
        case kRocProfVisDmOperationLaunch:         out << "instrumented"; break;
        case kRocProfVisDmOperationDispatch:       out << "dispatch"; break;
        case kRocProfVisDmOperationMemoryAllocate: out << "memory_alloc"; break;
        case kRocProfVisDmOperationMemoryCopy:     out << "memory_copy"; break;
        case kRocProfVisDmOperationLaunchSample:   out << "sampled"; break;
        default:                                   out << "other"; break;
    }
}

AssistantToolStartResult
DoneResult(const std::string& content, const std::string& status)
{
    AssistantToolStartResult result;
    result.content     = content;
    result.status_line = status;
    return result;
}

AssistantToolStartResult
PendingResult(uint64_t request_id, AssistantFetchKind kind, size_t row_limit,
              const std::string& status, bool started_fetch, TableType table_type,
              uint32_t kernel_id)
{
    AssistantToolStartResult result;
    result.pending       = true;
    result.started_fetch = started_fetch;
    result.request_id    = request_id;
    result.fetch_kind    = kind;
    result.table_type    = table_type;
    result.kernel_id     = kernel_id;
    result.row_limit     = row_limit;
    result.status_line   = status;
    return result;
}

jt::Json
MakeStringEnum(const std::vector<const char*>& values)
{
    jt::Json array;
    for(size_t i = 0; i < values.size(); ++i)
    {
        array[i] = values[i];
    }
    return array;
}

void
AddTool(jt::Json& tools, size_t index, const char* name, const char* description,
        jt::Json parameters)
{
    tools[index]["type"]                    = "function";
    tools[index]["function"]["name"]        = name;
    tools[index]["function"]["description"] = description;
    tools[index]["function"]["parameters"]  = parameters;
}

jt::Json
ObjectParams()
{
    jt::Json params;
    params["type"] = "object";
    params["properties"].setObject();
    return params;
}

}  // namespace

jt::Json
BuildAssistantToolsJson()
{
    jt::Json tools;

    jt::Json summary_params = ObjectParams();
    AddTool(tools, 0, "get_summary",
            "Load named GPU summary stats and the top kernels by execution time. "
            "Call this first when the briefing is empty or kernel_exec_time_total_ns is 0.",
            summary_params);

    jt::Json top_params = ObjectParams();
    top_params["properties"]["category"]["type"]        = "string";
    top_params["properties"]["category"]["description"] =
        "Event family to rank by duration.";
    top_params["properties"]["category"]["enum"] =
        MakeStringEnum({ "dispatch", "memory_copy", "memory_alloc", "instrumented",
                         "sampled" });
    top_params["properties"]["limit"]["type"]        = "integer";
    top_params["properties"]["limit"]["description"] =
        "Max rows to return (default 10, max 20).";
    top_params["required"][0] = "category";
    AddTool(tools, 1, "top_events",
            "Rank the hottest events of one category in the current time range "
            "(or the full trace). Uses the same analysis tables as View > Top Events. "
            "Does not run raw SQL.",
            top_params);

    jt::Json inst_params = ObjectParams();
    inst_params["properties"]["kernel_name"]["type"]        = "string";
    inst_params["properties"]["kernel_name"]["description"] =
        "Exact or unique kernel name from get_summary / top_events.";
    inst_params["properties"]["limit"]["type"]        = "integer";
    inst_params["properties"]["limit"]["description"] =
        "Max dispatch rows (default 10, max 20).";
    inst_params["required"][0] = "kernel_name";
    AddTool(tools, 2, "kernel_instances",
            "List individual GPU dispatches for one kernel name, with timestamps "
            "and track ids that goto can use.",
            inst_params);

    jt::Json metrics_params = ObjectParams();
    metrics_params["properties"]["kernel_name"]["type"]        = "string";
    metrics_params["properties"]["kernel_name"]["description"] =
        "Kernel name (compute traces) or summary kernel name (system traces).";
    metrics_params["properties"]["metric_name"]["type"]        = "string";
    metrics_params["properties"]["metric_name"]["description"] =
        "Optional substring of a hardware metric to fetch on a compute trace.";
    AddTool(tools, 3, "kernel_metrics",
            "Return duration/call stats for a kernel. On compute traces, can also "
            "fetch named hardware metrics. On system traces, returns summary stats.",
            metrics_params);

    jt::Json tracks_params = ObjectParams();
    AddTool(tools, 4, "list_tracks",
            "List timeline tracks with ids, names, and event families. Use before goto "
            "if you need a track_id.",
            tracks_params);

    jt::Json goto_params = ObjectParams();
    goto_params["properties"]["start_ns"]["type"]        = "number";
    goto_params["properties"]["start_ns"]["description"] =
        "Range start in nanoseconds.";
    goto_params["properties"]["end_ns"]["type"]        = "number";
    goto_params["properties"]["end_ns"]["description"] = "Range end in nanoseconds.";
    goto_params["properties"]["track_id"]["type"]        = "integer";
    goto_params["properties"]["track_id"]["description"] =
        "Optional track to scroll into view.";
    goto_params["properties"]["event_uuid"]["type"]        = "integer";
    goto_params["properties"]["event_uuid"]["description"] =
        "Optional event uuid from kernel_instances to highlight.";
    goto_params["properties"]["kernel_name"]["type"]        = "string";
    goto_params["properties"]["kernel_name"]["description"] =
        "On compute traces, select this kernel in the UI.";
    AddTool(tools, 5, "goto",
            "Move the Optiq UI to a time range and optionally highlight a track/event "
            "or select a compute kernel. Call this when you know where the user should look.",
            goto_params);

    return tools;
}

std::string
BuildAssistantBriefing(const AssistantToolContext& context)
{
    if(context.data_provider == nullptr)
    {
        return "No trace is open. Open a .db / .rpd file first.";
    }

    std::ostringstream out;
    out << "trace_name: " << context.trace_name << "\n";
    if(context.is_compute)
    {
        out << "kind: compute_trace\n";
        out << FormatComputeKernels(context, ASSISTANT_TOP_KERNEL_LIMIT);
        return out.str();
    }

    out << "kind: system_trace\n";
    const TopologyDataModel& topology = context.data_provider->DataModel().GetTopology();
    const std::vector<const NodeInfo*> nodes = topology.GetNodeList();
    for(const NodeInfo* node : nodes)
    {
        if(node == nullptr)
        {
            continue;
        }
        out << "node: " << node->host_name << "\n";
        for(uint64_t device_id : node->device_ids)
        {
            const DeviceInfo* device = topology.GetDevice(device_id);
            if(device == nullptr)
            {
                continue;
            }
            out << "  device: " << device->product_name << "\n";
        }
    }

    SettingsManager& settings    = SettingsManager::GetInstance();
    const TimeFormat time_format = settings.GetUserSettings().unit_settings.time_format;
    if(context.timeline_selection != nullptr)
    {
        double start_ns = 0.0;
        double end_ns   = 0.0;
        if(context.timeline_selection->GetSelectedTimeRange(start_ns, end_ns))
        {
            out << "selected_range: "
                << nanosecond_to_formatted_str(start_ns, time_format, true) << " .. "
                << nanosecond_to_formatted_str(end_ns, time_format, true) << "\n";
        }
        std::vector<uint64_t> track_ids;
        if(context.timeline_selection->GetSelectedTracks(track_ids) && !track_ids.empty())
        {
            out << "selected_track_count: " << track_ids.size() << "\n";
        }
    }

    out << "summary:\n";
    out << FormatSystemSummary(context);
    return out.str();
}

std::string
AssistantToolStatusLabel(const std::string& tool_name)
{
    if(tool_name == "get_summary")
    {
        return "Loading summary...";
    }
    if(tool_name == "top_events")
    {
        return "Querying top events...";
    }
    if(tool_name == "kernel_instances")
    {
        return "Loading kernel dispatches...";
    }
    if(tool_name == "kernel_metrics")
    {
        return "Loading kernel metrics...";
    }
    if(tool_name == "list_tracks")
    {
        return "Listing tracks...";
    }
    if(tool_name == "goto")
    {
        return "Moving the view...";
    }
    return "Using " + tool_name + "...";
}

AssistantToolStartResult
StartAssistantTool(const AssistantToolContext& context, const std::string& tool_name,
                   const std::string& arguments_json)
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

    const jt::Json args = ParseArgsObject(arguments_json);

    if(tool_name == "get_summary")
    {
        if(context.is_compute)
        {
            return DoneResult(FormatComputeKernels(context, ASSISTANT_TOP_KERNEL_LIMIT),
                              "Read compute kernel stats");
        }
        if(context.data_provider->IsRequestPending(DataProvider::SUMMARY_REQUEST_ID))
        {
            return PendingResult(DataProvider::SUMMARY_REQUEST_ID,
                                 AssistantFetchKind::kSummary,
                                 ASSISTANT_DEFAULT_ROW_LIMIT, "Loading summary...", false,
                                 TableType::kSummaryKernelTable, 0);
        }
        const SummaryInfo::GPUMetrics& gpu =
            context.data_provider->DataModel().GetSummary().GetSummaryData().gpu;
        if(!gpu.top_kernels.empty() || gpu.kernel_exec_time_total > 0.0)
        {
            return DoneResult(FormatSystemSummary(context), "Read summary");
        }
        if(context.data_provider->FetchSummary())
        {
            return PendingResult(DataProvider::SUMMARY_REQUEST_ID,
                                 AssistantFetchKind::kSummary,
                                 ASSISTANT_DEFAULT_ROW_LIMIT, "Loading summary...", true,
                                 TableType::kSummaryKernelTable, 0);
        }
        if(context.data_provider->IsRequestPending(DataProvider::SUMMARY_REQUEST_ID))
        {
            return PendingResult(DataProvider::SUMMARY_REQUEST_ID,
                                 AssistantFetchKind::kSummary,
                                 ASSISTANT_DEFAULT_ROW_LIMIT, "Loading summary...", false,
                                 TableType::kSummaryKernelTable, 0);
        }
        return DoneResult(FormatSystemSummary(context), "Read summary");
    }

    if(tool_name == "list_tracks")
    {
        if(context.is_compute)
        {
            return DoneResult("Compute traces have no timeline tracks. Use kernel_metrics.",
                              "No timeline tracks");
        }
        const std::vector<const TrackInfo*> tracks =
            context.data_provider->DataModel().GetTimeline().GetTrackList();
        std::ostringstream out;
        size_t             shown = 0;
        for(const TrackInfo* track : tracks)
        {
            if(track == nullptr)
            {
                continue;
            }
            if(track->topology.type != TrackInfo::Queue &&
               track->topology.type != TrackInfo::Stream &&
               track->topology.type != TrackInfo::InstrumentedThread &&
               track->topology.type != TrackInfo::SampledThread)
            {
                continue;
            }
            out << "track_id=" << track->id << " type=" << TrackTypeName(track->topology.type)
                << " name=\""
                << context.data_provider->DataModel().BuildTrackName(track->id) << "\"";
            out << " ops=";
            bool first = true;
            AppendOpName(out, first, kRocProfVisDmOperationDispatch, *track);
            AppendOpName(out, first, kRocProfVisDmOperationMemoryCopy, *track);
            AppendOpName(out, first, kRocProfVisDmOperationMemoryAllocate, *track);
            AppendOpName(out, first, kRocProfVisDmOperationLaunch, *track);
            AppendOpName(out, first, kRocProfVisDmOperationLaunchSample, *track);
            if(first)
            {
                out << "-";
            }
            out << "\n";
            ++shown;
            if(shown >= ASSISTANT_MAX_LIST_TRACKS)
            {
                out << "... additional tracks omitted\n";
                break;
            }
        }
        if(shown == 0)
        {
            return DoneResult("No queue/thread tracks found.", "No tracks");
        }
        return DoneResult(TrimResult(out.str()), "Listed tracks");
    }

    if(tool_name == "goto")
    {
        const double start_ns =
            JsonUtils::GetDouble(args, "start_ns",
                                 JsonUtils::GetDouble(args, "start", -1.0));
        const double end_ns =
            JsonUtils::GetDouble(args, "end_ns", JsonUtils::GetDouble(args, "end", -1.0));
        const uint64_t track_id =
            JsonU64(args, "track_id", TimelineSelection::INVALID_SELECTION_ID);
        const uint64_t event_uuid =
            JsonU64(args, "event_uuid",
                    JsonU64(args, "event_id", TimelineSelection::INVALID_SELECTION_ID));
        const std::string kernel_name = JsonUtils::GetString(args, "kernel_name", "");

        if(context.is_compute)
        {
            if(context.compute_selection == nullptr)
            {
                return DoneResult("Compute selection is not available.", "goto failed");
            }
            ComputeDataModel& model = context.data_provider->ComputeModel();
            uint32_t workload_id    = context.compute_selection->GetSelectedWorkload();
            const WorkloadInfo* workload = model.GetWorkload(workload_id);
            if(workload == nullptr && !model.GetWorkloadList().empty())
            {
                workload = model.GetWorkloadList().front();
            }
            if(workload == nullptr)
            {
                return DoneResult("No compute workload is loaded.", "goto failed");
            }
            const uint32_t kernel_id = static_cast<uint32_t>(
                JsonU64(args, "kernel_id", ComputeSelection::INVALID_SELECTION_ID));
            const KernelInfo* kernel = FindComputeKernel(*workload, kernel_name, kernel_id);
            if(kernel == nullptr)
            {
                return DoneResult("Could not find that kernel to select.", "goto failed");
            }
            context.compute_selection->SelectKernel(kernel->id);
            return DoneResult(std::string("Selected compute kernel ") + kernel->name,
                              "Selected kernel");
        }

        if(start_ns < 0.0 || end_ns < 0.0 || end_ns <= start_ns)
        {
            return DoneResult(
                "goto needs start_ns and end_ns (end > start) from kernel_instances "
                "or the selected range.",
                "goto failed");
        }
        if(context.timeline_selection == nullptr)
        {
            return DoneResult("Timeline selection is not available.", "goto failed");
        }

        context.timeline_selection->SelectTimeRange(start_ns, end_ns);
        if(track_id != TimelineSelection::INVALID_SELECTION_ID ||
           event_uuid != TimelineSelection::INVALID_SELECTION_ID)
        {
            context.timeline_selection->NavigateToEvent(track_id, event_uuid, start_ns,
                                                        end_ns - start_ns);
        }
        else
        {
            EventManager::GetInstance()->AddEvent(std::make_shared<RangeEvent>(
                static_cast<int>(RocEvents::kSetViewRange), start_ns, end_ns,
                context.data_provider->GetTraceFilePath()));
        }

        SettingsManager& settings    = SettingsManager::GetInstance();
        const TimeFormat time_format = settings.GetUserSettings().unit_settings.time_format;
        std::ostringstream out;
        out << "Moved the timeline to "
            << nanosecond_to_formatted_str(start_ns, time_format, true) << " .. "
            << nanosecond_to_formatted_str(end_ns, time_format, true);
        return DoneResult(out.str(), "Moved the view");
    }

    if(tool_name == "top_events")
    {
        if(context.is_compute)
        {
            return DoneResult(
                "top_events is for system traces. Use get_summary / kernel_metrics.",
                "Wrong trace kind");
        }
        const std::string category = JsonUtils::GetString(args, "category", "dispatch");
        const TopEventsSpec* spec  = FindTopEventsSpec(category);
        if(spec == nullptr)
        {
            return DoneResult(
                "Unknown category. Use dispatch, memory_copy, memory_alloc, "
                "instrumented, or sampled.",
                "Bad category");
        }
        const size_t limit =
            ClampRowLimit(JsonUtils::GetInt(args, "limit",
                                            static_cast<int32_t>(ASSISTANT_DEFAULT_ROW_LIMIT)));
        const std::vector<uint64_t> tracks = TracksForOperation(context, spec->op);
        if(tracks.empty())
        {
            return DoneResult(
                std::string("No tracks contain ") + spec->key +
                    " events. Call list_tracks or try another category.",
                "No matching tracks");
        }

        double start_ns = 0.0;
        double end_ns   = 0.0;
        SelectedOrFullTimeRange(context, start_ns, end_ns);

        if(context.data_provider->IsRequestPending(spec->request_id))
        {
            return PendingResult(spec->request_id, AssistantFetchKind::kTopEvents, limit,
                                 AssistantToolStatusLabel(tool_name), false,
                                 spec->table_type, 0);
        }

        const bool queued = context.data_provider->FetchTable(TrackTableRequestParams(
            spec->controller_type, tracks, start_ns, end_ns, "", "", "", "", 0,
            static_cast<uint64_t>(limit), 2, kRPVControllerSortOrderDescending));
        if(!queued && !context.data_provider->IsRequestPending(spec->request_id))
        {
            return DoneResult("Could not queue the top_events fetch.", "Fetch failed");
        }
        return PendingResult(spec->request_id, AssistantFetchKind::kTopEvents, limit,
                             AssistantToolStatusLabel(tool_name), queued, spec->table_type,
                             0);
    }

    if(tool_name == "kernel_instances")
    {
        if(context.is_compute)
        {
            return DoneResult(
                "kernel_instances is for system traces. Use kernel_metrics on compute traces.",
                "Wrong trace kind");
        }
        const std::string kernel_name = JsonUtils::GetString(args, "kernel_name", "");
        if(kernel_name.empty())
        {
            return DoneResult("kernel_instances needs kernel_name.", "Missing kernel_name");
        }
        const size_t limit =
            ClampRowLimit(JsonUtils::GetInt(args, "limit",
                                            static_cast<int32_t>(ASSISTANT_DEFAULT_ROW_LIMIT)));
        double start_ns = 0.0;
        double end_ns   = 0.0;
        SelectedOrFullTimeRange(context, start_ns, end_ns);

        const uint64_t request_id = DataProvider::SUMMARY_KERNEL_INSTANCE_TABLE_REQUEST_ID;
        if(context.data_provider->IsRequestPending(request_id))
        {
            return PendingResult(request_id, AssistantFetchKind::kKernelInstances, limit,
                                 AssistantToolStatusLabel(tool_name), false,
                                 TableType::kSummaryKernelTable, 0);
        }

        const bool queued = context.data_provider->FetchTable(EventSearchRequestParams(
            kRPVControllerTableTypeSummaryKernelInstances,
            { kRocProfVisDmOperationDispatch }, start_ns, end_ns, "", false, false, false,
            { kernel_name }, 0, static_cast<uint64_t>(limit), 0,
            kRPVControllerSortOrderAscending));
        if(!queued && !context.data_provider->IsRequestPending(request_id))
        {
            return DoneResult("Could not queue the kernel_instances fetch.", "Fetch failed");
        }
        return PendingResult(request_id, AssistantFetchKind::kKernelInstances, limit,
                             AssistantToolStatusLabel(tool_name), queued,
                             TableType::kSummaryKernelTable, 0);
    }

    if(tool_name == "kernel_metrics")
    {
        const std::string kernel_name = JsonUtils::GetString(args, "kernel_name", "");
        const std::string metric_name = JsonUtils::GetString(args, "metric_name", "");
        if(!context.is_compute)
        {
            std::ostringstream out;
            const std::vector<SummaryInfo::KernelMetrics>& kernels =
                context.data_provider->DataModel().GetSummary().GetSummaryData().gpu.top_kernels;
            bool found = false;
            for(const SummaryInfo::KernelMetrics& kernel : kernels)
            {
                if(kernel_name.empty() || kernel.name == kernel_name ||
                   ToLowerCopy(kernel.name).find(ToLowerCopy(kernel_name)) !=
                       std::string::npos)
                {
                    out << "name=" << kernel.name << " calls=" << kernel.invocations
                        << " sum_ns=" << kernel.exec_time_sum
                        << " min_ns=" << kernel.exec_time_min
                        << " max_ns=" << kernel.exec_time_max
                        << " pct=" << kernel.exec_time_pct << "\n";
                    found = true;
                    if(!kernel_name.empty())
                    {
                        break;
                    }
                }
            }
            if(!found)
            {
                out << "Kernel not in the current summary. Call get_summary first.\n";
            }
            return DoneResult(out.str(), "Read kernel summary stats");
        }

        ComputeDataModel& model = context.data_provider->ComputeModel();
        uint32_t workload_id    = ComputeSelection::INVALID_SELECTION_ID;
        if(context.compute_selection != nullptr)
        {
            workload_id = context.compute_selection->GetSelectedWorkload();
        }
        const WorkloadInfo* workload = model.GetWorkload(workload_id);
        if(workload == nullptr && !model.GetWorkloadList().empty())
        {
            workload = model.GetWorkloadList().front();
        }
        if(workload == nullptr)
        {
            return DoneResult("No compute workload is loaded.", "No workload");
        }

        uint32_t selected_kernel = ComputeSelection::INVALID_SELECTION_ID;
        if(context.compute_selection != nullptr)
        {
            selected_kernel = context.compute_selection->GetSelectedKernel();
        }
        const uint32_t kernel_id_arg = static_cast<uint32_t>(
            JsonU64(args, "kernel_id", selected_kernel));
        const KernelInfo* kernel =
            FindComputeKernel(*workload, kernel_name, kernel_id_arg);
        if(kernel == nullptr && selected_kernel != ComputeSelection::INVALID_SELECTION_ID)
        {
            kernel = FindComputeKernel(*workload, "", selected_kernel);
        }
        if(kernel == nullptr)
        {
            return DoneResult("Could not find that kernel. Call get_summary.",
                              "Unknown kernel");
        }

        std::ostringstream out;
        out << "kernel_id=" << kernel->id << " name=" << kernel->name << "\n";
        out << "invocation_count=" << kernel->dispatch_metrics[KernelInfo::InvocationCount]
            << "\n";
        out << "duration_total_ns=" << kernel->dispatch_metrics[KernelInfo::DurationTotal]
            << "\n";
        out << "duration_min_ns=" << kernel->dispatch_metrics[KernelInfo::DurationMin]
            << "\n";
        out << "duration_max_ns=" << kernel->dispatch_metrics[KernelInfo::DurationMax]
            << "\n";
        out << "duration_mean_ns=" << kernel->dispatch_metrics[KernelInfo::DurationMean]
            << "\n";
        out << "duration_median_ns="
            << kernel->dispatch_metrics[KernelInfo::DurationMedian] << "\n";

        if(metric_name.empty())
        {
            out << "available_metrics:\n";
            size_t listed = 0;
            for(const AvailableMetrics::Entry& entry : workload->available_metrics.list)
            {
                out << "  " << entry.name;
                if(!entry.unit.empty())
                {
                    out << " (" << entry.unit << ")";
                }
                out << "\n";
                ++listed;
                if(listed >= ASSISTANT_MAX_METRIC_NAMES)
                {
                    out << "  ... more metrics omitted; pass metric_name to fetch a subset\n";
                    break;
                }
            }
            return DoneResult(TrimResult(out.str()), "Read kernel dispatch stats");
        }

        const std::string needle = ToLowerCopy(metric_name);
        std::vector<MetricsRequestParams::MetricID> metric_ids;
        for(const AvailableMetrics::Entry& entry : workload->available_metrics.list)
        {
            if(ToLowerCopy(entry.name).find(needle) == std::string::npos)
            {
                continue;
            }
            MetricsRequestParams::MetricID id;
            id.category_id = entry.category_id;
            id.table_id    = entry.table_id;
            id.entry_id    = entry.id;
            metric_ids.push_back(id);
            if(metric_ids.size() >= ASSISTANT_MAX_METRIC_FETCH)
            {
                break;
            }
        }
        if(metric_ids.empty())
        {
            out << "No available metric name contains \"" << metric_name << "\".\n";
            return DoneResult(TrimResult(out.str()), "Metric not found");
        }

        model.ClearKernelMetricValues(context.metrics_client_id);
        const uint64_t request_id = RequestIdBuilder::MakeClientRequestId(
            RequestType::kFetchMetrics, context.metrics_client_id);
        if(context.data_provider->IsRequestPending(request_id))
        {
            return PendingResult(request_id, AssistantFetchKind::kMetrics,
                                 ASSISTANT_DEFAULT_ROW_LIMIT,
                                 AssistantToolStatusLabel(tool_name), false,
                                 TableType::kSummaryKernelTable, kernel->id);
        }
        const bool queued = context.data_provider->FetchMetrics(MetricsRequestParams(
            workload->id, { kernel->id }, metric_ids, context.metrics_client_id));
        if(!queued && !context.data_provider->IsRequestPending(request_id))
        {
            out << "Could not queue the metric fetch.\n";
            return DoneResult(TrimResult(out.str()), "Fetch failed");
        }
        AssistantToolStartResult pending =
            PendingResult(request_id, AssistantFetchKind::kMetrics,
                          ASSISTANT_DEFAULT_ROW_LIMIT, AssistantToolStatusLabel(tool_name),
                          queued, TableType::kSummaryKernelTable, kernel->id);
        pending.content = out.str();
        return pending;
    }

    return DoneResult("Unknown tool. Use get_summary, top_events, kernel_instances, "
                      "kernel_metrics, list_tracks, or goto.",
                      "Unknown tool");
}

std::string
FinishAssistantFetch(const AssistantToolContext& context, AssistantFetchKind kind,
                     TableType table_type, uint32_t kernel_id, size_t row_limit)
{
    if(context.data_provider == nullptr)
    {
        return "No trace is open.";
    }
    const size_t limit = row_limit == 0 ? ASSISTANT_DEFAULT_ROW_LIMIT : row_limit;

    if(kind == AssistantFetchKind::kSummary)
    {
        return FormatSystemSummary(context);
    }
    if(kind == AssistantFetchKind::kTopEvents)
    {
        const TablesModel& tables =
            context.data_provider->DataModel().GetAnalysis().GetTables();
        if(tables.GetTableData(table_type).empty() &&
           tables.GetTableHeader(table_type).empty())
        {
            return "top_events returned no rows for the current tracks and time range.";
        }
        return FormatTableSnapshot(tables, table_type, limit);
    }
    if(kind == AssistantFetchKind::kKernelInstances)
    {
        return FormatTableSnapshot(context.data_provider->DataModel().GetTables(),
                                   TableType::kSummaryKernelTable, limit);
    }
    if(kind == AssistantFetchKind::kMetrics)
    {
        ComputeDataModel& model = context.data_provider->ComputeModel();
        const std::vector<std::shared_ptr<MetricValue>>* values =
            model.GetKernelMetricsData(context.metrics_client_id, kernel_id);
        if(values == nullptr || values->empty())
        {
            return "Metric fetch finished with no values.";
        }
        std::ostringstream out;
        out << "fetched_metrics:\n";
        size_t shown = 0;
        for(const std::shared_ptr<MetricValue>& value : *values)
        {
            if(!value)
            {
                continue;
            }
            const char* metric_name = value->entry != nullptr ? value->entry->name.c_str()
                                                              : "(unnamed)";
            out << "  " << metric_name;
            for(const std::pair<const std::string, double>& named : value->values)
            {
                out << " " << named.first << "=" << named.second;
            }
            out << "\n";
            ++shown;
            if(shown >= ASSISTANT_MAX_METRIC_FETCH * 4)
            {
                break;
            }
        }
        return TrimResult(out.str());
    }
    (void) table_type;
    return "Fetch finished with nothing to report.";
}

}  // namespace View
}  // namespace RocProfVis
