// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// The tools that read the trace rather than change it, and everything that
// formats what they read. Most of these cannot answer in one call: they queue a
// fetch on the data provider and hand the panel a set of request ids to poll,
// and FinishAssistantFetch at the bottom of this file is what turns the rows
// into text once they land.
//
// Those request ids and result slots are shared with the normal UI, which is
// why every body here checks IsRequestPending before issuing its own query and
// reports whether it actually started the fetch. The panel needs that flag to
// know whether the rows that arrive are its own.
#include "rocprofvis_ai_tools_internal.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "json.h"

#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "model/rocprofvis_analysis_model.h"
#include "model/rocprofvis_common_defs.h"
#include "model/rocprofvis_summary_model.h"
#include "model/rocprofvis_tables_model.h"
#include "model/rocprofvis_timeline_model.h"
#include "model/rocprofvis_topology_model.h"
#include "rocprofvis_ai_tool_query.h"
#include "rocprofvis_ai_tool_schema.h"
#include "rocprofvis_c_interface_types.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_data_provider.h"
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

constexpr size_t   ASSISTANT_MAX_TRACKS       = 32;
constexpr size_t   ASSISTANT_MAX_LIST_TRACKS  = 80;
constexpr size_t   ASSISTANT_MAX_METRIC_NAMES = 40;
constexpr size_t   ASSISTANT_MAX_METRIC_FETCH = 8;
constexpr size_t   ASSISTANT_TOP_KERNEL_LIMIT = 10;
constexpr size_t   ASSISTANT_MAX_RESULT_CHARS = 20000;
constexpr size_t   ASSISTANT_MAX_CALL_STACK   = 40;
constexpr size_t   ASSISTANT_MAX_FLOW_ROWS    = 20;
constexpr size_t   ASSISTANT_MAX_EXT_ROWS     = 40;
constexpr size_t   ASSISTANT_MAX_EVENT_ARGS   = 40;
constexpr size_t   ASSISTANT_MAX_SEARCH_TERMS = 8;
constexpr size_t   ASSISTANT_OVERVIEW_BINS    = 32;
constexpr size_t   ASSISTANT_OVERVIEW_TRACKS  = 12;
constexpr double   ASSISTANT_OVERVIEW_SCALE   = 100.0;
constexpr uint64_t ASSISTANT_DURATION_COLUMN  = 2;

// Furthest a tool may page into a result set. limit is clamped too, but without
// a ceiling here the model could ask the database to skip billions of rows.
constexpr uint64_t ASSISTANT_MAX_OFFSET = 1000000;

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

// Reports whether a column name is one of the model's internal "__" names.
bool
NameLooksInternal(const std::string& name)
{
    return name.size() >= 2 && name[0] == '_' && name[1] == '_';
}

// Keeps the internal ids the model must pass back to goto or event_details.
bool
KeepInternalColumn(const std::string& name)
{
    return name == "__uuid" || name == "__trackId" || name == "__streamTrackId";
}

// Clamps a requested row count so one tool call cannot ask for the world.
size_t
ClampRowLimit(int32_t requested)
{
    if(requested <= 0)
    {
        return ASSISTANT_DEFAULT_ROW_LIMIT;
    }
    return std::min(static_cast<size_t>(requested), ASSISTANT_MAX_ROW_LIMIT);
}

// Clamps how far a tool may page in, for the same reason as ClampRowLimit: the
// number comes from the model and reaches the database unchanged otherwise.
uint64_t
ClampOffset(uint64_t requested)
{
    return std::min(requested, ASSISTANT_MAX_OFFSET);
}

// The row offset a paging tool was asked for, bounded. Every table read goes
// through here so none of them can forget to clamp.
uint64_t
OffsetFromArgs(const jt::Json& args)
{
    return ClampOffset(JsonU64(args, "offset", 0));
}

// Formats a percentage to one decimal place, the way the UI shows it.
std::string
FormatPercent(float value)
{
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%.1f", static_cast<double>(value));
    return std::string(buffer);
}

// Writes the GPU utilization lines and the top-kernel list of a summary.
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

// Caps a tool result so one big table cannot fill the model's context.
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

// Maps a category name, plus the synonyms the model likes, to its table spec.
const TopEventsSpec*
FindTopEventsSpec(const std::string& category)
{
    const std::string lowered = to_lower_copy(category);
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

// Prefers the selected tracks that carry an operation, else every such track.
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

// Lists the tracks of one kind, for a tool the model called without track ids.
std::vector<uint64_t>
TracksOfType(const AssistantToolContext&        context,
             rocprofvis_controller_track_type_t wanted)
{
    std::vector<uint64_t>               tracks;
    const std::vector<const TrackInfo*> all =
        context.data_provider->DataModel().GetTimeline().GetTrackList();
    for(const TrackInfo* track : all)
    {
        if(track == nullptr || track->track_type != wanted)
        {
            continue;
        }
        tracks.push_back(track->id);
        if(tracks.size() >= ASSISTANT_MAX_TRACKS)
        {
            break;
        }
    }
    return tracks;
}

// Drops named tracks of the wrong kind, so counter and event reads stay apart.
std::vector<uint64_t>
KeepTracksOfType(const AssistantToolContext& context, const std::vector<uint64_t>& tracks,
                 rocprofvis_controller_track_type_t wanted)
{
    const TimelineModel&  timeline = context.data_provider->DataModel().GetTimeline();
    std::vector<uint64_t> kept;
    kept.reserve(tracks.size());
    for(uint64_t track_id : tracks)
    {
        const TrackInfo* track = timeline.GetTrack(track_id);
        if(track != nullptr && track->track_type == wanted)
        {
            kept.push_back(track_id);
        }
    }
    return kept;
}

// Explicit start_ns/end_ns win, else the user's selection, else the whole
// trace. Search passes prefer_selection false to match its "Whole Trace" default.
void
TimeRangeFromArgs(const AssistantToolContext& context, const jt::Json& args,
                  double& start_ns, double& end_ns, bool prefer_selection = true)
{
    if(prefer_selection)
    {
        SelectedOrFullTimeRange(context, start_ns, end_ns);
    }
    else
    {
        const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
        start_ns                      = timeline.GetStartTime();
        end_ns                        = timeline.GetEndTime();
    }

    const double requested_start = JsonUtils::GetDouble(args, "start_ns", -1.0);
    const double requested_end   = JsonUtils::GetDouble(args, "end_ns", -1.0);
    // std::isfinite rejects the infinities; NaN fails the comparisons below on
    // its own, since every comparison against NaN is false.
    if(!std::isfinite(requested_start) || !std::isfinite(requested_end) ||
       requested_start < 0.0 || requested_end <= requested_start)
    {
        return;
    }

    // Keep the window inside the trace. A range the model invented out past the
    // end would otherwise come back empty and read as "nothing happened here"
    // rather than "you asked about time that was never recorded".
    const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
    const double         trace_start = timeline.GetStartTime();
    const double         trace_end   = timeline.GetEndTime();
    const double clamped_start = std::max(requested_start, trace_start);
    const double clamped_end   = std::min(requested_end, trace_end);
    if(clamped_end <= clamped_start)
    {
        return;
    }
    start_ns = clamped_start;
    end_ns   = clamped_end;
}

// Track ids named by the model, filtered to tracks that exist and carry the
// requested operation. Falls back to selection-or-all when none are named.
std::vector<uint64_t>
TracksFromArgs(const AssistantToolContext& context, const jt::Json& args,
               rocprofvis_dm_event_operation_t op, bool require_op,
               std::string& error_out)
{
    jt::Json& mutable_args = const_cast<jt::Json&>(args);
    if(!mutable_args.contains("track_ids") || !mutable_args["track_ids"].isArray())
    {
        return require_op ? TracksForOperation(context, op) : std::vector<uint64_t>();
    }

    std::vector<jt::Json>& entries = mutable_args["track_ids"].getArray();
    if(!CheckArrayLength(entries, "track_ids", error_out))
    {
        return std::vector<uint64_t>();
    }

    const TimelineModel&  timeline = context.data_provider->DataModel().GetTimeline();
    std::vector<uint64_t> tracks;
    tracks.reserve(std::min(entries.size(), ASSISTANT_MAX_TRACKS));
    for(jt::Json& entry : entries)
    {
        uint64_t track_id = 0;
        if(entry.isLong())
        {
            track_id = static_cast<uint64_t>(entry.getLong());
        }
        else if(entry.isDouble())
        {
            track_id = static_cast<uint64_t>(entry.getDouble());
        }
        else
        {
            error_out = "Every track_ids entry must be a number.";
            return std::vector<uint64_t>();
        }

        const TrackInfo* track = timeline.GetTrack(track_id);
        if(track == nullptr)
        {
            error_out = "Unknown track_id " + std::to_string(track_id) +
                        ". Call list_tracks first.";
            return std::vector<uint64_t>();
        }
        if(require_op && track->operation_types.count(op) == 0)
        {
            continue;
        }
        tracks.push_back(track_id);
        if(tracks.size() >= ASSISTANT_MAX_TRACKS)
        {
            break;
        }
    }
    return tracks;
}

// Renders a fetched table as named-value rows, hiding the internal columns.
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

// Names a track type for the model, using the same words the UI does.
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

// Renders one event's arguments, flow links, and call stack as text.
std::string
FormatEventDetails(const AssistantToolContext& context, uint64_t event_id)
{
    const EventInfo* event =
        context.data_provider->DataModel().GetEvents().GetEvent(event_id);
    if(event == nullptr)
    {
        return "No details came back for that event_uuid. Check the __uuid value.";
    }

    std::ostringstream out;
    out << "event_uuid: " << event_id << "\n";
    if(!event->basic_info.name.empty())
    {
        out << "name: " << event->basic_info.name << "\n";
        out << "start_ns: " << event->basic_info.start_ts << "\n";
        out << "duration_ns: " << event->basic_info.duration << "\n";
    }
    if(event->track_id != INVALID_UINT64_INDEX)
    {
        out << "track_id: " << event->track_id << "\n";
    }

    if(!event->args.empty())
    {
        out << "arguments:\n";
        size_t shown = 0;
        for(const EventArg& arg : event->args)
        {
            out << "  " << arg.name << "=" << arg.value;
            if(!arg.data_type.empty())
            {
                out << " (" << arg.data_type << ")";
            }
            out << "\n";
            if(++shown >= ASSISTANT_MAX_EVENT_ARGS)
            {
                break;
            }
        }
    }

    if(!event->ext_info.empty())
    {
        out << "extended_data:\n";
        size_t shown = 0;
        for(const EventExtData& ext : event->ext_info)
        {
            out << "  " << ext.category << "." << ext.name << "=" << ext.value << "\n";
            if(++shown >= ASSISTANT_MAX_EXT_ROWS)
            {
                break;
            }
        }
    }

    if(!event->flow_info.empty())
    {
        out << "flow:\n";
        size_t shown = 0;
        for(const EventFlowData& flow : event->flow_info)
        {
            out << "  uuid=" << flow.id.uuid << " track_id=" << flow.track_id
                << " start_ns=" << flow.start_timestamp
                << " end_ns=" << flow.end_timestamp << " name=\"" << flow.name << "\"\n";
            if(++shown >= ASSISTANT_MAX_FLOW_ROWS)
            {
                break;
            }
        }
    }

    if(!event->call_stack_info.empty())
    {
        out << "call_stack:\n";
        size_t shown = 0;
        for(const CallStackData& frame : event->call_stack_info)
        {
            out << "  " << (shown + 1) << ". " << frame.name;
            if(!frame.file.empty())
            {
                out << " (" << frame.file << ")";
            }
            out << "\n";
            if(++shown >= ASSISTANT_MAX_CALL_STACK)
            {
                break;
            }
        }
    }

    if(event->args.empty() && event->ext_info.empty() && event->flow_info.empty() &&
       event->call_stack_info.empty())
    {
        out << "This event has no arguments, flow links, or call stack recorded.\n";
    }
    return TrimResult(out.str());
}

// One minimap row reduced to a profile. The minimap holds event counts for
// event tracks and counter values for sample tracks, and is filled when the
// trace loads, so reading it costs no fetch.
struct OverviewProfile
{
    std::vector<double> bins;
    double              total     = 0.0;
    double              peak      = 0.0;
    size_t              peak_bin  = 0;
    double              min_value = 0.0;
    double              max_value = 0.0;
};

// Averages a minimap row down to at most bin_count buckets.
OverviewProfile
Downsample(const std::vector<double>& source, size_t bin_count)
{
    OverviewProfile profile;
    if(source.empty() || bin_count == 0)
    {
        return profile;
    }

    const size_t bins = std::min(bin_count, source.size());
    profile.bins.assign(bins, 0.0);
    std::vector<size_t> counts(bins, 0);
    for(size_t i = 0; i < source.size(); ++i)
    {
        const size_t bin = std::min(bins - 1, i * bins / source.size());
        profile.bins[bin] += source[i];
        ++counts[bin];
        profile.total += source[i];
    }

    bool seen = false;
    for(size_t i = 0; i < bins; ++i)
    {
        if(counts[i] > 0)
        {
            profile.bins[i] /= static_cast<double>(counts[i]);
        }
        if(profile.bins[i] > profile.peak)
        {
            profile.peak     = profile.bins[i];
            profile.peak_bin = i;
        }
        if(!seen || profile.bins[i] < profile.min_value)
        {
            profile.min_value = profile.bins[i];
        }
        if(!seen || profile.bins[i] > profile.max_value)
        {
            profile.max_value = profile.bins[i];
        }
        seen = true;
    }
    return profile;
}

// Writes a profile as small integers scaled to its own peak.
void
AppendBinRow(std::ostringstream& out, const OverviewProfile& profile)
{
    out << "activity_0_100:";
    for(size_t i = 0; i < profile.bins.size(); ++i)
    {
        const double scaled =
            profile.peak > 0.0 ? profile.bins[i] / profile.peak * ASSISTANT_OVERVIEW_SCALE
                               : 0.0;
        out << (i == 0 ? " " : ",") << static_cast<int>(scaled + 0.5);
    }
    out << "\n";
}

// Writes the nanosecond window one bin covers, so the model can cite it.
void
AppendBinWindow(std::ostringstream& out, const char* label, size_t bin, size_t bin_count,
                double start_ns, double span_ns)
{
    if(bin_count == 0)
    {
        return;
    }
    const double width = span_ns / static_cast<double>(bin_count);
    out << label << ": " << (start_ns + width * static_cast<double>(bin)) << " .. "
        << (start_ns + width * static_cast<double>(bin + 1)) << "\n";
}

// Sums the event rows of the minimap. This is exactly what the histogram strip
// above the timeline draws.
std::vector<double>
CombinedEventRow(const AssistantToolContext& context)
{
    const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
    std::vector<double>  combined;
    for(const std::pair<const uint64_t, std::vector<double>>& row :
        timeline.GetMiniMap())
    {
        const TrackInfo* track = timeline.GetTrack(row.first);
        if(track == nullptr || track->track_type == kRPVControllerTrackTypeSamples)
        {
            continue;
        }
        if(combined.size() < row.second.size())
        {
            combined.resize(row.second.size(), 0.0);
        }
        for(size_t i = 0; i < row.second.size(); ++i)
        {
            combined[i] += row.second[i];
        }
    }
    return combined;
}

// Renders the minimap as an activity profile plus the busiest tracks.
std::string
FormatTraceOverview(const AssistantToolContext& context, size_t bin_count,
                    uint64_t single_track)
{
    const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
    const std::map<uint64_t, std::vector<double>>& minimap = timeline.GetMiniMap();
    if(minimap.empty())
    {
        return "The timeline overview has not been built for this trace.";
    }

    const double start_ns = timeline.GetStartTime();
    const double end_ns   = timeline.GetEndTime();
    const double span_ns  = end_ns - start_ns;

    std::ostringstream out;
    out << "trace_start_ns: " << start_ns << "\n";
    out << "trace_end_ns: " << end_ns << "\n";
    out << "trace_duration_ns: " << span_ns << "\n";

    if(single_track != INVALID_UINT64_INDEX)
    {
        const std::map<uint64_t, std::vector<double>>::const_iterator row =
            minimap.find(single_track);
        if(row == minimap.end())
        {
            return "That track has no overview row. Call list_tracks.";
        }
        const TrackInfo*      track   = timeline.GetTrack(single_track);
        const OverviewProfile profile = Downsample(row->second, bin_count);
        const bool            counter =
            track != nullptr && track->track_type == kRPVControllerTrackTypeSamples;

        out << "track_id: " << single_track << "\n";
        out << "track_name: "
            << context.data_provider->DataModel().BuildTrackName(single_track) << "\n";
        out << "bins: " << profile.bins.size() << "\n";
        AppendBinRow(out, profile);
        AppendBinWindow(out, "busiest_window_ns", profile.peak_bin, profile.bins.size(),
                        start_ns, span_ns);
        if(counter)
        {
            out << "counter_min: " << profile.min_value << "\n";
            out << "counter_max: " << profile.max_value << "\n";
        }
        else
        {
            out << "event_count: " << profile.total << "\n";
        }
        return TrimResult(out.str());
    }

    std::vector<std::pair<uint64_t, OverviewProfile>> event_tracks;
    size_t                                            counter_count = 0;
    double                                            event_total   = 0.0;
    for(const std::pair<const uint64_t, std::vector<double>>& row : minimap)
    {
        const TrackInfo* track = timeline.GetTrack(row.first);
        if(track == nullptr)
        {
            continue;
        }
        if(track->track_type == kRPVControllerTrackTypeSamples)
        {
            ++counter_count;
            continue;
        }

        const OverviewProfile profile = Downsample(row.second, bin_count);
        event_tracks.emplace_back(row.first, profile);
        event_total += profile.total;
    }

    const OverviewProfile overall = Downsample(CombinedEventRow(context), bin_count);
    out << "bins: " << overall.bins.size() << "\n";
    if(!overall.bins.empty())
    {
        out << "bin_width_ns: " << (span_ns / static_cast<double>(overall.bins.size()))
            << "\n";
    }
    AppendBinRow(out, overall);
    AppendBinWindow(out, "busiest_window_ns", overall.peak_bin, overall.bins.size(),
                    start_ns, span_ns);

    size_t quietest = 0;
    for(size_t i = 0; i < overall.bins.size(); ++i)
    {
        if(overall.bins[i] < overall.bins[quietest])
        {
            quietest = i;
        }
    }
    AppendBinWindow(out, "quietest_window_ns", quietest, overall.bins.size(), start_ns,
                    span_ns);
    out << "total_events: " << event_total << "\n";

    std::sort(event_tracks.begin(), event_tracks.end(),
              [](const std::pair<uint64_t, OverviewProfile>& a,
                 const std::pair<uint64_t, OverviewProfile>& b) {
                  return a.second.total > b.second.total;
              });

    out << "busiest_tracks:\n";
    const size_t shown = std::min(event_tracks.size(), ASSISTANT_OVERVIEW_TRACKS);
    for(size_t i = 0; i < shown; ++i)
    {
        const uint64_t         track_id = event_tracks[i].first;
        const OverviewProfile& profile  = event_tracks[i].second;
        out << "  " << (i + 1) << ". track_id=" << track_id << " events="
            << profile.total;
        if(event_total > 0.0)
        {
            out << " share_pct="
                << FormatPercent(
                       static_cast<float>(profile.total / event_total * 100.0));
        }
        out << " name=\""
            << context.data_provider->DataModel().BuildTrackName(track_id) << "\"\n";
    }
    if(event_tracks.size() > shown)
    {
        out << "  ... " << (event_tracks.size() - shown) << " more event tracks\n";
    }

    if(counter_count > 0)
    {
        out << "counter_tracks: " << counter_count
            << " (use track_samples or track_statistics)\n";
    }
    return TrimResult(out.str());
}

// Renders the statistics the analysis model computed for one track.
std::string
FormatTrackStatistics(const AssistantToolContext& context, uint64_t track_id)
{
    const TrackInfo* track =
        context.data_provider->DataModel().GetTimeline().GetTrack(track_id);
    if(track == nullptr)
    {
        return "That track is no longer loaded.";
    }
    const AnalysisTrackStatistics* stats =
        context.data_provider->DataModel().GetAnalysis().RegisterTrack(*track);
    if(stats == nullptr || stats->state != AnalysisTrackStatistics::kReady)
    {
        return "Track statistics are not ready yet. Try again.";
    }

    std::ostringstream out;
    out << "track_id: " << track_id << "\n";
    out << "track_name: " << context.data_provider->DataModel().BuildTrackName(track_id)
        << "\n";
    out << "track_type: " << TrackTypeName(track->topology.type) << "\n";
    for(const AnalysisTrackStatistics::Stat& stat : stats->stats)
    {
        out << stat.name << ": " << stat.FullValue() << "\n";
    }
    return out.str();
}

// Renders a system trace's GPU summary, with a nudge when it has no kernels.
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

// Renders a compute workload's kernels, ranked by total duration.
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

// Appends one operation name to a comma-separated list, if the track has it.
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

// Builds the state FinishAssistantFetch needs to format a table once it lands.
AssistantFetchState
TableFetch(AssistantFetchKind kind, TableType table_type, size_t row_limit)
{
    AssistantFetchState fetch;
    fetch.kind       = kind;
    fetch.table_type = table_type;
    fetch.row_limit  = row_limit;
    return fetch;
}

// Builds the result of a tool that is waiting on a set of requests to land.
AssistantToolStartResult
PendingResult(const std::vector<uint64_t>& request_ids, const AssistantFetchState& fetch,
              const std::string& status, bool started_fetch)
{
    AssistantToolStartResult result;
    result.pending       = true;
    result.started_fetch = started_fetch;
    result.request_ids   = request_ids;
    result.fetch         = fetch;
    result.status_line   = status;
    return result;
}

// Forwards the single-request case, which is what most tools have.
AssistantToolStartResult
PendingResult(uint64_t request_id, const AssistantFetchState& fetch,
              const std::string& status, bool started_fetch)
{
    return PendingResult(std::vector<uint64_t>{ request_id }, fetch, status,
                         started_fetch);
}

}  // namespace


// Builds the short trace description the model sees before it asks anything.
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

    // Headline numbers only. The kernel breakdown is left out so the model has
    // to fetch it rather than answer from the briefing alone.
    const SummaryInfo::GPUMetrics& gpu =
        context.data_provider->DataModel().GetSummary().GetSummaryData().gpu;
    out << "summary:\n";
    if(gpu.gfx_utilization.has_value())
    {
        out << "gfx_util_pct: " << FormatPercent(gpu.gfx_utilization.value()) << "\n";
    }
    if(gpu.mem_utilization.has_value())
    {
        out << "mem_util_pct: " << FormatPercent(gpu.mem_utilization.value()) << "\n";
    }
    out << "kernel_exec_time_total_ns: " << gpu.kernel_exec_time_total << "\n";
    out << "named_kernels_available: " << gpu.top_kernels.size() << "\n";
    out << "note: kernel names, per-kernel times, and event rows are NOT in this "
           "briefing. Call get_summary, top_events, or track_events to see them.\n";
    return out.str();
}

// Returns one track's activity, or the whole trace's, normalized to its peak.
std::vector<double>
GetAssistantActivityBins(const AssistantToolContext& context, uint64_t track_id,
                         size_t bin_count)
{
    if(context.data_provider == nullptr)
    {
        return std::vector<double>();
    }

    std::vector<double> source;
    if(track_id == INVALID_UINT64_INDEX)
    {
        source = CombinedEventRow(context);
    }
    else
    {
        const std::map<uint64_t, std::vector<double>>& minimap =
            context.data_provider->DataModel().GetTimeline().GetMiniMap();
        const std::map<uint64_t, std::vector<double>>::const_iterator row =
            minimap.find(track_id);
        if(row == minimap.end())
        {
            return std::vector<double>();
        }
        source = row->second;
    }

    const OverviewProfile profile = Downsample(source, bin_count);
    std::vector<double>   bins    = profile.bins;
    if(profile.peak > 0.0)
    {
        for(double& value : bins)
        {
            value /= profile.peak;
        }
    }
    return bins;
}

// The busiest tracks with their activity rows, for the panel's chart. Rows are
// scaled against the busiest bin of the whole set, not their own, so a quiet
// track reads as quiet instead of peaking at full brightness.
std::vector<AssistantActivityRow>
GetAssistantActivityRows(const AssistantToolContext& context, size_t bin_count,
                         size_t max_rows)
{
    std::vector<AssistantActivityRow> rows;
    if(context.data_provider == nullptr)
    {
        return rows;
    }

    const TimelineModel& timeline = context.data_provider->DataModel().GetTimeline();
    const std::map<uint64_t, std::vector<double>>& minimap = timeline.GetMiniMap();

    std::vector<std::pair<double, uint64_t>> ranked;
    for(const std::pair<const uint64_t, std::vector<double>>& row : minimap)
    {
        const TrackInfo* track = timeline.GetTrack(row.first);
        if(track == nullptr || track->track_type == kRPVControllerTrackTypeSamples)
        {
            continue;
        }
        double total = 0.0;
        for(double value : row.second)
        {
            total += value;
        }
        if(total > 0.0)
        {
            ranked.emplace_back(total, row.first);
        }
    }

    std::sort(ranked.begin(), ranked.end(),
              [](const std::pair<double, uint64_t>& a,
                 const std::pair<double, uint64_t>& b) { return a.first > b.first; });

    const size_t count = std::min(ranked.size(), max_rows);
    rows.reserve(count);
    double peak = 0.0;
    for(size_t i = 0; i < count; ++i)
    {
        const std::map<uint64_t, std::vector<double>>::const_iterator source =
            minimap.find(ranked[i].second);
        if(source == minimap.end())
        {
            continue;
        }

        const OverviewProfile profile = Downsample(source->second, bin_count);
        peak                          = std::max(peak, profile.peak);

        AssistantActivityRow entry;
        entry.track_id = ranked[i].second;
        entry.name = context.data_provider->DataModel().BuildTrackName(entry.track_id);
        entry.bins = profile.bins;
        rows.push_back(entry);
    }

    if(peak > 0.0)
    {
        for(AssistantActivityRow& row : rows)
        {
            for(double& value : row.bins)
            {
                value /= peak;
            }
        }
    }
    return rows;
}


namespace
{

// Implements the trace_overview tool.
AssistantToolStartResult
ToolTraceOverview(const AssistantToolContext& context, const jt::Json& args,
                  const std::string&)
{
    if(context.is_compute)
    {
        return DoneResult(
            "Compute traces have no timeline. Call get_summary instead.",
            "Wrong trace kind");
    }
    const int32_t requested_bins = JsonUtils::GetInt(
        args, "bins", static_cast<int32_t>(ASSISTANT_OVERVIEW_BINS));
    const size_t bins =
        requested_bins <= 0
            ? ASSISTANT_OVERVIEW_BINS
            : std::min(static_cast<size_t>(requested_bins), ASSISTANT_MAX_ROW_LIMIT);
    const uint64_t track_id = JsonU64(args, "track_id", INVALID_UINT64_INDEX);

    AssistantToolStartResult result =
        DoneResult(FormatTraceOverview(context, bins, track_id),
                   "Read the timeline overview");
    result.chart          = true;
    result.chart_track_id = track_id;
    return result;
}

// Implements the get_summary tool.
AssistantToolStartResult
ToolGetSummary(const AssistantToolContext& context, const jt::Json& args,
               const std::string&)
{
    if(context.is_compute)
    {
        return DoneResult(FormatComputeKernels(context, ASSISTANT_TOP_KERNEL_LIMIT),
                          "Read compute kernel stats");
    }
    const AssistantFetchState summary_fetch =
        TableFetch(AssistantFetchKind::kSummary, TableType::kSummaryKernelTable,
                   ASSISTANT_DEFAULT_ROW_LIMIT);
    if(context.data_provider->IsRequestPending(DataProvider::SUMMARY_REQUEST_ID))
    {
        return PendingResult(DataProvider::SUMMARY_REQUEST_ID, summary_fetch,
                             "Loading summary...", false);
    }
    const SummaryInfo::GPUMetrics& gpu =
        context.data_provider->DataModel().GetSummary().GetSummaryData().gpu;
    if(!gpu.top_kernels.empty() || gpu.kernel_exec_time_total > 0.0)
    {
        return DoneResult(FormatSystemSummary(context), "Read summary");
    }
    if(context.data_provider->FetchSummary())
    {
        return PendingResult(DataProvider::SUMMARY_REQUEST_ID, summary_fetch,
                             "Loading summary...", true);
    }
    if(context.data_provider->IsRequestPending(DataProvider::SUMMARY_REQUEST_ID))
    {
        return PendingResult(DataProvider::SUMMARY_REQUEST_ID, summary_fetch,
                             "Loading summary...", false);
    }
    // The fetch was refused and nothing is in flight, so the summary really
    // is empty. Formatting it anyway would hand the model a page of zeroes
    // that reads like a measurement.
    return DoneResult("The GPU summary could not be loaded for this trace. "
                      "Use top_events or track_events instead.",
                      "Summary unavailable");
}

// Implements the list_tracks tool.
AssistantToolStartResult
ToolListTracks(const AssistantToolContext& context, const jt::Json& args,
               const std::string&)
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
           track->topology.type != TrackInfo::SampledThread &&
           track->topology.type != TrackInfo::Counter)
        {
            continue;
        }
        out << "track_id=" << track->id << " type=" << TrackTypeName(track->topology.type)
            << " reads_with="
            << (track->track_type == kRPVControllerTrackTypeEvents ? "track_events"
                                                                  : "track_samples")
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

// Implements the top_events tool.
AssistantToolStartResult
ToolTopEvents(const AssistantToolContext& context, const jt::Json& args,
              const std::string& tool_name)
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

    std::string track_error;
    const std::vector<uint64_t> tracks =
        TracksFromArgs(context, args, spec->op, true, track_error);
    if(!track_error.empty())
    {
        return DoneResult(track_error, "Bad track_ids");
    }
    if(tracks.empty())
    {
        return DoneResult(
            std::string("No tracks contain ") + spec->key +
                " events. Call list_tracks or try another category.",
            "No matching tracks");
    }

    std::string       query_error;
    const std::string where    = BuildAssistantWhereClause(args, query_error);
    const std::string group_by = AssistantGroupByFromArgs(args, query_error);
    if(!query_error.empty())
    {
        return DoneResult(query_error, "Bad query argument");
    }

    double start_ns = 0.0;
    double end_ns   = 0.0;
    TimeRangeFromArgs(context, args, start_ns, end_ns);

    const AssistantFetchState fetch =
        TableFetch(AssistantFetchKind::kTopEvents, spec->table_type, limit);
    if(context.data_provider->IsRequestPending(spec->request_id))
    {
        return PendingResult(spec->request_id, fetch,
                             AssistantToolStatusLabel(tool_name), false);
    }

    const TablesModel& tables =
        context.data_provider->DataModel().GetAnalysis().GetTables();
    const bool queued = context.data_provider->FetchTable(TrackTableRequestParams(
        spec->controller_type, tracks, start_ns, end_ns, where.c_str(), "",
        group_by.c_str(), "", OffsetFromArgs(args),
        static_cast<uint64_t>(limit),
        ResolveAssistantSortColumn(tables, spec->table_type,
                          JsonUtils::GetString(args, "sort_by", ""),
                          ASSISTANT_DURATION_COLUMN),
        AssistantSortOrderFromArgs(args, kRPVControllerSortOrderDescending)));
    if(!queued && !context.data_provider->IsRequestPending(spec->request_id))
    {
        return DoneResult("Could not queue the top_events fetch.", "Fetch failed");
    }
    return PendingResult(spec->request_id, fetch,
                         AssistantToolStatusLabel(tool_name), queued);
}

// Implements the kernel_instances tool.
AssistantToolStartResult
ToolKernelInstances(const AssistantToolContext& context, const jt::Json& args,
                    const std::string& tool_name)
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

    std::string       query_error;
    const std::string where = BuildAssistantWhereClause(args, query_error);
    if(!query_error.empty())
    {
        return DoneResult(query_error, "Bad query argument");
    }

    double start_ns = 0.0;
    double end_ns   = 0.0;
    TimeRangeFromArgs(context, args, start_ns, end_ns);

    const uint64_t request_id = DataProvider::SUMMARY_KERNEL_INSTANCE_TABLE_REQUEST_ID;
    const AssistantFetchState fetch =
        TableFetch(AssistantFetchKind::kKernelInstances, TableType::kSummaryKernelTable,
                   limit);
    if(context.data_provider->IsRequestPending(request_id))
    {
        return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                             false);
    }

    const TablesModel& tables = context.data_provider->DataModel().GetTables();
    const bool queued = context.data_provider->FetchTable(EventSearchRequestParams(
        kRPVControllerTableTypeSummaryKernelInstances,
        { kRocProfVisDmOperationDispatch }, start_ns, end_ns, where.c_str(), false,
        false, false, { kernel_name }, OffsetFromArgs(args),
        static_cast<uint64_t>(limit),
        ResolveAssistantSortColumn(tables, TableType::kSummaryKernelTable,
                          JsonUtils::GetString(args, "sort_by", ""), 0),
        AssistantSortOrderFromArgs(args, kRPVControllerSortOrderAscending)));
    if(!queued && !context.data_provider->IsRequestPending(request_id))
    {
        return DoneResult("Could not queue the kernel_instances fetch.", "Fetch failed");
    }
    return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                         queued);
}

// Implements the kernel_metrics tool.
AssistantToolStartResult
ToolKernelMetrics(const AssistantToolContext& context, const jt::Json& args,
                  const std::string& tool_name)
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
               to_lower_copy(kernel.name).find(to_lower_copy(kernel_name)) !=
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

    const std::string needle = to_lower_copy(metric_name);
    std::vector<MetricsRequestParams::MetricID> metric_ids;
    for(const AvailableMetrics::Entry& entry : workload->available_metrics.list)
    {
        if(to_lower_copy(entry.name).find(needle) == std::string::npos)
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
    AssistantFetchState metrics_fetch =
        TableFetch(AssistantFetchKind::kMetrics, TableType::kSummaryKernelTable,
                   ASSISTANT_DEFAULT_ROW_LIMIT);
    metrics_fetch.kernel_id = kernel->id;
    if(context.data_provider->IsRequestPending(request_id))
    {
        return PendingResult(request_id, metrics_fetch,
                             AssistantToolStatusLabel(tool_name), false);
    }
    const bool queued = context.data_provider->FetchMetrics(MetricsRequestParams(
        workload->id, { kernel->id }, metric_ids, context.metrics_client_id));
    if(!queued && !context.data_provider->IsRequestPending(request_id))
    {
        out << "Could not queue the metric fetch.\n";
        return DoneResult(TrimResult(out.str()), "Fetch failed");
    }
    AssistantToolStartResult pending = PendingResult(
        request_id, metrics_fetch, AssistantToolStatusLabel(tool_name), queued);
    pending.content = out.str();
    return pending;
}

// Implements track_events and track_samples, which differ only in which table
// they read; tool_name is what tells them apart.
AssistantToolStartResult
ToolTrackRows(const AssistantToolContext& context, const jt::Json& args,
                const std::string& tool_name)
{
    if(context.is_compute)
    {
        return DoneResult(
            "Compute traces have no timeline tracks. Use kernel_metrics.",
            "Wrong trace kind");
    }

    const bool      events   = tool_name == "track_events";
    const TableType type     = events ? TableType::kEventTable : TableType::kSampleTable;
    const uint64_t request_id = events ? DataProvider::EVENT_TABLE_REQUEST_ID
                                       : DataProvider::SAMPLE_TABLE_REQUEST_ID;
    const rocprofvis_controller_track_type_t wanted =
        events ? kRPVControllerTrackTypeEvents : kRPVControllerTrackTypeSamples;

    std::string           track_error;
    std::vector<uint64_t> tracks = TracksFromArgs(
        context, args, kRocProfVisDmOperationDispatch, false, track_error);
    if(!track_error.empty())
    {
        return DoneResult(track_error, "Bad track_ids");
    }
    if(tracks.empty())
    {
        tracks = TracksOfType(context, wanted);
    }
    else
    {
        tracks = KeepTracksOfType(context, tracks, wanted);
        if(tracks.empty())
        {
            return DoneResult(
                std::string("Those track_ids are not ") +
                    (events ? "event tracks. Use track_samples for counter tracks."
                            : "counter tracks. Use track_events for queue, stream, "
                              "and thread tracks."),
                "Wrong track kind");
        }
    }
    if(tracks.empty())
    {
        return DoneResult(std::string("No ") +
                              (events ? "event" : "counter") +
                              " tracks found. Call list_tracks.",
                          "No matching tracks");
    }

    std::string       query_error;
    const std::string where    = BuildAssistantWhereClause(args, query_error);
    const std::string group_by = AssistantGroupByFromArgs(args, query_error);
    if(!query_error.empty())
    {
        return DoneResult(query_error, "Bad query argument");
    }

    const size_t limit =
        ClampRowLimit(JsonUtils::GetInt(args, "limit",
                                        static_cast<int32_t>(ASSISTANT_DEFAULT_ROW_LIMIT)));
    double start_ns = 0.0;
    double end_ns   = 0.0;
    TimeRangeFromArgs(context, args, start_ns, end_ns);

    const AssistantFetchState fetch =
        TableFetch(AssistantFetchKind::kDataTable, type, limit);
    if(context.data_provider->IsRequestPending(request_id))
    {
        return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                             false);
    }

    const TablesModel& tables = context.data_provider->DataModel().GetTables();
    const bool queued = context.data_provider->FetchTable(TrackTableRequestParams(
        events ? kRPVControllerTableTypeEvents : kRPVControllerTableTypeSamples,
        tracks, start_ns, end_ns, where.c_str(), "", group_by.c_str(), "",
        OffsetFromArgs(args), static_cast<uint64_t>(limit),
        ResolveAssistantSortColumn(tables, type, JsonUtils::GetString(args, "sort_by", ""), 0),
        AssistantSortOrderFromArgs(args, kRPVControllerSortOrderAscending)));
    if(!queued && !context.data_provider->IsRequestPending(request_id))
    {
        return DoneResult("Could not queue the " + tool_name + " fetch.",
                          "Fetch failed");
    }
    return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                         queued);
}

// Implements the event_details tool.
AssistantToolStartResult
ToolEventDetails(const AssistantToolContext& context, const jt::Json& args,
                 const std::string& tool_name)
{
    if(context.is_compute)
    {
        return DoneResult("event_details is for system traces.", "Wrong trace kind");
    }
    const uint64_t event_uuid =
        JsonU64(args, "event_uuid", JsonU64(args, "event_id", 0));
    if(event_uuid == 0)
    {
        return DoneResult("event_details needs event_uuid from the __uuid column.",
                          "Missing event_uuid");
    }

    const std::vector<uint64_t> request_ids = {
        DataProvider::EVENT_EXTENDED_DATA_REQUEST_ID,
        DataProvider::EVENT_FLOW_DATA_REQUEST_ID,
        DataProvider::EVENT_CALL_STACK_DATA_REQUEST_ID
    };
    // These request ids are shared with the event panel, and re-issuing one
    // that is already in flight would drop its future on the floor.
    for(uint64_t request_id : request_ids)
    {
        if(context.data_provider->IsRequestPending(request_id))
        {
            return DoneResult("Event details are already loading. Try again.",
                              "Busy");
        }
    }

    const uint64_t track_id = JsonU64(args, "track_id", INVALID_UINT64_INDEX);
    // FetchEvent seeds name/duration from loaded track data and chains the
    // extended-data fetch; without a track id only the fetches below run.
    bool extended = false;
    if(track_id != INVALID_UINT64_INDEX)
    {
        extended = context.data_provider->FetchEvent(track_id, event_uuid);
    }
    const bool flow  = context.data_provider->FetchEventFlowDetails(event_uuid);
    const bool stack = context.data_provider->FetchEventCallStackData(event_uuid);
    if(!extended && !flow && !stack)
    {
        return DoneResult("Could not load details for that event.", "Fetch failed");
    }

    AssistantFetchState fetch;
    fetch.kind     = AssistantFetchKind::kEventDetails;
    fetch.event_id = event_uuid;
    fetch.track_id = track_id;
    return PendingResult(request_ids, fetch, AssistantToolStatusLabel(tool_name),
                         true);
}

// Implements the track_statistics tool.
AssistantToolStartResult
ToolTrackStatistics(const AssistantToolContext& context, const jt::Json& args,
                    const std::string& tool_name)
{
    if(context.is_compute)
    {
        return DoneResult("track_statistics is for system traces.",
                          "Wrong trace kind");
    }
    const uint64_t track_id = JsonU64(args, "track_id", INVALID_UINT64_INDEX);
    const TrackInfo* track =
        context.data_provider->DataModel().GetTimeline().GetTrack(track_id);
    if(track == nullptr)
    {
        return DoneResult("Unknown track_id. Call list_tracks first.",
                          "Unknown track");
    }
    if(track->topology.type != TrackInfo::Queue &&
       track->topology.type != TrackInfo::Counter)
    {
        return DoneResult(
            "track_statistics only works on Queue and Counter tracks. Use "
            "track_events for thread and stream tracks.",
            "Wrong track type");
    }

    const AnalysisTrackStatistics* stats =
        context.data_provider->DataModel().GetAnalysis().RegisterTrack(*track);
    if(stats == nullptr)
    {
        return DoneResult("Statistics are not available for that track.",
                          "No statistics");
    }

    double start_ns = 0.0;
    double end_ns   = 0.0;
    TimeRangeFromArgs(context, args, start_ns, end_ns);

    AssistantFetchState fetch;
    fetch.kind     = AssistantFetchKind::kTrackStatistics;
    fetch.track_id = track_id;

    const uint64_t request_id = RequestIdBuilder::MakeTrackDataRequestId(
        static_cast<uint32_t>(track_id), 0, 0,
        RequestType::kFetchAnalysisTrackStatistics);
    if(context.data_provider->IsRequestPending(request_id))
    {
        return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                             false);
    }

    const bool queued = context.data_provider->FetchAnalysisTrackStatistics(
        AnalysisTrackStatisticsRequestParams(track_id, start_ns, end_ns));
    stats->state = queued ? AnalysisTrackStatistics::kRequested
                          : AnalysisTrackStatistics::kPending;
    if(!queued)
    {
        return DoneResult("Could not queue the track statistics fetch.",
                          "Fetch failed");
    }
    return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name), true);
}

// Implements the search_events tool.
AssistantToolStartResult
ToolSearchEvents(const AssistantToolContext& context, const jt::Json& args,
                 const std::string& tool_name)
{
    if(context.is_compute)
    {
        return DoneResult(
            "search_events is for system traces. Use kernel_metrics on compute "
            "traces.",
            "Wrong trace kind");
    }

    std::vector<std::string> terms = JsonUtils::GetStringArray(args, "terms");
    const std::string        single = JsonUtils::GetString(args, "query", "");
    if(terms.empty() && !single.empty())
    {
        terms.push_back(single);
    }
    if(terms.empty())
    {
        return DoneResult(
            "search_events needs terms, e.g. {\"terms\":[\"gemm\"]}.",
            "Missing terms");
    }
    if(terms.size() > ASSISTANT_MAX_SEARCH_TERMS)
    {
        terms.resize(ASSISTANT_MAX_SEARCH_TERMS);
    }

    std::vector<rocprofvis_dm_event_operation_t> ops;
    for(const std::string& category : JsonUtils::GetStringArray(args, "categories"))
    {
        const TopEventsSpec* spec = FindTopEventsSpec(category);
        if(spec == nullptr)
        {
            return DoneResult("Unknown category \"" + category +
                                  "\". Use dispatch, memory_copy, memory_alloc, "
                                  "instrumented, or sampled.",
                              "Bad category");
        }
        // Only a handful of categories exist, so a repeat is the model
        // being redundant rather than asking for more. Dropping duplicates
        // also bounds this list however long the argument was.
        if(std::find(ops.begin(), ops.end(), spec->op) == ops.end())
        {
            ops.push_back(spec->op);
        }
    }
    if(ops.empty())
    {
        ops = { kRocProfVisDmOperationLaunch, kRocProfVisDmOperationDispatch,
                kRocProfVisDmOperationMemoryCopy,
                kRocProfVisDmOperationMemoryAllocate,
                kRocProfVisDmOperationLaunchSample };
    }

    const bool contains =
        to_lower_copy(JsonUtils::GetString(args, "match", "contains")) != "equals";
    const bool any_term =
        to_lower_copy(JsonUtils::GetString(args, "combine", "all")) == "any";
    const bool include_category =
        JsonUtils::GetBool(args, "include_category", false);

    std::string       query_error;
    const std::string where = BuildAssistantWhereClause(args, query_error);
    if(!query_error.empty())
    {
        return DoneResult(query_error, "Bad query argument");
    }

    const size_t limit =
        ClampRowLimit(JsonUtils::GetInt(args, "limit",
                                        static_cast<int32_t>(ASSISTANT_DEFAULT_ROW_LIMIT)));
    double start_ns = 0.0;
    double end_ns   = 0.0;
    TimeRangeFromArgs(context, args, start_ns, end_ns, false);

    const uint64_t            request_id = DataProvider::EVENT_SEARCH_REQUEST_ID;
    const AssistantFetchState fetch = TableFetch(
        AssistantFetchKind::kDataTable, TableType::kEventSearchTable, limit);
    if(context.data_provider->IsRequestPending(request_id))
    {
        return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                             false);
    }

    const TablesModel& tables = context.data_provider->DataModel().GetTables();
    const bool queued = context.data_provider->FetchTable(EventSearchRequestParams(
        kRPVControllerTableTypeSearchResults, ops, start_ns, end_ns, where.c_str(),
        contains, include_category, any_term, terms, OffsetFromArgs(args),
        static_cast<uint64_t>(limit),
        ResolveAssistantSortColumn(tables, TableType::kEventSearchTable,
                          JsonUtils::GetString(args, "sort_by", ""), 1),
        AssistantSortOrderFromArgs(args, kRPVControllerSortOrderAscending)));
    if(!queued && !context.data_provider->IsRequestPending(request_id))
    {
        return DoneResult("Could not queue the search.", "Fetch failed");
    }
    return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                         queued);
}

const AssistantToolEntry k_data_tool_handlers[] = {
    { "trace_overview", ToolTraceOverview },
    { "get_summary", ToolGetSummary },
    { "list_tracks", ToolListTracks },
    { "top_events", ToolTopEvents },
    { "kernel_instances", ToolKernelInstances },
    { "kernel_metrics", ToolKernelMetrics },
    { "track_events", ToolTrackRows },
    { "track_samples", ToolTrackRows },
    { "event_details", ToolEventDetails },
    { "track_statistics", ToolTrackStatistics },
    { "search_events", ToolSearchEvents },
};

}  // namespace

// The data half of the tool set, for StartAssistantTool to search.
AssistantToolTable
GetAssistantDataToolHandlers()
{
    AssistantToolTable table;
    table.entries = k_data_tool_handlers;
    table.count   = sizeof(k_data_tool_handlers) / sizeof(k_data_tool_handlers[0]);
    return table;
}

// Formats the rows of a fetch that has landed, by the kind of fetch it was.
std::string
FinishAssistantFetch(const AssistantToolContext& context,
                     const AssistantFetchState&  fetch)
{
    if(context.data_provider == nullptr)
    {
        return "No trace is open.";
    }
    const size_t limit =
        fetch.row_limit == 0 ? ASSISTANT_DEFAULT_ROW_LIMIT : fetch.row_limit;

    if(fetch.kind == AssistantFetchKind::kSummary)
    {
        return FormatSystemSummary(context);
    }
    if(fetch.kind == AssistantFetchKind::kTopEvents)
    {
        const TablesModel& tables =
            context.data_provider->DataModel().GetAnalysis().GetTables();
        if(tables.GetTableData(fetch.table_type).empty() &&
           tables.GetTableHeader(fetch.table_type).empty())
        {
            return "top_events returned no rows for the current tracks and time range.";
        }
        return FormatTableSnapshot(tables, fetch.table_type, limit);
    }
    if(fetch.kind == AssistantFetchKind::kKernelInstances ||
       fetch.kind == AssistantFetchKind::kDataTable)
    {
        return FormatTableSnapshot(context.data_provider->DataModel().GetTables(),
                                   fetch.table_type, limit);
    }
    if(fetch.kind == AssistantFetchKind::kEventDetails)
    {
        return FormatEventDetails(context, fetch.event_id);
    }
    if(fetch.kind == AssistantFetchKind::kTrackStatistics)
    {
        return FormatTrackStatistics(context, fetch.track_id);
    }
    if(fetch.kind == AssistantFetchKind::kScript)
    {
        return FinishAssistantScriptFetch(context);
    }
    if(fetch.kind == AssistantFetchKind::kMetrics)
    {
        ComputeDataModel& model = context.data_provider->ComputeModel();
        const std::vector<std::shared_ptr<MetricValue>>* values =
            model.GetKernelMetricsData(context.metrics_client_id, fetch.kernel_id);
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
    return "Fetch finished with nothing to report.";
}

}  // namespace View
}  // namespace RocProfVis
