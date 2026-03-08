// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

// Result type for all controller operations
enum class Result
{
    Success = 0,
    Timeout,
    NotLoaded,
    InvalidArgument,
    NotSupported,
    Cancelled,
    OutOfMemory,
    UnknownError
};

class IController;
class ITrack;
class ApiStringTable;

class ApiStringTable
{
public:
    ApiStringTable();
    ~ApiStringTable();

    uint32_t AddString(const char* str);
    const char* GetString(uint32_t index) const;
    uint32_t GetCount() const;
    void Clear();

private:
    std::vector<std::string>             m_strings;
    std::unordered_map<std::string, uint32_t> m_index_map;
};

using RequestHandle = uint64_t;

enum class RequestState : uint8_t
{
    Invalid = 0,
    Pending,
    Ready,
    Error,
    Cancelled
};

struct RequestStatus
{
    RequestState state;
    Result       result;
    uint8_t      reserved[6];
};

struct EventRecord
{
    uint64_t id;
    double timestamp;
    double duration;
    uint32_t category_index;         // Index into StringTable
    uint32_t name_index;              // Index into StringTable
    uint32_t child_count;             // Number of child events
    uint32_t top_combined_name_index; // Index into StringTable (for events with children)
    uint8_t level;
    uint8_t operation;
    uint8_t reserved[6];
};

struct SampleRecord
{
    double  timestamp;
    double  value;
    double  end_timestamp;
    uint8_t reserved[4];
};

struct KernelMetrics
{
    uint32_t id;
    uint32_t name_index;           // Index into StringTable
    uint32_t invocation_count;
    uint32_t _padding;
    uint64_t duration_total_ns;
    uint32_t duration_min_ns;
    uint32_t duration_max_ns;
    uint32_t duration_median_ns;
    uint32_t duration_mean_ns;
};

struct MetricData
{
    uint32_t kernel_id;
    uint32_t metric_id_index;      // Index into StringTable
    uint32_t metric_name_index;    // Index into StringTable
    uint32_t value_name_index;     // Index into StringTable
    double value;
};

struct ControllerSummaryMetrics
{
    float gpu_utilization;
    float memory_utilization;
    uint32_t top_kernel_count;
    uint32_t reserved0;
    double kernel_exec_time_total_ns;
};

enum class TrackTopologyType : uint8_t
{
    Unknown = 0,
    Queue,
    Stream,
    InstrumentedThread,
    SampledThread,
    Counter
};

struct TrackTopologyInfo
{
    TrackTopologyType type;
    uint8_t           reserved[7];
    uint64_t          id;
    uint64_t          process_id;
    uint64_t          node_id;
    uint64_t          device_id;
};

class IController
{
public:
    virtual ~IController() = default;

    // `Create()` allocates a controller instance owned by the caller.
    // `Attach()` wraps an existing controller handle without taking ownership.
    static IController* Create(const char* filename);
    static IController* Attach(void* controller_handle);
    static void Destroy(IController* controller);

    virtual Result BeginLoad(RequestHandle* out_request) = 0;
    virtual Result BeginFetchSummary(double start_time, double end_time,
                                     RequestHandle* out_request) = 0;

    virtual Result GetRequestStatus(RequestHandle request,
                                    RequestStatus* out_status) = 0;
    virtual Result CancelRequest(RequestHandle request) = 0;
    virtual Result ReleaseRequest(RequestHandle request) = 0;

    virtual Result GetLoadResult(RequestHandle request) = 0;
    // Result buffers returned from `GetSummaryResult()` remain valid until
    // `ReleaseRequest(request)` is called.
    virtual Result GetSummaryResult(RequestHandle request,
                                    ControllerSummaryMetrics* out_metrics,
                                    const KernelMetrics** out_kernels,
                                    uint32_t* out_count,
                                    const ApiStringTable** out_strings) = 0;

    virtual Result GetTrackCount(uint32_t* out_count) const = 0;
    virtual Result GetTrack(uint32_t index, ITrack** out_track) const = 0;
    virtual Result GetTrackById(uint64_t id, ITrack** out_track) const = 0;

    // Returned string tables remain owned by the controller and stay valid for the
    // lifetime of the controller unless otherwise documented by an implementation.
    virtual Result GetStringTable(const ApiStringTable** out_strings) const = 0;
    virtual Result GetTimeRange(double* out_min_timestamp,
                                double* out_max_timestamp) const = 0;
    virtual const char* GetTraceFilePath() const = 0;
};

class ITrack
{
public:
    enum class Type : uint8_t
    {
        Events = 0,
        Samples = 1
    };

    virtual ~ITrack() = default;

    virtual uint64_t GetId() const = 0;
    virtual uint32_t GetCategoryIndex() const = 0;
    virtual uint32_t GetMainNameIndex() const = 0;
    virtual uint32_t GetSubNameIndex() const = 0;
    virtual Type   GetType() const = 0;
    virtual double GetMinTimestamp() const = 0;
    virtual double GetMaxTimestamp() const = 0;
    virtual uint32_t GetEntryCount() const = 0;
    virtual uint64_t GetOwnerProcessOrAgentId() const = 0;
    virtual uint64_t GetOwnerQueueOrThreadId() const = 0;
    virtual double   GetMinValue() const = 0;
    virtual double   GetMaxValue() const = 0;
    virtual TrackTopologyInfo GetTopology() const = 0;

    virtual Result BeginFetch(double start_time, double end_time,
                              RequestHandle* out_request) = 0;
    virtual Result BeginFetchLod(uint32_t pixel_range, double start_time,
                                 double end_time, RequestHandle* out_request) = 0;

    // Result buffers returned from `GetEventResult()` / `GetSampleResult()` remain
    // valid until `ReleaseRequest(request)` is called on the owning controller.
    virtual Result GetEventResult(RequestHandle request,
                                  const EventRecord** out_events,
                                  uint32_t* out_count,
                                  const ApiStringTable** out_strings) = 0;
    virtual Result GetSampleResult(RequestHandle request,
                                   const SampleRecord** out_samples,
                                   uint32_t* out_count) = 0;
};

}  // namespace Controller
}  // namespace RocProfVis
