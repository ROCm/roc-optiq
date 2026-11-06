// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_protocol.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "model/rocprofvis_summary_model.h"
#include "model/rocprofvis_topology_model.h"
#include "model/rocprofvis_trace_data_model.h"

#include "spdlog/spdlog.h"

#include <algorithm>
#include <limits>
#include <map>
#include <unordered_map>
#include <unordered_set>

namespace RocProfVis
{
namespace View
{

namespace
{

// How UpdateValueStrings formats a summary value; see SummaryModel.
enum SummaryValueCategory : int
{
    kSummaryValueUnregistered = 0,
    kSummaryValueEvent,
    kSummaryValueCounter,
    kSummaryValueTimeunit,
};

struct SummaryValueRegistry
{
    std::unordered_set<const SummaryInfo::Value*> event_values;
    std::unordered_set<const SummaryInfo::Value*> counter_values;
    std::unordered_set<const SummaryInfo::Value*> timeunit_values;
};

jt::Json
make_array()
{
    jt::Json array;
    array.setArray();
    return array;
}

// Unordered containers are written in key order, so equal models encode to the
// same text regardless of how they were filled.
template <typename Map>
std::vector<typename Map::key_type>
sorted_keys(const Map& map)
{
    std::vector<typename Map::key_type> keys;
    keys.reserve(map.size());
    for(const auto& entry : map)
    {
        keys.push_back(entry.first);
    }
    std::sort(keys.begin(), keys.end());
    return keys;
}

jt::Json
encode_string_list(const std::vector<std::string>& values)
{
    jt::Json array = make_array();
    array.getArray().reserve(values.size());
    for(const std::string& value : values)
    {
        array.getArray().emplace_back(value);
    }
    return array;
}

std::vector<std::string>
decode_string_list(jt::Json& object, const char* key)
{
    std::vector<std::string> values;
    std::vector<jt::Json>&   array = json_get_array(object, key);
    values.reserve(array.size());
    for(jt::Json& value : array)
    {
        values.push_back(json_to_string(value));
    }
    return values;
}

jt::Json
encode_uint64_list(const std::vector<uint64_t>& values)
{
    jt::Json array = make_array();
    array.getArray().reserve(values.size());
    for(uint64_t value : values)
    {
        array.getArray().push_back(json_from_uint64(value));
    }
    return array;
}

std::vector<uint64_t>
decode_uint64_list(jt::Json& object, const char* key)
{
    std::vector<uint64_t>  values;
    std::vector<jt::Json>& array = json_get_array(object, key);
    values.reserve(array.size());
    for(jt::Json& value : array)
    {
        values.push_back(json_to_uint64(value));
    }
    return values;
}

jt::Json
encode_double_list(const std::vector<double>& values)
{
    jt::Json array = make_array();
    array.getArray().reserve(values.size());
    for(double value : values)
    {
        array.getArray().push_back(json_from_double(value));
    }
    return array;
}

std::vector<double>
decode_double_list(jt::Json& object, const char* key)
{
    std::vector<double>    values;
    std::vector<jt::Json>& array = json_get_array(object, key);
    values.reserve(array.size());
    for(jt::Json& value : array)
    {
        values.push_back(json_to_double(value));
    }
    return values;
}

jt::Json
encode_string_table(const std::vector<std::vector<std::string>>& rows)
{
    jt::Json array = make_array();
    array.getArray().reserve(rows.size());
    for(const std::vector<std::string>& row : rows)
    {
        array.getArray().push_back(encode_string_list(row));
    }
    return array;
}

std::vector<std::vector<std::string>>
decode_string_table(jt::Json& object, const char* key)
{
    std::vector<std::vector<std::string>> rows;
    std::vector<jt::Json>&                array = json_get_array(object, key);
    rows.reserve(array.size());
    for(jt::Json& row_json : array)
    {
        std::vector<std::string>& row = rows.emplace_back();
        if(row_json.isArray())
        {
            row.reserve(row_json.getArray().size());
            for(jt::Json& cell : row_json.getArray())
            {
                row.push_back(json_to_string(cell));
            }
        }
    }
    return rows;
}

void
encode_optional_uint64(jt::Json& object, const char* key, const c_optional_uint64_t& value)
{
    if(value.has_value)
    {
        object[key] = json_from_uint64(value.value);
    }
}

c_optional_uint64_t
decode_optional_uint64(jt::Json& object, const char* key)
{
    c_optional_uint64_t value;
    if(object.contains(key) && !object[key].isNull())
    {
        value = c_optional_uint64_t(json_get_uint64(object, key));
    }
    return value;
}

void
encode_table_common(const TableRequestParams& params, jt::Json& json)
{
    json["table_type"]    = static_cast<long long>(params.m_table_type);
    json["start"]         = json_from_double(params.m_start_ts);
    json["end"]           = json_from_double(params.m_end_ts);
    json["start_row"]     = json_from_uint64(params.m_start_row);
    json["row_count"]     = json_from_uint64(params.m_req_row_count);
    json["sort_column"]   = json_from_uint64(params.m_sort_column_index);
    json["sort_order"]    = static_cast<long long>(params.m_sort_order);
    json["filter"]        = params.m_filter;
    json["group"]         = params.m_group;
    json["group_columns"] = params.m_group_columns;
    json["export"]        = !params.m_export_to_file_path.empty();
    json["view_table"]    = static_cast<long long>(params.m_view_table_type);
    json["request_id"]    = json_from_uint64(params.m_request_id);
    json["client_id"]     = json_from_uint64(params.m_client_id);
    if(params.m_source_filter.has_value())
    {
        jt::Json source;
        source.setObject();
        encode_optional_uint64(source, "node", params.m_source_filter->node_id);
        encode_optional_uint64(source, "agent", params.m_source_filter->agent_id);
        encode_optional_uint64(source, "process", params.m_source_filter->process_id);
        encode_optional_uint64(source, "thread", params.m_source_filter->thread_id);
        json["source"] = std::move(source);
    }
}

void
decode_table_common(jt::Json& json, TableRequestParams& params)
{
    params.m_table_type = static_cast<rocprofvis_controller_table_type_t>(
        json_get_uint64(json, "table_type"));
    params.m_start_ts          = json_get_double(json, "start");
    params.m_end_ts            = json_get_double(json, "end");
    params.m_start_row         = json_get_uint64(json, "start_row");
    params.m_req_row_count     = json_get_uint64(json, "row_count");
    params.m_sort_column_index = json_get_uint64(json, "sort_column");
    params.m_sort_order        = static_cast<rocprofvis_controller_sort_order_t>(
        json_get_uint64(json, "sort_order"));
    params.m_filter        = json_get_string(json, "filter");
    params.m_group         = json_get_string(json, "group");
    params.m_group_columns = json_get_string(json, "group_columns");
    params.m_export_to_file_path.clear();
    params.m_view_table_type = static_cast<TableType>(json_get_uint64(json, "view_table"));
    if(params.m_view_table_type > TableType::__kTableTypeCount)
    {
        params.m_view_table_type = TableType::__kTableTypeCount;
    }
    params.m_request_id = json_get_uint64(json, "request_id");
    params.m_client_id  = json_get_uint64(json, "client_id");
    params.m_source_filter.reset();
    if(json.contains("source") && json["source"].isObject())
    {
        jt::Json&                      source = json["source"];
        rocprofvis_dm_query_criteria_t criteria;
        criteria.node_id    = decode_optional_uint64(source, "node");
        criteria.agent_id   = decode_optional_uint64(source, "agent");
        criteria.process_id = decode_optional_uint64(source, "process");
        criteria.thread_id  = decode_optional_uint64(source, "thread");
        params.m_source_filter = criteria;
    }
}

jt::Json
encode_summary_value(const SummaryInfo::Value& value, const SummaryValueRegistry& registry)
{
    int category = kSummaryValueUnregistered;
    if(registry.event_values.count(&value))
    {
        category = kSummaryValueEvent;
    }
    else if(registry.counter_values.count(&value))
    {
        category = kSummaryValueCounter;
    }
    else if(registry.timeunit_values.count(&value))
    {
        category = kSummaryValueTimeunit;
    }

    jt::Json json = make_array();
    json.getArray().emplace_back(static_cast<int>(value.type));
    switch(value.type)
    {
        case SummaryInfo::Value::Type::UInt:
            json.getArray().push_back(json_from_uint64(value.numeric.uint));
            break;
        case SummaryInfo::Value::Type::Double:
            json.getArray().push_back(json_from_double(value.numeric.dbl));
            break;
        case SummaryInfo::Value::Type::Float:
            json.getArray().push_back(json_from_double(value.numeric.flt));
            break;
    }
    json.getArray().emplace_back(category);
    return json;
}

void
decode_summary_value(jt::Json& json, SummaryInfo::Value& value, SummaryModel& summary)
{
    value = SummaryInfo::Value{};
    if(!json.isArray() || json.getArray().size() < 3)
    {
        return;
    }
    std::vector<jt::Json>& parts = json.getArray();
    value.type = static_cast<SummaryInfo::Value::Type>(json_to_uint64(parts[0]));
    switch(value.type)
    {
        case SummaryInfo::Value::Type::UInt:
            value.numeric.uint = json_to_uint64(parts[1]);
            break;
        case SummaryInfo::Value::Type::Double:
            value.numeric.dbl = json_to_double(parts[1]);
            break;
        case SummaryInfo::Value::Type::Float:
            value.numeric.flt = static_cast<float>(json_to_double(parts[1]));
            break;
        default:
            value.type        = SummaryInfo::Value::Type::UInt;
            value.numeric.uint = 0;
            break;
    }
    switch(json_to_uint64(parts[2]))
    {
        case kSummaryValueEvent: summary.RegisterEventValue(value); break;
        case kSummaryValueCounter: summary.RegisterCounterValue(value); break;
        case kSummaryValueTimeunit: summary.RegisterTimeunitValue(value); break;
        default: break;
    }
}

template <size_t N>
jt::Json
encode_summary_values(const std::array<SummaryInfo::Value, N>& values,
                      const SummaryValueRegistry&               registry)
{
    jt::Json json = make_array();
    for(const SummaryInfo::Value& value : values)
    {
        json.getArray().push_back(encode_summary_value(value, registry));
    }
    return json;
}

template <size_t N>
void
decode_summary_values(jt::Json& object, const char* key,
                      std::array<SummaryInfo::Value, N>& values, SummaryModel& summary)
{
    std::vector<jt::Json>& array = json_get_array(object, key);
    for(size_t i = 0; i < N; i++)
    {
        if(i < array.size())
        {
            decode_summary_value(array[i], values[i], summary);
        }
        else
        {
            values[i] = SummaryInfo::Value{};
        }
    }
}

jt::Json
encode_top_op_events(const SummaryInfo::TopOpEvents& events,
                     const SummaryValueRegistry&     registry)
{
    jt::Json json;
    json["metrics"] = encode_summary_values(events.metrics, registry);
    jt::Json top    = make_array();
    for(const SummaryInfo::TopOpEvents::Event& event : events.top)
    {
        jt::Json event_json;
        event_json["name"]    = event.name;
        event_json["metrics"] = encode_summary_values(event.metrics, registry);
        top.getArray().push_back(std::move(event_json));
    }
    json["top"] = std::move(top);
    return json;
}

void
decode_top_op_events(jt::Json& object, const char* key, SummaryInfo::TopOpEvents& events,
                     SummaryModel& summary)
{
    events = SummaryInfo::TopOpEvents{};
    if(!object.contains(key))
    {
        return;
    }
    jt::Json& json = object[key];
    decode_summary_values(json, "metrics", events.metrics, summary);
    std::vector<jt::Json>& top = json_get_array(json, "top");
    // Sized before any value is registered: the summary model keeps pointers.
    events.top.resize(top.size());
    for(size_t i = 0; i < top.size(); i++)
    {
        events.top[i].name = json_get_string(top[i], "name");
        decode_summary_values(top[i], "metrics", events.top[i].metrics, summary);
    }
}

jt::Json
encode_counters(const SummaryInfo::Counters& counters, const SummaryValueRegistry& registry)
{
    jt::Json json = make_array();
    for(const SummaryInfo::Counters::Counter& counter : counters.list)
    {
        jt::Json counter_json;
        counter_json["id"]      = json_from_uint64(counter.info ? counter.info->GetId() : 0);
        counter_json["metrics"] = encode_summary_values(counter.metrics, registry);
        json.getArray().push_back(std::move(counter_json));
    }
    return json;
}

void
decode_counters(jt::Json& object, const char* key, SummaryInfo::Counters& counters,
                SummaryModel& summary, const TopologyTree& topology)
{
    counters = SummaryInfo::Counters{};
    std::vector<jt::Json>& list = json_get_array(object, key);
    counters.list.resize(list.size());
    for(size_t i = 0; i < list.size(); i++)
    {
        counters.list[i].info = topology.GetCounter(json_get_uint64(list[i], "id"));
        decode_summary_values(list[i], "metrics", counters.list[i].metrics, summary);
    }
}

jt::Json
encode_aggregate_metrics(const SummaryInfo::AggregateMetrics& metrics,
                         const SummaryValueRegistry&          registry)
{
    jt::Json json;
    json["level"] = static_cast<long long>(metrics.type);
    switch(metrics.type)
    {
        case kRPVAnalysisSummaryAggregationLevelNode:
            json["id"] = json_from_uint64(metrics.info.node ? metrics.info.node->GetId() : 0);
            break;
        case kRPVAnalysisSummaryAggregationLevelProcessor:
            json["id"] = json_from_uint64(
                metrics.info.processor ? metrics.info.processor->GetId() : 0);
            break;
        case kRPVAnalysisSummaryAggregationLevelProcess:
            json["id"] =
                json_from_uint64(metrics.info.process ? metrics.info.process->GetId() : 0);
            break;
        case kRPVAnalysisSummaryAggregationLevelThread:
            json["id"] =
                json_from_uint64(metrics.info.thread ? metrics.info.thread->GetId() : 0);
            json["thread_kind"] = static_cast<int>(
                metrics.info.thread ? metrics.info.thread->kind
                                    : ThreadInfo::Kind::kInstrumented);
            break;
        default: break;
    }

    jt::Json gpu;
    gpu["dispatch"] = encode_top_op_events(metrics.gpu.dispatch, registry);
    gpu["memory"]   = encode_top_op_events(metrics.gpu.memory, registry);
    gpu["counters"] = encode_counters(metrics.gpu.counters, registry);
    json["gpu"]     = std::move(gpu);

    jt::Json cpu;
    cpu["memory"]   = encode_top_op_events(metrics.cpu.memory, registry);
    cpu["counters"] = encode_counters(metrics.cpu.counters, registry);
    json["cpu"]     = std::move(cpu);

    json["thread"] = encode_top_op_events(metrics.thread, registry);
    json["memory"] = encode_top_op_events(metrics.memory, registry);

    jt::Json sub_metrics = make_array();
    for(const SummaryInfo::AggregateMetrics& sub : metrics.sub_metrics)
    {
        sub_metrics.getArray().push_back(encode_aggregate_metrics(sub, registry));
    }
    json["sub_metrics"] = std::move(sub_metrics);
    return json;
}

bool
decode_aggregate_metrics(jt::Json& json, SummaryInfo::AggregateMetrics& metrics,
                         SummaryModel& summary, const TopologyTree& topology)
{
    bool result  = true;
    metrics.type = static_cast<rocprofvis_analysis_summary_aggregation_level_t>(
        json_get_uint64(json, "level"));
    metrics.info.node = nullptr;
    const uint64_t id = json_get_uint64(json, "id");
    switch(metrics.type)
    {
        case kRPVAnalysisSummaryAggregationLevelNode:
            metrics.info.node = topology.GetNode(id);
            result            = metrics.info.node != nullptr;
            break;
        case kRPVAnalysisSummaryAggregationLevelProcessor:
            metrics.info.processor = topology.GetProcessor(id);
            result                 = metrics.info.processor != nullptr;
            break;
        case kRPVAnalysisSummaryAggregationLevelProcess:
            metrics.info.process = topology.GetProcess(id);
            result               = metrics.info.process != nullptr;
            break;
        case kRPVAnalysisSummaryAggregationLevelThread:
            metrics.info.thread = topology.GetThread(
                id, static_cast<ThreadInfo::Kind>(json_get_uint64(json, "thread_kind")));
            result = metrics.info.thread != nullptr;
            break;
        default: break;
    }

    jt::Json& gpu = json["gpu"];
    decode_top_op_events(gpu, "dispatch", metrics.gpu.dispatch, summary);
    decode_top_op_events(gpu, "memory", metrics.gpu.memory, summary);
    decode_counters(gpu, "counters", metrics.gpu.counters, summary, topology);
    jt::Json& cpu = json["cpu"];
    decode_top_op_events(cpu, "memory", metrics.cpu.memory, summary);
    decode_counters(cpu, "counters", metrics.cpu.counters, summary, topology);
    decode_top_op_events(json, "thread", metrics.thread, summary);
    decode_top_op_events(json, "memory", metrics.memory, summary);

    std::vector<jt::Json>& sub_metrics = json_get_array(json, "sub_metrics");
    // Sized before recursing: the summary model keeps pointers into it.
    metrics.sub_metrics.clear();
    metrics.sub_metrics.resize(sub_metrics.size());
    for(size_t i = 0; i < sub_metrics.size(); i++)
    {
        result = decode_aggregate_metrics(sub_metrics[i], metrics.sub_metrics[i], summary,
                                          topology) &&
                 result;
    }
    return result;
}

jt::Json
encode_topology_node_common(const TopologyNode& node)
{
    jt::Json json;
    json["id"]     = json_from_uint64(node.GetId());
    json["name"]   = node.GetName();
    json["header"] = node.GetHeader();
    if(node.HasTrack())
    {
        json["track"] = json_from_uint64(node.GetTrackId());
    }
    return json;
}

void
decode_topology_node_common(jt::Json& json, TopologyTree& topology, TopologyNode& node)
{
    node.SetName(json_get_string(json, "name"));
    std::string header = json_get_string(json, "header");
    if(!header.empty())
    {
        node.SetHeader(header);
    }
    if(json.contains("track"))
    {
        topology.BindTrack(&node, json_get_uint64(json, "track"));
    }
}

jt::Json
encode_topology(const TopologyTree& topology)
{
    jt::Json nodes = make_array();
    for(const TopologyNode* node_base : topology.GetNodes())
    {
        const NodeInfo& node = *static_cast<const NodeInfo*>(node_base);
        jt::Json        node_json  = encode_topology_node_common(node);
        node_json["host_name"]     = node.host_name;
        node_json["os_name"]       = node.os_name;
        node_json["os_release"]    = node.os_release;
        node_json["os_version"]    = node.os_version;

        jt::Json processors = make_array();
        for(const TopologyNode* processor_base : node.GetChildren(TopologyNodeType::kProcessor))
        {
            const ProcessorInfo& processor =
                *static_cast<const ProcessorInfo*>(processor_base);
            jt::Json processor_json      = encode_topology_node_common(processor);
            processor_json["product"]    = processor.product_name;
            processor_json["type"]       = static_cast<long long>(processor.type);
            processor_json["type_index"] = json_from_uint64(processor.type_index);

            jt::Json queues = make_array();
            for(const TopologyNode* queue : processor.GetChildren(TopologyNodeType::kQueue))
            {
                queues.getArray().push_back(encode_topology_node_common(*queue));
            }
            processor_json["queues"] = std::move(queues);

            jt::Json counters = make_array();
            for(const TopologyNode* counter_base :
                processor.GetChildren(TopologyNodeType::kCounter))
            {
                const CounterInfo& counter = *static_cast<const CounterInfo*>(counter_base);
                jt::Json counter_json       = encode_topology_node_common(counter);
                counter_json["description"] = counter.description;
                counter_json["units"]       = counter.units;
                counter_json["value_type"]  = counter.value_type;
                counters.getArray().push_back(std::move(counter_json));
            }
            processor_json["counters"] = std::move(counters);
            processors.getArray().push_back(std::move(processor_json));
        }
        node_json["processors"] = std::move(processors);

        jt::Json processes = make_array();
        for(const TopologyNode* process_base : node.GetChildren(TopologyNodeType::kProcess))
        {
            const ProcessInfo& process = *static_cast<const ProcessInfo*>(process_base);
            jt::Json           process_json = encode_topology_node_common(process);
            process_json["start"]           = json_from_double(process.start_time);
            process_json["end"]             = json_from_double(process.end_time);
            process_json["command"]         = process.command;
            process_json["environment"]     = process.environment;

            jt::Json threads = make_array();
            for(const TopologyNode* thread_base :
                process.GetChildren(TopologyNodeType::kThread))
            {
                const ThreadInfo& thread = *static_cast<const ThreadInfo*>(thread_base);
                jt::Json          thread_json = encode_topology_node_common(thread);
                thread_json["kind"]           = static_cast<int>(thread.kind);
                thread_json["tid"]            = json_from_uint64(thread.tid);
                thread_json["start"]          = json_from_double(thread.start_time);
                thread_json["end"]            = json_from_double(thread.end_time);
                threads.getArray().push_back(std::move(thread_json));
            }
            process_json["threads"] = std::move(threads);

            jt::Json streams = make_array();
            for(const TopologyNode* stream : process.GetChildren(TopologyNodeType::kStream))
            {
                jt::Json stream_json = encode_topology_node_common(*stream);
                jt::Json linked_processors = make_array();
                for(const TopologyNode* processor :
                    stream->GetLinkedChildren(TopologyNodeType::kProcessor))
                {
                    linked_processors.getArray().push_back(
                        json_from_uint64(processor->GetId()));
                }
                stream_json["processors"] = std::move(linked_processors);
                jt::Json linked_queues    = make_array();
                for(const TopologyNode* queue_base :
                    stream->GetLinkedChildren(TopologyNodeType::kQueue))
                {
                    const QueueInfo* queue = static_cast<const QueueInfo*>(queue_base);
                    jt::Json         queue_json;
                    queue_json["id"]        = json_from_uint64(queue->GetId());
                    queue_json["processor"] = json_from_uint64(queue->GetProcessorId());
                    linked_queues.getArray().push_back(std::move(queue_json));
                }
                stream_json["queues"] = std::move(linked_queues);
                streams.getArray().push_back(std::move(stream_json));
            }
            process_json["streams"] = std::move(streams);
            processes.getArray().push_back(std::move(process_json));
        }
        node_json["processes"] = std::move(processes);
        nodes.getArray().push_back(std::move(node_json));
    }
    return nodes;
}

void
decode_topology(std::vector<jt::Json>& nodes, TopologyTree& topology)
{
    struct PendingStream
    {
        StreamInfo* stream;
        jt::Json*   json;
    };
    std::vector<PendingStream> pending_streams;

    for(jt::Json& node_json : nodes)
    {
        NodeInfo* node   = topology.AddNode(json_get_uint64(node_json, "id"));
        node->host_name  = json_get_string(node_json, "host_name");
        node->os_name    = json_get_string(node_json, "os_name");
        node->os_release = json_get_string(node_json, "os_release");
        node->os_version = json_get_string(node_json, "os_version");
        decode_topology_node_common(node_json, topology, *node);

        for(jt::Json& processor_json : json_get_array(node_json, "processors"))
        {
            TopologyId processor_id = {};
            processor_id.value      = json_get_uint64(processor_json, "id");
            ProcessorInfo* processor = topology.AddProcessor(node, processor_id);
            processor->product_name  = json_get_string(processor_json, "product");
            processor->type          = static_cast<rocprofvis_controller_processor_type_t>(
                json_get_uint64(processor_json, "type"));
            processor->type_index = json_get_uint64(processor_json, "type_index");
            decode_topology_node_common(processor_json, topology, *processor);

            for(jt::Json& queue_json : json_get_array(processor_json, "queues"))
            {
                QueueInfo* queue =
                    topology.AddQueue(processor, json_get_uint64(queue_json, "id"));
                decode_topology_node_common(queue_json, topology, *queue);
            }
            for(jt::Json& counter_json : json_get_array(processor_json, "counters"))
            {
                CounterInfo* counter =
                    topology.AddCounter(processor, json_get_uint64(counter_json, "id"));
                counter->description = json_get_string(counter_json, "description");
                counter->units       = json_get_string(counter_json, "units");
                counter->value_type  = json_get_string(counter_json, "value_type");
                decode_topology_node_common(counter_json, topology, *counter);
            }
        }

        for(jt::Json& process_json : json_get_array(node_json, "processes"))
        {
            ProcessInfo* process =
                topology.AddProcess(node, json_get_uint64(process_json, "id"));
            process->start_time  = json_get_double(process_json, "start");
            process->end_time    = json_get_double(process_json, "end");
            process->command     = json_get_string(process_json, "command");
            process->environment = json_get_string(process_json, "environment");
            decode_topology_node_common(process_json, topology, *process);

            for(jt::Json& thread_json : json_get_array(process_json, "threads"))
            {
                ThreadInfo* thread = topology.AddThread(
                    process, json_get_uint64(thread_json, "id"),
                    static_cast<ThreadInfo::Kind>(json_get_uint64(thread_json, "kind")));
                thread->tid        = json_get_uint64(thread_json, "tid");
                thread->start_time = json_get_double(thread_json, "start");
                thread->end_time   = json_get_double(thread_json, "end");
                decode_topology_node_common(thread_json, topology, *thread);
            }
            for(jt::Json& stream_json : json_get_array(process_json, "streams"))
            {
                StreamInfo* stream =
                    topology.AddStream(process, json_get_uint64(stream_json, "id"));
                decode_topology_node_common(stream_json, topology, *stream);
                pending_streams.push_back({ stream, &stream_json });
            }
        }
    }

    // Streams reference processors and queues that only exist once every node
    // has been added, as in DataProvider::LinkStreamTopology.
    for(PendingStream& pending : pending_streams)
    {
        for(uint64_t processor_id : decode_uint64_list(*pending.json, "processors"))
        {
            ProcessorInfo* processor = topology.GetProcessorMutable(processor_id);
            if(processor)
            {
                topology.LinkStreamProcessor(pending.stream, processor);
            }
        }
        for(jt::Json& queue_json : json_get_array(*pending.json, "queues"))
        {
            QueueInfo* queue = topology.GetQueueMutable(
                json_get_uint64(queue_json, "id"), json_get_uint64(queue_json, "processor"));
            if(queue)
            {
                topology.LinkStreamQueue(pending.stream, queue);
            }
        }
    }
    topology.Finalize();
}

jt::Json
encode_track_info(const TrackInfo& track)
{
    jt::Json json;
    json["index"]        = json_from_uint64(track.index);
    json["id"]           = json_from_uint64(track.id);
    json["track_type"]   = static_cast<long long>(track.track_type);
    json["min_ts"]       = json_from_double(track.min_ts);
    json["max_ts"]       = json_from_double(track.max_ts);
    json["num_entries"]  = json_from_uint64(track.num_entries);
    json["instance_id"]  = json_from_uint64(track.instance_id);
    json["file_id"]      = json_from_uint64(track.file_id);
    json["order_rank"]   = json_from_uint64(track.order_rank);
    json["agent_or_pid"] = json_from_uint64(track.agent_or_pid);
    json["queue_or_tid"] = json_from_uint64(track.queue_id_or_tid);
    json["min_value"]    = json_from_double(track.min_value);
    json["max_value"]    = json_from_double(track.max_value);
    json["category"]     = track.category;
    json["main_name"]    = track.main_name;
    json["sub_name"]     = track.sub_name;

    std::vector<uint64_t> operation_types;
    for(rocprofvis_dm_event_operation_t operation : track.operation_types)
    {
        operation_types.push_back(static_cast<uint64_t>(operation));
    }
    std::sort(operation_types.begin(), operation_types.end());
    json["operation_types"] = encode_uint64_list(operation_types);

    jt::Json compare;
    compare["id"]           = track.compare_source.id;
    compare["name"]         = track.compare_source.name;
    compare["path"]         = track.compare_source.path;
    json["compare_source"] = std::move(compare);

    jt::Json topology;
    topology["node_id"]    = json_from_uint64(track.topology.node_id);
    topology["process_id"] = json_from_uint64(track.topology.process_id);
    topology["device_id"]  = json_from_uint64(track.topology.device_id);
    topology["type"]       = static_cast<int>(track.topology.type);
    topology["id"]         = json_from_uint64(track.topology.id.value);
    json["topology"]       = std::move(topology);
    return json;
}

TrackInfo
decode_track_info(jt::Json& json)
{
    TrackInfo track    = {};
    track.index        = json_get_uint64(json, "index");
    track.id           = json_get_uint64(json, "id");
    track.track_type   = static_cast<rocprofvis_controller_track_type_t>(
        json_get_uint64(json, "track_type"));
    track.min_ts          = json_get_double(json, "min_ts");
    track.max_ts          = json_get_double(json, "max_ts");
    track.num_entries     = json_get_uint64(json, "num_entries");
    track.instance_id     = json_get_uint64(json, "instance_id");
    track.file_id         = json_get_uint64(json, "file_id");
    track.order_rank      = json_get_uint64(json, "order_rank");
    track.agent_or_pid    = json_get_uint64(json, "agent_or_pid");
    track.queue_id_or_tid = json_get_uint64(json, "queue_or_tid");
    track.min_value       = json_get_double(json, "min_value");
    track.max_value       = json_get_double(json, "max_value");
    track.graph_handle    = nullptr;
    track.category        = json_get_string(json, "category");
    track.main_name       = json_get_string(json, "main_name");
    track.sub_name        = json_get_string(json, "sub_name");
    for(uint64_t operation : decode_uint64_list(json, "operation_types"))
    {
        track.operation_types.insert(static_cast<rocprofvis_dm_event_operation_t>(operation));
    }
    if(json.contains("compare_source"))
    {
        jt::Json& compare         = json["compare_source"];
        track.compare_source.id   = json_get_string(compare, "id");
        track.compare_source.name = json_get_string(compare, "name");
        track.compare_source.path = json_get_string(compare, "path");
    }
    if(json.contains("topology"))
    {
        jt::Json& topology           = json["topology"];
        track.topology.node_id       = json_get_uint64(topology, "node_id");
        track.topology.process_id    = json_get_uint64(topology, "process_id");
        track.topology.device_id     = json_get_uint64(topology, "device_id");
        track.topology.type          = static_cast<TrackInfo::TrackType>(
            json_get_uint64(topology, "type"));
        track.topology.id.value = json_get_uint64(topology, "id");
    }
    return track;
}

jt::Json
encode_point(const Point& point)
{
    jt::Json json = make_array();
    json.getArray().push_back(json_from_double(point.x));
    json.getArray().push_back(json_from_double(point.y));
    return json;
}

Point
decode_point(jt::Json& object, const char* key)
{
    Point                  point = { 0.0, 0.0 };
    std::vector<jt::Json>& array = json_get_array(object, key);
    if(array.size() >= 2)
    {
        point.x = json_to_double(array[0]);
        point.y = json_to_double(array[1]);
    }
    return point;
}

jt::Json
encode_ceiling(const WorkloadInfo::Roofline::Ceiling& ceiling)
{
    jt::Json json;
    json["bandwidth_type"] = static_cast<long long>(ceiling.bandwidth_type);
    json["compute_type"]   = static_cast<long long>(ceiling.compute_type);
    json["p1"]             = encode_point(ceiling.position.p1);
    json["p2"]             = encode_point(ceiling.position.p2);
    json["throughput"]     = json_from_double(ceiling.throughput);
    return json;
}

WorkloadInfo::Roofline::Ceiling
decode_ceiling(jt::Json& json)
{
    WorkloadInfo::Roofline::Ceiling ceiling = {};
    ceiling.bandwidth_type =
        static_cast<rocprofvis_controller_roofline_ceiling_bandwidth_type_t>(
            json_get_uint64(json, "bandwidth_type"));
    ceiling.compute_type = static_cast<rocprofvis_controller_roofline_ceiling_compute_type_t>(
        json_get_uint64(json, "compute_type"));
    ceiling.position.p1 = decode_point(json, "p1");
    ceiling.position.p2 = decode_point(json, "p2");
    ceiling.throughput  = json_get_double(json, "throughput");
    return ceiling;
}

jt::Json
encode_workload(const WorkloadInfo& workload, const std::string& memory_chart_layout)
{
    jt::Json json;
    json["id"]   = static_cast<long long>(workload.id);
    json["name"] = workload.name;

    jt::Json system_info = make_array();
    for(const std::vector<std::string>& column : workload.system_info)
    {
        system_info.getArray().push_back(encode_string_list(column));
    }
    json["system_info"]     = std::move(system_info);
    jt::Json profiling_config = make_array();
    for(const std::vector<std::string>& column : workload.profiling_config)
    {
        profiling_config.getArray().push_back(encode_string_list(column));
    }
    json["profiling_config"]    = std::move(profiling_config);
    json["memory_chart_layout"] = memory_chart_layout;

    jt::Json metrics = make_array();
    for(const AvailableMetrics::Entry& entry : workload.available_metrics.list)
    {
        jt::Json entry_json;
        entry_json["category_id"] = static_cast<long long>(entry.category_id);
        entry_json["table_id"]    = static_cast<long long>(entry.table_id);
        entry_json["id"]          = static_cast<long long>(entry.id);
        entry_json["name"]        = entry.name;
        entry_json["description"] = entry.description;
        entry_json["unit"]        = entry.unit;
        auto category_it          = workload.available_metrics.tree.find(entry.category_id);
        if(category_it != workload.available_metrics.tree.end())
        {
            entry_json["category_name"] = category_it->second.name;
            auto table_it               = category_it->second.tables.find(entry.table_id);
            if(table_it != category_it->second.tables.end())
            {
                entry_json["table_name"] = table_it->second.name;
            }
        }
        metrics.getArray().push_back(std::move(entry_json));
    }
    json["available_metrics"] = std::move(metrics);

    jt::Json value_names = make_array();
    for(uint32_t category_id : sorted_keys(workload.available_metrics.tree))
    {
        const AvailableMetrics::Category& category =
            workload.available_metrics.tree.at(category_id);
        for(uint32_t table_id : sorted_keys(category.tables))
        {
            const AvailableMetrics::Table& table = category.tables.at(table_id);
            if(table.value_names.empty())
            {
                continue;
            }
            jt::Json table_json;
            table_json["category_id"] = static_cast<long long>(category_id);
            table_json["table_id"]    = static_cast<long long>(table_id);
            table_json["names"]       = encode_string_list(table.value_names);
            value_names.getArray().push_back(std::move(table_json));
        }
    }
    json["value_names"] = std::move(value_names);

    jt::Json kernels = make_array();
    for(uint32_t kernel_id : sorted_keys(workload.kernels))
    {
        const KernelInfo& kernel = workload.kernels.at(kernel_id);
        jt::Json          kernel_json;
        kernel_json["id"]            = static_cast<long long>(kernel.id);
        kernel_json["name"]          = kernel.name;
        kernel_json["has_isa_lines"] = kernel.has_isa_lines;
        kernel_json["dispatch_metrics"] = encode_uint64_list(std::vector<uint64_t>(
            kernel.dispatch_metrics.begin(), kernel.dispatch_metrics.end()));
        jt::Json intensities = make_array();
        for(rocprofvis_controller_roofline_kernel_intensity_type_t type :
            sorted_keys(kernel.roofline.intensities))
        {
            const KernelInfo::Roofline::Intensity& intensity =
                kernel.roofline.intensities.at(type);
            jt::Json intensity_json;
            intensity_json["type"]     = static_cast<long long>(intensity.type);
            intensity_json["position"] = encode_point(intensity.position);
            intensities.getArray().push_back(std::move(intensity_json));
        }
        kernel_json["intensities"] = std::move(intensities);
        kernels.getArray().push_back(std::move(kernel_json));
    }
    json["kernels"] = std::move(kernels);

    jt::Json ceiling_bandwidth = make_array();
    for(rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth_type :
        sorted_keys(workload.roofline.ceiling_bandwidth))
    {
        const auto& by_compute = workload.roofline.ceiling_bandwidth.at(bandwidth_type);
        for(rocprofvis_controller_roofline_ceiling_compute_type_t compute_type :
            sorted_keys(by_compute))
        {
            ceiling_bandwidth.getArray().push_back(encode_ceiling(by_compute.at(compute_type)));
        }
    }
    json["ceiling_bandwidth"] = std::move(ceiling_bandwidth);
    jt::Json ceiling_compute  = make_array();
    for(rocprofvis_controller_roofline_ceiling_compute_type_t compute_type :
        sorted_keys(workload.roofline.ceiling_compute))
    {
        const auto& by_bandwidth = workload.roofline.ceiling_compute.at(compute_type);
        for(rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth_type :
            sorted_keys(by_bandwidth))
        {
            ceiling_compute.getArray().push_back(encode_ceiling(by_bandwidth.at(bandwidth_type)));
        }
    }
    json["ceiling_compute"] = std::move(ceiling_compute);
    return json;
}

void
decode_workload(jt::Json& json, WorkloadInfo& workload)
{
    workload.id   = static_cast<uint32_t>(json_get_uint64(json, "id"));
    workload.name = json_get_string(json, "name");

    workload.system_info.clear();
    for(jt::Json& column : json_get_array(json, "system_info"))
    {
        std::vector<std::string>& values = workload.system_info.emplace_back();
        if(column.isArray())
        {
            for(jt::Json& value : column.getArray())
            {
                values.push_back(json_to_string(value));
            }
        }
    }
    workload.profiling_config.clear();
    for(jt::Json& column : json_get_array(json, "profiling_config"))
    {
        std::vector<std::string>& values = workload.profiling_config.emplace_back();
        if(column.isArray())
        {
            for(jt::Json& value : column.getArray())
            {
                values.push_back(json_to_string(value));
            }
        }
    }

    std::string memory_chart_json = json_get_string(json, "memory_chart_layout");
    if(!memory_chart_json.empty())
    {
        std::string memory_chart_error;
        if(!MemChartLayout::ParseFromString(memory_chart_json, workload.memory_chart_layout,
                                            &memory_chart_error))
        {
            spdlog::warn("Workload {} memory-chart layout blob invalid: {}", workload.id,
                         memory_chart_error);
        }
    }

    // As DataProvider::LoadMetricList: the list is sized first, because each
    // table's entries refer into it.
    std::vector<jt::Json>& metrics = json_get_array(json, "available_metrics");
    workload.available_metrics.list.resize(metrics.size());
    for(size_t j = 0; j < metrics.size(); j++)
    {
        AvailableMetrics::Entry& entry = workload.available_metrics.list[j];
        entry.index                    = j;
        entry.category_id = static_cast<uint32_t>(json_get_uint64(metrics[j], "category_id"));
        entry.table_id    = static_cast<uint32_t>(json_get_uint64(metrics[j], "table_id"));
        entry.id          = static_cast<uint32_t>(json_get_uint64(metrics[j], "id"));
        entry.name        = json_get_string(metrics[j], "name");
        entry.description = json_get_string(metrics[j], "description");
        entry.unit        = json_get_string(metrics[j], "unit");
        AvailableMetrics::Category& category =
            workload.available_metrics.tree[entry.category_id];
        category.id                    = entry.category_id;
        category.name                  = json_get_string(metrics[j], "category_name");
        AvailableMetrics::Table& table = category.tables[entry.table_id];
        table.id                       = entry.table_id;
        table.name                     = json_get_string(metrics[j], "table_name");
        table.entries.insert({ entry.id, entry });
    }
    for(jt::Json& table_json : json_get_array(json, "value_names"))
    {
        auto category_it = workload.available_metrics.tree.find(
            static_cast<uint32_t>(json_get_uint64(table_json, "category_id")));
        if(category_it == workload.available_metrics.tree.end())
        {
            continue;
        }
        auto table_it = category_it->second.tables.find(
            static_cast<uint32_t>(json_get_uint64(table_json, "table_id")));
        if(table_it != category_it->second.tables.end())
        {
            table_it->second.value_names = decode_string_list(table_json, "names");
        }
    }

    for(jt::Json& kernel_json : json_get_array(json, "kernels"))
    {
        KernelInfo kernel;
        kernel.id               = static_cast<uint32_t>(json_get_uint64(kernel_json, "id"));
        kernel.name             = json_get_string(kernel_json, "name");
        kernel.has_isa_lines    = json_get_bool(kernel_json, "has_isa_lines");
        kernel.dispatch_metrics = {};
        std::vector<uint64_t> dispatch_metrics =
            decode_uint64_list(kernel_json, "dispatch_metrics");
        for(size_t i = 0; i < dispatch_metrics.size() && i < kernel.dispatch_metrics.size();
            i++)
        {
            kernel.dispatch_metrics[i] = dispatch_metrics[i];
        }
        for(jt::Json& intensity_json : json_get_array(kernel_json, "intensities"))
        {
            KernelInfo::Roofline::Intensity intensity;
            intensity.type = static_cast<rocprofvis_controller_roofline_kernel_intensity_type_t>(
                json_get_uint64(intensity_json, "type"));
            intensity.position = decode_point(intensity_json, "position");
            kernel.roofline.intensities[intensity.type] = intensity;
        }
        workload.kernels[kernel.id] = std::move(kernel);
    }

    for(jt::Json& ceiling_json : json_get_array(json, "ceiling_bandwidth"))
    {
        WorkloadInfo::Roofline::Ceiling ceiling = decode_ceiling(ceiling_json);
        workload.roofline.ceiling_bandwidth[ceiling.bandwidth_type][ceiling.compute_type] =
            ceiling;
    }
    for(jt::Json& ceiling_json : json_get_array(json, "ceiling_compute"))
    {
        WorkloadInfo::Roofline::Ceiling ceiling = decode_ceiling(ceiling_json);
        workload.roofline.ceiling_compute[ceiling.compute_type][ceiling.bandwidth_type] =
            ceiling;
    }
}

}  // namespace

std::string
server_token_file_name(int port)
{
    return "server-token-" + std::to_string(port);
}

jt::Json
json_from_uint64(uint64_t value)
{
    return jt::Json(static_cast<long long>(value));
}

jt::Json
json_from_double(double value)
{
    return jt::Json(value);
}

uint64_t
json_to_uint64(jt::Json& value)
{
    uint64_t result = 0;
    if(value.isLong())
    {
        result = static_cast<uint64_t>(value.getLong());
    }
    else if(value.isDouble() || value.isFloat())
    {
        double number = value.getNumber();
        result        = number > 0.0 ? static_cast<uint64_t>(number) : 0;
    }
    else if(value.isBool())
    {
        result = value.getBool() ? 1 : 0;
    }
    return result;
}

double
json_to_double(jt::Json& value)
{
    double result = 0.0;
    if(value.isNumber())
    {
        result = value.getNumber();
    }
    else if(value.isNull())
    {
        result = std::numeric_limits<double>::quiet_NaN();
    }
    return result;
}

std::string
json_to_string(jt::Json& value)
{
    return value.isString() ? value.getString() : std::string();
}

uint64_t
json_get_uint64(jt::Json& object, const char* key)
{
    return object.contains(key) ? json_to_uint64(object[key]) : 0;
}

int64_t
json_get_int64(jt::Json& object, const char* key)
{
    return static_cast<int64_t>(json_get_uint64(object, key));
}

double
json_get_double(jt::Json& object, const char* key)
{
    return object.contains(key) ? json_to_double(object[key]) : 0.0;
}

bool
json_get_bool(jt::Json& object, const char* key)
{
    bool result = false;
    if(object.contains(key))
    {
        jt::Json& value = object[key];
        result          = value.isBool() ? value.getBool() : json_to_uint64(value) != 0;
    }
    return result;
}

std::string
json_get_string(jt::Json& object, const char* key)
{
    return object.contains(key) ? json_to_string(object[key]) : std::string();
}

std::vector<jt::Json>&
json_get_array(jt::Json& object, const char* key)
{
    thread_local std::vector<jt::Json> tls_empty_array;
    if(object.contains(key) && object[key].isArray())
    {
        return object[key].getArray();
    }
    tls_empty_array.clear();
    return tls_empty_array;
}

std::string
encode_message(const jt::Json& message)
{
    return message.toString();
}

bool
decode_message(const std::string& payload, jt::Json& message)
{
    std::pair<jt::Json::Status, jt::Json> parsed = jt::Json::parse(payload);
    if(parsed.first != jt::Json::success || !parsed.second.isObject())
    {
        spdlog::warn("Discarding malformed message: {}",
                     jt::Json::StatusToString(parsed.first));
        return false;
    }
    message = std::move(parsed.second);
    return true;
}

jt::Json
encode_track_request(const TrackRequestParams& params)
{
    jt::Json json;
    json["track_id"]    = static_cast<long long>(params.m_track_id);
    json["start"]       = json_from_double(params.m_start_ts);
    json["end"]         = json_from_double(params.m_end_ts);
    json["pixels"]      = static_cast<long long>(params.m_horz_pixel_range);
    json["group"]       = static_cast<long long>(params.m_data_group_id);
    json["chunk_index"] = static_cast<long long>(params.m_chunk_index);
    json["chunk_count"] = json_from_uint64(params.m_chunk_count);
    return json;
}

TrackRequestParams
decode_track_request(jt::Json& params)
{
    return TrackRequestParams(static_cast<uint32_t>(json_get_uint64(params, "track_id")),
                              json_get_double(params, "start"),
                              json_get_double(params, "end"),
                              static_cast<uint32_t>(json_get_uint64(params, "pixels")),
                              static_cast<uint8_t>(json_get_uint64(params, "group")),
                              static_cast<uint16_t>(json_get_uint64(params, "chunk_index")),
                              static_cast<size_t>(json_get_uint64(params, "chunk_count")));
}

jt::Json
encode_track_table_request(const TrackTableRequestParams& params)
{
    jt::Json json;
    encode_table_common(params, json);
    json["track_ids"] = encode_uint64_list(params.m_track_ids);
    return json;
}

TrackTableRequestParams
decode_track_table_request(jt::Json& params)
{
    TrackTableRequestParams result;
    decode_table_common(params, result);
    result.m_track_ids = decode_uint64_list(params, "track_ids");
    return result;
}

jt::Json
encode_event_search_request(const EventSearchRequestParams& params)
{
    jt::Json json;
    encode_table_common(params, json);
    std::vector<uint64_t> op_types;
    for(rocprofvis_dm_event_operation_t op : params.m_op_types)
    {
        op_types.push_back(static_cast<uint64_t>(op));
    }
    json["op_types"]           = encode_uint64_list(op_types);
    json["string_filters"]     = encode_string_list(params.m_string_table_filters);
    json["include_substrings"] = params.m_include_substrings;
    json["include_category"]   = params.m_include_category;
    json["partial_matching"]   = params.m_partial_matching;
    return json;
}

EventSearchRequestParams
decode_event_search_request(jt::Json& params)
{
    EventSearchRequestParams result;
    decode_table_common(params, result);
    result.m_op_types.clear();
    for(uint64_t op : decode_uint64_list(params, "op_types"))
    {
        result.m_op_types.push_back(static_cast<rocprofvis_dm_event_operation_t>(op));
    }
    result.m_string_table_filters = decode_string_list(params, "string_filters");
    result.m_include_substrings   = json_get_bool(params, "include_substrings");
    result.m_include_category     = json_get_bool(params, "include_category");
    result.m_partial_matching     = json_get_bool(params, "partial_matching");
    return result;
}

jt::Json
encode_track_statistics_request(const AnalysisTrackStatisticsRequestParams& params)
{
    jt::Json json;
    json["track_id"] = json_from_uint64(params.m_track_id);
    json["start"]    = json_from_double(params.m_start_ts);
    json["end"]      = json_from_double(params.m_end_ts);
    return json;
}

AnalysisTrackStatisticsRequestParams
decode_track_statistics_request(jt::Json& params)
{
    return AnalysisTrackStatisticsRequestParams(json_get_uint64(params, "track_id"),
                                                json_get_double(params, "start"),
                                                json_get_double(params, "end"));
}

jt::Json
encode_metrics_request(const MetricsRequestParams& params)
{
    jt::Json json;
    json["workload_id"] = static_cast<long long>(params.m_workload_id);
    json["client_id"]   = json_from_uint64(params.m_client_id);
    json["kernel_ids"]  = encode_uint64_list(
        std::vector<uint64_t>(params.m_kernel_ids.begin(), params.m_kernel_ids.end()));
    jt::Json metric_ids = make_array();
    for(const MetricsRequestParams::MetricID& metric_id : params.m_metric_ids)
    {
        jt::Json id_json;
        id_json["category_id"] = static_cast<long long>(metric_id.category_id);
        if(metric_id.table_id)
        {
            id_json["table_id"] = static_cast<long long>(metric_id.table_id.value());
        }
        if(metric_id.entry_id)
        {
            id_json["entry_id"] = static_cast<long long>(metric_id.entry_id.value());
        }
        metric_ids.getArray().push_back(std::move(id_json));
    }
    json["metric_ids"] = std::move(metric_ids);
    return json;
}

MetricsRequestParams
decode_metrics_request(jt::Json& params)
{
    std::vector<uint32_t> kernel_ids;
    for(uint64_t kernel_id : decode_uint64_list(params, "kernel_ids"))
    {
        kernel_ids.push_back(static_cast<uint32_t>(kernel_id));
    }
    std::vector<MetricsRequestParams::MetricID> metric_ids;
    for(jt::Json& id_json : json_get_array(params, "metric_ids"))
    {
        MetricsRequestParams::MetricID metric_id;
        metric_id.category_id = static_cast<uint32_t>(json_get_uint64(id_json, "category_id"));
        if(id_json.contains("table_id"))
        {
            metric_id.table_id = static_cast<uint32_t>(json_get_uint64(id_json, "table_id"));
        }
        if(id_json.contains("entry_id"))
        {
            metric_id.entry_id = static_cast<uint32_t>(json_get_uint64(id_json, "entry_id"));
        }
        metric_ids.push_back(metric_id);
    }
    return MetricsRequestParams(static_cast<uint32_t>(json_get_uint64(params, "workload_id")),
                                kernel_ids, metric_ids, json_get_uint64(params, "client_id"));
}

jt::Json
encode_compute_table_request(const ComputeTableRequestParams& params)
{
    jt::Json json;
    json["workload_id"]      = static_cast<long long>(params.m_workload_id);
    json["metric_selectors"] = encode_string_list(params.m_metric_selectors);
    json["sort_column"]      = json_from_uint64(params.m_sort_column_index);
    json["sort_order"]       = static_cast<long long>(params.m_sort_order);
    jt::Json filters         = make_array();
    for(const auto& filter : params.m_column_filters)
    {
        jt::Json filter_json;
        filter_json["column"]     = json_from_uint64(filter.first);
        filter_json["expression"] = filter.second;
        filters.getArray().push_back(std::move(filter_json));
    }
    json["column_filters"] = std::move(filters);
    return json;
}

ComputeTableRequestParams
decode_compute_table_request(jt::Json& params)
{
    std::unordered_map<uint64_t, std::string> column_filters;
    for(jt::Json& filter_json : json_get_array(params, "column_filters"))
    {
        column_filters[json_get_uint64(filter_json, "column")] =
            json_get_string(filter_json, "expression");
    }
    return ComputeTableRequestParams(
        static_cast<uint32_t>(json_get_uint64(params, "workload_id")),
        decode_string_list(params, "metric_selectors"),
        json_get_uint64(params, "sort_column"),
        static_cast<rocprofvis_controller_sort_order_t>(json_get_uint64(params, "sort_order")),
        column_filters);
}

jt::Json
encode_pc_sampling_request(const PcSamplingRequestParams& params)
{
    jt::Json json;
    json["layer"]            = static_cast<long long>(params.m_layer);
    json["workload_id"]      = static_cast<long long>(params.m_workload_id);
    json["kernel_id"]        = static_cast<long long>(params.m_kernel_id);
    json["source_file_uuid"] = json_from_uint64(params.m_source_file_uuid);
    json["generation"]       = static_cast<long long>(params.m_generation);
    json["request_token"]    = json_from_uint64(params.m_request_token);
    return json;
}

PcSamplingRequestParams
decode_pc_sampling_request(jt::Json& params)
{
    return PcSamplingRequestParams(
        static_cast<PcSamplingLayer>(json_get_uint64(params, "layer")),
        static_cast<uint32_t>(json_get_uint64(params, "workload_id")),
        static_cast<uint32_t>(json_get_uint64(params, "kernel_id")),
        json_get_uint64(params, "source_file_uuid"),
        static_cast<uint32_t>(json_get_uint64(params, "generation")),
        json_get_uint64(params, "request_token"));
}

jt::Json
encode_system_trace(const TraceDataModel& model)
{
    const TimelineModel& timeline = model.GetTimeline();
    jt::Json             json;
    json["start"]        = json_from_double(timeline.GetStartTime());
    json["end"]          = json_from_double(timeline.GetEndTime());
    json["track_count"]  = json_from_uint64(timeline.GetTrackCount());
    json["buckets"]      = json_from_uint64(timeline.GetHistogram().size());

    std::vector<const TrackInfo*> tracks = timeline.GetTrackList();
    jt::Json                      tracks_json = make_array();
    for(const TrackInfo* track : tracks)
    {
        if(track)
        {
            tracks_json.getArray().push_back(encode_track_info(*track));
        }
    }
    json["tracks"] = std::move(tracks_json);

    // Keyed by graph index, as the load wrote it; event tracks also sum into
    // the histogram, which the receiver rebuilds the same way.
    jt::Json mini_map = make_array();
    for(const auto& entry : timeline.GetMiniMap())
    {
        jt::Json entry_json;
        entry_json["key"]    = json_from_uint64(entry.first);
        entry_json["events"] = entry.first < tracks.size() && tracks[entry.first] &&
                               tracks[entry.first]->track_type ==
                                   kRPVControllerTrackTypeEvents;
        entry_json["values"] = encode_double_list(entry.second);
        mini_map.getArray().push_back(std::move(entry_json));
    }
    json["mini_map"] = std::move(mini_map);
    json["topology"] = encode_topology(model.GetTopology());
    return json;
}

bool
decode_system_trace(jt::Json& data, TraceDataModel& model)
{
    TopologyTree& topology = model.GetTopology();
    topology.Clear();
    decode_topology(json_get_array(data, "topology"), topology);

    // Mirrors DataProvider::ProcessLoadSystemTrace.
    TimelineModel& timeline    = model.GetTimeline();
    const uint64_t num_buckets = json_get_uint64(data, "buckets");
    timeline.ResizeHistogram(num_buckets);
    timeline.SetTrackCount(json_get_uint64(data, "track_count"));
    std::vector<double>& histogram = timeline.GetHistogram();
    histogram.assign(num_buckets, 0.0);
    std::map<uint64_t, std::vector<double>> mini_map;
    for(jt::Json& entry_json : json_get_array(data, "mini_map"))
    {
        std::vector<double> values = decode_double_list(entry_json, "values");
        if(json_get_bool(entry_json, "events"))
        {
            for(size_t bin = 0; bin < values.size() && bin < histogram.size(); bin++)
            {
                histogram[bin] += values[bin];
            }
        }
        mini_map[json_get_uint64(entry_json, "key")] = std::move(values);
    }
    timeline.SetMiniMap(std::move(mini_map));
    if(!histogram.empty())
    {
        timeline.NormalizeHistogram();
    }
    timeline.SetTimeRange(json_get_double(data, "start"), json_get_double(data, "end"));

    timeline.ClearTrackMetaData();
    timeline.FreeAllTrackData();
    for(jt::Json& track_json : json_get_array(data, "tracks"))
    {
        TrackInfo track = decode_track_info(track_json);
        uint64_t  id    = track.id;
        timeline.AddTrackMetaData(id, std::move(track));
    }
    return true;
}

jt::Json
encode_track_data(const TrackDataResult& result)
{
    jt::Json json;
    if(!result.events.empty())
    {
        std::unordered_map<std::string, uint64_t> string_index;
        std::vector<std::string>                  strings;
        auto intern = [&](const std::string& value) -> uint64_t {
            auto it = string_index.find(value);
            if(it != string_index.end())
            {
                return it->second;
            }
            uint64_t index = strings.size();
            strings.push_back(value);
            string_index.emplace(value, index);
            return index;
        };

        std::vector<uint64_t> ids;
        std::vector<double>   start;
        std::vector<double>   duration;
        std::vector<uint64_t> level;
        std::vector<uint64_t> child_count;
        std::vector<uint64_t> name;
        std::vector<uint64_t> top_combined_name;
        const size_t          count = result.events.size();
        ids.reserve(count);
        start.reserve(count);
        duration.reserve(count);
        level.reserve(count);
        child_count.reserve(count);
        name.reserve(count);
        top_combined_name.reserve(count);
        for(const TraceEvent& event : result.events)
        {
            ids.push_back(event.m_id.uuid);
            start.push_back(event.m_start_ts);
            duration.push_back(event.m_duration);
            level.push_back(event.m_level);
            child_count.push_back(event.m_child_count);
            name.push_back(intern(event.m_name));
            top_combined_name.push_back(intern(event.m_top_combined_name));
        }
        json["id"]                = encode_uint64_list(ids);
        json["start"]             = encode_double_list(start);
        json["duration"]          = encode_double_list(duration);
        json["level"]             = encode_uint64_list(level);
        json["child_count"]       = encode_uint64_list(child_count);
        json["name"]              = encode_uint64_list(name);
        json["top_combined_name"] = encode_uint64_list(top_combined_name);
        json["strings"]           = encode_string_list(strings);
    }
    if(!result.samples.empty())
    {
        std::vector<double> start;
        std::vector<double> end;
        std::vector<double> value;
        start.reserve(result.samples.size());
        end.reserve(result.samples.size());
        value.reserve(result.samples.size());
        for(const TraceCounter& sample : result.samples)
        {
            start.push_back(sample.m_start_ts);
            end.push_back(sample.m_end_ts);
            value.push_back(sample.m_value);
        }
        json["sample_start"] = encode_double_list(start);
        json["sample_end"]   = encode_double_list(end);
        json["sample_value"] = encode_double_list(value);
    }
    if(!json.isObject())
    {
        json.setObject();
    }
    return json;
}

void
decode_track_data(jt::Json& data, TrackDataResult& result)
{
    result.events.clear();
    result.samples.clear();

    std::vector<uint64_t>    ids               = decode_uint64_list(data, "id");
    std::vector<double>      start             = decode_double_list(data, "start");
    std::vector<double>      duration          = decode_double_list(data, "duration");
    std::vector<uint64_t>    level             = decode_uint64_list(data, "level");
    std::vector<uint64_t>    child_count       = decode_uint64_list(data, "child_count");
    std::vector<uint64_t>    name              = decode_uint64_list(data, "name");
    std::vector<uint64_t>    top_combined_name = decode_uint64_list(data, "top_combined_name");
    std::vector<std::string> strings           = decode_string_list(data, "strings");
    const size_t             count             = ids.size();
    if(start.size() == count && duration.size() == count && level.size() == count &&
       child_count.size() == count && name.size() == count &&
       top_combined_name.size() == count)
    {
        result.events.resize(count);
        for(size_t i = 0; i < count; i++)
        {
            TraceEvent& event   = result.events[i];
            event.m_id.uuid     = ids[i];
            event.m_start_ts    = start[i];
            event.m_duration    = duration[i];
            event.m_level       = static_cast<uint32_t>(level[i]);
            event.m_child_count = static_cast<uint32_t>(child_count[i]);
            if(name[i] < strings.size())
            {
                event.m_name = strings[name[i]];
            }
            if(top_combined_name[i] < strings.size())
            {
                event.m_top_combined_name = strings[top_combined_name[i]];
            }
        }
    }

    std::vector<double> sample_start = decode_double_list(data, "sample_start");
    std::vector<double> sample_end   = decode_double_list(data, "sample_end");
    std::vector<double> sample_value = decode_double_list(data, "sample_value");
    if(sample_end.size() == sample_start.size() && sample_value.size() == sample_start.size())
    {
        result.samples.resize(sample_start.size());
        for(size_t i = 0; i < sample_start.size(); i++)
        {
            result.samples[i].m_start_ts = sample_start[i];
            result.samples[i].m_end_ts   = sample_end[i];
            result.samples[i].m_value    = sample_value[i];
        }
    }
}

jt::Json
encode_table(const TableInfo& table)
{
    jt::Json              json;
    std::vector<uint64_t> column_types;
    for(rocprofvis_controller_primitive_type_t type : table.table_column_types)
    {
        column_types.push_back(static_cast<uint64_t>(type));
    }
    json["header"]       = encode_string_list(table.table_header);
    json["column_types"] = encode_uint64_list(column_types);
    json["rows"]         = encode_string_table(table.table_data);
    json["total_rows"]   = json_from_uint64(table.total_row_count);
    return json;
}

void
decode_table(jt::Json& data, std::vector<std::string>& header,
             std::vector<rocprofvis_controller_primitive_type_t>& column_types,
             std::vector<std::vector<std::string>>& rows, uint64_t& total_rows)
{
    header = decode_string_list(data, "header");
    column_types.clear();
    for(uint64_t type : decode_uint64_list(data, "column_types"))
    {
        column_types.push_back(static_cast<rocprofvis_controller_primitive_type_t>(type));
    }
    rows       = decode_string_table(data, "rows");
    total_rows = json_get_uint64(data, "total_rows");
}

jt::Json
encode_event_ext_data(const EventExtDataResult& result)
{
    jt::Json rows = make_array();
    for(const EventExtDataRow& row : result.rows)
    {
        jt::Json row_json;
        row_json["category_enum"] = json_from_uint64(row.category_enum);
        row_json["data_type"]     = json_from_uint64(row.data_type);
        row_json["category"]      = row.category;
        row_json["name"]          = row.name;
        row_json["value"]         = row.value;
        if(row.has_uint_value)
        {
            row_json["uint_value"] = json_from_uint64(row.uint_value);
        }
        row_json["arg_position"] = static_cast<long long>(row.arg_position);
        row_json["arg_type"]     = row.arg_type;
        rows.getArray().push_back(std::move(row_json));
    }
    jt::Json json;
    json["rows"] = std::move(rows);
    return json;
}

void
decode_event_ext_data(jt::Json& data, EventExtDataResult& result)
{
    result.rows.clear();
    for(jt::Json& row_json : json_get_array(data, "rows"))
    {
        EventExtDataRow row = {};
        row.category_enum   = json_get_uint64(row_json, "category_enum");
        row.data_type       = json_get_uint64(row_json, "data_type");
        row.category        = json_get_string(row_json, "category");
        row.name            = json_get_string(row_json, "name");
        row.value           = json_get_string(row_json, "value");
        row.has_uint_value  = row_json.contains("uint_value");
        row.uint_value      = json_get_uint64(row_json, "uint_value");
        row.arg_position    = static_cast<uint16_t>(json_get_uint64(row_json, "arg_position"));
        row.arg_type        = json_get_string(row_json, "arg_type");
        result.rows.push_back(std::move(row));
    }
}

jt::Json
encode_event_flow(const EventFlowResult& result)
{
    jt::Json rows = make_array();
    for(const EventFlowData& row : result.rows)
    {
        jt::Json row_json;
        row_json["id"]        = json_from_uint64(row.id.uuid);
        row_json["start"]     = json_from_uint64(row.start_timestamp);
        row_json["end"]       = json_from_uint64(row.end_timestamp);
        row_json["track_id"]  = json_from_uint64(row.track_id);
        row_json["level"]     = json_from_uint64(row.level);
        row_json["direction"] = json_from_uint64(row.direction);
        row_json["name"]      = row.name;
        rows.getArray().push_back(std::move(row_json));
    }
    jt::Json json;
    json["rows"] = std::move(rows);
    return json;
}

void
decode_event_flow(jt::Json& data, EventFlowResult& result)
{
    result.rows.clear();
    for(jt::Json& row_json : json_get_array(data, "rows"))
    {
        EventFlowData row   = {};
        row.id.uuid         = json_get_uint64(row_json, "id");
        row.start_timestamp = json_get_uint64(row_json, "start");
        row.end_timestamp   = json_get_uint64(row_json, "end");
        row.track_id        = json_get_uint64(row_json, "track_id");
        row.level           = json_get_uint64(row_json, "level");
        row.direction       = json_get_uint64(row_json, "direction");
        row.name            = json_get_string(row_json, "name");
        result.rows.push_back(std::move(row));
    }
}

jt::Json
encode_event_call_stack(const EventCallStackResult& result)
{
    jt::Json rows = make_array();
    for(const CallStackRow& row : result.rows)
    {
        jt::Json row_json;
        row_json["file"]    = row.file;
        row_json["pc"]      = row.pc;
        row_json["name"]    = row.name;
        row_json["address"] = row.address;
        if(row.has_region_id)
        {
            row_json["region_id"] = json_from_uint64(row.region_id);
        }
        rows.getArray().push_back(std::move(row_json));
    }
    jt::Json json;
    json["rows"] = std::move(rows);
    return json;
}

void
decode_event_call_stack(jt::Json& data, EventCallStackResult& result)
{
    result.rows.clear();
    for(jt::Json& row_json : json_get_array(data, "rows"))
    {
        CallStackRow row  = {};
        row.file          = json_get_string(row_json, "file");
        row.pc            = json_get_string(row_json, "pc");
        row.name          = json_get_string(row_json, "name");
        row.address       = json_get_string(row_json, "address");
        row.has_region_id = row_json.contains("region_id");
        row.region_id     = json_get_uint64(row_json, "region_id");
        result.rows.push_back(std::move(row));
    }
}

jt::Json
encode_summary(const SummaryModel& summary)
{
    SummaryValueRegistry registry;
    registry.event_values.insert(summary.GetEventValues().begin(),
                                 summary.GetEventValues().end());
    registry.counter_values.insert(summary.GetCounterValues().begin(),
                                   summary.GetCounterValues().end());
    registry.timeunit_values.insert(summary.GetTimeunitValues().begin(),
                                    summary.GetTimeunitValues().end());
    return encode_aggregate_metrics(summary.GetSummaryData(), registry);
}

bool
decode_summary(jt::Json& data, SummaryModel& summary, const TopologyTree& topology)
{
    return decode_aggregate_metrics(data, summary.GetSummaryData(), summary, topology);
}

jt::Json
encode_track_statistics(const AnalysisTrackStatisticsRequestParams& params)
{
    // The output is a union; the receiver knows which member from the track type.
    jt::Json json;
    json["queue_util"] = json_from_double(params.m_output.queue_util);
    json["min"]        = json_from_double(params.m_output.counter_stats.min_value);
    json["max"]        = json_from_double(params.m_output.counter_stats.max_value);
    json["mean"]       = json_from_double(params.m_output.counter_stats.mean_value);
    json["std_dev"]    = json_from_double(params.m_output.counter_stats.std_dev);
    return json;
}

void
decode_track_statistics(jt::Json& data, AnalysisTrackStatisticsRequestParams& params,
                        bool is_counter)
{
    if(is_counter)
    {
        params.m_output.counter_stats.min_value  = json_get_double(data, "min");
        params.m_output.counter_stats.max_value  = json_get_double(data, "max");
        params.m_output.counter_stats.mean_value = json_get_double(data, "mean");
        params.m_output.counter_stats.std_dev    = json_get_double(data, "std_dev");
    }
    else
    {
        params.m_output.queue_util = json_get_double(data, "queue_util");
    }
}

jt::Json
encode_compute_trace(const ComputeDataModel&                              model,
                     const std::vector<std::pair<uint32_t, std::string>>& memory_chart_layouts)
{
    jt::Json            json;
    const AnalysisInfo& analysis_info = model.GetAnalysisInfo();
    jt::Json            analysis;
    analysis["profiler_version"]     = analysis_info.profiler_version;
    analysis["profiler_git_version"] = analysis_info.profiler_git_version;
    analysis["schema_version"]       = analysis_info.schema_version;
    json["analysis_info"]            = std::move(analysis);

    jt::Json workloads = make_array();
    for(const WorkloadInfo* workload : model.GetWorkloadList())
    {
        std::string layout;
        for(const std::pair<uint32_t, std::string>& entry : memory_chart_layouts)
        {
            if(entry.first == workload->id)
            {
                layout = entry.second;
                break;
            }
        }
        workloads.getArray().push_back(encode_workload(*workload, layout));
    }
    json["workloads"] = std::move(workloads);
    return json;
}

bool
decode_compute_trace(jt::Json& data, ComputeDataModel& model)
{
    // Mirrors DataProvider::ProcessLoadComputeTrace.
    AnalysisInfo analysis_info;
    if(data.contains("analysis_info"))
    {
        jt::Json& analysis                 = data["analysis_info"];
        analysis_info.profiler_version     = json_get_string(analysis, "profiler_version");
        analysis_info.profiler_git_version = json_get_string(analysis, "profiler_git_version");
        analysis_info.schema_version       = json_get_string(analysis, "schema_version");
    }
    model.SetAnalysisInfo(analysis_info);
    for(jt::Json& workload_json : json_get_array(data, "workloads"))
    {
        WorkloadInfo workload;
        decode_workload(workload_json, workload);
        model.AddWorkload(workload);
    }
    return true;
}

jt::Json
encode_metrics(const MetricsResult& result)
{
    jt::Json rows = make_array();
    for(const MetricValueRow& row : result.rows)
    {
        jt::Json row_json;
        row_json["source_type"] = static_cast<long long>(row.source_type);
        row_json["source_id"]   = static_cast<long long>(row.source_id);
        row_json["category_id"] = static_cast<long long>(row.category_id);
        row_json["table_id"]    = static_cast<long long>(row.table_id);
        row_json["entry_id"]    = static_cast<long long>(row.entry_id);
        row_json["value_name"]  = row.value_name;
        row_json["value"]       = json_from_double(row.value);
        rows.getArray().push_back(std::move(row_json));
    }
    jt::Json json;
    json["rows"] = std::move(rows);
    return json;
}

void
decode_metrics(jt::Json& data, MetricsResult& result)
{
    result.rows.clear();
    for(jt::Json& row_json : json_get_array(data, "rows"))
    {
        MetricValueRow row;
        row.source_type = static_cast<rocprofvis_controller_metric_source_type_t>(
            json_get_uint64(row_json, "source_type"));
        row.source_id   = static_cast<uint32_t>(json_get_uint64(row_json, "source_id"));
        row.category_id = static_cast<uint32_t>(json_get_uint64(row_json, "category_id"));
        row.table_id    = static_cast<uint32_t>(json_get_uint64(row_json, "table_id"));
        row.entry_id    = static_cast<uint32_t>(json_get_uint64(row_json, "entry_id"));
        row.value_name  = json_get_string(row_json, "value_name");
        row.value       = json_get_double(row_json, "value");
        result.rows.push_back(std::move(row));
    }
}

jt::Json
encode_kernel_selection_table(const ComputeTableInfo& table)
{
    jt::Json json;
    json["header"] = encode_string_list(table.table_header);
    json["rows"]   = encode_string_table(table.table_data);
    return json;
}

void
decode_kernel_selection_table(jt::Json& data, ComputeTableInfo& table)
{
    table.table_header = decode_string_list(data, "header");
    table.table_data   = decode_string_table(data, "rows");
}

jt::Json
encode_pc_sampling(const PcSamplingData& data, PcSamplingLayer layer)
{
    jt::Json json;
    switch(layer)
    {
        case PcSamplingLayer::kIsa:
        {
            jt::Json code_objects = make_array();
            for(const CodeObjectStore& code_object : data.code_objects)
            {
                jt::Json code_object_json;
                code_object_json["uuid"] = json_from_uint64(code_object.code_object_uuid);
                jt::Json symbols         = make_array();
                for(const KernelSymbol& symbol : code_object.kernel_symbols)
                {
                    jt::Json symbol_json;
                    symbol_json["uuid"] = json_from_uint64(symbol.kernel_symbol_uuid);
                    symbol_json["code_object_uuid"] =
                        json_from_uint64(symbol.code_object_uuid);
                    jt::Json lines = make_array();
                    for(const InstructionLine& line : symbol.instruction_lines)
                    {
                        jt::Json line_json;
                        line_json["uuid"] = json_from_uint64(line.instruction_uuid);
                        if(line.instruction_type_uuid)
                        {
                            line_json["type_uuid"] =
                                json_from_uint64(line.instruction_type_uuid.value());
                        }
                        line_json["offset"]      = json_from_uint64(line.code_object_offset);
                        line_json["instruction"] = line.instruction;
                        lines.getArray().push_back(std::move(line_json));
                    }
                    symbol_json["lines"] = std::move(lines);
                    symbols.getArray().push_back(std::move(symbol_json));
                }
                code_object_json["symbols"] = std::move(symbols);
                code_objects.getArray().push_back(std::move(code_object_json));
            }
            json["code_objects"] = std::move(code_objects);
            jt::Json types       = make_array();
            for(uint64_t uuid : sorted_keys(data.instruction_type_lookup_map))
            {
                jt::Json type_json;
                type_json["uuid"] = json_from_uint64(uuid);
                type_json["text"] = data.instruction_type_lookup_map.at(uuid);
                types.getArray().push_back(std::move(type_json));
            }
            json["instruction_types"] = std::move(types);
            break;
        }
        case PcSamplingLayer::kSource:
        {
            jt::Json files = make_array();
            for(const SourceFile& file : data.source_files)
            {
                jt::Json file_json;
                file_json["uuid"] = json_from_uint64(file.source_file_uuid);
                file_json["path"] = file.file_path;
                jt::Json lines    = make_array();
                for(const SourceLine& line : file.source_lines)
                {
                    jt::Json line_json;
                    line_json["uuid"]    = json_from_uint64(line.source_line_uuid);
                    line_json["number"]  = json_from_uint64(line.line_number);
                    line_json["content"] = line.content;
                    lines.getArray().push_back(std::move(line_json));
                }
                file_json["lines"] = std::move(lines);
                files.getArray().push_back(std::move(file_json));
            }
            json["source_files"] = std::move(files);
            jt::Json mappings    = make_array();
            for(const InstructionSourceLine& mapping : data.instruction_source_lines)
            {
                jt::Json mapping_json;
                mapping_json["instruction_uuid"] = json_from_uint64(mapping.instruction_uuid);
                mapping_json["source_line_uuid"] = json_from_uint64(mapping.source_line_uuid);
                mapping_json["source_file_uuid"] = json_from_uint64(mapping.source_file_uuid);
                mapping_json["frame_index"]      = json_from_uint64(mapping.frame_index);
                mappings.getArray().push_back(std::move(mapping_json));
            }
            json["instruction_source_lines"] = std::move(mappings);
            break;
        }
        case PcSamplingLayer::kStalls:
        {
            json["sample_states_loaded"] = data.sample_states_loaded;
            json["stall_details_loaded"] = data.stall_details_loaded;
            jt::Json states              = make_array();
            for(const PcSampleState& state : data.pc_sample_states)
            {
                jt::Json state_json;
                state_json["uuid"]             = json_from_uint64(state.pc_sample_state_uuid);
                state_json["instruction_uuid"] = json_from_uint64(state.instruction_uuid);
                state_json["total"]            = json_from_uint64(state.total_count);
                if(state.issue_count)
                {
                    state_json["issue"] = json_from_uint64(state.issue_count.value());
                }
                if(state.stall_count)
                {
                    state_json["stall"] = json_from_uint64(state.stall_count.value());
                }
                states.getArray().push_back(std::move(state_json));
            }
            json["states"] = std::move(states);
            jt::Json reasons = make_array();
            for(const PcSampleStallReason& reason : data.pc_sample_stall_reasons)
            {
                jt::Json reason_json;
                reason_json["state_uuid"]  = json_from_uint64(reason.pc_sample_state_uuid);
                reason_json["lookup_uuid"] =
                    json_from_uint64(reason.pc_sample_stall_reason_lookup_uuid);
                reason_json["count"] = json_from_uint64(reason.count);
                reasons.getArray().push_back(std::move(reason_json));
            }
            json["stall_reasons"] = std::move(reasons);
            jt::Json lookups      = make_array();
            for(const PcSampleStallReasonLookup& lookup : data.pc_sample_stall_reason_lookups)
            {
                jt::Json lookup_json;
                lookup_json["uuid"] =
                    json_from_uint64(lookup.pc_sample_stall_reason_lookup_uuid);
                lookup_json["text"] = lookup.text;
                lookups.getArray().push_back(std::move(lookup_json));
            }
            json["stall_reason_lookups"] = std::move(lookups);
            break;
        }
    }
    if(!json.isObject())
    {
        json.setObject();
    }
    return json;
}

void
decode_pc_sampling(jt::Json& json, PcSamplingData& data, PcSamplingLayer layer)
{
    switch(layer)
    {
        case PcSamplingLayer::kIsa:
        {
            data.code_objects.clear();
            for(jt::Json& code_object_json : json_get_array(json, "code_objects"))
            {
                CodeObjectStore& code_object = data.code_objects.emplace_back();
                code_object.code_object_uuid = json_get_uint64(code_object_json, "uuid");
                for(jt::Json& symbol_json : json_get_array(code_object_json, "symbols"))
                {
                    KernelSymbol& symbol      = code_object.kernel_symbols.emplace_back();
                    symbol.kernel_symbol_uuid = json_get_uint64(symbol_json, "uuid");
                    symbol.code_object_uuid = json_get_uint64(symbol_json, "code_object_uuid");
                    for(jt::Json& line_json : json_get_array(symbol_json, "lines"))
                    {
                        InstructionLine& line = symbol.instruction_lines.emplace_back();
                        line.instruction_uuid = json_get_uint64(line_json, "uuid");
                        if(line_json.contains("type_uuid"))
                        {
                            line.instruction_type_uuid =
                                json_get_uint64(line_json, "type_uuid");
                        }
                        line.code_object_offset = json_get_uint64(line_json, "offset");
                        line.instruction        = json_get_string(line_json, "instruction");
                    }
                }
            }
            data.instruction_type_lookup_map.clear();
            for(jt::Json& type_json : json_get_array(json, "instruction_types"))
            {
                data.instruction_type_lookup_map[json_get_uint64(type_json, "uuid")] =
                    json_get_string(type_json, "text");
            }
            break;
        }
        case PcSamplingLayer::kSource:
        {
            data.source_files.clear();
            for(jt::Json& file_json : json_get_array(json, "source_files"))
            {
                SourceFile& file      = data.source_files.emplace_back();
                file.source_file_uuid = json_get_uint64(file_json, "uuid");
                file.file_path        = json_get_string(file_json, "path");
                for(jt::Json& line_json : json_get_array(file_json, "lines"))
                {
                    SourceLine& line      = file.source_lines.emplace_back();
                    line.source_line_uuid = json_get_uint64(line_json, "uuid");
                    line.line_number      = json_get_uint64(line_json, "number");
                    line.content          = json_get_string(line_json, "content");
                }
            }
            data.instruction_source_lines.clear();
            for(jt::Json& mapping_json : json_get_array(json, "instruction_source_lines"))
            {
                InstructionSourceLine& mapping = data.instruction_source_lines.emplace_back();
                mapping.instruction_uuid = json_get_uint64(mapping_json, "instruction_uuid");
                mapping.source_line_uuid = json_get_uint64(mapping_json, "source_line_uuid");
                mapping.source_file_uuid = json_get_uint64(mapping_json, "source_file_uuid");
                mapping.frame_index      = json_get_uint64(mapping_json, "frame_index");
            }
            break;
        }
        case PcSamplingLayer::kStalls:
        {
            data.sample_states_loaded = json_get_bool(json, "sample_states_loaded");
            data.stall_details_loaded = json_get_bool(json, "stall_details_loaded");
            data.pc_sample_states.clear();
            for(jt::Json& state_json : json_get_array(json, "states"))
            {
                PcSampleState& state       = data.pc_sample_states.emplace_back();
                state.pc_sample_state_uuid = json_get_uint64(state_json, "uuid");
                state.instruction_uuid     = json_get_uint64(state_json, "instruction_uuid");
                state.total_count          = json_get_uint64(state_json, "total");
                if(state_json.contains("issue"))
                {
                    state.issue_count = json_get_uint64(state_json, "issue");
                }
                if(state_json.contains("stall"))
                {
                    state.stall_count = json_get_uint64(state_json, "stall");
                }
            }
            data.pc_sample_stall_reasons.clear();
            for(jt::Json& reason_json : json_get_array(json, "stall_reasons"))
            {
                PcSampleStallReason& reason  = data.pc_sample_stall_reasons.emplace_back();
                reason.pc_sample_state_uuid = json_get_uint64(reason_json, "state_uuid");
                reason.pc_sample_stall_reason_lookup_uuid =
                    json_get_uint64(reason_json, "lookup_uuid");
                reason.count = json_get_uint64(reason_json, "count");
            }
            data.pc_sample_stall_reason_lookups.clear();
            for(jt::Json& lookup_json : json_get_array(json, "stall_reason_lookups"))
            {
                PcSampleStallReasonLookup& lookup =
                    data.pc_sample_stall_reason_lookups.emplace_back();
                lookup.pc_sample_stall_reason_lookup_uuid =
                    json_get_uint64(lookup_json, "uuid");
                lookup.text = json_get_string(lookup_json, "text");
            }
            break;
        }
    }
}

}  // namespace View
}  // namespace RocProfVis
