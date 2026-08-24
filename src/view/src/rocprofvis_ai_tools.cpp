// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_ai_tools.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "json.h"
#include "spdlog/spdlog.h"

#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "rocprofvis_ai_actions.h"
#include "model/rocprofvis_analysis_model.h"
#include "model/rocprofvis_common_defs.h"
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

constexpr size_t   ASSISTANT_MAX_TRACKS       = 32;
constexpr size_t   ASSISTANT_MAX_LIST_TRACKS  = 80;
constexpr size_t   ASSISTANT_MAX_METRIC_NAMES = 40;
constexpr size_t   ASSISTANT_MAX_METRIC_FETCH = 8;
constexpr size_t   ASSISTANT_TOP_KERNEL_LIMIT = 10;
constexpr size_t   ASSISTANT_MAX_RESULT_CHARS = 20000;
constexpr size_t   ASSISTANT_MAX_FILTERS      = 8;
constexpr size_t   ASSISTANT_MAX_CALL_STACK   = 40;
constexpr size_t   ASSISTANT_MAX_FLOW_ROWS    = 20;
constexpr size_t   ASSISTANT_MAX_EXT_ROWS     = 40;
constexpr size_t   ASSISTANT_MAX_EVENT_ARGS   = 40;
constexpr size_t   ASSISTANT_MAX_SEARCH_TERMS = 8;
constexpr size_t   ASSISTANT_OVERVIEW_BINS    = 32;
constexpr size_t   ASSISTANT_OVERVIEW_TRACKS  = 12;
constexpr size_t   ASSISTANT_MAX_HIGHLIGHTS      = 12;
constexpr size_t   ASSISTANT_MAX_NEXT_STEPS      = 3;
constexpr size_t   ASSISTANT_MAX_NEXT_STEP_CHARS = 80;
constexpr double   ASSISTANT_OVERVIEW_SCALE   = 100.0;
constexpr uint64_t ASSISTANT_DURATION_COLUMN  = 2;

// Columns the model is allowed to filter, group, and sort on. Filters reach the
// database as a SQL fragment, so only names on this list are accepted and the
// values beside them are quoted by QuoteSqlLiteral.
const char* const ASSISTANT_QUERY_COLUMNS[] = {
    "name",       "category",  "duration",   "start",      "end",
    "id",         "__uuid",    "PID",        "TID",        "queue",
    "stream",     "node",      "nodeId",     "size",       "address",
    "SrcAddr",    "value",     "counter",    "arguments",  "GridSizeX",
    "GridSizeY",  "GridSizeZ", "WGSizeX",    "WGSizeY",    "WGSizeZ",
    "LDSSize",    "ScratchSize", "StaticLDSSize", "StaticScratchSize",
    "AgentAbsoluteIndex", "AgentType", "AgentTypeIndex", "AgentName",
    "SrcAgentAbsoluteIndex", "SrcAgentType", "SrcAgentTypeIndex",
    "SrcAgentName", "__trackId", "__streamTrackId",
};

enum class QueryWildcard
{
    kNone,
    kContains,
    kPrefix
};

struct QueryOperator
{
    const char*   key;
    const char*   sql;
    QueryWildcard wildcard;
};

const QueryOperator ASSISTANT_QUERY_OPERATORS[] = {
    { "=", "=", QueryWildcard::kNone },
    { "==", "=", QueryWildcard::kNone },
    { "!=", "!=", QueryWildcard::kNone },
    { "<>", "!=", QueryWildcard::kNone },
    { "<", "<", QueryWildcard::kNone },
    { "<=", "<=", QueryWildcard::kNone },
    { ">", ">", QueryWildcard::kNone },
    { ">=", ">=", QueryWildcard::kNone },
    { "contains", "LIKE", QueryWildcard::kContains },
    { "starts_with", "LIKE", QueryWildcard::kPrefix },
};

struct TopEventsSpec
{
    const char*                        key;
    rocprofvis_controller_table_type_t controller_type;
    TableType                          table_type;
    uint64_t                           request_id;
    rocprofvis_dm_event_operation_t    op;
};

struct AssistantToolLabel
{
    const char* name;
    const char* status;
};

// Every tool the model can call, with the line the panel shows while it runs.
// One list keeps the status text and the "unknown tool" reply in step with what
// BuildAssistantToolsJson registers.
const AssistantToolLabel ASSISTANT_TOOL_LABELS[] = {
    { "trace_overview", "Reading the timeline overview..." },
    { "get_summary", "Loading summary..." },
    { "top_events", "Querying top events..." },
    { "kernel_instances", "Loading kernel dispatches..." },
    { "kernel_metrics", "Loading kernel metrics..." },
    { "list_tracks", "Listing tracks..." },
    { "search_events", "Searching the trace..." },
    { "track_events", "Reading track events..." },
    { "track_samples", "Reading counter samples..." },
    { "event_details", "Loading event details..." },
    { "track_statistics", "Computing track statistics..." },
    { "goto", "Moving the view..." },
    { "show_panel", "Opening a panel..." },
    { "switch_tab", "Switching tabs..." },
    { "flow_arrows", "Adjusting flow arrows..." },
    { "annotate", "Leaving a note..." },
    { "bookmark", "Working with bookmarks..." },
    { "measure", "Measuring..." },
    { "reset_view", "Resetting the view..." },
    { "offer_next_steps", "Offering next steps..." },
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

// Every UI-facing thing a tool does goes through here.
OptiqActions
Actions(const AssistantToolContext& context)
{
    return OptiqActions(context.data_provider, context.timeline_selection,
                        context.compute_selection, context.trace_view);
}

// Lowercases a copy, for the case-insensitive matching the tools do on names.
std::string
ToLowerCopy(std::string value)
{
    for(char& c : value)
    {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

// Strips leading and trailing whitespace from a follow-up the model offered.
std::string
TrimCopy(std::string value)
{
    size_t start = 0;
    while(start < value.size() &&
          std::isspace(static_cast<unsigned char>(value[start])) != 0)
    {
        ++start;
    }
    size_t end = value.size();
    while(end > start &&
          std::isspace(static_cast<unsigned char>(value[end - 1])) != 0)
    {
        --end;
    }
    return value.substr(start, end - start);
}

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

// Maps a category name, plus the synonyms the model likes, to its table spec.
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

// Reports whether a column name is one a tool argument is allowed to name.
bool
IsAllowedQueryColumn(const std::string& name)
{
    for(const char* column : ASSISTANT_QUERY_COLUMNS)
    {
        if(name == column)
        {
            return true;
        }
    }
    return false;
}

// Lists the allowed columns, so a rejection tells the model what to use.
std::string
AllowedQueryColumnList()
{
    std::ostringstream out;
    bool               first = true;
    for(const char* column : ASSISTANT_QUERY_COLUMNS)
    {
        if(!first)
        {
            out << ", ";
        }
        first = false;
        out << column;
    }
    return out.str();
}

// Looks up a filter operator, so only known spellings reach the query.
const QueryOperator*
FindQueryOperator(const std::string& key)
{
    for(const QueryOperator& op : ASSISTANT_QUERY_OPERATORS)
    {
        if(key == op.key)
        {
            return &op;
        }
    }
    return nullptr;
}

// Renders a string as a SQL literal: single quotes doubled, control characters
// dropped, so a value cannot terminate the literal and inject syntax.
std::string
QuoteSqlLiteral(const std::string& value)
{
    std::string quoted = "'";
    for(char c : value)
    {
        if(static_cast<unsigned char>(c) < 0x20)
        {
            continue;
        }
        if(c == '\'')
        {
            quoted += '\'';
        }
        quoted += c;
    }
    quoted += '\'';
    return quoted;
}

// Prints a double at full precision so a filter value survives the trip.
std::string
FormatNumberLiteral(double value)
{
    std::ostringstream out;
    out << std::setprecision(17) << value;
    return out.str();
}

// Turns a validated filter list into a boolean SQL fragment. The query builder
// appends this after "AND", so it must not contain the WHERE keyword.
std::string
BuildWhereClause(const jt::Json& args, std::string& error_out)
{
    jt::Json& mutable_args = const_cast<jt::Json&>(args);
    if(!mutable_args.contains("filters") || !mutable_args["filters"].isArray())
    {
        return std::string();
    }

    std::vector<jt::Json>& entries = mutable_args["filters"].getArray();
    if(entries.size() > ASSISTANT_MAX_FILTERS)
    {
        error_out = "Too many filters. Use at most " +
                    std::to_string(ASSISTANT_MAX_FILTERS) + ".";
        return std::string();
    }

    std::ostringstream out;
    size_t             used = 0;
    for(jt::Json& entry : entries)
    {
        if(!entry.isObject())
        {
            continue;
        }

        const std::string column = JsonUtils::GetString(entry, "column", "");
        if(!IsAllowedQueryColumn(column))
        {
            error_out = "Unknown filter column \"" + column +
                        "\". Allowed columns: " + AllowedQueryColumnList();
            return std::string();
        }

        const QueryOperator* op =
            FindQueryOperator(ToLowerCopy(JsonUtils::GetString(entry, "op", "=")));
        if(op == nullptr)
        {
            error_out = "Unknown filter op. Use =, !=, <, <=, >, >=, contains, "
                        "or starts_with.";
            return std::string();
        }

        if(!entry.contains("value"))
        {
            error_out = "Filter on \"" + column + "\" is missing value.";
            return std::string();
        }

        jt::Json&   value = entry["value"];
        std::string literal;
        if(value.isString())
        {
            std::string text = value.getString();
            if(op->wildcard == QueryWildcard::kContains)
            {
                text = "%" + text + "%";
            }
            else if(op->wildcard == QueryWildcard::kPrefix)
            {
                text += "%";
            }
            literal = QuoteSqlLiteral(text);
        }
        else if(value.isLong())
        {
            literal = std::to_string(value.getLong());
        }
        else if(value.isDouble())
        {
            literal = FormatNumberLiteral(value.getDouble());
        }
        else
        {
            error_out = "Filter on \"" + column + "\" needs a string or number value.";
            return std::string();
        }

        if(used > 0)
        {
            out << " AND ";
        }
        out << column << " " << op->sql << " " << literal;
        ++used;
    }
    return out.str();
}

// group_by goes into the query as a bare column name, so it gets the same
// whitelist treatment as filter columns.
std::string
GroupByFromArgs(const jt::Json& args, std::string& error_out)
{
    const std::string group = JsonUtils::GetString(args, "group_by", "");
    if(group.empty() || !error_out.empty())
    {
        return std::string();
    }
    if(!IsAllowedQueryColumn(group))
    {
        error_out = "Unknown group_by column \"" + group +
                    "\". Allowed columns: " + AllowedQueryColumnList();
        return std::string();
    }
    return group;
}

// Turns a sort_by column name into its index in the table's own header.
uint64_t
ResolveSortColumn(const TablesModel& tables, TableType type, const std::string& name,
                  uint64_t fallback)
{
    if(name.empty())
    {
        return fallback;
    }
    const std::vector<std::string>& header  = tables.GetTableHeader(type);
    const std::string               lowered = ToLowerCopy(name);
    for(size_t i = 0; i < header.size(); ++i)
    {
        if(ToLowerCopy(header[i]) == lowered)
        {
            return static_cast<uint64_t>(i);
        }
    }
    return fallback;
}

// Reads asc/desc, keeping each tool's own default when the model omits it.
rocprofvis_controller_sort_order_t
SortOrderFromArgs(const jt::Json& args, rocprofvis_controller_sort_order_t fallback)
{
    const std::string order = ToLowerCopy(JsonUtils::GetString(args, "sort_order", ""));
    if(order == "asc" || order == "ascending")
    {
        return kRPVControllerSortOrderAscending;
    }
    if(order == "desc" || order == "descending")
    {
        return kRPVControllerSortOrderDescending;
    }
    return fallback;
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
    if(requested_start >= 0.0 && requested_end > requested_start)
    {
        start_ns = requested_start;
        end_ns   = requested_end;
    }
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

    const TimelineModel&   timeline = context.data_provider->DataModel().GetTimeline();
    std::vector<jt::Json>& entries  = mutable_args["track_ids"].getArray();
    std::vector<uint64_t>  tracks;
    tracks.reserve(entries.size());
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
            continue;
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

// Builds the result of a tool that finished without waiting on a fetch.
AssistantToolStartResult
DoneResult(const std::string& content, const std::string& status)
{
    AssistantToolStartResult result;
    result.content     = content;
    result.status_line = status;
    return result;
}

// Reads the stacked follow-up buttons out of offer_next_steps arguments.
AssistantToolStartResult
OfferNextStepsResult(const jt::Json& args)
{
    jt::Json& mutable_args = const_cast<jt::Json&>(args);
    if(!mutable_args.contains("steps") || !mutable_args["steps"].isArray())
    {
        return DoneResult("offer_next_steps needs a steps array of 1 to 3 strings.",
                          "Missing steps");
    }

    AssistantToolStartResult result =
        DoneResult("Offered next steps under the chat.", "Offered next steps");
    result.set_next_steps = true;
    for(jt::Json& entry : mutable_args["steps"].getArray())
    {
        if(!entry.isString())
        {
            continue;
        }
        std::string step = TrimCopy(entry.getString());
        if(step.empty())
        {
            continue;
        }
        if(step.size() > ASSISTANT_MAX_NEXT_STEP_CHARS)
        {
            step.resize(ASSISTANT_MAX_NEXT_STEP_CHARS);
        }
        result.next_steps.push_back(step);
        if(result.next_steps.size() >= ASSISTANT_MAX_NEXT_STEPS)
        {
            break;
        }
    }
    if(result.next_steps.empty())
    {
        return DoneResult("offer_next_steps needs at least one non-empty step.",
                          "Missing steps");
    }
    return result;
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

// Builds a JSON string array, for the enum of a tool parameter.
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

// Registers one tool in the schema array the model receives.
void
AddTool(jt::Json& tools, size_t index, const char* name, const char* description,
        jt::Json parameters)
{
    tools[index]["type"]                    = "function";
    tools[index]["function"]["name"]        = name;
    tools[index]["function"]["description"] = description;
    tools[index]["function"]["parameters"]  = parameters;
}

// Starts an empty object schema, which every tool's parameters build on.
jt::Json
ObjectParams()
{
    jt::Json params;
    params["type"] = "object";
    params["properties"].setObject();
    return params;
}

// Adds one typed, described property to a parameter schema.
void
AddParam(jt::Json& params, const char* name, const char* type, const char* description)
{
    params["properties"][name]["type"]        = type;
    params["properties"][name]["description"] = description;
}

// The window and tracks a table tool reads from.
void
AddScopeParams(jt::Json& params)
{
    AddParam(params, "start_ns", "number",
             "Window start in nanoseconds. Defaults to the user's selected range, "
             "or the whole trace.");
    AddParam(params, "end_ns", "number", "Window end in nanoseconds.");
    AddParam(params, "track_ids", "array",
             "Track ids from list_tracks. Defaults to every track that carries the "
             "requested events.");
    params["properties"]["track_ids"]["items"]["type"] = "integer";
}

// The structured stand-in for a SQL query: filters, sort, paging, grouping.
void
AddQueryParams(jt::Json& params)
{
    jt::Json filter_item = ObjectParams();
    AddParam(filter_item, "column", "string",
             "Column name. A bad name comes back with the allowed list.");
    AddParam(filter_item, "op", "string",
             "=, !=, <, <=, >, >=, contains, or starts_with.");
    filter_item["properties"]["value"]["description"] =
        "String or number to compare against.";
    filter_item["required"][0] = "column";
    filter_item["required"][1] = "value";

    AddParam(params, "filters", "array",
             "Conditions combined with AND, e.g. "
             "[{\"column\":\"duration\",\"op\":\">=\",\"value\":5000}].");
    params["properties"]["filters"]["items"] = filter_item;

    AddParam(params, "sort_by", "string",
             "Column name to sort by. Call once without it to see the columns.");
    AddParam(params, "sort_order", "string", "asc or desc.");
    params["properties"]["sort_order"]["enum"] = MakeStringEnum({ "asc", "desc" });
    AddParam(params, "limit", "integer", "Rows to return (default 20, max 200).");
    AddParam(params, "offset", "integer",
             "Rows to skip. Use with limit to page through a big result.");
    AddParam(params, "group_by", "string",
             "Optional column to aggregate rows by, e.g. name.");
}

// Every registered tool name, for telling the model when it invents one.
std::string
AssistantToolNameList()
{
    std::ostringstream out;
    bool               first = true;
    for(const AssistantToolLabel& label : ASSISTANT_TOOL_LABELS)
    {
        if(!first)
        {
            out << ", ";
        }
        first = false;
        out << label.name;
    }
    return out.str();
}

}  // namespace

// Builds the tool schema sent with every request. The descriptions here are
// the only instructions the model gets about what each tool is for.
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
    AddParam(top_params, "category", "string", "Event family to rank by duration.");
    top_params["properties"]["category"]["enum"] =
        MakeStringEnum({ "dispatch", "memory_copy", "memory_alloc", "instrumented",
                         "sampled" });
    AddScopeParams(top_params);
    AddQueryParams(top_params);
    top_params["required"][0] = "category";
    AddTool(tools, 1, "top_events",
            "Rank the hottest events of one category, optionally narrowed to named "
            "tracks, an explicit time window, and column filters. Uses the same "
            "analysis tables as View > Top Events. Does not run raw SQL.",
            top_params);

    jt::Json inst_params = ObjectParams();
    AddParam(inst_params, "kernel_name", "string",
             "Exact or unique kernel name from get_summary / top_events.");
    AddScopeParams(inst_params);
    AddQueryParams(inst_params);
    inst_params["required"][0] = "kernel_name";
    AddTool(tools, 2, "kernel_instances",
            "List individual GPU dispatches for one kernel name, with timestamps "
            "and track ids that goto can use.",
            inst_params);

    jt::Json metrics_params = ObjectParams();
    AddParam(metrics_params, "kernel_name", "string",
             "Kernel name (compute traces) or summary kernel name (system traces).");
    AddParam(metrics_params, "kernel_id", "integer",
             "Kernel id from get_summary, instead of the name, on a compute trace.");
    AddParam(metrics_params, "metric_name", "string",
             "Optional substring of a hardware metric to fetch on a compute trace.");
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
    AddParam(goto_params, "start_ns", "number", "Range start in nanoseconds.");
    AddParam(goto_params, "end_ns", "number", "Range end in nanoseconds.");
    AddParam(goto_params, "track_id", "integer",
             "__trackId of the event. Pass it whenever you pass event_uuid; without "
             "it the flow arrows cannot be drawn.");
    AddParam(goto_params, "event_uuid", "integer",
             "__uuid of the event to select. Selecting it expands its flow arrows "
             "and call stack, the same as clicking it.");
    AddParam(goto_params, "kernel_name", "string",
             "On compute traces, select this kernel in the UI.");
    AddParam(goto_params, "kernel_id", "integer",
             "On compute traces, select this kernel id instead of the name.");

    jt::Json goto_event_item = ObjectParams();
    AddParam(goto_event_item, "track_id", "integer", "Track the event lives on.");
    AddParam(goto_event_item, "event_uuid", "integer", "__uuid of the event.");
    goto_event_item["required"][0] = "event_uuid";
    AddParam(goto_params, "events", "array",
             "Events to light up on the timeline, from the __trackId and __uuid "
             "columns. They stay highlighted while the user reads, and the first "
             "one gets selected so its flow arrows and call stack expand.");
    goto_params["properties"]["events"]["items"] = goto_event_item;
    AddParam(goto_params, "zoom", "boolean",
             "Also zoom the timeline to the range, not just select it. Use this "
             "when the range is small enough that the user would have to zoom in "
             "by hand.");

    AddTool(tools, 5, "goto",
            "Point the UI at what you are talking about: move the timeline to a "
            "range, highlight the specific events behind your claim, and expand "
            "the first one's connecting arrows. Call this on your own whenever you "
            "name a window or an outlier, so the user sees exactly what you saw. "
            "A range on its own only scrolls the view - pass event_uuid and "
            "track_id, or nothing gets highlighted and no arrows appear. Do not "
            "use it to pin notes or change other UI.",
            goto_params);

    jt::Json track_events_params = ObjectParams();
    AddScopeParams(track_events_params);
    AddQueryParams(track_events_params);
    AddTool(tools, 6, "track_events",
            "Read raw events off specific timeline tracks. This is the closest thing "
            "to querying the database: pick tracks with track_ids, a window with "
            "start_ns/end_ns, narrow rows with filters, then sort and page. Returns "
            "the same rows as the Event Table tab.",
            track_events_params);

    jt::Json track_samples_params = ObjectParams();
    AddScopeParams(track_samples_params);
    AddQueryParams(track_samples_params);
    AddTool(tools, 7, "track_samples",
            "Read counter/PMC samples off specific counter tracks over a window. "
            "Use track_ids from list_tracks where type=Counter. Same filtering, "
            "sorting, and paging as track_events.",
            track_samples_params);

    jt::Json event_details_params = ObjectParams();
    AddParam(event_details_params, "event_uuid", "integer",
             "Event uuid from the __uuid column of track_events, top_events, or "
             "kernel_instances.");
    AddParam(event_details_params, "track_id", "integer",
             "Optional owning track id, which fills in the event name and duration.");
    event_details_params["required"][0] = "event_uuid";
    AddTool(tools, 8, "event_details",
            "Drill into one event: its arguments, extended data, flow links to and "
            "from other events, and its call stack. Use after track_events or "
            "top_events gives you a __uuid.",
            event_details_params);

    jt::Json track_stats_params = ObjectParams();
    AddParam(track_stats_params, "track_id", "integer",
             "Queue track (returns utilization) or counter track (returns "
             "min/max/mean/stddev). From list_tracks.");
    AddParam(track_stats_params, "start_ns", "number",
             "Window start in nanoseconds. Defaults to the selection or whole trace.");
    AddParam(track_stats_params, "end_ns", "number", "Window end in nanoseconds.");
    track_stats_params["required"][0] = "track_id";
    AddTool(tools, 9, "track_statistics",
            "Compute statistics for one track over a window: queue busy percentage "
            "for a queue track, or min/max/mean/standard deviation for a counter "
            "track. Answers \"how busy was this GPU\" and \"how hot did it get\".",
            track_stats_params);

    jt::Json search_params = ObjectParams();
    AddParam(search_params, "terms", "array",
             "Names to search for, e.g. [\"gemm\", \"memcpy\"].");
    search_params["properties"]["terms"]["items"]["type"] = "string";
    AddParam(search_params, "match", "string",
             "contains (default, substring) or equals (exact name).");
    search_params["properties"]["match"]["enum"] =
        MakeStringEnum({ "contains", "equals" });
    AddParam(search_params, "combine", "string",
             "all (default, an event must match every term) or any.");
    search_params["properties"]["combine"]["enum"] = MakeStringEnum({ "all", "any" });
    AddParam(search_params, "include_category", "boolean",
             "Also match against event category names. Default false.");
    AddParam(search_params, "categories", "array",
             "Limit to these event families. Defaults to all of them.");
    search_params["properties"]["categories"]["items"]["type"] = "string";
    search_params["properties"]["categories"]["items"]["enum"] =
        MakeStringEnum({ "dispatch", "memory_copy", "memory_alloc", "instrumented",
                         "sampled" });
    AddParam(search_params, "start_ns", "number",
             "Window start in nanoseconds. Defaults to the whole trace.");
    AddParam(search_params, "end_ns", "number", "Window end in nanoseconds.");
    AddQueryParams(search_params);
    search_params["required"][0] = "terms";
    AddTool(tools, 10, "search_events",
            "Search the whole trace by event name, the same as the search box in "
            "the toolbar. Use this when you know part of a name but not which track "
            "or when it ran. Returns matching events with track ids, timestamps, and "
            "__uuid values you can pass to goto or event_details.",
            search_params);

    jt::Json overview_params = ObjectParams();
    AddParam(overview_params, "track_id", "integer",
             "Optional: profile one track instead of the whole trace.");
    AddParam(overview_params, "bins", "integer",
             "Time buckets to report (default 32).");
    AddTool(tools, 11, "trace_overview",
            "Preliminary analysis. Reads the timeline histogram and minimap that "
            "Optiq already built: where activity is concentrated in time, the "
            "busiest and quietest windows as real nanosecond ranges, total event "
            "count, and which tracks own the most events. Costs no database query. "
            "Call this FIRST so later tools can target a window and a track instead "
            "of scanning the whole trace.",
            overview_params);

    jt::Json panel_params = ObjectParams();
    AddParam(panel_params, "panel", "string",
             ("Which panel: " + OptiqActions::PanelNameList()).c_str());
    AddParam(panel_params, "visible", "boolean",
             "true to open, false to close. Defaults to true.");
    panel_params["required"][0] = "panel";
    AddTool(tools, 12, "show_panel",
            "Open or close one of Optiq's panels, the same as the View menu. Only "
            "call this when the user asked you to show or hide that panel.",
            panel_params);

    jt::Json tab_params = ObjectParams();
    AddParam(tab_params, "name", "string",
             "Tab to switch to. Part of the name is enough. Omit to list every "
             "tab that is available.");
    AddTool(tools, 13, "switch_tab",
            "Switch tabs among open traces or the details panel. Only call this "
            "when the user asked you to change tabs. Call with no name to list "
            "what is available.",
            tab_params);

    jt::Json flow_params = ObjectParams();
    AddParam(flow_params, "visible", "boolean",
             "Show or hide the flow arrows. Defaults to true.");
    AddParam(flow_params, "style", "string",
             "fan draws every link from the selected event; chain follows the "
             "sequence end to end.");
    flow_params["properties"]["style"]["enum"] = MakeStringEnum({ "fan", "chain" });
    AddTool(tools, 14, "flow_arrows",
            "Show, hide, or restyle the arrows that connect an event to the events "
            "it launched or waited on. Selecting an event with goto expands that "
            "event's arrows, but only while arrows are switched on, so pair this "
            "with goto using visible=true whenever you select an event - the user "
            "may have turned them off. Wait to be asked only before restyling them "
            "or switching them off.",
            flow_params);

    jt::Json note_params = ObjectParams();
    AddParam(note_params, "time_ns", "number",
             "Where on the timeline to pin the note.");
    AddParam(note_params, "title", "string", "Short heading, a few words.");
    AddParam(note_params, "text", "string",
             "What you found here and why it matters.");
    AddParam(note_params, "track_id", "integer",
             "Optional track to attach the note to.");
    note_params["required"][0] = "time_ns";
    note_params["required"][1] = "title";
    note_params["required"][2] = "text";
    AddTool(tools, 15, "annotate",
            "Pin a sticky note on the timeline. Notes are saved with the project. "
            "Only call this when the user asked you to leave a note.",
            note_params);

    jt::Json bookmark_params = ObjectParams();
    AddParam(bookmark_params, "action", "string",
             "save the current view, go to a saved one, remove one, or list them.");
    bookmark_params["properties"]["action"]["enum"] =
        MakeStringEnum({ "save", "goto", "remove", "list" });
    AddParam(bookmark_params, "slot", "integer", "Bookmark slot, 0 to 9.");
    AddTool(tools, 16, "bookmark",
            "Save, jump to, remove, or list numbered zoom bookmarks. Only call "
            "this when the user asked you to work with bookmarks.",
            bookmark_params);

    jt::Json measure_params = ObjectParams();
    AddParam(measure_params, "start_ns", "number", "Span start in nanoseconds.");
    AddParam(measure_params, "end_ns", "number", "Span end in nanoseconds.");
    AddParam(measure_params, "clear", "boolean",
             "Set true to clear the measurement instead of taking one.");
    AddTool(tools, 17, "measure",
            "Drop the two measurement pins on a span so the toolbar shows its "
            "duration. Only call this when the user asked you to measure a span.",
            measure_params);

    jt::Json reset_params = ObjectParams();
    AddTool(tools, 18, "reset_view",
            "Zoom the timeline back out to the whole trace. Only call this when "
            "the user asked you to reset the view.",
            reset_params);

    jt::Json next_params = ObjectParams();
    AddParam(next_params, "steps", "array",
             "Two or three short follow-ups the user can click, most useful first. "
             "Each is a complete thing they would type, under 80 characters.");
    next_params["properties"]["steps"]["items"]["type"] = "string";
    next_params["required"][0] = "steps";
    AddTool(tools, 19, "offer_next_steps",
            "Puts stacked buttons under the chat for what to look at next. Call it "
            "as the last tool of an investigation, then write your answer in the "
            "response after it. Do not list those same options in the prose.",
            next_params);

    return tools;
}

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

// The line the panel shows under the transcript while a tool runs.
std::string
AssistantToolStatusLabel(const std::string& tool_name)
{
    for(const AssistantToolLabel& label : ASSISTANT_TOOL_LABELS)
    {
        if(tool_name == label.name)
        {
            return label.status;
        }
    }
    return "Using " + tool_name + "...";
}

// Dispatches one named tool call, returning either a finished result or a
// pending fetch the panel polls until the rows land.
AssistantToolStartResult
StartAssistantTool(const AssistantToolContext& context, const std::string& tool_name,
                   const std::string& arguments_json)
{
    // Next-step buttons are a UI side-effect, so they do not need an open trace.
    if(tool_name == "offer_next_steps")
    {
        bool           args_ok = true;
        const jt::Json args    = ParseArgsObject(arguments_json, args_ok);
        if(!args_ok)
        {
            return DoneResult("Those arguments were not a JSON object, so none of them "
                              "were read. Send arguments as one JSON object.",
                              "Bad arguments");
        }
        return OfferNextStepsResult(args);
    }

    if(context.data_provider == nullptr)
    {
        return DoneResult("No trace is open.", "No trace");
    }
    if(context.data_provider->GetState() != ProviderState::kReady)
    {
        return DoneResult("The trace is still loading. Wait and try again.",
                          "Trace not ready");
    }

    bool           args_ok = true;
    const jt::Json args    = ParseArgsObject(arguments_json, args_ok);
    if(!args_ok)
    {
        return DoneResult("Those arguments were not a JSON object, so none of them "
                          "were read. Send arguments as one JSON object.",
                          "Bad arguments");
    }

    if(tool_name == "show_panel")
    {
        const std::string panel_name = JsonUtils::GetString(args, "panel", "");
        const OptiqPanel  panel      = OptiqActions::PanelFromName(panel_name);
        if(panel == OptiqPanel::kUnknown)
        {
            return DoneResult("Unknown panel \"" + panel_name + "\". Use one of: " +
                                  OptiqActions::PanelNameList() + ".",
                              "Unknown panel");
        }

        const bool visible = JsonUtils::GetBool(args, "visible", true);
        if(!Actions(context).ShowPanel(panel, visible))
        {
            return DoneResult(std::string("Could not change the ") +
                                  OptiqActions::PanelName(panel) + " panel here.",
                              "Panel unavailable");
        }
        return DoneResult(std::string(visible ? "Opened the " : "Closed the ") +
                              OptiqActions::PanelName(panel) + " panel.",
                          visible ? "Opened a panel" : "Closed a panel");
    }

    if(tool_name == "reset_view")
    {
        if(!Actions(context).ResetView())
        {
            return DoneResult("Reset view is only available on a system trace.",
                              "Not available");
        }
        return DoneResult("Zoomed back out to the whole trace.", "Reset the view");
    }

    if(tool_name == "measure")
    {
        OptiqActions actions = Actions(context);
        if(JsonUtils::GetBool(args, "clear", false))
        {
            actions.ClearMeasurement();
            return DoneResult("Cleared the measurement.", "Cleared measurement");
        }

        const double start_ns = JsonUtils::GetDouble(args, "start_ns", -1.0);
        const double end_ns   = JsonUtils::GetDouble(args, "end_ns", -1.0);
        if(start_ns < 0.0 || end_ns <= start_ns)
        {
            return DoneResult("measure needs start_ns and end_ns with end > start.",
                              "Bad range");
        }
        if(!actions.MeasureRange(start_ns, end_ns))
        {
            return DoneResult("Could not measure on this trace.", "Not available");
        }

        SettingsManager& settings    = SettingsManager::GetInstance();
        const TimeFormat time_format = settings.GetUserSettings().unit_settings.time_format;
        return DoneResult(
            "Measured " +
                nanosecond_to_formatted_str(end_ns - start_ns, time_format, true) +
                " on the timeline.",
            "Measured a span");
    }

    if(tool_name == "bookmark")
    {
        OptiqActions      actions = Actions(context);
        const std::string action  = ToLowerCopy(JsonUtils::GetString(args, "action", "list"));
        const std::vector<int> slots = actions.ListBookmarks();

        std::ostringstream saved;
        saved << "saved_bookmarks:";
        if(slots.empty())
        {
            saved << " none";
        }
        for(int slot : slots)
        {
            saved << " " << slot;
        }
        saved << "\n";

        if(action == "list")
        {
            return DoneResult(saved.str(), "Listed bookmarks");
        }

        const int slot = static_cast<int>(JsonUtils::GetInt(args, "slot", -1));
        if(slot < 0)
        {
            return DoneResult("bookmark needs a slot from 0 to 9.\n" + saved.str(),
                              "Missing slot");
        }
        if(action == "save")
        {
            if(!actions.SaveBookmark(slot))
            {
                return DoneResult("Could not save that bookmark.", "Bookmark failed");
            }
            return DoneResult("Saved the current view to bookmark " +
                                  std::to_string(slot) + ".",
                              "Saved a bookmark");
        }
        if(action == "goto")
        {
            if(!actions.GotoBookmark(slot))
            {
                return DoneResult("Bookmark " + std::to_string(slot) +
                                      " is empty.\n" + saved.str(),
                                  "No such bookmark");
            }
            return DoneResult("Jumped to bookmark " + std::to_string(slot) + ".",
                              "Used a bookmark");
        }
        if(action == "remove")
        {
            if(!actions.RemoveBookmark(slot))
            {
                return DoneResult("Bookmark " + std::to_string(slot) + " is empty.",
                                  "No such bookmark");
            }
            return DoneResult("Removed bookmark " + std::to_string(slot) + ".",
                              "Removed a bookmark");
        }
        return DoneResult("Unknown action. Use save, goto, remove, or list.",
                          "Bad action");
    }

    if(tool_name == "annotate")
    {
        if(context.is_compute)
        {
            return DoneResult("Notes are a timeline feature, so system traces only.",
                              "Wrong trace kind");
        }
        const double      time_ns = JsonUtils::GetDouble(args, "time_ns", -1.0);
        const std::string title   = JsonUtils::GetString(args, "title", "");
        const std::string text    = JsonUtils::GetString(args, "text", "");
        if(time_ns < 0.0 || title.empty() || text.empty())
        {
            return DoneResult("annotate needs time_ns, title, and text.",
                              "Missing arguments");
        }

        double view_start = 0.0;
        double view_end   = 0.0;
        SelectedOrFullTimeRange(context, view_start, view_end);
        if(!Actions(context).AddNote(time_ns, title, text, view_start, view_end,
                                     JsonU64(args, "track_id", INVALID_UINT64_INDEX)))
        {
            return DoneResult("Could not pin a note on this trace.", "Note failed");
        }
        return DoneResult("Pinned a note titled \"" + title + "\" on the timeline.",
                          "Left a note");
    }

    if(tool_name == "switch_tab")
    {
        OptiqActions                   actions       = Actions(context);
        const std::vector<std::string> trace_tabs    = actions.ListTabs();
        const std::vector<std::string> analysis_tabs = actions.ListAnalysisTabs();

        std::ostringstream out;
        out << "trace_tabs:";
        for(const std::string& tab : trace_tabs)
        {
            out << "\n  " << tab;
        }
        out << "\nactive_trace_tab: " << actions.ActiveTab() << "\n";
        if(!analysis_tabs.empty())
        {
            out << "details_panel_tabs:";
            for(const std::string& tab : analysis_tabs)
            {
                out << "\n  " << tab;
            }
            out << "\nactive_details_tab: " << actions.ActiveAnalysisTab() << "\n";
        }

        const std::string name = JsonUtils::GetString(args, "name", "");
        if(name.empty())
        {
            return DoneResult(out.str(), "Listed tabs");
        }

        // Details tabs are the ones people usually mean by name, so try those
        // first; trace tabs are file names and rarely collide.
        if(actions.SelectAnalysisTab(name))
        {
            return DoneResult("Opened the " + actions.ActiveAnalysisTab() +
                                  " tab in the details panel.",
                              "Switched tabs");
        }
        if(actions.SelectTab(name))
        {
            return DoneResult("Switched to trace " + actions.ActiveTab() + ".",
                              "Switched tabs");
        }

        out << "Nothing matches \"" << name << "\".\n";
        return DoneResult(out.str(), "No matching tab");
    }

    if(tool_name == "flow_arrows")
    {
        OptiqActions actions = Actions(context);
        const bool   visible = JsonUtils::GetBool(args, "visible", true);
        if(!actions.SetFlowArrowsVisible(visible))
        {
            return DoneResult("Flow arrows are only available on a system trace.",
                              "Not available");
        }

        const std::string style = ToLowerCopy(JsonUtils::GetString(args, "style", ""));
        if(style == "chain" || style == "fan")
        {
            actions.SetFlowRenderChained(style == "chain");
        }

        std::string message = visible ? "Flow arrows are on." : "Flow arrows are off.";
        if(!style.empty())
        {
            message += " Style set to " + style + ".";
        }
        if(visible)
        {
            message +=
                " They only draw for a selected event, so call goto with an "
                "event_uuid and track_id to see them.";
        }
        return DoneResult(message, visible ? "Flow arrows on" : "Flow arrows off");
    }

    if(tool_name == "trace_overview")
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

    if(tool_name == "get_summary")
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
            Actions(context).SelectKernel(kernel->id);
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

        OptiqActions actions = Actions(context);
        actions.SelectRange(start_ns, end_ns);
        const bool zoom = JsonUtils::GetBool(args, "zoom", false);
        if(zoom)
        {
            actions.ZoomToRange(start_ns, end_ns);
        }

        // The model may name one event via track_id/event_uuid or several via
        // events[]; both shapes collapse to this list.
        std::vector<std::pair<uint64_t, uint64_t>> targets;
        if(event_uuid != TimelineSelection::INVALID_SELECTION_ID)
        {
            targets.emplace_back(track_id, event_uuid);
        }
        jt::Json& mutable_args = const_cast<jt::Json&>(args);
        if(mutable_args.contains("events") && mutable_args["events"].isArray())
        {
            for(jt::Json& entry : mutable_args["events"].getArray())
            {
                if(!entry.isObject() || targets.size() >= ASSISTANT_MAX_HIGHLIGHTS)
                {
                    continue;
                }
                const uint64_t entry_uuid =
                    JsonU64(entry, "event_uuid",
                            JsonU64(entry, "__uuid",
                                    TimelineSelection::INVALID_SELECTION_ID));
                if(entry_uuid != TimelineSelection::INVALID_SELECTION_ID)
                {
                    targets.emplace_back(JsonU64(entry, "track_id", track_id), entry_uuid);
                }
            }
        }

        if(targets.empty())
        {
            actions.ShowRange(start_ns, end_ns);
        }
        else
        {
            actions.NavigateToEvent(targets.front().first, targets.front().second,
                                    start_ns, end_ns - start_ns);
        }

        // Clicking the first target is what makes the trace view load its
        // details, flow arrows, and call stack. NavigateToEvent only highlights,
        // so without this the arrows never appear.
        size_t highlighted = 0;
        if(!targets.empty())
        {
            actions.ClearHighlights();
            for(const std::pair<uint64_t, uint64_t>& target : targets)
            {
                if(!actions.HighlightEvent(target.first, target.second))
                {
                    continue;
                }
                if(highlighted == 0)
                {
                    actions.ClickEvent(target.first, target.second);
                    actions.ScrollToTrack(target.first);
                }
                ++highlighted;
            }
        }

        SettingsManager& settings    = SettingsManager::GetInstance();
        const TimeFormat time_format = settings.GetUserSettings().unit_settings.time_format;
        std::ostringstream out;
        out << "Moved the timeline to "
            << nanosecond_to_formatted_str(start_ns, time_format, true) << " .. "
            << nanosecond_to_formatted_str(end_ns, time_format, true);
        if(zoom)
        {
            out << ", zoomed in";
        }
        if(highlighted > 0)
        {
            out << " and highlighted " << highlighted
                << " event(s) there, with flow arrows expanded on the first";
        }
        out << ".";
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
        const std::string where    = BuildWhereClause(args, query_error);
        const std::string group_by = GroupByFromArgs(args, query_error);
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
            group_by.c_str(), "", JsonU64(args, "offset", 0),
            static_cast<uint64_t>(limit),
            ResolveSortColumn(tables, spec->table_type,
                              JsonUtils::GetString(args, "sort_by", ""),
                              ASSISTANT_DURATION_COLUMN),
            SortOrderFromArgs(args, kRPVControllerSortOrderDescending)));
        if(!queued && !context.data_provider->IsRequestPending(spec->request_id))
        {
            return DoneResult("Could not queue the top_events fetch.", "Fetch failed");
        }
        return PendingResult(spec->request_id, fetch,
                             AssistantToolStatusLabel(tool_name), queued);
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

        std::string       query_error;
        const std::string where = BuildWhereClause(args, query_error);
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
            false, false, { kernel_name }, JsonU64(args, "offset", 0),
            static_cast<uint64_t>(limit),
            ResolveSortColumn(tables, TableType::kSummaryKernelTable,
                              JsonUtils::GetString(args, "sort_by", ""), 0),
            SortOrderFromArgs(args, kRPVControllerSortOrderAscending)));
        if(!queued && !context.data_provider->IsRequestPending(request_id))
        {
            return DoneResult("Could not queue the kernel_instances fetch.", "Fetch failed");
        }
        return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                             queued);
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

    if(tool_name == "track_events" || tool_name == "track_samples")
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
        const std::string where    = BuildWhereClause(args, query_error);
        const std::string group_by = GroupByFromArgs(args, query_error);
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
            JsonU64(args, "offset", 0), static_cast<uint64_t>(limit),
            ResolveSortColumn(tables, type, JsonUtils::GetString(args, "sort_by", ""), 0),
            SortOrderFromArgs(args, kRPVControllerSortOrderAscending)));
        if(!queued && !context.data_provider->IsRequestPending(request_id))
        {
            return DoneResult("Could not queue the " + tool_name + " fetch.",
                              "Fetch failed");
        }
        return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                             queued);
    }

    if(tool_name == "event_details")
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

    if(tool_name == "track_statistics")
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

    if(tool_name == "search_events")
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
            ops.push_back(spec->op);
        }
        if(ops.empty())
        {
            ops = { kRocProfVisDmOperationLaunch, kRocProfVisDmOperationDispatch,
                    kRocProfVisDmOperationMemoryCopy,
                    kRocProfVisDmOperationMemoryAllocate,
                    kRocProfVisDmOperationLaunchSample };
        }

        const bool contains =
            ToLowerCopy(JsonUtils::GetString(args, "match", "contains")) != "equals";
        const bool any_term =
            ToLowerCopy(JsonUtils::GetString(args, "combine", "all")) == "any";
        const bool include_category =
            JsonUtils::GetBool(args, "include_category", false);

        std::string       query_error;
        const std::string where = BuildWhereClause(args, query_error);
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
            contains, include_category, any_term, terms, JsonU64(args, "offset", 0),
            static_cast<uint64_t>(limit),
            ResolveSortColumn(tables, TableType::kEventSearchTable,
                              JsonUtils::GetString(args, "sort_by", ""), 1),
            SortOrderFromArgs(args, kRPVControllerSortOrderAscending)));
        if(!queued && !context.data_provider->IsRequestPending(request_id))
        {
            return DoneResult("Could not queue the search.", "Fetch failed");
        }
        return PendingResult(request_id, fetch, AssistantToolStatusLabel(tool_name),
                             queued);
    }

    return DoneResult("Unknown tool. Use one of: " + AssistantToolNameList() + ".",
                      "Unknown tool");
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
