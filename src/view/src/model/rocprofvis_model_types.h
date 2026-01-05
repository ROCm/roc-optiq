// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"

#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Forward declarations
class TableRequestParams;
class TrackItem;

enum class GraphType
{
    TYPE_LINECHART,
    TYPE_FLAMECHART
};

// Track Graph Information
struct TrackGraph
{
    GraphType  graph_type;
    bool       display;
    bool       display_changed;
    TrackItem* chart;
    bool       selected;

};

// Track Information
struct TrackInfo
{
    uint64_t                           index;  // index of the track in the controller
    uint64_t                           id;     // id of the track in the controller
    std::string                        name;   // name of the track
    rocprofvis_controller_track_type_t track_type;   // the type of track
    double                             min_ts;       // starting time stamp of track
    double                             max_ts;       // ending time stamp of track
    uint64_t                           num_entries;  // number of entries in the track
    double min_value;  // minimum value in the track (for samples) or level (for events)
    double max_value;  // maximum value in the track (for samples) or level (for events)
    rocprofvis_handle_t* graph_handle;  // handle to the graph object owned by the track

    struct Topology
    {
        uint64_t node_id;     // ID of track's parent node
        uint64_t process_id;  // ID of track's parent process
        uint64_t device_id;   // ID of track's parent device
        enum
        {
            Unknown,
            Queue,
            Stream,
            InstrumentedThread,
            SampledThread,
            Counter
        } type;
        uint64_t id;  // ID of queue/stream/thread/counter
    } topology;
};

// Event Information
struct EventExtData
{
    std::string category;
    std::string name;
    std::string value;
    uint64_t    category_enum;
};

struct EventFlowData
{
    uint64_t    id;
    uint64_t    start_timestamp;
    uint64_t    end_timestamp;
    uint64_t    track_id;
    uint64_t    level;
    uint64_t    direction;
    std::string name;
};

struct CallStackData
{
    std::string file;
    std::string pc;
    std::string name;
    std::string address;
};

struct TraceEvent
{
    uint64_t    m_id;
    std::string m_name;
    double      m_start_ts;
    double      m_duration;
    uint32_t    m_level;
    uint32_t    m_child_count;
    std::string m_top_combined_name;
};

struct EventInfo
{
    uint64_t                   track_id;  // ID of owning track.
    TraceEvent                 basic_info;
    std::vector<EventExtData>  ext_info;
    std::vector<EventFlowData> flow_info;
    std::vector<CallStackData> call_stack_info;
};

// Topology Information
struct NodeInfo
{
    uint64_t              id;
    std::string           host_name;
    std::string           os_name;
    std::string           os_release;
    std::string           os_version;
    std::vector<uint64_t> device_ids;   // IDs of this node's devices
    std::vector<uint64_t> process_ids;  // IDs of this node's processes
};

struct DeviceInfo
{
    uint64_t                               id;
    std::string                            product_name;
    rocprofvis_controller_processor_type_t type;        // GPU/CPU
    uint64_t                               type_index;  // GPU0, GPU1...etc
};

struct ProcessInfo
{
    uint64_t    id;
    double      start_time;
    double      end_time;
    std::string command;
    std::string environment;
    std::vector<uint64_t>
        instrumented_thread_ids;  // IDs of this process' instrumented threads
    std::vector<uint64_t> sampled_thread_ids;  // IDs of this process' sampled threads
    std::vector<uint64_t> queue_ids;           // IDs of this process' queues
    std::vector<uint64_t> stream_ids;          // IDs of this process' streams
    std::vector<uint64_t> counter_ids;         // IDs of this process' counters
};

struct IterableInfo
{
    uint64_t    id;
    std::string name;
};

struct ThreadInfo : public IterableInfo
{
    double   start_time;
    double   end_time;
    uint64_t tid;
};

struct QueueInfo : public IterableInfo
{
    uint64_t device_id;  // ID of owning device.
};

struct StreamInfo : public IterableInfo
{
    uint64_t device_id;  // ID of owning device.
};

struct CounterInfo : public IterableInfo
{
    uint64_t    device_id;  // ID of owning device.
    std::string description;
    std::string units;
    std::string value_type;
};

// Summary Information
struct SummaryInfo
{
    struct KernelMetrics
    {
        std::string name;
        uint64_t    invocations;
        double      exec_time_sum;
        double      exec_time_min;
        double      exec_time_max;
        float       exec_time_pct;
    };

    struct GPUMetrics
    {
        std::optional<float>       gfx_utilization;
        std::optional<float>       mem_utilization;
        std::vector<KernelMetrics> top_kernels;
        double                     kernel_exec_time_total;
    };

    struct CPUMetrics
    {};

    struct AggregateMetrics
    {
        rocprofvis_controller_summary_aggregation_level_t     type;
        std::optional<uint64_t>                               id;
        std::optional<std::string>                            name;
        std::optional<rocprofvis_controller_processor_type_t> device_type;
        std::optional<uint64_t>                               device_type_index;
        GPUMetrics                                            gpu;
        CPUMetrics                                            cpu;
        std::vector<AggregateMetrics>                         sub_metrics;
    };
};

// Table Information
struct FormattedColumnInfo
{
    bool                     needs_formatting;
    std::vector<std::string> formatted_row_value;
};

struct TableInfo
{
    std::shared_ptr<TableRequestParams>   table_params;
    std::vector<std::string>              table_header;
    std::vector<std::vector<std::string>> table_data;
    std::vector<FormattedColumnInfo>      formatted_column_data;
    uint64_t                              total_row_count;
};

}  // namespace View
}  // namespace RocProfVis
