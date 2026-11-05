// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED

#include "rocprofvis_db_profiler_hub_ext.h"
#include "rocprofvis_db_profiler_hub.h"
#include <cstring>
#include <string>

namespace RocProfVis
{
namespace DataModel
{

struct category_name_t
{
    const char*                           name;
    rocprofvis_event_data_category_enum_t category;
};

static const category_name_t CATEGORY_NAMES[] = {
    { "id", kRocProfVisEventEssentialDataId },
    { "category", kRocProfVisEventEssentialDataCategory },
    { "event_category", kRocProfVisEventEssentialDataCategory },
    { "name", kRocProfVisEventEssentialDataName },
    { "display_name", kRocProfVisEventEssentialDataName },
    { "region_name", kRocProfVisEventEssentialDataName },
    { "start_timestamp", kRocProfVisEventEssentialDataStart },
    { "end_timestamp", kRocProfVisEventEssentialDataEnd },
    { "duration", kRocProfVisEventEssentialDataDuration },
    { "nid", kRocProfVisEventEssentialDataNode },
    { "node_id", kRocProfVisEventEssentialDataNode },
    { "pid", kRocProfVisEventEssentialDataProcess },
    { "tid", kRocProfVisEventEssentialDataThread },
    { "thread_id", kRocProfVisEventEssentialDataThread },
    { "agent_type", kRocProfVisEventEssentialDataAgentType },
    { "type_index", kRocProfVisEventEssentialDataAgentIndex },
    { "absolute_index", kRocProfVisEventEssentialDataAgentIndex },
    { "logical_index", kRocProfVisEventEssentialDataAgentIndex },
    { "queue_id", kRocProfVisEventEssentialDataQueue },
    { "stream_id", kRocProfVisEventEssentialDataStream },
    { "track_id", kRocProfVisEventEssentialDataTrack },
    { "trackId", kRocProfVisEventEssentialDataTrack },
    { "stack_id", kRocProfVisEventEssentialDataInternal },
    { "parent_stack_id", kRocProfVisEventEssentialDataInternal },
    { "correlation_id", kRocProfVisEventEssentialDataInternal },
    { "level", kRocProfVisEventEssentialDataLevel },
};

static rocprofvis_event_data_category_enum_t
category_enum_for_member(const char* name)
{
    if(name == nullptr) return kRocProfVisEventEssentialDataUncategorized;
    const size_t count = sizeof(CATEGORY_NAMES) / sizeof(CATEGORY_NAMES[0]);
    for(size_t index = 0; index < count; ++index)
    {
        if(std::strcmp(name, CATEGORY_NAMES[index].name) == 0)
        {
            return CATEGORY_NAMES[index].category;
        }
    }
    return kRocProfVisEventEssentialDataUncategorized;
}

// The category string is the name prefix. Most callers already end it with '_'.
// Track records keep the field name, matching the existing trackId label.
static std::string
prefixed_name(const char* category, const char* name)
{
    const char* field = name != nullptr ? name : "";
    if(category == nullptr || category[0] == '\0' || std::strcmp(category, "Track") == 0)
    {
        return field;
    }
    std::string result = category;
    if(result.back() != '_' && field[0] != '\0')
    {
        result.push_back('_');
    }
    result += field;
    return result;
}

ProfilerHubExtData::ProfilerHubExtData(ProfilerHub*                    hub,
                                       rocprofvis_dm_extdata_t         container,
                                       rocprofvis_dm_event_operation_t operation,
                                       uint32_t db_instance, uint64_t origin_timestamp)
: m_hub(hub)
, m_container(container)
, m_operation(operation)
, m_db_instance(db_instance)
, m_origin(origin_timestamp)
{}

bool
ProfilerHubExtData::AddInt(const char* category, const char* name, uint64_t value)
{
    rocprofvis_db_ext_data_t record{};
    const std::string        text       = std::to_string(value);
    const std::string        field_name = prefixed_name(category, name);
    record.category                     = category;
    record.name                         = field_name.c_str();
    record.data                         = text.c_str();
    record.type                         = kRPVDataTypeInt;
    record.db_instance                  = m_db_instance;
    record.category_enum                = category_enum_for_member(name);
    (void) m_operation;
    return m_hub->BindObject()->FuncAddExtDataRecord(m_container, record) ==
           kRocProfVisDmResultSuccess;
}

bool
ProfilerHubExtData::AddText(const char* category, const char* name, const char* value)
{
    rocprofvis_db_ext_data_t record{};
    const std::string        field_name = prefixed_name(category, name);
    record.category                     = category;
    record.name                         = field_name.c_str();
    record.data                         = value != nullptr ? value : "";
    record.type                         = kRPVDataTypeString;
    record.db_instance                  = m_db_instance;
    record.category_enum                = category_enum_for_member(name);
    (void) m_operation;
    return m_hub->BindObject()->FuncAddExtDataRecord(m_container, record) ==
           kRocProfVisDmResultSuccess;
}

bool
ProfilerHubExtData::AddOptional(const char* category, const char* name,
                                ph_optional_uint64_t value)
{
    if(value.is_valid == 0)
    {
        rocprofvis_db_ext_data_t record{};
        const std::string        field_name = prefixed_name(category, name);
        record.category                     = category;
        record.name                         = field_name.c_str();
        record.data                         = "";
        record.type                         = kRPVDataTypeNull;
        record.db_instance                  = m_db_instance;
        record.category_enum                = category_enum_for_member(name);
        (void) m_operation;
        return m_hub->BindObject()->FuncAddExtDataRecord(m_container, record) ==
               kRocProfVisDmResultSuccess;
    }
    return AddInt(category, name, value.value);
}

bool
ProfilerHubExtData::AddUints(const char* category, const uint_field_t* fields,
                             size_t count)
{
    for(size_t index = 0; index < count; ++index)
    {
        if(!AddInt(category, fields[index].name, fields[index].value))
        {
            return false;
        }
    }
    return true;
}

bool
ProfilerHubExtData::AddTexts(const char* category, const text_field_t* fields,
                             size_t count)
{
    for(size_t index = 0; index < count; ++index)
    {
        if(!AddText(category, fields[index].name, fields[index].value))
        {
            return false;
        }
    }
    return true;
}

bool
ProfilerHubExtData::AddOptionals(const char* category, const optional_field_t* fields,
                                 size_t count)
{
    for(size_t index = 0; index < count; ++index)
    {
        if(!AddOptional(category, fields[index].name, fields[index].value))
        {
            return false;
        }
    }
    return true;
}

bool
ProfilerHubExtData::AddEventData(const ph_event_data_t& event)
{
    if(event.is_valid == 0) return true;
    const char*        category  = "event_";
    const uint_field_t numbers[] = {
        { "stack_id", event.stack_id },
        { "parent_stack_id", event.parent_stack_id },
        { "correlation_id", event.correlation_id },
    };
    const text_field_t texts[] = {
        { "event_category", event.event_category },
        { "event_extdata", event.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0]));
}

bool
ProfilerHubExtData::AddNode(const ph_detail_node_t& node)
{
    if(node.is_valid == 0) return true;
    const char*        category  = "node_";
    const uint_field_t numbers[] = {
        { "node_id", node.node_id },
        { "hash", node.hash },
    };
    const text_field_t texts[] = {
        { "machine_id", node.machine_id },   { "system_name", node.system_name },
        { "hostname", node.hostname },       { "release", node.release },
        { "version", node.version },         { "hardware_name", node.hardware_name },
        { "domain_name", node.domain_name },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0]));
}

bool
ProfilerHubExtData::AddProcess(const ph_detail_process_t& process)
{
    if(process.is_valid == 0) return true;
    const char*        category  = "process_";
    const uint_field_t numbers[] = {
        { "id", process.id },
        { "pid", process.pid },
    };
    const optional_field_t optionals[] = {
        { "ppid", process.ppid },   { "init", process.init }, { "fini", process.fini },
        { "start", process.start }, { "end", process.end },
    };
    const text_field_t texts[] = {
        { "command", process.command },
        { "environment", process.environment },
        { "process_extdata", process.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddOptionals(category, optionals, sizeof(optionals) / sizeof(optionals[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0]));
}

bool
ProfilerHubExtData::AddThread(const ph_detail_thread_t& thread)
{
    if(thread.is_valid == 0) return true;
    const char*        category  = "thread_";
    const uint_field_t numbers[] = {
        { "id", thread.id },
        { "thread_id", thread.thread_id },
    };
    const optional_field_t optionals[] = {
        { "parent_process_id", thread.parent_process_id },
        { "start", thread.start },
        { "end", thread.end },
    };
    const text_field_t texts[] = {
        { "name", thread.name },
        { "thread_extdata", thread.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddOptionals(category, optionals, sizeof(optionals) / sizeof(optionals[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0]));
}

bool
ProfilerHubExtData::AddAgent(const char* category, const ph_detail_agent_t& agent)
{
    if(agent.is_valid == 0) return true;
    const uint_field_t numbers[] = {
        { "id", agent.id },
        { "type_index", agent.type_index },
    };
    const optional_field_t optionals[] = {
        { "absolute_index", agent.absolute_index },
        { "logical_index", agent.logical_index },
        { "uuid", agent.uuid },
    };
    const text_field_t texts[] = {
        { "agent_type", agent.agent_type },     { "name", agent.name },
        { "model_name", agent.model_name },     { "vendor_name", agent.vendor_name },
        { "product_name", agent.product_name }, { "user_name", agent.user_name },
        { "agent_extdata", agent.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddOptionals(category, optionals, sizeof(optionals) / sizeof(optionals[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0]));
}

bool
ProfilerHubExtData::AddStream(const ph_detail_stream_t& stream)
{
    if(stream.is_valid == 0) return true;
    const char*        category  = "stream_";
    const uint_field_t numbers[] = {
        { "id", stream.stream_id },
        { "nid", stream.nid },
        { "pid", stream.pid },
    };
    const text_field_t texts[] = {
        { "name", stream.name },
        { "extdata", stream.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0]));
}

bool
ProfilerHubExtData::AddQueue(const ph_detail_queue_t& queue)
{
    if(queue.is_valid == 0) return true;
    const char*        category  = "queue_";
    const uint_field_t numbers[] = {
        { "queue_id", queue.queue_id },
        { "nid", queue.nid },
        { "pid", queue.pid },
    };
    const text_field_t texts[] = {
        { "name", queue.name },
        { "extdata", queue.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0]));
}

bool
ProfilerHubExtData::AddKernelSymbol(const ph_detail_kernel_symbol_t& symbol)
{
    const char*        category  = "kernel_";
    const uint_field_t numbers[] = {
        { "id", symbol.id },
    };
    const optional_field_t optionals[] = {
        { "object", symbol.kernel_object },
        { "kernarg_segment_size", symbol.kernarg_segment_size },
        { "kernarg_segment_alignment", symbol.kernarg_segment_alignment },
        { "group_segment_size", symbol.group_segment_size },
        { "private_segment_size", symbol.private_segment_size },
        { "sgpr_count", symbol.sgpr_count },
        { "arch_vgpr_count", symbol.arch_vgpr_count },
        { "accum_vgpr_count", symbol.accum_vgpr_count },
    };
    const text_field_t texts[] = {
        { "name", symbol.name },
        { "display_name", symbol.display_name },
        { "symbol_info_extdata", symbol.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddOptionals(category, optionals, sizeof(optionals) / sizeof(optionals[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0]));
}

bool
ProfilerHubExtData::AddCodeObject(const ph_detail_code_object_t& object)
{
    const char*        category  = "code_object_";
    const uint_field_t numbers[] = {
        { "id", object.id },
    };
    const optional_field_t optionals[] = {
        { "load_base", object.load_base },
        { "load_size", object.load_size },
        { "load_delta", object.load_delta },
    };
    const text_field_t texts[] = {
        { "uri", object.uri },
        { "storage_type", object.storage_type },
        { "extdata", object.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddOptionals(category, optionals, sizeof(optionals) / sizeof(optionals[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0]));
}

bool
ProfilerHubExtData::AddTrack(uint32_t track_id, const ph_event_data_t& event)
{
    return AddInt("Track", "trackId", track_id) && AddEventData(event);
}

bool
ProfilerHubExtData::AddRegion(const ph_region_details_t& region)
{
    const char*        category  = "region_";
    const uint_field_t numbers[] = {
        { "start_timestamp", region.start_timestamp - m_origin },
        { "end_timestamp", region.end_timestamp - m_origin },
    };
    const text_field_t texts[] = {
        { "name", region.name },
        { "extdata", region.extdata },
    };
    if(!AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) ||
       !AddTexts(category, texts, sizeof(texts) / sizeof(texts[0])) ||
       !AddEventData(region.event))
    {
        return false;
    }
    for(uint32_t index = 0; index < region.arg_count; ++index)
    {
        const ph_arg_data_t& arg         = region.args[index];
        const text_field_t   arg_texts[] = {
            { "type", arg.type },
            { "name", arg.name },
            { "value", arg.value },
            { "arg_extdata", arg.extdata },
        };
        if(!AddInt("arg_data_t", "position", arg.position) ||
           !AddTexts("arg_data_t", arg_texts, sizeof(arg_texts) / sizeof(arg_texts[0])))
        {
            return false;
        }
    }
    return true;
}

bool
ProfilerHubExtData::AddKernelDispatch(const ph_kernel_dispatch_details_t& kernel)
{
    const char*        category  = "kernel_dispatch_";
    const uint_field_t numbers[] = {
        { "dispatch_id", kernel.dispatch_id },
        { "start_timestamp", kernel.start_timestamp - m_origin },
        { "end_timestamp", kernel.end_timestamp - m_origin },
        { "private_segment_size", kernel.private_segment_size },
        { "group_segment_size", kernel.group_segment_size },
        { "workgroup_size_x", kernel.workgroup_size_x },
        { "workgroup_size_y", kernel.workgroup_size_y },
        { "workgroup_size_z", kernel.workgroup_size_z },
        { "grid_size_x", kernel.grid_size_x },
        { "grid_size_y", kernel.grid_size_y },
        { "grid_size_z", kernel.grid_size_z },
    };
    const text_field_t texts[] = {
        { "name", kernel.name },
        { "extdata", kernel.extdata },
    };
    bool stored = AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
                  AddTexts(category, texts, sizeof(texts) / sizeof(texts[0])) &&
                  AddEventData(kernel.event) && AddNode(kernel.node_info) &&
                  AddProcess(kernel.process_info) && AddThread(kernel.thread_info) &&
                  AddAgent("agent_info_t", kernel.agent_info) &&
                  AddStream(kernel.stream_info) && AddQueue(kernel.queue_info);
    if(stored && kernel.kernel_symbol_info.is_valid != 0)
    {
        stored = AddKernelSymbol(kernel.kernel_symbol_info);
    }
    if(stored && kernel.code_object_info.is_valid != 0)
    {
        stored = AddCodeObject(kernel.code_object_info);
    }
    return stored;
}

bool
ProfilerHubExtData::AddMemoryCopy(const ph_memory_copy_details_t& copy)
{
    const char*        category  = "memory_copy_";
    const uint_field_t numbers[] = {
        { "start_timestamp", copy.start_timestamp - m_origin },
        { "end_timestamp", copy.end_timestamp - m_origin },
        { "size", copy.size },
    };
    const optional_field_t optionals[] = {
        { "dst_address", copy.dst_address },
        { "src_address", copy.src_address },
    };
    const text_field_t texts[] = {
        { "name", copy.name },
        { "region_name", copy.region_name },
        { "extdata", copy.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddOptionals(category, optionals, sizeof(optionals) / sizeof(optionals[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0])) &&
           AddEventData(copy.event) && AddAgent("dst_agent_id", copy.dst_agent_id) &&
           AddAgent("src_agent_id", copy.src_agent_id) && AddNode(copy.node_info) &&
           AddProcess(copy.process_info) && AddThread(copy.thread_info) &&
           AddStream(copy.stream_info) && AddQueue(copy.queue_info);
}

bool
ProfilerHubExtData::AddMemoryAlloc(const ph_memory_alloc_details_t& alloc)
{
    const char*        category  = "memory_alloc_";
    const uint_field_t numbers[] = {
        { "start_timestamp", alloc.start_timestamp - m_origin },
        { "end_timestamp", alloc.end_timestamp - m_origin },
        { "size", alloc.size },
    };
    const optional_field_t optionals[] = {
        { "address", alloc.address },
    };
    const text_field_t texts[] = {
        { "type", alloc.type },
        { "level", alloc.level },
        { "extdata", alloc.extdata },
    };
    return AddUints(category, numbers, sizeof(numbers) / sizeof(numbers[0])) &&
           AddOptionals(category, optionals, sizeof(optionals) / sizeof(optionals[0])) &&
           AddTexts(category, texts, sizeof(texts) / sizeof(texts[0])) &&
           AddEventData(alloc.event) && AddNode(alloc.node_info) &&
           AddProcess(alloc.process_info) && AddThread(alloc.thread_info) &&
           AddAgent("agent_info_t", alloc.agent_info) && AddStream(alloc.stream_info) &&
           AddQueue(alloc.queue_info);
}

}  // namespace DataModel
}  // namespace RocProfVis

#endif  // ROCPROFVIS_PROFILER_HUB_ENABLED
