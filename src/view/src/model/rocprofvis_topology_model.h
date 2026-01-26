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


/**
 * @brief Manages static system topology information.
 * 
 * This model holds hardware and process hierarchy data that is typically
 * loaded once at trace initialization and rarely changes.
 */
class TopologyDataModel
{
public:
    TopologyDataModel() = default;
    ~TopologyDataModel() = default;

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

    // Helpers

    /* Gets the device ID associated with a given topology info ID and track type
        @param info_id - id of the topology item ex: TrackInfo::topology.id
        @param track_type - type of the topology item ex: TrackInfo::topology.type
        @return device ID if found, INVALID_UINT64_INDEX otherwise
    */
    uint64_t GetDeviceIdByInfoId(uint64_t info_id, TrackInfo::TrackType track_type) const;

    /* Gets the device info associated with a given topology info ID and track type
        @param info_id - id of the topology item ex: TrackInfo::topology.id
        @param track_type - type of the topology item ex: TrackInfo::topology.type
        @return a pointer to the device info if found, nullptr otherwise
    */
    const DeviceInfo* GetDeviceByInfoId(uint64_t             info_id,
                                        TrackInfo::TrackType track_type) const;

    std::string GetDeviceTypeLabelByInfoId(
        uint64_t info_id, TrackInfo::TrackType track_type,
        std::string_view default_label = "Unknown Device") const;

    std::string GetDeviceProductLabelByInfoId(
        uint64_t info_id, TrackInfo::TrackType track_type,
        std::string_view default_label = "Unknown") const;

    bool GetDeviceTypeLabel(const DeviceInfo& device_info, std::string& label_out) const;

    // Debug
    std::string TopologyToString();
    std::string DeviceInfoToString(const DeviceInfo* device_info, int indent = 0) const;
    std::string ProcessInfoToString(const ProcessInfo* process_info, int indent = 0) const;
    std::string ThreadInfoToString(const ThreadInfo* thread_info, int indent = 0) const;
    std::string QueueInfoToString(const QueueInfo* queue_info, int indent = 0) const;
    std::string StreamInfoToString(const StreamInfo* stream_info, int indent = 0) const;
    std::string CounterInfoToString(const CounterInfo* counter_info, int indent = 0) const;

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

}  // namespace View
}  // namespace RocProfVis
