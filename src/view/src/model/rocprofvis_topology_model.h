// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_model_types.h"

#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{
namespace Model
{

/**
 * @brief Manages static system topology information.
 * 
 * This model holds hardware and process hierarchy data that is typically
 * loaded once at trace initialization and rarely changes.
 */
class TopologyModel
{
public:
    TopologyModel() = default;
    ~TopologyModel() = default;

    // Node access
    const NodeInfo* GetNode(uint64_t node_id) const;
    std::vector<const NodeInfo*> GetNodeList() const;
    void AddNode(uint64_t node_id, NodeInfo&& node);
    void ClearNodes();

    // Device access
    const DeviceInfo* GetDevice(uint64_t device_id) const;
    void AddDevice(uint64_t device_id, DeviceInfo&& device);
    void ClearDevices();

    // Process access
    const ProcessInfo* GetProcess(uint64_t process_id) const;
    void AddProcess(uint64_t process_id, ProcessInfo&& process);
    void ClearProcesses();

    // Thread access (instrumented)
    const ThreadInfo* GetInstrumentedThread(uint64_t thread_id) const;
    void AddInstrumentedThread(uint64_t thread_id, ThreadInfo&& thread);
    void ClearInstrumentedThreads();

    // Thread access (sampled)
    const ThreadInfo* GetSampledThread(uint64_t thread_id) const;
    void AddSampledThread(uint64_t thread_id, ThreadInfo&& thread);
    void ClearSampledThreads();

    // Queue access
    const QueueInfo* GetQueue(uint64_t queue_id) const;
    void AddQueue(uint64_t queue_id, QueueInfo&& queue);
    void ClearQueues();

    // Stream access
    const StreamInfo* GetStream(uint64_t stream_id) const;
    void AddStream(uint64_t stream_id, StreamInfo&& stream);
    void ClearStreams();

    // Counter access
    const CounterInfo* GetCounter(uint64_t counter_id) const;
    void AddCounter(uint64_t counter_id, CounterInfo&& counter);
    void ClearCounters();

    // Clear all topology data
    void Clear();

private:
    std::unordered_map<uint64_t, NodeInfo>    m_nodes;
    std::unordered_map<uint64_t, DeviceInfo>  m_devices;
    std::unordered_map<uint64_t, ProcessInfo> m_processes;
    std::unordered_map<uint64_t, ThreadInfo>  m_instrumented_threads;
    std::unordered_map<uint64_t, ThreadInfo>  m_sampled_threads;
    std::unordered_map<uint64_t, QueueInfo>   m_queues;
    std::unordered_map<uint64_t, StreamInfo>  m_streams;
    std::unordered_map<uint64_t, CounterInfo> m_counters;
};

}  // namespace Model
}  // namespace View
}  // namespace RocProfVis
