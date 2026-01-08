// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_topology_model.h"

namespace RocProfVis
{
namespace View
{

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

}  // namespace View
}  // namespace RocProfVis
