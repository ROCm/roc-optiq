// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_data_provider_json.h"

#include <algorithm>
#include <cmath>
#include <iterator>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "model/compute/rocprofvis_compute_data_model.h"
#include "model/rocprofvis_common_defs.h"
#include "model/rocprofvis_trace_data_model.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_requests.h"

namespace RocProfVis
{
namespace View
{
namespace DataProviderJson
{

using roofline_ceiling_t   = WorkloadInfo::Roofline::Ceiling;
using roofline_intensity_t = KernelInfo::Roofline::Intensity;

constexpr uint64_t JAVASCRIPT_MAX_SAFE_INTEGER = (1ULL << 53) - 1;

constexpr const char* PROVIDER_STATE_NAMES[] = { "init", "loading", "ready", "error" };
static_assert(std::size(PROVIDER_STATE_NAMES) ==
                  static_cast<size_t>(ProviderState::kError) + 1,
              "Name every ProviderState");

constexpr const char* TRACE_KIND_NAMES[] = { "system", "compute" };
static_assert(std::size(TRACE_KIND_NAMES) == static_cast<size_t>(TraceKind::kCompute) + 1,
              "Name every TraceKind");

constexpr const char* TRACK_DATA_TYPE_NAMES[] = { "samples", "events" };
static_assert(std::size(TRACK_DATA_TYPE_NAMES) ==
                  static_cast<size_t>(kRPVControllerTrackTypeEvents) + 1,
              "Name every rocprofvis_controller_track_type_t");

constexpr const char* TRACK_KIND_NAMES[] = { "unknown",        "queue",
                                             "stream",         "instrumented_thread",
                                             "sampled_thread", "counter" };
static_assert(std::size(TRACK_KIND_NAMES) == static_cast<size_t>(TrackInfo::Count),
              "Name every TrackInfo::TrackType");

constexpr const char* OPERATION_NAMES[] = { "none",        "launch",
                                            "dispatch",    "memory_allocate",
                                            "memory_copy", "launch_sample",
                                            "hip_event" };
static_assert(std::size(OPERATION_NAMES) ==
                  static_cast<size_t>(kRocProfVisDmNumOperation),
              "Name every rocprofvis_dm_event_operation_t");

constexpr const char* PRIMITIVE_TYPE_NAMES[] = { "uint64", "double", "string", "object" };
static_assert(std::size(PRIMITIVE_TYPE_NAMES) ==
                  static_cast<size_t>(kRPVControllerPrimitiveTypeObject) + 1,
              "Name every rocprofvis_controller_primitive_type_t");

constexpr const char* SORT_ORDER_NAMES[] = { "ascending", "descending" };
static_assert(std::size(SORT_ORDER_NAMES) ==
                  static_cast<size_t>(kRPVControllerSortOrderDescending) + 1,
              "Name every rocprofvis_controller_sort_order_t");

constexpr const char* TOPOLOGY_NODE_TYPE_NAMES[] = { "root",    "node",   "processor",
                                                     "process", "thread", "stream",
                                                     "queue",   "counter" };
static_assert(std::size(TOPOLOGY_NODE_TYPE_NAMES) ==
                  static_cast<size_t>(TopologyNodeType::kCounter) + 1,
              "Name every TopologyNodeType");

constexpr const char* PROCESSOR_TYPE_NAMES[] = { "undefined", "gpu", "cpu", "nic" };
static_assert(std::size(PROCESSOR_TYPE_NAMES) ==
                  static_cast<size_t>(kRPVControllerProcessorTypeNIC) + 1,
              "Name every rocprofvis_controller_processor_type_t");

constexpr const char* THREAD_KIND_NAMES[] = { "instrumented", "sampled" };
static_assert(std::size(THREAD_KIND_NAMES) ==
                  static_cast<size_t>(ThreadInfo::Kind::kSampled) + 1,
              "Name every ThreadInfo::Kind");

constexpr const char* AGGREGATION_LEVEL_NAMES[] = { "trace", "node", "processor",
                                                    "process", "thread" };
static_assert(std::size(AGGREGATION_LEVEL_NAMES) ==
                  static_cast<size_t>(__kRPVAnalysisSummaryAggregationLevelLast),
              "Name every rocprofvis_analysis_summary_aggregation_level_t");

constexpr const char* SUMMARY_CATEGORY_NAMES[] = { "kernel", "memory", "thread" };
static_assert(std::size(SUMMARY_CATEGORY_NAMES) ==
                  static_cast<size_t>(__kRPVAnalysisSummaryCategoryLast),
              "Name every rocprofvis_analysis_summary_category_t");

constexpr const char* TOP_EVENTS_METRIC_NAMES[] = { "num_top_events", "invocations",
                                                    "duration_sum_ns" };
static_assert(std::size(TOP_EVENTS_METRIC_NAMES) ==
                  static_cast<size_t>(__kRPVAnalysisSummaryTopEventsMetricLast),
              "Name every rocprofvis_analysis_summary_top_events_metric_t");

constexpr const char* TOP_EVENT_METRIC_NAMES[] = { "invocations",     "duration_sum_ns",
                                                   "duration_min_ns", "duration_max_ns",
                                                   "duration_pct",    "duration_avg_ns" };
static_assert(std::size(TOP_EVENT_METRIC_NAMES) ==
                  static_cast<size_t>(__kRPVAnalysisSummaryTopEventMetricLast),
              "Name every rocprofvis_analysis_summary_top_event_metric_t");

constexpr const char* COUNTER_METRIC_NAMES[] = { "min", "max", "mean", "std_dev" };
static_assert(std::size(COUNTER_METRIC_NAMES) ==
                  static_cast<size_t>(__kRPVAnalysisSummaryCounterMetricLast),
              "Name every rocprofvis_analysis_summary_counter_metric_t");

constexpr const char* STATISTICS_STATE_NAMES[] = { "stale", "pending", "requested",
                                                   "ready" };
static_assert(std::size(STATISTICS_STATE_NAMES) ==
                  static_cast<size_t>(AnalysisTrackStatistics::kReady) + 1,
              "Name every AnalysisTrackStatistics::State");

constexpr const char* METRIC_SOURCE_NAMES[] = { "workload", "kernel" };
static_assert(std::size(METRIC_SOURCE_NAMES) ==
                  static_cast<size_t>(kRPVControllerMetricSourceTypeKernel) + 1,
              "Name every rocprofvis_controller_metric_source_type_t");

constexpr const char* MEMORY_CHART_ARROW_DIRECTION_NAMES[] = { "forward", "backward",
                                                               "both" };
static_assert(std::size(MEMORY_CHART_ARROW_DIRECTION_NAMES) ==
                  static_cast<size_t>(MemChartArrowDir::kBoth) + 1,
              "Name every MemChartArrowDir");

constexpr const char* DISPATCH_METRIC_NAMES[] = {
    "invocation_count", "duration_total_ns", "duration_min_ns",
    "duration_max_ns",  "duration_mean_ns",  "duration_median_ns"
};
static_assert(std::size(DISPATCH_METRIC_NAMES) ==
                  static_cast<size_t>(KernelInfo::NumMetrics),
              "Name every KernelInfo::DispatchMetric");

constexpr const char* ROOFLINE_COMPUTE_NAMES[] = { "mfma_fp4",  "mfma_fp6",  "mfma_fp8",
                                                   "valu_i8",   "mfma_i8",   "valu_fp16",
                                                   "mfma_fp16", "mfma_bf16", "valu_fp32",
                                                   "mfma_fp32", "valu_i32",  "valu_fp64",
                                                   "mfma_fp64", "valu_i64" };
static_assert(std::size(ROOFLINE_COMPUTE_NAMES) ==
                  static_cast<size_t>(__KRPVControllerRooflineCeilingComputeTypeLast),
              "Name every rocprofvis_controller_roofline_ceiling_compute_type_t");

constexpr const char* ROOFLINE_BANDWIDTH_NAMES[] = { "hbm", "l2", "l1", "lds" };
static_assert(std::size(ROOFLINE_BANDWIDTH_NAMES) ==
                  static_cast<size_t>(__KRPVControllerRooflineCeilingBandwidthTypeLast),
              "Name every rocprofvis_controller_roofline_ceiling_bandwidth_type_t");

constexpr const char* ROOFLINE_INTENSITY_NAMES[] = { "hbm", "l2", "l1", "lds" };
static_assert(std::size(ROOFLINE_INTENSITY_NAMES) ==
                  static_cast<size_t>(__kRPVControllerRooflineKernelIntensityTypeLast),
              "Name every rocprofvis_controller_roofline_kernel_intensity_type_t");

struct TableSlotName
{
    TableType   type;
    const char* name;
};

// Clients address table slots by these names, so an entry is never renamed.
constexpr TableSlotName TABLE_SLOT_NAMES[] = {
    { TableType::kSampleTable, "sample_table" },
    { TableType::kEventTable, "event_table" },
    { TableType::kCompareEventTableA, "compare_event_table_a" },
    { TableType::kCompareEventTableB, "compare_event_table_b" },
    { TableType::kCompareSampleTableA, "compare_sample_table_a" },
    { TableType::kCompareSampleTableB, "compare_sample_table_b" },
    { TableType::kEventSearchTable, "event_search_table" },
    { TableType::kSummaryKernelTable, "summary_kernel_table" },
    { TableType::kSummaryMemoryTable, "summary_memory_table" },
    { TableType::kSummaryThreadTable, "summary_thread_table" },
    { TableType::kAnalysisTopInstrumentedEventsTable, "top_instrumented_events_table" },
    { TableType::kAnalysisTopDispatchEventsTable, "top_dispatch_events_table" },
    { TableType::kAnalysisTopMemoryAllocationEventsTable,
      "top_memory_allocation_events_table" },
    { TableType::kAnalysisTopMemoryCopyEventsTable, "top_memory_copy_events_table" },
    { TableType::kAnalysisTopSampledEventsTable, "top_sampled_events_table" },
    { TableType::kAnalysisTopHipEventsTable, "top_hip_events_table" },
    { TableType::kAnalysisTopInstrumentedEventsTableB,
      "top_instrumented_events_table_b" },
    { TableType::kAnalysisTopDispatchEventsTableB, "top_dispatch_events_table_b" },
    { TableType::kAnalysisTopMemoryAllocationEventsTableB,
      "top_memory_allocation_events_table_b" },
    { TableType::kAnalysisTopMemoryCopyEventsTableB, "top_memory_copy_events_table_b" },
    { TableType::kAnalysisTopSampledEventsTableB, "top_sampled_events_table_b" },
    { TableType::kAnalysisTopHipEventsTableB, "top_hip_events_table_b" },
    { TableType::kAssistantEventTable, "assistant_event_table" },
    { TableType::kAssistantSampleTable, "assistant_sample_table" },
    { TableType::kAssistantSearchTable, "assistant_search_table" },
    { TableType::kAssistantSummaryKernelTable, "assistant_summary_kernel_table" },
    { TableType::kAssistantTopEventsTable, "assistant_top_events_table" },
    { TableType::kAssistantSummaryMemoryTable, "assistant_summary_memory_table" },
    { TableType::kAssistantSummaryThreadTable, "assistant_summary_thread_table" },
};

static constexpr bool
TableSlotNamesFollowEnumOrder()
{
    bool in_order = true;
    for(size_t i = 0; i < std::size(TABLE_SLOT_NAMES); i++)
    {
        in_order = in_order && TABLE_SLOT_NAMES[i].type == static_cast<TableType>(i);
    }
    return in_order;
}

static_assert(std::size(TABLE_SLOT_NAMES) ==
                  static_cast<size_t>(TableType::__kTableTypeCount),
              "Name every TableType");
static_assert(TableSlotNamesFollowEnumOrder(),
              "TABLE_SLOT_NAMES must list TableType in declaration order");

template <size_t N>
static const char*
EnumName(const char* const (&names)[N], size_t value)
{
    return value < N ? names[value] : "unknown";
}

static jt::Json
IdJson(uint64_t id)
{
    return jt::Json(std::to_string(id));
}

static jt::Json
UInt64Json(uint64_t value)
{
    return value <= JAVASCRIPT_MAX_SAFE_INTEGER ? jt::Json(value) : IdJson(value);
}

static jt::Json
OptionalIdJson(const c_optional_uint64_t& id)
{
    return id.has_value ? IdJson(id.value) : jt::Json();
}

static jt::Json
OptionalCountJson(const std::optional<uint64_t>& count)
{
    return count ? UInt64Json(*count) : jt::Json();
}

// Request params use INVALID_UINT64_INDEX for "no limit".
static jt::Json
RowCountJson(uint64_t count)
{
    return count == INVALID_UINT64_INDEX ? jt::Json() : UInt64Json(count);
}

static jt::Json
EmptyArray()
{
    jt::Json array;
    array.setArray();
    return array;
}

static jt::Json
FiniteDoubleJson(double value)
{
    return std::isfinite(value) ? jt::Json(value) : jt::Json();
}

static jt::Json
StringsJson(const std::vector<std::string>& strings)
{
    jt::Json array = EmptyArray();
    for(const std::string& value : strings)
    {
        array.getArray().push_back(value);
    }
    return array;
}

static jt::Json
StringRowsJson(const std::vector<std::vector<std::string>>& rows)
{
    jt::Json array = EmptyArray();
    array.getArray().reserve(rows.size());
    for(const std::vector<std::string>& row : rows)
    {
        array.getArray().push_back(StringsJson(row));
    }
    return array;
}

static jt::Json
DoublesJson(const std::vector<double>& values)
{
    jt::Json array = EmptyArray();
    array.getArray().reserve(values.size());
    for(double value : values)
    {
        array.getArray().push_back(FiniteDoubleJson(value));
    }
    return array;
}

static jt::Json
IdsJson(const std::vector<uint64_t>& ids)
{
    jt::Json array = EmptyArray();
    for(uint64_t id : ids)
    {
        array.getArray().push_back(IdJson(id));
    }
    return array;
}

static jt::Json
OperationsJson(const std::vector<rocprofvis_dm_event_operation_t>& operations)
{
    jt::Json array = EmptyArray();
    for(rocprofvis_dm_event_operation_t operation : operations)
    {
        array.getArray().push_back(
            EnumName(OPERATION_NAMES, static_cast<size_t>(operation)));
    }
    return array;
}

static jt::Json
PointJson(const Point& point)
{
    jt::Json json;
    json["x"] = FiniteDoubleJson(point.x);
    json["y"] = FiniteDoubleJson(point.y);
    return json;
}

static jt::Json
CompareSourceJson(const CompareSourceInfo& source)
{
    jt::Json json;
    json["id"]   = source.id;
    json["name"] = source.name;
    json["path"] = source.path;
    return json;
}

static jt::Json
CompareSourcesJson(const TraceDataModel& model)
{
    jt::Json                 sources = EmptyArray();
    size_t                   index   = 0;
    const CompareSourceInfo* source  = model.GetCompareSource(index);
    while(source)
    {
        sources.getArray().push_back(CompareSourceJson(*source));
        index++;
        source = model.GetCompareSource(index);
    }
    return sources;
}

static jt::Json
TrackJson(const TraceDataModel& model, const TrackInfo& track)
{
    std::vector<rocprofvis_dm_event_operation_t> operations(track.operation_types.begin(),
                                                            track.operation_types.end());
    std::sort(operations.begin(), operations.end());

    jt::Json topology;
    topology["node_id"]    = IdJson(track.topology.node_id);
    topology["process_id"] = IdJson(track.topology.process_id);
    topology["device_id"]  = IdJson(track.topology.device_id);
    topology["id"]         = IdJson(track.topology.id.value);

    jt::Json json;
    json["id"]    = IdJson(track.id);
    json["index"] = UInt64Json(track.index);
    json["name"]  = model.BuildTrackName(track.id);
    json["data_type"] =
        EnumName(TRACK_DATA_TYPE_NAMES, static_cast<size_t>(track.track_type));
    json["kind"] = EnumName(TRACK_KIND_NAMES, static_cast<size_t>(track.topology.type));
    json["category"]        = track.category;
    json["main_name"]       = track.main_name;
    json["sub_name"]        = track.sub_name;
    json["start_ns"]        = FiniteDoubleJson(track.min_ts);
    json["end_ns"]          = FiniteDoubleJson(track.max_ts);
    json["entry_count"]     = UInt64Json(track.num_entries);
    json["min_value"]       = FiniteDoubleJson(track.min_value);
    json["max_value"]       = FiniteDoubleJson(track.max_value);
    json["operations"]      = OperationsJson(operations);
    json["instance_id"]     = UInt64Json(track.instance_id);
    json["file_id"]         = UInt64Json(track.file_id);
    json["order_rank"]      = UInt64Json(track.order_rank);
    json["agent_or_pid"]    = IdJson(track.agent_or_pid);
    json["queue_id_or_tid"] = IdJson(track.queue_id_or_tid);
    json["topology"]        = std::move(topology);
    json["compare_source"]  = track.compare_source.id.empty()
                                  ? jt::Json()
                                  : CompareSourceJson(track.compare_source);
    return json;
}

static jt::Json
TraceEventJson(const TraceEvent& event)
{
    jt::Json json;
    json["uuid"] = IdJson(event.m_id.uuid);
    json["op"] =
        EnumName(OPERATION_NAMES, static_cast<size_t>(event.m_id.bitfield.event_op));
    json["name"]              = event.m_name;
    json["start_ns"]          = FiniteDoubleJson(event.m_start_ts);
    json["duration_ns"]       = FiniteDoubleJson(event.m_duration);
    json["level"]             = event.m_level;
    json["child_count"]       = event.m_child_count;
    json["top_combined_name"] = event.m_top_combined_name;
    return json;
}

static jt::Json
TraceCounterJson(const TraceCounter& counter)
{
    jt::Json json;
    json["start_ns"] = FiniteDoubleJson(counter.m_start_ts);
    json["end_ns"]   = FiniteDoubleJson(counter.m_end_ts);
    json["value"]    = FiniteDoubleJson(counter.m_value);
    return json;
}

static bool
IsEventTrack(const TimelineModel& timeline, uint64_t track_id)
{
    const TrackInfo* track = timeline.GetTrack(track_id);
    return track && track->track_type == kRPVControllerTrackTypeEvents;
}

static jt::Json
TopologyNodeJson(const TopologyTree& topology, const TopologyNode& node);

static void
AddTopologyChildren(const TopologyTree& topology, const TopologyNode& node,
                    TopologyNodeType child_type, const char* key, jt::Json& json)
{
    jt::Json children = EmptyArray();
    for(const TopologyNode* child : node.GetChildren(child_type))
    {
        children.getArray().push_back(TopologyNodeJson(topology, *child));
    }
    json[key] = std::move(children);
}

static void
AddStreamLinks(const TopologyNode& stream, jt::Json& json)
{
    jt::Json processors = EmptyArray();
    for(const TopologyNode* processor :
        stream.GetLinkedChildren(TopologyNodeType::kProcessor))
    {
        processors.getArray().push_back(IdJson(processor->GetId()));
    }

    // Queue ids are only unique within their processor.
    jt::Json queues = EmptyArray();
    for(const TopologyNode* queue : stream.GetLinkedChildren(TopologyNodeType::kQueue))
    {
        jt::Json link;
        link["processor_id"] =
            IdJson(static_cast<const QueueInfo*>(queue)->GetProcessorId());
        link["id"]       = IdJson(queue->GetId());
        link["track_id"] = queue->HasTrack() ? IdJson(queue->GetTrackId()) : jt::Json();
        queues.getArray().push_back(std::move(link));
    }

    json["linked_processor_ids"] = std::move(processors);
    json["linked_queues"]        = std::move(queues);
}

static jt::Json
TopologyNodeJson(const TopologyTree& topology, const TopologyNode& node)
{
    jt::Json json;
    json["type"] =
        EnumName(TOPOLOGY_NODE_TYPE_NAMES, static_cast<size_t>(node.GetNodeType()));
    json["id"]       = IdJson(node.GetId());
    json["name"]     = node.GetName();
    json["header"]   = node.GetHeader();
    json["track_id"] = node.HasTrack() ? IdJson(node.GetTrackId()) : jt::Json();
    switch(node.GetNodeType())
    {
        case TopologyNodeType::kNode:
        {
            const NodeInfo& info  = static_cast<const NodeInfo&>(node);
            json["host_name"]     = info.host_name;
            json["os_name"]       = info.os_name;
            json["os_release"]    = info.os_release;
            json["os_version"]    = info.os_version;
            json["display_index"] = UInt64Json(static_cast<uint64_t>(info.display_index));
            AddTopologyChildren(topology, node, TopologyNodeType::kProcessor,
                                "processors", json);
            AddTopologyChildren(topology, node, TopologyNodeType::kProcess, "processes",
                                json);
            break;
        }
        case TopologyNodeType::kProcessor:
        {
            const ProcessorInfo& info = static_cast<const ProcessorInfo&>(node);
            std::string          label;
            json["product_name"] = info.product_name;
            json["processor_type"] =
                EnumName(PROCESSOR_TYPE_NAMES, static_cast<size_t>(info.type));
            json["type_index"] = UInt64Json(info.type_index);
            json["label"] = topology.GetProcessorTypeLabel(info, label) ? jt::Json(label)
                                                                        : jt::Json();
            AddTopologyChildren(topology, node, TopologyNodeType::kQueue, "queues", json);
            AddTopologyChildren(topology, node, TopologyNodeType::kCounter, "counters",
                                json);
            break;
        }
        case TopologyNodeType::kProcess:
        {
            const ProcessInfo& info = static_cast<const ProcessInfo&>(node);
            json["pid"]             = IdJson(info.pid);
            json["start_ns"]        = FiniteDoubleJson(info.start_time);
            json["end_ns"]          = FiniteDoubleJson(info.end_time);
            json["command"]         = info.command;
            json["environment"]     = info.environment;
            AddTopologyChildren(topology, node, TopologyNodeType::kThread, "threads",
                                json);
            AddTopologyChildren(topology, node, TopologyNodeType::kStream, "streams",
                                json);
            break;
        }
        case TopologyNodeType::kThread:
        {
            const ThreadInfo& info = static_cast<const ThreadInfo&>(node);
            json["kind"] = EnumName(THREAD_KIND_NAMES, static_cast<size_t>(info.kind));
            json["tid"]  = IdJson(info.tid);
            json["start_ns"] = FiniteDoubleJson(info.start_time);
            json["end_ns"]   = FiniteDoubleJson(info.end_time);
            break;
        }
        case TopologyNodeType::kStream:
        {
            AddStreamLinks(node, json);
            break;
        }
        case TopologyNodeType::kQueue:
        {
            json["processor_id"] =
                IdJson(static_cast<const QueueInfo&>(node).GetProcessorId());
            break;
        }
        case TopologyNodeType::kCounter:
        {
            const CounterInfo& info = static_cast<const CounterInfo&>(node);
            json["processor_id"]    = IdJson(info.GetProcessorId());
            json["description"]     = info.description;
            json["units"]           = info.units;
            json["value_type"]      = info.value_type;
            break;
        }
        case TopologyNodeType::kRoot:
        {
            break;
        }
    }
    return json;
}

// Must agree with DataProvider::ClientTableSlot, which decides the model each
// fetch writes its slot into.
static const TablesModel&
TablesFor(const TraceDataModel& model, TableType type)
{
    const TablesModel* tables = &model.GetTables();
    switch(type)
    {
        case TableType::kSummaryKernelTable:
        case TableType::kSummaryMemoryTable:
        case TableType::kSummaryThreadTable:
        {
            tables = &model.GetSummary().GetTables();
            break;
        }
        case TableType::kAnalysisTopInstrumentedEventsTable:
        case TableType::kAnalysisTopDispatchEventsTable:
        case TableType::kAnalysisTopMemoryAllocationEventsTable:
        case TableType::kAnalysisTopMemoryCopyEventsTable:
        case TableType::kAnalysisTopSampledEventsTable:
        case TableType::kAnalysisTopHipEventsTable:
        case TableType::kAnalysisTopInstrumentedEventsTableB:
        case TableType::kAnalysisTopDispatchEventsTableB:
        case TableType::kAnalysisTopMemoryAllocationEventsTableB:
        case TableType::kAnalysisTopMemoryCopyEventsTableB:
        case TableType::kAnalysisTopSampledEventsTableB:
        case TableType::kAnalysisTopHipEventsTableB:
        {
            tables = &model.GetAnalysis().GetTables();
            break;
        }
        default:
        {
            break;
        }
    }
    return *tables;
}

static jt::Json
TableColumnsJson(const std::vector<std::string>&                            header,
                 const std::vector<rocprofvis_controller_primitive_type_t>& types)
{
    jt::Json columns = EmptyArray();
    for(size_t i = 0; i < header.size(); i++)
    {
        jt::Json column;
        column["name"] = header[i];
        if(i < types.size())
        {
            column["type"] =
                EnumName(PRIMITIVE_TYPE_NAMES, static_cast<size_t>(types[i]));
        }
        columns.getArray().push_back(std::move(column));
    }
    return columns;
}

static jt::Json
QueryCriteriaJson(const rocprofvis_dm_query_criteria_t& criteria)
{
    jt::Json json;
    json["node_id"]    = OptionalIdJson(criteria.node_id);
    json["agent_id"]   = OptionalIdJson(criteria.agent_id);
    json["process_id"] = OptionalIdJson(criteria.process_id);
    json["thread_id"]  = OptionalIdJson(criteria.thread_id);
    return json;
}

static jt::Json
TableRequestJson(const TableRequestParams& params)
{
    jt::Json json;
    json["start_ns"]    = FiniteDoubleJson(params.m_start_ts);
    json["end_ns"]      = FiniteDoubleJson(params.m_end_ts);
    json["start_row"]   = RowCountJson(params.m_start_row);
    json["row_count"]   = RowCountJson(params.m_req_row_count);
    json["sort_column"] = UInt64Json(params.m_sort_column_index);
    json["sort_order"] =
        EnumName(SORT_ORDER_NAMES, static_cast<size_t>(params.m_sort_order));
    json["filter"]        = params.m_filter;
    json["group"]         = params.m_group;
    json["group_columns"] = params.m_group_columns;
    json["client_id"]     = IdJson(params.m_client_id);
    json["source_filter"] =
        params.m_source_filter ? QueryCriteriaJson(*params.m_source_filter) : jt::Json();

    const TrackTableRequestParams* track_params =
        dynamic_cast<const TrackTableRequestParams*>(&params);
    if(track_params)
    {
        json["track_ids"] = IdsJson(track_params->m_track_ids);
    }

    const EventSearchRequestParams* search_params =
        dynamic_cast<const EventSearchRequestParams*>(&params);
    if(search_params)
    {
        jt::Json search;
        search["terms"]              = StringsJson(search_params->m_string_table_filters);
        search["operations"]         = OperationsJson(search_params->m_op_types);
        search["include_substrings"] = search_params->m_include_substrings;
        search["include_category"]   = search_params->m_include_category;
        search["partial_matching"]   = search_params->m_partial_matching;
        json["search"]               = std::move(search);
    }
    return json;
}

static jt::Json
EventInfoJson(const EventInfo& event)
{
    const BasicEventData& basic = event.basic_info;

    jt::Json args = EmptyArray();
    for(const EventArg& arg : event.args)
    {
        jt::Json entry;
        entry["position"]  = arg.position;
        entry["name"]      = arg.name;
        entry["value"]     = arg.value;
        entry["data_type"] = arg.data_type;
        args.getArray().push_back(std::move(entry));
    }

    jt::Json ext = EmptyArray();
    for(const EventExtData& data : event.ext_info)
    {
        jt::Json entry;
        entry["category"]      = data.category;
        entry["name"]          = data.name;
        entry["value"]         = data.value;
        entry["category_enum"] = UInt64Json(data.category_enum);
        ext.getArray().push_back(std::move(entry));
    }

    jt::Json flows = EmptyArray();
    for(const EventFlowData& flow : event.flow_info)
    {
        jt::Json entry;
        entry["uuid"] = IdJson(flow.id.uuid);
        entry["op"] =
            EnumName(OPERATION_NAMES, static_cast<size_t>(flow.id.bitfield.event_op));
        entry["name"]      = flow.name;
        entry["track_id"]  = IdJson(flow.track_id);
        entry["start_ns"]  = UInt64Json(flow.start_timestamp);
        entry["end_ns"]    = UInt64Json(flow.end_timestamp);
        entry["level"]     = UInt64Json(flow.level);
        entry["direction"] = UInt64Json(flow.direction);
        flows.getArray().push_back(std::move(entry));
    }

    jt::Json call_stack = EmptyArray();
    for(const CallStackData& frame : event.call_stack_info)
    {
        jt::Json entry;
        entry["uuid"]    = IdJson(frame.id.uuid);
        entry["name"]    = frame.name;
        entry["file"]    = frame.file;
        entry["pc"]      = frame.pc;
        entry["address"] = frame.address;
        call_stack.getArray().push_back(std::move(entry));
    }

    jt::Json json;
    json["uuid"] = IdJson(basic.id.uuid);
    json["op"] =
        EnumName(OPERATION_NAMES, static_cast<size_t>(basic.id.bitfield.event_op));
    json["track_id"]     = IdJson(event.track_id);
    json["name"]         = basic.name;
    json["start_ns"]     = FiniteDoubleJson(basic.start_ts);
    json["duration_ns"]  = FiniteDoubleJson(basic.duration);
    json["level"]        = basic.level;
    json["stream_level"] = basic.stream_level;
    json["args"]         = std::move(args);
    json["ext"]          = std::move(ext);
    json["flows"]        = std::move(flows);
    json["call_stack"]   = std::move(call_stack);
    return json;
}

static jt::Json
SummaryValueJson(const SummaryInfo::Value& value)
{
    jt::Json json;
    switch(value.type)
    {
        case SummaryInfo::Value::UInt:
        {
            json = UInt64Json(value.numeric.uint);
            break;
        }
        case SummaryInfo::Value::Double:
        {
            json = FiniteDoubleJson(value.numeric.dbl);
            break;
        }
        case SummaryInfo::Value::Float:
        {
            json = FiniteDoubleJson(static_cast<double>(value.numeric.flt));
            break;
        }
    }
    return json;
}

static jt::Json
TopOpEventsJson(const SummaryInfo::TopOpEvents& events)
{
    jt::Json top = EmptyArray();
    for(const SummaryInfo::TopOpEvents::Event& event : events.top)
    {
        jt::Json entry;
        entry["name"] = event.name;
        for(size_t i = 0; i < std::size(TOP_EVENT_METRIC_NAMES); i++)
        {
            entry[TOP_EVENT_METRIC_NAMES[i]] = SummaryValueJson(event.metrics[i]);
        }
        top.getArray().push_back(std::move(entry));
    }

    jt::Json json;
    for(size_t i = 0; i < std::size(TOP_EVENTS_METRIC_NAMES); i++)
    {
        json[TOP_EVENTS_METRIC_NAMES[i]] = SummaryValueJson(events.metrics[i]);
    }
    json["top"] = std::move(top);
    return json;
}

static void
AppendCountersJson(const SummaryInfo::Counters& counters, const char* device,
                   jt::Json& out)
{
    for(const SummaryInfo::Counters::Counter& counter : counters.list)
    {
        if(counter.info)
        {
            jt::Json json;
            json["device"]     = device;
            json["id"]         = IdJson(counter.info->GetId());
            json["name"]       = counter.info->GetName();
            json["units"]      = counter.info->units;
            json["value_type"] = counter.info->value_type;
            for(size_t i = 0; i < std::size(COUNTER_METRIC_NAMES); i++)
            {
                json[COUNTER_METRIC_NAMES[i]] = SummaryValueJson(counter.metrics[i]);
            }
            out.getArray().push_back(std::move(json));
        }
    }
}

static jt::Json
AggregateScopeIdJson(const SummaryInfo::AggregateMetrics& metrics)
{
    const TopologyNode* scope = nullptr;
    switch(metrics.type)
    {
        case kRPVAnalysisSummaryAggregationLevelNode:
        {
            scope = metrics.info.node;
            break;
        }
        case kRPVAnalysisSummaryAggregationLevelProcessor:
        {
            scope = metrics.info.processor;
            break;
        }
        case kRPVAnalysisSummaryAggregationLevelProcess:
        {
            scope = metrics.info.process;
            break;
        }
        case kRPVAnalysisSummaryAggregationLevelThread:
        {
            scope = metrics.info.thread;
            break;
        }
        default:
        {
            break;
        }
    }
    return scope ? IdJson(scope->GetId()) : jt::Json();
}

static jt::Json
AggregateJson(const SummaryModel& summary, const SummaryInfo::AggregateMetrics& metrics)
{
    jt::Json top_events;
    for(size_t i = 0; i < std::size(SUMMARY_CATEGORY_NAMES); i++)
    {
        const SummaryInfo::TopOpEvents* events = summary.TopOpEvents(
            metrics, static_cast<rocprofvis_analysis_summary_category_t>(i));
        top_events[SUMMARY_CATEGORY_NAMES[i]] =
            events ? TopOpEventsJson(*events) : jt::Json();
    }

    jt::Json counters = EmptyArray();
    AppendCountersJson(metrics.gpu.counters, "gpu", counters);
    AppendCountersJson(metrics.cpu.counters, "cpu", counters);

    jt::Json children = EmptyArray();
    for(const SummaryInfo::AggregateMetrics& child : metrics.sub_metrics)
    {
        children.getArray().push_back(AggregateJson(summary, child));
    }

    jt::Json json;
    json["level"] = EnumName(AGGREGATION_LEVEL_NAMES, static_cast<size_t>(metrics.type));
    json["id"]    = AggregateScopeIdJson(metrics);
    json["top_events"] = std::move(top_events);
    json["counters"]   = std::move(counters);
    json["children"]   = std::move(children);
    return json;
}

static jt::Json
MetricIdJson(uint32_t category_id, uint32_t table_id, uint32_t entry_id)
{
    MetricId metric_id;
    metric_id.category_id = category_id;
    metric_id.table_id    = table_id;
    metric_id.entry_id    = entry_id;
    return jt::Json(metric_id.ToString());
}

static jt::Json
AvailableMetricsJson(const AvailableMetrics& metrics)
{
    jt::Json categories = EmptyArray();
    for(const AvailableMetrics::Category* category : metrics.ordered_categories)
    {
        jt::Json tables = EmptyArray();
        for(const AvailableMetrics::Table* table : category->ordered_tables)
        {
            jt::Json entries = EmptyArray();
            for(const AvailableMetrics::Entry* entry : table->ordered_entries)
            {
                jt::Json entry_json;
                entry_json["metric_id"] =
                    MetricIdJson(entry->category_id, entry->table_id, entry->id);
                entry_json["id"]          = entry->id;
                entry_json["name"]        = entry->name;
                entry_json["description"] = entry->description;
                entry_json["unit"]        = entry->unit;
                entries.getArray().push_back(std::move(entry_json));
            }
            jt::Json table_json;
            table_json["id"]          = table->id;
            table_json["name"]        = table->name;
            table_json["value_names"] = StringsJson(table->value_names);
            table_json["entries"]     = std::move(entries);
            tables.getArray().push_back(std::move(table_json));
        }
        jt::Json category_json;
        category_json["id"]     = category->id;
        category_json["name"]   = category->name;
        category_json["tables"] = std::move(tables);
        categories.getArray().push_back(std::move(category_json));
    }
    return categories;
}

static jt::Json
MemoryChartMetricJson(const MemChartMetricRef& metric)
{
    jt::Json json;
    json["valid"]       = metric.valid;
    json["metric_id"]   = metric.valid ? jt::Json(metric.metric_id) : jt::Json();
    json["metric_name"] = metric.metric_name;
    return json;
}

static jt::Json
MemoryChartBlockJson(const MemChartBlock& block)
{
    jt::Json content = EmptyArray();
    for(const MemChartContentItem& item : block.content)
    {
        jt::Json json;
        json["metric"]   = MemoryChartMetricJson(item.metric);
        json["title"]    = item.title;
        json["category"] = item.category;
        json["unit"]     = item.unit;
        content.getArray().push_back(std::move(json));
    }

    jt::Json children = EmptyArray();
    for(const MemChartBlock& child : block.children)
    {
        children.getArray().push_back(MemoryChartBlockJson(child));
    }

    jt::Json json;
    json["id"]       = block.id;
    json["column"]   = block.column;
    json["row"]      = block.row;
    json["order"]    = block.order;
    json["title"]    = block.title;
    json["content"]  = std::move(content);
    json["children"] = std::move(children);
    return json;
}

static jt::Json
MemoryChartJson(const MemChartLayout& layout)
{
    jt::Json blocks = EmptyArray();
    for(const MemChartBlock& block : layout.blocks)
    {
        blocks.getArray().push_back(MemoryChartBlockJson(block));
    }

    jt::Json arrows = EmptyArray();
    for(const MemChartArrow& arrow : layout.arrows)
    {
        jt::Json json;
        json["from"]      = arrow.from;
        json["to"]        = arrow.to;
        json["direction"] = EnumName(MEMORY_CHART_ARROW_DIRECTION_NAMES,
                                     static_cast<size_t>(arrow.direction));
        json["metric"]    = MemoryChartMetricJson(arrow.metric);
        json["title"]     = arrow.title;
        json["category"]  = arrow.category;
        arrows.getArray().push_back(std::move(json));
    }

    jt::Json json;
    json["version"] = layout.version;
    json["blocks"]  = std::move(blocks);
    json["arrows"]  = std::move(arrows);
    return json;
}

static jt::Json
CeilingsJson(std::vector<const roofline_ceiling_t*>& ceilings)
{
    std::sort(ceilings.begin(), ceilings.end(),
              [](const roofline_ceiling_t* a, const roofline_ceiling_t* b) {
                  return std::make_pair(a->bandwidth_type, a->compute_type) <
                         std::make_pair(b->bandwidth_type, b->compute_type);
              });
    jt::Json array = EmptyArray();
    for(const roofline_ceiling_t* ceiling : ceilings)
    {
        jt::Json json;
        json["bandwidth"] = EnumName(ROOFLINE_BANDWIDTH_NAMES,
                                     static_cast<size_t>(ceiling->bandwidth_type));
        json["compute"] =
            EnumName(ROOFLINE_COMPUTE_NAMES, static_cast<size_t>(ceiling->compute_type));
        json["from"]       = PointJson(ceiling->position.p1);
        json["to"]         = PointJson(ceiling->position.p2);
        json["throughput"] = FiniteDoubleJson(ceiling->throughput);
        array.getArray().push_back(std::move(json));
    }
    return array;
}

static jt::Json
WorkloadRooflineJson(const WorkloadInfo::Roofline& roofline)
{
    std::vector<const roofline_ceiling_t*> bandwidth;
    for(const auto& by_bandwidth : roofline.ceiling_bandwidth)
    {
        for(const auto& by_compute : by_bandwidth.second)
        {
            bandwidth.push_back(&by_compute.second);
        }
    }
    std::vector<const roofline_ceiling_t*> compute;
    for(const auto& by_compute : roofline.ceiling_compute)
    {
        for(const auto& by_bandwidth : by_compute.second)
        {
            compute.push_back(&by_bandwidth.second);
        }
    }

    jt::Json json;
    json["bandwidth_ceilings"] = CeilingsJson(bandwidth);
    json["compute_ceilings"]   = CeilingsJson(compute);
    return json;
}

static jt::Json
MetricValueJson(const MetricValue& metric)
{
    jt::Json values;
    values.setObject();
    for(const std::pair<const std::string, double>& value : metric.values)
    {
        values[value.first] = FiniteDoubleJson(value.second);
    }

    jt::Json json;
    json["metric_id"] =
        MetricIdJson(metric.entry->category_id, metric.entry->table_id, metric.entry->id);
    json["name"] = metric.entry->name;
    json["unit"] = metric.entry->unit;
    json["source"] =
        EnumName(METRIC_SOURCE_NAMES, static_cast<size_t>(metric.source_type));
    json["values"] = std::move(values);
    return json;
}

static jt::Json
ComputeTableRequestJson(const ComputeTableRequestParams& params)
{
    jt::Json filters;
    filters.setObject();
    for(const std::pair<const uint64_t, std::string>& filter : params.m_column_filters)
    {
        filters[std::to_string(filter.first)] = filter.second;
    }

    jt::Json json;
    json["workload_id"]      = params.m_workload_id;
    json["metric_selectors"] = StringsJson(params.m_metric_selectors);
    json["sort_column"]      = UInt64Json(params.m_sort_column_index);
    json["sort_order"] =
        EnumName(SORT_ORDER_NAMES, static_cast<size_t>(params.m_sort_order));
    json["column_filters"] = std::move(filters);
    return json;
}

static jt::Json
CodeObjectsJson(const PcSamplingData& data)
{
    jt::Json code_objects = EmptyArray();
    for(const CodeObjectStore& code_object : data.code_objects)
    {
        jt::Json symbols = EmptyArray();
        for(const KernelSymbol& symbol : code_object.kernel_symbols)
        {
            jt::Json instructions = EmptyArray();
            instructions.getArray().reserve(symbol.instruction_lines.size());
            for(const InstructionLine& line : symbol.instruction_lines)
            {
                jt::Json instruction;
                instruction["uuid"]      = IdJson(line.instruction_uuid);
                instruction["type_uuid"] = line.instruction_type_uuid
                                               ? IdJson(*line.instruction_type_uuid)
                                               : jt::Json();
                instruction["offset"]    = UInt64Json(line.code_object_offset);
                instruction["text"]      = line.instruction;
                instructions.getArray().push_back(std::move(instruction));
            }
            jt::Json symbol_json;
            symbol_json["uuid"]             = IdJson(symbol.kernel_symbol_uuid);
            symbol_json["code_object_uuid"] = IdJson(symbol.code_object_uuid);
            symbol_json["instructions"]     = std::move(instructions);
            symbols.getArray().push_back(std::move(symbol_json));
        }
        jt::Json code_object_json;
        code_object_json["uuid"]           = IdJson(code_object.code_object_uuid);
        code_object_json["kernel_symbols"] = std::move(symbols);
        code_objects.getArray().push_back(std::move(code_object_json));
    }
    return code_objects;
}

static jt::Json
SourceFilesJson(const PcSamplingData& data)
{
    jt::Json files = EmptyArray();
    for(const SourceFile& file : data.source_files)
    {
        jt::Json lines = EmptyArray();
        lines.getArray().reserve(file.source_lines.size());
        for(const SourceLine& line : file.source_lines)
        {
            jt::Json line_json;
            line_json["uuid"]        = IdJson(line.source_line_uuid);
            line_json["line_number"] = UInt64Json(line.line_number);
            line_json["content"]     = line.content;
            lines.getArray().push_back(std::move(line_json));
        }
        jt::Json file_json;
        file_json["uuid"]  = IdJson(file.source_file_uuid);
        file_json["path"]  = file.file_path;
        file_json["lines"] = std::move(lines);
        files.getArray().push_back(std::move(file_json));
    }
    return files;
}

static jt::Json
SampleStatesJson(const PcSamplingData& data)
{
    jt::Json states = EmptyArray();
    states.getArray().reserve(data.pc_sample_states.size());
    for(const PcSampleState& state : data.pc_sample_states)
    {
        jt::Json json;
        json["uuid"]             = IdJson(state.pc_sample_state_uuid);
        json["instruction_uuid"] = IdJson(state.instruction_uuid);
        json["total_count"]      = UInt64Json(state.total_count);
        json["issue_count"]      = OptionalCountJson(state.issue_count);
        json["stall_count"]      = OptionalCountJson(state.stall_count);
        states.getArray().push_back(std::move(json));
    }
    return states;
}

jt::Json
Snapshot(DataProvider& provider, TraceKind kind)
{
    const TraceDataModel& model = provider.DataModel();

    jt::Json json;
    json["status"] = Status(provider, kind);
    if(kind == TraceKind::kSystem)
    {
        jt::Json system;
        system["timeline"]        = Timeline(model);
        system["histogram"]       = Histogram(model.GetTimeline());
        system["topology"]        = Topology(model.GetTopology());
        system["summary"]         = Summary(model);
        system["compare_sources"] = CompareSourcesJson(model);
        json["system"]            = std::move(system);
        json["compute"]           = jt::Json();
    }
    else
    {
        json["system"]  = jt::Json();
        json["compute"] = Compute(provider.ComputeModel());
    }
    return json;
}

const char*
TraceKindName(TraceKind kind)
{
    return EnumName(TRACE_KIND_NAMES, static_cast<size_t>(kind));
}

jt::Json
Status(DataProvider& provider, TraceKind kind)
{
    jt::Json json;
    json["state"] =
        EnumName(PROVIDER_STATE_NAMES, static_cast<size_t>(provider.GetState()));
    json["trace_type"]       = TraceKindName(kind);
    json["progress_message"] = provider.GetProgressMessage();
    json["trace_file_path"]  = provider.GetTraceFilePath();
    json["pending_requests"] =
        UInt64Json(static_cast<uint64_t>(provider.GetPendingRequestCount()));
    return json;
}

jt::Json
Timeline(const TraceDataModel& model)
{
    const TimelineModel& timeline = model.GetTimeline();

    jt::Json tracks = EmptyArray();
    for(const TrackInfo* track : timeline.GetTrackList())
    {
        if(track)
        {
            tracks.getArray().push_back(TrackJson(model, *track));
        }
    }

    jt::Json json;
    json["start_ns"] = FiniteDoubleJson(timeline.GetStartTime());
    json["end_ns"]   = FiniteDoubleJson(timeline.GetEndTime());
    json["tracks"]   = std::move(tracks);
    return json;
}

jt::Json
Track(const TraceDataModel& model, uint64_t track_id)
{
    const TrackInfo* track = model.GetTimeline().GetTrack(track_id);
    return track ? TrackJson(model, *track) : jt::Json();
}

jt::Json
TrackData(const TimelineModel& timeline, uint64_t track_id)
{
    jt::Json            json;
    const RawTrackData* data = timeline.GetTrackData(track_id);
    if(data)
    {
        json["track_id"] = IdJson(track_id);
        json["data_type"] =
            EnumName(TRACK_DATA_TYPE_NAMES, static_cast<size_t>(data->GetType()));
        json["start_ns"] = FiniteDoubleJson(data->GetStartTs());
        json["end_ns"]   = FiniteDoubleJson(data->GetEndTs());
        json["complete"] = data->AllDataReady();

        const RawTrackEventData*  events  = dynamic_cast<const RawTrackEventData*>(data);
        const RawTrackSampleData* samples = dynamic_cast<const RawTrackSampleData*>(data);
        if(events)
        {
            jt::Json list = EmptyArray();
            list.getArray().reserve(events->GetData().size());
            for(const TraceEvent& event : events->GetData())
            {
                list.getArray().push_back(TraceEventJson(event));
            }
            json["events"] = std::move(list);
        }
        else if(samples)
        {
            jt::Json list = EmptyArray();
            list.getArray().reserve(samples->GetData().size());
            for(const TraceCounter& counter : samples->GetData())
            {
                list.getArray().push_back(TraceCounterJson(counter));
            }
            json["samples"] = std::move(list);
        }
    }
    return json;
}

// The provider sizes its histogram but leaves the summing to the timeline view's
// track items, so with no UI the bins are never filled in. Histogram() and the
// scale in Minimap() are worked out from the minimaps instead.
jt::Json
Histogram(const TimelineModel& timeline)
{
    std::vector<double> bins(timeline.GetHistogram().size(), 0.0);
    for(const std::pair<const uint64_t, std::vector<double>>& entry :
        timeline.GetMiniMap())
    {
        if(IsEventTrack(timeline, entry.first))
        {
            for(size_t i = 0; i < entry.second.size() && i < bins.size(); i++)
            {
                bins[i] += entry.second[i];
            }
        }
    }
    double max_value = 0.0;
    for(double bin : bins)
    {
        max_value = std::max(max_value, bin);
    }
    if(max_value > 0.0)
    {
        for(double& bin : bins)
        {
            bin /= max_value;
        }
    }

    jt::Json json;
    json["start_ns"]  = FiniteDoubleJson(timeline.GetStartTime());
    json["end_ns"]    = FiniteDoubleJson(timeline.GetEndTime());
    json["max_value"] = FiniteDoubleJson(max_value);
    json["bins"]      = DoublesJson(bins);
    return json;
}

jt::Json
Minimap(const TimelineModel& timeline)
{
    double   min_value = std::numeric_limits<double>::max();
    double   max_value = std::numeric_limits<double>::lowest();
    jt::Json tracks;
    tracks.setObject();
    for(const std::pair<const uint64_t, std::vector<double>>& entry :
        timeline.GetMiniMap())
    {
        tracks[std::to_string(entry.first)] = DoublesJson(entry.second);
        if(IsEventTrack(timeline, entry.first))
        {
            for(double value : entry.second)
            {
                // Empty bins stay out of the scale, as on the timeline.
                if(value != 0.0)
                {
                    min_value = std::min(min_value, value);
                    max_value = std::max(max_value, value);
                }
            }
        }
    }
    if(min_value > max_value)
    {
        min_value = 0.0;
        max_value = 0.0;
    }

    jt::Json json;
    json["start_ns"]  = FiniteDoubleJson(timeline.GetStartTime());
    json["end_ns"]    = FiniteDoubleJson(timeline.GetEndTime());
    json["min_value"] = FiniteDoubleJson(min_value);
    json["max_value"] = FiniteDoubleJson(max_value);
    json["tracks"]    = std::move(tracks);
    return json;
}

jt::Json
Topology(const TopologyTree& topology)
{
    jt::Json nodes = EmptyArray();
    for(const TopologyNode* node : topology.GetNodes())
    {
        nodes.getArray().push_back(TopologyNodeJson(topology, *node));
    }

    jt::Json json;
    json["revision"]    = UInt64Json(topology.GetRevision());
    json["gpu_count"]   = topology.GetGPUCount();
    json["cpu_count"]   = topology.GetCPUCount();
    json["track_order"] = IdsJson(topology.GetTrackOrder());
    json["nodes"]       = std::move(nodes);
    return json;
}

jt::Json
Table(const TraceDataModel& model, TableType type)
{
    jt::Json     json;
    const size_t index = static_cast<size_t>(type);
    if(index < static_cast<size_t>(TableType::__kTableTypeCount))
    {
        const TableInfo& table = TablesFor(model, type).GetTable(type);
        if(table.table_params || !table.table_header.empty())
        {
            json["type"] = TableTypeName(type);
            json["columns"] =
                TableColumnsJson(table.table_header, table.table_column_types);
            json["rows"]            = StringRowsJson(table.table_data);
            json["total_row_count"] = UInt64Json(table.total_row_count);
            json["request"] =
                table.table_params ? TableRequestJson(*table.table_params) : jt::Json();
        }
    }
    return json;
}

const char*
TableTypeName(TableType type)
{
    const size_t index = static_cast<size_t>(type);
    return index < std::size(TABLE_SLOT_NAMES) ? TABLE_SLOT_NAMES[index].name : "unknown";
}

bool
TableTypeFromName(const std::string& name, TableType& type_out)
{
    bool found = false;
    for(const TableSlotName& slot : TABLE_SLOT_NAMES)
    {
        if(name == slot.name)
        {
            type_out = slot.type;
            found    = true;
            break;
        }
    }
    return found;
}

jt::Json
Event(const EventModel& events, uint64_t event_uuid)
{
    const EventInfo* event = events.GetEvent(event_uuid);
    return event ? EventInfoJson(*event) : jt::Json();
}

jt::Json
Summary(const TraceDataModel& model)
{
    jt::Json            json;
    const SummaryModel& summary  = model.GetSummary();
    const uint64_t      revision = summary.GetRevision();
    if(revision != 0 && revision == model.GetTopology().GetRevision())
    {
        json["revision"] = UInt64Json(revision);
        json["root"]     = AggregateJson(summary, summary.GetSummaryData());
    }
    return json;
}

jt::Json
TrackStatistics(const AnalysisTrackStatistics& statistics)
{
    jt::Json stats = EmptyArray();
    for(const AnalysisTrackStatistics::Stat& stat : statistics.stats)
    {
        jt::Json entry;
        entry["name"]  = stat.name ? stat.name : "";
        entry["value"] = FiniteDoubleJson(stat.value);
        entry["units"] = stat.suffix;
        stats.getArray().push_back(std::move(entry));
    }

    jt::Json json;
    json["track_id"] = statistics.track ? IdJson(statistics.track->id) : jt::Json();
    json["state"] =
        EnumName(STATISTICS_STATE_NAMES, static_cast<size_t>(statistics.state));
    json["stats"] = std::move(stats);
    return json;
}

jt::Json
Compute(const ComputeDataModel& model)
{
    const AnalysisInfo& analysis = model.GetAnalysisInfo();
    jt::Json            profiler;
    profiler["version"]        = analysis.profiler_version;
    profiler["git_version"]    = analysis.profiler_git_version;
    profiler["schema_version"] = analysis.schema_version;

    jt::Json workloads = EmptyArray();
    for(const WorkloadInfo* workload : model.GetWorkloadList())
    {
        workloads.getArray().push_back(Workload(*workload));
    }

    jt::Json json;
    json["profiler"]  = std::move(profiler);
    json["workloads"] = std::move(workloads);
    return json;
}

jt::Json
Workload(const WorkloadInfo& workload)
{
    jt::Json kernels = EmptyArray();
    for(const KernelInfo* kernel : workload.ordered_kernels)
    {
        kernels.getArray().push_back(Kernel(*kernel));
    }

    jt::Json json;
    json["id"]               = workload.id;
    json["name"]             = workload.name;
    json["system_info"]      = StringRowsJson(workload.system_info);
    json["profiling_config"] = StringRowsJson(workload.profiling_config);
    json["metrics"]          = AvailableMetricsJson(workload.available_metrics);
    json["memory_chart"]     = MemoryChartJson(workload.memory_chart_layout);
    json["kernels"]          = std::move(kernels);
    json["roofline"]         = WorkloadRooflineJson(workload.roofline);
    return json;
}

jt::Json
Kernel(const KernelInfo& kernel)
{
    jt::Json dispatch;
    for(size_t i = 0; i < std::size(DISPATCH_METRIC_NAMES); i++)
    {
        dispatch[DISPATCH_METRIC_NAMES[i]] = UInt64Json(kernel.dispatch_metrics[i]);
    }

    std::vector<const roofline_intensity_t*> intensities;
    for(const auto& entry : kernel.roofline.intensities)
    {
        intensities.push_back(&entry.second);
    }
    std::sort(intensities.begin(), intensities.end(),
              [](const roofline_intensity_t* a, const roofline_intensity_t* b) {
                  return a->type < b->type;
              });
    jt::Json roofline = EmptyArray();
    for(const roofline_intensity_t* intensity : intensities)
    {
        jt::Json point = PointJson(intensity->position);
        point["type"] =
            EnumName(ROOFLINE_INTENSITY_NAMES, static_cast<size_t>(intensity->type));
        roofline.getArray().push_back(std::move(point));
    }

    jt::Json json;
    json["id"]            = kernel.id;
    json["name"]          = kernel.name;
    json["has_isa_lines"] = kernel.has_isa_lines;
    json["dispatch"]      = std::move(dispatch);
    json["roofline"]      = std::move(roofline);
    return json;
}

jt::Json
KernelMetrics(const ComputeDataModel& model, uint64_t client_id, uint32_t kernel_id)
{
    jt::Json                                         json;
    const std::vector<std::shared_ptr<MetricValue>>* metrics =
        model.GetKernelMetricsData(client_id, kernel_id);
    if(metrics)
    {
        jt::Json list = EmptyArray();
        for(const std::shared_ptr<MetricValue>& metric : *metrics)
        {
            if(metric && metric->entry)
            {
                list.getArray().push_back(MetricValueJson(*metric));
            }
        }
        json["client_id"] = IdJson(client_id);
        json["kernel_id"] = kernel_id;
        json["metrics"]   = std::move(list);
    }
    return json;
}

jt::Json
WorkloadMetrics(const ComputeDataModel& model, uint64_t client_id,
                const WorkloadInfo& workload)
{
    jt::Json list = EmptyArray();
    for(const AvailableMetrics::Category* category :
        workload.available_metrics.ordered_categories)
    {
        for(const AvailableMetrics::Table* table : category->ordered_tables)
        {
            for(const AvailableMetrics::Entry* entry : table->ordered_entries)
            {
                std::shared_ptr<MetricValue> metric = model.GetWorkloadMetricValue(
                    client_id, workload.id, entry->category_id, entry->table_id,
                    entry->id);
                if(metric && metric->entry)
                {
                    list.getArray().push_back(MetricValueJson(*metric));
                }
            }
        }
    }

    jt::Json json;
    json["client_id"]   = IdJson(client_id);
    json["workload_id"] = workload.id;
    json["metrics"]     = std::move(list);
    return json;
}

jt::Json
KernelSelectionTable(const ComputeKernelSelectionTable& selection_table)
{
    jt::Json                json;
    const ComputeTableInfo& table = selection_table.GetTableInfo();
    if(table.table_params || !table.table_header.empty())
    {
        json["columns"] = TableColumnsJson(table.table_header, {});
        json["rows"]    = StringRowsJson(table.table_data);
        json["request"] = table.table_params
                              ? ComputeTableRequestJson(*table.table_params)
                              : jt::Json();
    }
    return json;
}

jt::Json
PcSampling(const PcSamplingData& data)
{
    jt::Json instruction_types;
    instruction_types.setObject();
    for(const std::pair<const uint64_t, std::string>& type :
        data.instruction_type_lookup_map)
    {
        instruction_types[std::to_string(type.first)] = type.second;
    }

    jt::Json source_mappings = EmptyArray();
    for(const InstructionSourceLine& mapping : data.instruction_source_lines)
    {
        jt::Json entry;
        entry["instruction_uuid"] = IdJson(mapping.instruction_uuid);
        entry["source_line_uuid"] = IdJson(mapping.source_line_uuid);
        entry["source_file_uuid"] = IdJson(mapping.source_file_uuid);
        entry["frame_index"]      = UInt64Json(mapping.frame_index);
        source_mappings.getArray().push_back(std::move(entry));
    }

    jt::Json stall_reasons = EmptyArray();
    for(const PcSampleStallReason& reason : data.pc_sample_stall_reasons)
    {
        jt::Json entry;
        entry["sample_state_uuid"] = IdJson(reason.pc_sample_state_uuid);
        entry["lookup_uuid"]       = IdJson(reason.pc_sample_stall_reason_lookup_uuid);
        entry["count"]             = UInt64Json(reason.count);
        stall_reasons.getArray().push_back(std::move(entry));
    }

    jt::Json stall_reason_lookups = EmptyArray();
    for(const PcSampleStallReasonLookup& lookup : data.pc_sample_stall_reason_lookups)
    {
        jt::Json entry;
        entry["uuid"] = IdJson(lookup.pc_sample_stall_reason_lookup_uuid);
        entry["text"] = lookup.text;
        stall_reason_lookups.getArray().push_back(std::move(entry));
    }

    jt::Json json;
    json["sample_states_loaded"]     = data.sample_states_loaded;
    json["stall_details_loaded"]     = data.stall_details_loaded;
    json["code_objects"]             = CodeObjectsJson(data);
    json["instruction_types"]        = std::move(instruction_types);
    json["source_files"]             = SourceFilesJson(data);
    json["instruction_source_lines"] = std::move(source_mappings);
    json["sample_states"]            = SampleStatesJson(data);
    json["stall_reasons"]            = std::move(stall_reasons);
    json["stall_reason_lookups"]     = std::move(stall_reason_lookups);
    return json;
}

}  // namespace DataProviderJson
}  // namespace View
}  // namespace RocProfVis
