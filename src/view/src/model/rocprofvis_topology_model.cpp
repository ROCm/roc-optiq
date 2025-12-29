// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_topology_model.h"

namespace RocProfVis
{
namespace View
{
namespace Model
{

// Node access
const NodeInfo* TopologyModel::GetNode(uint64_t node_id) const
{
    auto it = m_nodes.find(node_id);
    return (it != m_nodes.end()) ? &it->second : nullptr;
}

std::vector<const NodeInfo*> TopologyModel::GetNodeList() const
{
    std::vector<const NodeInfo*> result;
    result.reserve(m_nodes.size());
    for (const auto& pair : m_nodes)
    {
        result.push_back(&pair.second);
    }
    return result;
}

void TopologyModel::AddNode(uint64_t node_id, NodeInfo&& node)
{
    m_nodes[node_id] = std::move(node);
}

void TopologyModel::ClearNodes()
{
    m_nodes.clear();
}

// Device access
const DeviceInfo* TopologyModel::GetDevice(uint64_t device_id) const
{
    auto it = m_devices.find(device_id);
    return (it != m_devices.end()) ? &it->second : nullptr;
}

void TopologyModel::AddDevice(uint64_t device_id, DeviceInfo&& device)
{
    m_devices[device_id] = std::move(device);
}

void TopologyModel::ClearDevices()
{
    m_devices.clear();
}

// Process access
const ProcessInfo* TopologyModel::GetProcess(uint64_t process_id) const
{
    auto it = m_processes.find(process_id);
    return (it != m_processes.end()) ? &it->second : nullptr;
}

void TopologyModel::AddProcess(uint64_t process_id, ProcessInfo&& process)
{
    m_processes[process_id] = std::move(process);
}

void TopologyModel::ClearProcesses()
{
    m_processes.clear();
}

// Instrumented thread access
const ThreadInfo* TopologyModel::GetInstrumentedThread(uint64_t thread_id) const
{
    auto it = m_instrumented_threads.find(thread_id);
    return (it != m_instrumented_threads.end()) ? &it->second : nullptr;
}

void TopologyModel::AddInstrumentedThread(uint64_t thread_id, ThreadInfo&& thread)
{
    m_instrumented_threads[thread_id] = std::move(thread);
}

void TopologyModel::ClearInstrumentedThreads()
{
    m_instrumented_threads.clear();
}

// Sampled thread access
const ThreadInfo* TopologyModel::GetSampledThread(uint64_t thread_id) const
{
    auto it = m_sampled_threads.find(thread_id);
    return (it != m_sampled_threads.end()) ? &it->second : nullptr;
}

void TopologyModel::AddSampledThread(uint64_t thread_id, ThreadInfo&& thread)
{
    m_sampled_threads[thread_id] = std::move(thread);
}

void TopologyModel::ClearSampledThreads()
{
    m_sampled_threads.clear();
}

// Queue access
const QueueInfo* TopologyModel::GetQueue(uint64_t queue_id) const
{
    auto it = m_queues.find(queue_id);
    return (it != m_queues.end()) ? &it->second : nullptr;
}

void TopologyModel::AddQueue(uint64_t queue_id, QueueInfo&& queue)
{
    m_queues[queue_id] = std::move(queue);
}

void TopologyModel::ClearQueues()
{
    m_queues.clear();
}

// Stream access
const StreamInfo* TopologyModel::GetStream(uint64_t stream_id) const
{
    auto it = m_streams.find(stream_id);
    return (it != m_streams.end()) ? &it->second : nullptr;
}

void TopologyModel::AddStream(uint64_t stream_id, StreamInfo&& stream)
{
    m_streams[stream_id] = std::move(stream);
}

void TopologyModel::ClearStreams()
{
    m_streams.clear();
}

// Counter access
const CounterInfo* TopologyModel::GetCounter(uint64_t counter_id) const
{
    auto it = m_counters.find(counter_id);
    return (it != m_counters.end()) ? &it->second : nullptr;
}

void TopologyModel::AddCounter(uint64_t counter_id, CounterInfo&& counter)
{
    m_counters[counter_id] = std::move(counter);
}

void TopologyModel::ClearCounters()
{
    m_counters.clear();
}

// Clear all
void TopologyModel::Clear()
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

}  // namespace Model
}  // namespace View
}  // namespace RocProfVis
