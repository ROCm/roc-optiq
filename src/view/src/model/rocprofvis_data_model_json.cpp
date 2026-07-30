// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_data_model_json.h"

#include "rocprofvis_raw_track_data.h"

#include <algorithm>
#include <vector>

namespace RocProfVis
{
namespace View
{

/* Lookup keys become decimal strings so bit-packed 64-bit ids survive a
 * JavaScript client, which would otherwise round them to a double. */
static jt::Json
id_to_json(uint64_t id)
{
    return jt::Json(std::to_string(id));
}

static jt::Json
id_array_to_json(const std::vector<uint64_t>& ids)
{
    jt::Json array;
    array.setArray();
    for(size_t i = 0; i < ids.size(); ++i)
    {
        array[i] = id_to_json(ids[i]);
    }
    return array;
}

static jt::Json
string_array_to_json(const std::vector<std::string>& values)
{
    jt::Json array;
    array.setArray();
    for(size_t i = 0; i < values.size(); ++i)
    {
        array[i] = values[i];
    }
    return array;
}

/* Track type is the one enum a consumer must branch on, because it decides
 * whether a track carries events or samples, so it goes out readable. */
static const char*
track_type_name(rocprofvis_controller_track_type_t track_type)
{
    const char* name = "unknown";
    switch(track_type)
    {
    case kRPVControllerTrackTypeSamples:
        name = "samples";
        break;
    case kRPVControllerTrackTypeEvents:
        name = "events";
        break;
    default:
        break;
    }
    return name;
}

static const char*
table_type_name(TableType type)
{
    const char* name = "unknown";
    switch(type)
    {
    case TableType::kSampleTable:
        name = "sample_table";
        break;
    case TableType::kEventTable:
        name = "event_table";
        break;
    case TableType::kEventSearchTable:
        name = "event_search_table";
        break;
    case TableType::kSummaryKernelTable:
        name = "summary_kernel_table";
        break;
    case TableType::kAnalysisTopInstrumentedEventsTable:
        name = "analysis_top_instrumented_events_table";
        break;
    case TableType::kAnalysisTopDispatchEventsTable:
        name = "analysis_top_dispatch_events_table";
        break;
    case TableType::kAnalysisTopMemoryAllocationEventsTable:
        name = "analysis_top_memory_allocation_events_table";
        break;
    case TableType::kAnalysisTopMemoryCopyEventsTable:
        name = "analysis_top_memory_copy_events_table";
        break;
    case TableType::kAnalysisTopSampledEventsTable:
        name = "analysis_top_sampled_events_table";
        break;
    default:
        break;
    }
    return name;
}

static jt::Json
compare_source_to_json(const CompareSourceInfo& source)
{
    jt::Json json;
    json["id"]   = source.id;
    json["name"] = source.name;
    json["path"] = source.path;
    return json;
}

static jt::Json
node_to_json(const NodeInfo& node, size_t display_index)
{
    jt::Json json;
    json["id"]            = id_to_json(node.id);
    json["display_index"] = static_cast<long long>(display_index);
    json["host_name"]     = node.host_name;
    json["os_name"]       = node.os_name;
    json["os_release"]    = node.os_release;
    json["os_version"]    = node.os_version;
    json["device_ids"]    = id_array_to_json(node.device_ids);
    json["process_ids"]   = id_array_to_json(node.process_ids);
    return json;
}

static jt::Json
device_to_json(const DeviceInfo& device, const std::string& type_label)
{
    jt::Json json;
    json["id"]           = id_to_json(device.id.value);
    json["product_name"] = device.product_name;
    json["type"]         = static_cast<long long>(device.type);
    json["type_label"]   = type_label;
    json["type_index"]   = static_cast<long long>(device.type_index);
    json["queue_ids"]    = id_array_to_json(device.queue_ids);
    json["counter_ids"]  = id_array_to_json(device.counter_ids);
    return json;
}

static jt::Json
process_to_json(const ProcessInfo& process)
{
    jt::Json json;
    json["id"]                      = id_to_json(process.id);
    json["start_time"]              = process.start_time;
    json["end_time"]                = process.end_time;
    json["command"]                 = process.command;
    json["environment"]             = process.environment;
    json["instrumented_thread_ids"] = id_array_to_json(process.instrumented_thread_ids);
    json["sampled_thread_ids"]      = id_array_to_json(process.sampled_thread_ids);
    json["stream_ids"]              = id_array_to_json(process.stream_ids);
    return json;
}

static jt::Json
thread_to_json(const ThreadInfo& thread)
{
    jt::Json json;
    json["id"]         = id_to_json(thread.id);
    json["name"]       = thread.name;
    json["start_time"] = thread.start_time;
    json["end_time"]   = thread.end_time;
    json["tid"]        = static_cast<long long>(thread.tid);
    return json;
}

static jt::Json
queue_to_json(const QueueInfo& queue)
{
    jt::Json json;
    json["id"]        = id_to_json(queue.id);
    json["name"]      = queue.name;
    json["device_id"] = id_to_json(queue.device_id);
    return json;
}

static jt::Json
stream_to_json(const StreamInfo& stream)
{
    jt::Json json;
    json["id"]   = id_to_json(stream.id);
    json["name"] = stream.name;

    jt::Json processors;
    processors.setArray();
    for(size_t i = 0; i < stream.processors.size(); ++i)
    {
        jt::Json processor;
        processor["id"]        = id_to_json(stream.processors[i].id);
        processor["queue_ids"] = id_array_to_json(stream.processors[i].queue_ids);
        processors[i]          = processor;
    }
    json["processors"] = processors;
    return json;
}

static jt::Json
counter_to_json(const CounterInfo& counter)
{
    jt::Json json;
    json["id"]          = id_to_json(counter.id);
    json["name"]        = counter.name;
    json["device_id"]   = id_to_json(counter.device_id);
    json["description"] = counter.description;
    json["units"]       = counter.units;
    json["value_type"]  = counter.value_type;
    return json;
}

static jt::Json
track_to_json(const TrackInfo& track, const std::string& name)
{
    jt::Json json;
    json["index"]           = static_cast<long long>(track.index);
    json["id"]              = id_to_json(track.id);
    json["name"]            = name;
    json["track_type"]      = track_type_name(track.track_type);
    json["min_ts"]          = track.min_ts;
    json["max_ts"]          = track.max_ts;
    json["num_entries"]     = static_cast<long long>(track.num_entries);
    json["instance_id"]     = static_cast<long long>(track.instance_id);
    json["file_id"]         = static_cast<long long>(track.file_id);
    json["order_rank"]      = static_cast<long long>(track.order_rank);
    json["agent_or_pid"]    = static_cast<long long>(track.agent_or_pid);
    json["queue_id_or_tid"] = static_cast<long long>(track.queue_id_or_tid);
    json["min_value"]       = track.min_value;
    json["max_value"]       = track.max_value;
    json["category"]        = track.category;
    json["main_name"]       = track.main_name;
    json["sub_name"]        = track.sub_name;
    json["compare_source"]  = compare_source_to_json(track.compare_source);

    /* The set iterates in an unspecified order; sort so the same model always
     * translates to the same bytes. */
    std::vector<int64_t> operations;
    operations.reserve(track.operation_types.size());
    for(const rocprofvis_dm_event_operation_t& operation : track.operation_types)
    {
        operations.push_back(static_cast<int64_t>(operation));
    }
    std::sort(operations.begin(), operations.end());

    jt::Json operation_types;
    operation_types.setArray();
    for(size_t i = 0; i < operations.size(); ++i)
    {
        operation_types[i] = static_cast<long long>(operations[i]);
    }
    json["operation_types"] = operation_types;

    jt::Json topology;
    topology["node_id"]    = id_to_json(track.topology.node_id);
    topology["process_id"] = id_to_json(track.topology.process_id);
    topology["device_id"]  = id_to_json(track.topology.device_id);
    topology["type"]       = static_cast<long long>(track.topology.type);
    topology["id"]         = id_to_json(track.topology.id.value);
    json["topology"]       = topology;

    return json;
}

static jt::Json
trace_event_to_json(const TraceEvent& event)
{
    jt::Json json;
    json["id"]                = id_to_json(event.m_id.uuid);
    json["name"]              = event.m_name;
    json["start_ts"]          = event.m_start_ts;
    json["duration"]          = event.m_duration;
    json["level"]             = static_cast<long long>(event.m_level);
    json["child_count"]       = static_cast<long long>(event.m_child_count);
    json["top_combined_name"] = event.m_top_combined_name;
    return json;
}

static jt::Json
trace_counter_to_json(const TraceCounter& counter)
{
    jt::Json json;
    json["start_ts"] = counter.m_start_ts;
    json["end_ts"]   = counter.m_end_ts;
    json["value"]    = counter.m_value;
    return json;
}

static jt::Json
event_to_json(const EventInfo& event)
{
    jt::Json json;
    json["track_id"] = id_to_json(event.track_id);

    jt::Json basic_info;
    basic_info["id"]           = id_to_json(event.basic_info.id.uuid);
    basic_info["name"]         = event.basic_info.name;
    basic_info["start_ts"]     = event.basic_info.start_ts;
    basic_info["duration"]     = event.basic_info.duration;
    basic_info["level"]        = static_cast<long long>(event.basic_info.level);
    basic_info["stream_level"] = static_cast<long long>(event.basic_info.stream_level);
    json["basic_info"]         = basic_info;

    jt::Json args;
    args.setArray();
    for(size_t i = 0; i < event.args.size(); ++i)
    {
        jt::Json arg;
        arg["position"]  = static_cast<long long>(event.args[i].position);
        arg["name"]      = event.args[i].name;
        arg["value"]     = event.args[i].value;
        arg["data_type"] = event.args[i].data_type;
        args[i]          = arg;
    }
    json["args"] = args;

    jt::Json ext_info;
    ext_info.setArray();
    for(size_t i = 0; i < event.ext_info.size(); ++i)
    {
        jt::Json ext;
        ext["category"]      = event.ext_info[i].category;
        ext["name"]          = event.ext_info[i].name;
        ext["value"]         = event.ext_info[i].value;
        ext["category_enum"] = static_cast<long long>(event.ext_info[i].category_enum);
        ext_info[i]          = ext;
    }
    json["ext_info"] = ext_info;

    jt::Json flow_info;
    flow_info.setArray();
    for(size_t i = 0; i < event.flow_info.size(); ++i)
    {
        jt::Json flow;
        flow["id"]              = id_to_json(event.flow_info[i].id.uuid);
        flow["start_timestamp"] = static_cast<long long>(event.flow_info[i].start_timestamp);
        flow["end_timestamp"]   = static_cast<long long>(event.flow_info[i].end_timestamp);
        flow["track_id"]        = id_to_json(event.flow_info[i].track_id);
        flow["level"]           = static_cast<long long>(event.flow_info[i].level);
        flow["direction"]       = static_cast<long long>(event.flow_info[i].direction);
        flow["name"]            = event.flow_info[i].name;
        flow_info[i]            = flow;
    }
    json["flow_info"] = flow_info;

    jt::Json call_stack_info;
    call_stack_info.setArray();
    for(size_t i = 0; i < event.call_stack_info.size(); ++i)
    {
        jt::Json frame;
        frame["id"]        = id_to_json(event.call_stack_info[i].id.uuid);
        frame["file"]      = event.call_stack_info[i].file;
        frame["pc"]        = event.call_stack_info[i].pc;
        frame["name"]      = event.call_stack_info[i].name;
        frame["address"]   = event.call_stack_info[i].address;
        call_stack_info[i] = frame;
    }
    json["call_stack_info"] = call_stack_info;

    return json;
}

static jt::Json
aggregate_metrics_to_json(const SummaryInfo::AggregateMetrics& metrics)
{
    jt::Json json;
    json["type"] = static_cast<long long>(metrics.type);
    if(metrics.id.has_value())
    {
        json["id"] = id_to_json(metrics.id.value());
    }
    if(metrics.name.has_value())
    {
        json["name"] = metrics.name.value();
    }
    if(metrics.device_type.has_value())
    {
        json["device_type"] = static_cast<long long>(metrics.device_type.value());
    }
    if(metrics.device_type_index.has_value())
    {
        json["device_type_index"] =
            static_cast<long long>(metrics.device_type_index.value());
    }

    jt::Json gpu;
    if(metrics.gpu.gfx_utilization.has_value())
    {
        gpu["gfx_utilization"] = metrics.gpu.gfx_utilization.value();
    }
    if(metrics.gpu.mem_utilization.has_value())
    {
        gpu["mem_utilization"] = metrics.gpu.mem_utilization.value();
    }
    gpu["kernel_exec_time_total"] = metrics.gpu.kernel_exec_time_total;

    jt::Json top_kernels;
    top_kernels.setArray();
    for(size_t i = 0; i < metrics.gpu.top_kernels.size(); ++i)
    {
        const SummaryInfo::KernelMetrics& kernel = metrics.gpu.top_kernels[i];
        jt::Json                          entry;
        entry["name"]          = kernel.name;
        entry["invocations"]   = static_cast<long long>(kernel.invocations);
        entry["exec_time_sum"] = kernel.exec_time_sum;
        entry["exec_time_min"] = kernel.exec_time_min;
        entry["exec_time_max"] = kernel.exec_time_max;
        entry["exec_time_pct"] = kernel.exec_time_pct;
        top_kernels[i]         = entry;
    }
    gpu["top_kernels"] = top_kernels;
    json["gpu"]        = gpu;

    jt::Json sub_metrics;
    sub_metrics.setArray();
    for(size_t i = 0; i < metrics.sub_metrics.size(); ++i)
    {
        sub_metrics[i] = aggregate_metrics_to_json(metrics.sub_metrics[i]);
    }
    json["sub_metrics"] = sub_metrics;

    return json;
}

DataModelJson::DataModelJson(const TraceDataModel& model)
: m_model(model)
{
}

jt::Json
DataModelJson::GetTopology() const
{
    const TopologyDataModel& topology = m_model.GetTopology();

    /* The model stores one map per kind and only exposes a list for nodes, so
     * reach everything else the way the UI does: down the node's device and
     * process id lists. Emitted flat, one array per kind, with the parent
     * links preserved as id arrays. */
    std::vector<const NodeInfo*> nodes = topology.GetNodeList();
    std::sort(nodes.begin(), nodes.end(),
              [](const NodeInfo* lhs, const NodeInfo* rhs) { return lhs->id < rhs->id; });

    jt::Json node_array;
    jt::Json device_array;
    jt::Json process_array;
    jt::Json instrumented_thread_array;
    jt::Json sampled_thread_array;
    jt::Json queue_array;
    jt::Json stream_array;
    jt::Json counter_array;
    node_array.setArray();
    device_array.setArray();
    process_array.setArray();
    instrumented_thread_array.setArray();
    sampled_thread_array.setArray();
    queue_array.setArray();
    stream_array.setArray();
    counter_array.setArray();

    size_t device_count              = 0;
    size_t process_count             = 0;
    size_t instrumented_thread_count = 0;
    size_t sampled_thread_count      = 0;
    size_t queue_count               = 0;
    size_t stream_count              = 0;
    size_t counter_count             = 0;

    for(size_t i = 0; i < nodes.size(); ++i)
    {
        const NodeInfo* node = nodes[i];
        node_array[i] = node_to_json(*node, topology.GetNodeDisplayIndex(node->id));

        for(const uint64_t& device_id : node->device_ids)
        {
            const DeviceInfo* device = topology.GetDevice(device_id);
            if(device == nullptr)
            {
                continue;
            }

            std::string type_label;
            topology.GetDeviceTypeLabel(*device, type_label);
            device_array[device_count++] = device_to_json(*device, type_label);

            for(const uint64_t& queue_id : device->queue_ids)
            {
                const QueueInfo* queue = topology.GetQueue(queue_id, device->id.value);
                if(queue != nullptr)
                {
                    queue_array[queue_count++] = queue_to_json(*queue);
                }
            }

            for(const uint64_t& counter_id : device->counter_ids)
            {
                const CounterInfo* counter = topology.GetCounter(counter_id);
                if(counter != nullptr)
                {
                    counter_array[counter_count++] = counter_to_json(*counter);
                }
            }
        }

        for(const uint64_t& process_id : node->process_ids)
        {
            const ProcessInfo* process = topology.GetProcess(process_id);
            if(process == nullptr)
            {
                continue;
            }
            process_array[process_count++] = process_to_json(*process);

            for(const uint64_t& thread_id : process->instrumented_thread_ids)
            {
                const ThreadInfo* thread = topology.GetInstrumentedThread(thread_id);
                if(thread != nullptr)
                {
                    instrumented_thread_array[instrumented_thread_count++] =
                        thread_to_json(*thread);
                }
            }

            for(const uint64_t& thread_id : process->sampled_thread_ids)
            {
                const ThreadInfo* thread = topology.GetSampledThread(thread_id);
                if(thread != nullptr)
                {
                    sampled_thread_array[sampled_thread_count++] = thread_to_json(*thread);
                }
            }

            for(const uint64_t& stream_id : process->stream_ids)
            {
                const StreamInfo* stream = topology.GetStream(stream_id);
                if(stream != nullptr)
                {
                    stream_array[stream_count++] = stream_to_json(*stream);
                }
            }
        }
    }

    jt::Json json;
    json["nodes"]                = node_array;
    json["devices"]              = device_array;
    json["processes"]            = process_array;
    json["instrumented_threads"] = instrumented_thread_array;
    json["sampled_threads"]      = sampled_thread_array;
    json["queues"]               = queue_array;
    json["streams"]              = stream_array;
    json["counters"]             = counter_array;
    return json;
}

jt::Json
DataModelJson::GetTimeline() const
{
    const TimelineModel& timeline = m_model.GetTimeline();

    std::vector<const TrackInfo*> tracks = timeline.GetTrackList();
    std::sort(tracks.begin(), tracks.end(),
              [](const TrackInfo* lhs, const TrackInfo* rhs) {
                  return (lhs->index != rhs->index) ? (lhs->index < rhs->index)
                                                    : (lhs->id < rhs->id);
              });

    jt::Json track_array;
    track_array.setArray();
    for(size_t i = 0; i < tracks.size(); ++i)
    {
        track_array[i] = track_to_json(*tracks[i], m_model.BuildTrackName(tracks[i]->id));
    }

    jt::Json json;
    json["start_time"]  = timeline.GetStartTime();
    json["end_time"]    = timeline.GetEndTime();
    json["track_count"] = static_cast<long long>(timeline.GetTrackCount());
    json["tracks"]      = track_array;
    return json;
}

jt::Json
DataModelJson::GetTrack(uint64_t track_id) const
{
    jt::Json         json;
    const TrackInfo* track = m_model.GetTimeline().GetTrack(track_id);
    if(track != nullptr)
    {
        json = track_to_json(*track, m_model.BuildTrackName(track_id));
    }
    return json;
}

jt::Json
DataModelJson::GetTrackData(uint64_t track_id) const
{
    jt::Json            json;
    const RawTrackData* data = m_model.GetTimeline().GetTrackData(track_id);
    if(data == nullptr)
    {
        return json;
    }

    json["track_id"]       = id_to_json(data->GetTrackID());
    json["track_type"]     = track_type_name(data->GetType());
    json["start_ts"]       = data->GetStartTs();
    json["end_ts"]         = data->GetEndTs();
    json["data_group_id"]  = id_to_json(data->GetDataGroupID());
    json["chunk_count"]    = static_cast<long long>(data->GetChunkCount());
    json["all_data_ready"] = data->AllDataReady();

    jt::Json entries;
    entries.setArray();
    if(data->GetType() == kRPVControllerTrackTypeEvents)
    {
        const RawTrackEventData* events = static_cast<const RawTrackEventData*>(data);
        const std::vector<TraceEvent>& values = events->GetData();
        for(size_t i = 0; i < values.size(); ++i)
        {
            entries[i] = trace_event_to_json(values[i]);
        }
    }
    else if(data->GetType() == kRPVControllerTrackTypeSamples)
    {
        const RawTrackSampleData* samples = static_cast<const RawTrackSampleData*>(data);
        const std::vector<TraceCounter>& values = samples->GetData();
        for(size_t i = 0; i < values.size(); ++i)
        {
            entries[i] = trace_counter_to_json(values[i]);
        }
    }
    json["entries"] = entries;

    return json;
}

jt::Json
DataModelJson::GetTable(TableType type) const
{
    const TableInfo& table = m_model.GetTables().GetTable(type);

    jt::Json rows;
    rows.setArray();
    for(size_t i = 0; i < table.table_data.size(); ++i)
    {
        rows[i] = string_array_to_json(table.table_data[i]);
    }

    jt::Json json;
    json["table_header"]    = string_array_to_json(table.table_header);
    json["table_data"]      = rows;
    json["total_row_count"] = static_cast<long long>(table.total_row_count);
    return json;
}

jt::Json
DataModelJson::GetEvent(uint64_t event_id) const
{
    jt::Json         json;
    const EventInfo* event = m_model.GetEvents().GetEvent(event_id);
    if(event != nullptr)
    {
        json = event_to_json(*event);
    }
    return json;
}

jt::Json
DataModelJson::GetSummary() const
{
    return aggregate_metrics_to_json(m_model.GetSummary().GetSummaryData());
}

jt::Json
DataModelJson::ToJson() const
{
    jt::Json json;
    json["trace_file_path"] = m_model.GetTraceFilePath();
    json["topology"]        = GetTopology();
    json["timeline"]        = GetTimeline();
    json["summary"]         = GetSummary();

    /* Keyed by id rather than listed, because a track only appears once the
     * provider has fetched it. */
    jt::Json track_data;
    track_data.setObject();
    std::vector<const TrackInfo*> tracks = m_model.GetTimeline().GetTrackList();
    for(const TrackInfo* track : tracks)
    {
        if(m_model.GetTimeline().GetTrackData(track->id) != nullptr)
        {
            track_data[std::to_string(track->id)] = GetTrackData(track->id);
        }
    }
    json["track_data"] = track_data;

    jt::Json tables;
    tables.setObject();
    for(size_t i = 0; i < static_cast<size_t>(TableType::__kTableTypeCount); ++i)
    {
        const TableType type = static_cast<TableType>(i);
        if(!m_model.GetTables().GetTable(type).table_header.empty())
        {
            tables[table_type_name(type)] = GetTable(type);
        }
    }
    json["tables"] = tables;

    /* Events are deliberately absent: the model only holds the ones the user
     * has clicked and offers no way to enumerate them. Ask per id with
     * GetEvent(). */

    return json;
}

std::string
DataModelJson::ToString(bool pretty) const
{
    const jt::Json json = ToJson();
    return pretty ? json.toStringPretty() : json.toString();
}

}  // namespace View
}  // namespace RocProfVis
