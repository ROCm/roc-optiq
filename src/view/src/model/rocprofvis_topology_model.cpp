// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_topology_model.h"

#include <limits>
#include <sstream>

namespace RocProfVis
{
namespace View
{

constexpr uint64_t INVALID_UINT64_INDEX = std::numeric_limits<uint64_t>::max();

// Node access
const NodeInfo*
TopologyDataModel::GetNode(uint64_t node_id) const
{
    auto it = m_nodes.find(node_id);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

std::vector<const NodeInfo*>
TopologyDataModel::GetNodeList() const
{
    std::vector<const NodeInfo*> result;
    result.reserve(m_nodes.size());
    for(const auto& pair : m_nodes)
    {
        result.push_back(&pair.second);
    }
    return result;
}

void
TopologyDataModel::AddNode(uint64_t node_id, NodeInfo&& node)
{
    m_nodes[node_id] = std::move(node);
}

void
TopologyDataModel::ClearNodes()
{
    m_nodes.clear();
}

// Device access
const DeviceInfo*
TopologyDataModel::GetDevice(uint64_t device_id) const
{
    auto it = m_devices.find(device_id);
    return (it != m_devices.end()) ? &it->second : nullptr;
}

void
TopologyDataModel::AddDevice(uint64_t device_id, DeviceInfo&& device)
{
    m_devices[device_id] = std::move(device);
}

void
TopologyDataModel::ClearDevices()
{
    m_devices.clear();
}

// Process access
const ProcessInfo*
TopologyDataModel::GetProcess(uint64_t process_id) const
{
    auto it = m_processes.find(process_id);
    return (it != m_processes.end()) ? &it->second : nullptr;
}

void
TopologyDataModel::AddProcess(uint64_t process_id, ProcessInfo&& process)
{
    m_processes[process_id] = std::move(process);
}

void
TopologyDataModel::ClearProcesses()
{
    m_processes.clear();
}

// Instrumented thread access
const ThreadInfo*
TopologyDataModel::GetInstrumentedThread(uint64_t thread_id) const
{
    auto it = m_instrumented_threads.find(thread_id);
    return (it != m_instrumented_threads.end()) ? &it->second : nullptr;
}

void
TopologyDataModel::AddInstrumentedThread(uint64_t thread_id, ThreadInfo&& thread)
{
    m_instrumented_threads[thread_id] = std::move(thread);
}

void
TopologyDataModel::ClearInstrumentedThreads()
{
    m_instrumented_threads.clear();
}

// Sampled thread access
const ThreadInfo*
TopologyDataModel::GetSampledThread(uint64_t thread_id) const
{
    auto it = m_sampled_threads.find(thread_id);
    return (it != m_sampled_threads.end()) ? &it->second : nullptr;
}

void
TopologyDataModel::AddSampledThread(uint64_t thread_id, ThreadInfo&& thread)
{
    m_sampled_threads[thread_id] = std::move(thread);
}

void
TopologyDataModel::ClearSampledThreads()
{
    m_sampled_threads.clear();
}

// Queue access
const QueueInfo*
TopologyDataModel::GetQueue(uint64_t queue_id) const
{
    auto it = m_queues.find(queue_id);
    return (it != m_queues.end()) ? &it->second : nullptr;
}

void
TopologyDataModel::AddQueue(uint64_t queue_id, QueueInfo&& queue)
{
    m_queues[queue_id] = std::move(queue);
}

void
TopologyDataModel::ClearQueues()
{
    m_queues.clear();
}

// Stream access
const StreamInfo*
TopologyDataModel::GetStream(uint64_t stream_id) const
{
    auto it = m_streams.find(stream_id);
    return (it != m_streams.end()) ? &it->second : nullptr;
}

void
TopologyDataModel::AddStream(uint64_t stream_id, StreamInfo&& stream)
{
    m_streams[stream_id] = std::move(stream);
}

void
TopologyDataModel::ClearStreams()
{
    m_streams.clear();
}

// Counter access
const CounterInfo*
TopologyDataModel::GetCounter(uint64_t counter_id) const
{
    auto it = m_counters.find(counter_id);
    return (it != m_counters.end()) ? &it->second : nullptr;
}

void
TopologyDataModel::AddCounter(uint64_t counter_id, CounterInfo&& counter)
{
    m_counters[counter_id] = std::move(counter);
}

void
TopologyDataModel::ClearCounters()
{
    m_counters.clear();
}

// Clear all
void
TopologyDataModel::Clear()
{
    ClearNodes();
    ClearDevices();
    ClearProcesses();
    ClearInstrumentedThreads();
    ClearSampledThreads();
    ClearQueues();
    ClearStreams();
    ClearCounters();
}

uint64_t
TopologyDataModel::GetDeviceIdByInfoId(uint64_t             info_id,
                                       TrackInfo::TrackType track_type) const
{
    uint64_t device_id = INVALID_UINT64_INDEX;

    switch(track_type)
    {
        case TrackInfo::TrackType::Counter:
        {
            const CounterInfo* counter_info = GetCounter(info_id);
            if(counter_info)
            {
                device_id = counter_info->device_id;
            }
        }
        break;
        case TrackInfo::TrackType::Queue:
        {
            const QueueInfo* queue_info = GetQueue(info_id);
            if(queue_info)
            {
                device_id = queue_info->device_id;
            }
        }
        break;
        case TrackInfo::TrackType::Stream:
        {
            const StreamInfo* stream_info = GetStream(info_id);
            if(stream_info)
            {
                device_id = stream_info->device_id;
            }
        }
        break;
        default: break;
    }
    return device_id;
}

const DeviceInfo*
TopologyDataModel::GetDeviceByInfoId(uint64_t             info_id,
                                     TrackInfo::TrackType track_type) const
{
    const DeviceInfo* device_info = nullptr;
    uint64_t          device_id   = GetDeviceIdByInfoId(info_id, track_type);

    if(device_id != INVALID_UINT64_INDEX)
    {
        device_info = GetDevice(device_id);
    }
    return device_info;
}

std::string
TopologyDataModel::GetDeviceTypeLabelByInfoId(uint64_t             info_id,
                                              TrackInfo::TrackType track_type,
                                              std::string_view default_label) const
{
    const DeviceInfo* device_info = GetDeviceByInfoId(info_id, track_type);
    if(device_info)
    {
        std::string label;
        if(GetDeviceTypeLabel(*device_info, label))
        {
            return label;
        }
    }
    return std::string(default_label);
}

std::string
TopologyDataModel::GetDeviceProductLabelByInfoId(uint64_t             info_id,
                                                 TrackInfo::TrackType track_type,
                                                std::string_view default_label) const
{
    const DeviceInfo* device_info = GetDeviceByInfoId(info_id, track_type);
    if(device_info)
    {
        return device_info->product_name;
    }
    return std::string(default_label);
}

bool
TopologyDataModel::GetDeviceTypeLabel(const DeviceInfo& device_info,
                                      std::string&      label_out) const
{
    switch(device_info.type)
    {
        case rocprofvis_controller_processor_type_t::kRPVControllerProcessorTypeCPU:
            label_out = "CPU" + std::to_string(device_info.type_index);
            return true;
        case rocprofvis_controller_processor_type_t::kRPVControllerProcessorTypeGPU:
            label_out = "GPU" + std::to_string(device_info.type_index);
            return true;
        default: return false;
    }
}

std::string
TopologyDataModel::TopologyToString()
{
    std::ostringstream ss;

    auto indent = [](int level) {
        return std::string(level, ' ');
    };
    int level = 1;

    // iterate nodes
    ss << "Nodes: " << m_nodes.size() << std::endl;
    for(auto it = m_nodes.begin(); it != m_nodes.end(); it++)
    {
        ss << indent(level) << "Node ID: " << it->second.id << std::endl;
        ss << indent(level) << "Hostname: " << it->second.host_name << std::endl;
        ss << indent(level) << "OS name: " << it->second.os_name << std::endl;
        ss << indent(level) << "OS release: " << it->second.os_release << std::endl;
        ss << indent(level) << "OS version: " << it->second.os_version << std::endl;
        
        const std::vector<uint64_t>& device_ids = it->second.device_ids;
        ss  << indent(level) << "Devices: " << device_ids.size() << std::endl;
        for(const uint64_t& d_id : device_ids)
        {
            const DeviceInfo* devInfo = GetDevice(d_id);
            ss << DeviceInfoToString(devInfo, level+1) << std::endl;
        }

        const std::vector<uint64_t>& process_ids = it->second.process_ids;
        ss << indent(level) << "Processes: " << process_ids.size() << std::endl;
       
        level++;
        for(const uint64_t& p_id : process_ids)
        {
            const ProcessInfo* procInfo = GetProcess(p_id);
            if(procInfo)
            {
                ss << ProcessInfoToString(procInfo, level) << std::endl;

                ss << indent(level) << "Instrumented Threads: " << procInfo->instrumented_thread_ids.size() << std::endl;
                for(const uint64_t& d : procInfo->instrumented_thread_ids)
                {
                    const ThreadInfo* threadInfo = GetInstrumentedThread(d);
                    ss << ThreadInfoToString(threadInfo, level+1) << std::endl;                       
                }

                ss << indent(level) << "Sampled Threads: " << procInfo->sampled_thread_ids.size() << std::endl;
                for(const uint64_t& d : procInfo->sampled_thread_ids)
                {
                    const ThreadInfo* threadInfo = GetSampledThread(d);
                    ss << ThreadInfoToString(threadInfo, level+1) << std::endl;                       
                }

                ss << indent(level) << "Queues: " << procInfo->queue_ids.size() << std::endl;
                for(const uint64_t& d : procInfo->queue_ids)
                {
                    const QueueInfo* queueInfo = GetQueue(d);
                    ss << QueueInfoToString(queueInfo, level+1) << std::endl;                       
                }

                ss << indent(level) << "Streams: " << procInfo->stream_ids.size() << std::endl;
                for(const uint64_t& d : procInfo->stream_ids)
                {
                    const StreamInfo* streamInfo = GetStream(d);
                    ss << StreamInfoToString(streamInfo, level+1) << std::endl;                       
                }

                ss << indent(level) << "Counters: " << procInfo->counter_ids.size() << std::endl;
                for(const uint64_t& d : procInfo->counter_ids)
                {
                    const CounterInfo* counterInfo = GetCounter(d);
                    ss << CounterInfoToString(counterInfo, level+1) << std::endl;                       
                }
            }
        }
    }
    return ss.str();
}

std::string
TopologyDataModel::DeviceInfoToString(const DeviceInfo* device_info, int indent) const
{
    std::ostringstream ss;
    std::string indent_str = std::string(indent, ' ');
    if(device_info)
    {
        ss << indent_str << "Device ID: " << device_info->id << std::endl;
        ss << indent_str << "Product Name: " << device_info->product_name << std::endl;
        ss << indent_str << "Type: " << static_cast<uint32_t>(device_info->type) << std::endl;
        ss << indent_str << "Type Index: " << device_info->type_index << std::endl;
    }
    else
    {
        ss << indent_str << "DeviceInfo: nullptr";
    }
    return ss.str();
}

std::string
TopologyDataModel::ProcessInfoToString(const ProcessInfo* process_info, int indent) const
{
    std::ostringstream ss;
    std::string indent_str = std::string(indent, ' ');
    if(process_info)
    {
        ss << indent_str << "Process ID: " << process_info->id << std::endl;
        ss << indent_str << "Command: " << process_info->command << std::endl;
        ss << indent_str << "Start Time: " << process_info->start_time << std::endl;
        ss << indent_str << "End Time: " << process_info->end_time << std::endl;
        ss << indent_str << "Environment: " << process_info->environment << std::endl;
    }
    else
    {
        ss << indent_str << "ProcessInfo: nullptr" << std::endl;
    }
    return ss.str();
}

std::string
TopologyDataModel::ThreadInfoToString(const ThreadInfo* thread_info, int indent) const
{
    std::ostringstream ss;
    std::string indent_str = std::string(indent, ' ');
    if(thread_info)
    {
        ss << indent_str << "Thread ID: " << thread_info->id << std::endl;
        ss << indent_str << "Name: " << thread_info->name << std::endl;
        ss << indent_str << "TID: " << thread_info->tid << std::endl;
        ss << indent_str << "Start Time: " << thread_info->start_time << std::endl;
        ss << indent_str << "End Time: " << thread_info->end_time << std::endl;
    }
    else
    {
        ss << indent_str << "ThreadInfo: nullptr" << std::endl;
    }
    return ss.str();
}

std::string
TopologyDataModel::QueueInfoToString(const QueueInfo* queue_info, int indent) const
{
    std::ostringstream ss;
    std::string indent_str = std::string(indent, ' ');
    if(queue_info)
    {
        ss << indent_str << "Queue ID: " << queue_info->id << std::endl;
        ss << indent_str << "Name: " << queue_info->name << std::endl;
        ss << indent_str << "Device ID: " << queue_info->device_id << std::endl;
    }
    else
    {
        ss << indent_str << "QueueInfo: nullptr" << std::endl;
    }
    return ss.str();
}

std::string
TopologyDataModel::StreamInfoToString(const StreamInfo* stream_info, int indent) const
{
    std::ostringstream ss;
    std::string indent_str = std::string(indent, ' ');
    if(stream_info)
    {
        ss << indent_str << "Stream ID: " << stream_info->id << std::endl;
        ss << indent_str << "Name: " << stream_info->name << std::endl;
        ss << indent_str << "Device ID: " << stream_info->device_id << std::endl;
    }
    else
    {
        ss << indent_str << "StreamInfo: nullptr" << std::endl;
    }
    return ss.str();
}

std::string
TopologyDataModel::CounterInfoToString(const CounterInfo* counter_info, int indent) const
{
    std::ostringstream ss;
    std::string indent_str = std::string(indent, ' ');
    if(counter_info)
    {
        ss << indent_str << "Counter ID: " << counter_info->id << std::endl;
        ss << indent_str << "Name: " << counter_info->name << std::endl;
        ss << indent_str << "Device ID: " << counter_info->device_id << std::endl;
        ss << indent_str << "Description: " << counter_info->description << std::endl;
        ss << indent_str << "Units: " << counter_info->units << std::endl;
        ss << indent_str << "Value Type: " << counter_info->value_type << std::endl;
    }
    else
    {
        ss << indent_str << "CounterInfo: nullptr" << std::endl;
    }
    return ss.str();
}

}  // namespace View
}  // namespace RocProfVis
