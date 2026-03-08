// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_api.h"
#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

// Result type for protocol operations
using Result = Controller::Result;
using ProtocolRequestHandle = Controller::RequestHandle;
using ProtocolRequestState = Controller::RequestState;
using ProtocolRequestStatus = Controller::RequestStatus;
using TrackTopologyType = Controller::TrackTopologyType;

// String table for DLL-safe string management
// Stores strings on backend side, returns indices to frontend
class StringTable
{
public:
    StringTable() = default;
    ~StringTable() = default;

    uint32_t AddString(const char* str);
    const char* GetString(uint32_t index) const;
    uint32_t GetCount() const { return static_cast<uint32_t>(m_strings.size()); }
    void Clear();

private:
    std::vector<std::string> m_strings;
    std::unordered_map<std::string, uint32_t> m_index_map;
};

// Frontend data structures (DLL-safe, POD types only)

// Protocol track information is the authoritative metadata payload for the
// migrated system-trace view path.
struct ProtocolTrackInfo
{
    uint64_t index;
    uint64_t id;
    uint32_t category_index;
    uint32_t main_name_index;
    uint32_t sub_name_index;
    uint32_t entry_count;
    double min_timestamp;
    double max_timestamp;
    uint64_t owner_process_or_agent_id;
    uint64_t owner_queue_or_thread_id;
    double min_value;
    double max_value;
    TrackTopologyType topology_type;
    uint8_t reserved[7];
    uint64_t topology_id;
    uint64_t topology_process_id;
    uint64_t topology_node_id;
    uint64_t topology_device_id;
    uint8_t track_type;
    uint8_t reserved1[7];
};

// Event data structure (replaces property iteration)
struct EventData
{
    uint64_t id;
    double timestamp;
    double duration;
    uint32_t category_index;         // Index into string table
    uint32_t name_index;              // Index into string table
    uint32_t child_count;             // Number of child events
    uint32_t top_combined_name_index; // Index into string table (for events with children)
    uint8_t level;
    uint8_t operation;
    uint8_t reserved[6];
};

// Sample data structure (for counter/line/bar tracks)
struct SampleData
{
    double timestamp;      // X-axis (time)
    double value;          // Y-axis (counter value)
    double end_timestamp;  // End time (for bar width in box plot mode)
    uint8_t reserved[4];
};

// Kernel metrics data structure
struct KernelMetricsData
{
    uint32_t id;
    uint32_t name_index;              // Index into string table
    uint32_t invocation_count;
    uint32_t reserved0;
    uint64_t duration_total_ns;
    uint32_t duration_min_ns;
    uint32_t duration_max_ns;
    uint32_t duration_median_ns;
    uint32_t duration_mean_ns;
};

// Summary metrics data structure
struct SummaryData
{
    float gpu_utilization;          // Use -1.0f for "not available"
    float memory_utilization;       // Use -1.0f for "not available"
    uint32_t top_kernel_count;      // Count of kernels in top_kernels array
    uint32_t reserved0;
    double kernel_exec_time_total_ns;
};

class IDataProtocol
{
public:
    virtual ~IDataProtocol() = default;

    virtual Result Initialize(const char* trace_path) = 0;
    virtual void SetController(void* controller_handle) = 0;
    virtual void Shutdown() = 0;

    virtual Result BeginLoad(ProtocolRequestHandle* out_request) = 0;
    virtual Result BeginFetchTrack(uint64_t track_id, double start_time, double end_time,
                                   ProtocolRequestHandle* out_request) = 0;
    virtual Result BeginFetchTrackLod(uint64_t track_id, double start_time,
                                      double end_time, uint32_t pixel_range,
                                      ProtocolRequestHandle* out_request) = 0;
    virtual Result BeginFetchSummary(double start_time, double end_time,
                                     ProtocolRequestHandle* out_request) = 0;

    virtual Result GetRequestStatus(ProtocolRequestHandle request, ProtocolRequestStatus* out_status) = 0;
    virtual Result CancelRequest(ProtocolRequestHandle request) = 0;
    virtual Result ReleaseRequest(ProtocolRequestHandle request) = 0;

    // Returned track arrays and string tables remain valid until the next
    // protocol call that refreshes the same cache, `Shutdown()`, or destruction.
    virtual Result GetTrackList(
        const ProtocolTrackInfo** out_tracks,
        uint32_t* out_count,
        const StringTable** out_strings) = 0;

    // Request result arrays remain valid until `ReleaseRequest(request)` is called.
    virtual Result GetEventResult(ProtocolRequestHandle request,
                                  const EventData** out_events,
                                  uint32_t* out_count,
                                  const StringTable** out_strings) = 0;
    virtual Result GetSampleResult(ProtocolRequestHandle request,
                                   const SampleData** out_samples,
                                   uint32_t* out_count) = 0;
    virtual Result GetSummaryResult(ProtocolRequestHandle request,
                                    SummaryData* out_summary,
                                    const KernelMetricsData** out_top_kernels,
                                    uint32_t* out_top_kernel_count,
                                    const StringTable** out_strings) = 0;

    virtual const char* ResolveString(uint32_t string_index) const = 0;
    virtual Result GetTimeRange(double* out_min_timestamp,
                                double* out_max_timestamp) const = 0;
    virtual const char* GetTraceFilePath() const = 0;
};

}  // namespace View
}  // namespace RocProfVis
