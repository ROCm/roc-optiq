// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <array>

namespace RocProfVis
{
namespace View
{

class ComputeTableRequestParams;

struct AvailableMetrics
{
    struct Entry
    {
        size_t      index;
        uint32_t    category_id;
        uint32_t    table_id;
        uint32_t    id;
        std::string name;
        std::string description;
        std::string unit; 
    };
    struct Table
    {
        uint32_t                             id;
        std::string                          name;
        std::unordered_map<uint32_t, Entry&> entries;
        std::vector<const Entry*>            ordered_entries;  // built from map values; never null
        std::vector<std::string>             value_names;
    };
    struct Category
    {
        uint32_t                            id;
        std::string                         name;
        std::unordered_map<uint32_t, Table> tables;
        std::vector<const Table*>           ordered_tables;   // built from map values; never null
    };
    std::vector<Entry>                     list;
    std::unordered_map<uint32_t, Category> tree;
    std::vector<const Category*>           ordered_categories;  // built from map values; never null
};

struct Point
{
    double x;
    double y;
};

struct PcStallReason
{
    int32_t     type_id  = 0;
    std::string type_name;
    int32_t     count    = 0;
};

struct StallRecord
{
    uint32_t id                 = 0;
    uint32_t isa_line_id        = 0;
    uint64_t dispatch_id        = 0;
    float    avg_active_lanes   = 0.0f;
    uint32_t wave_issued_count  = 0;
    uint32_t total_sample_count = 0;

    std::vector<PcStallReason> stall_reasons;
};

struct IsaToIsaDep
{
    uint32_t dependent_isa_line_id  = 0;
    uint32_t dependency_isa_line_id = 0;
};

struct IsaToSourceDep
{
    uint32_t isa_line_id    = 0;
    uint32_t source_line_id = 0;
    uint32_t depth          = 0;
};

struct IsaLine
{
    uint32_t    id                  = 0;
    uint64_t    code_object_offset  = 0;
    uint32_t    instruction_type_id = 0;
    std::string instruction;
    std::string comment;

    StallRecord stall_record;
    std::vector<uint32_t>
        source_line_ids;
};

struct CodeObject
{
    uint32_t             id = 0;
    std::string          uri;
    std::string          content_checksum;
    std::vector<IsaLine> isa_lines;
};

struct SourceLine
{
    uint32_t               id          = 0;
    uint32_t               line_number = 0;
    std::string            content;
    std::vector<uint32_t*> isa_line_ids;
};

struct SourceFile
{
    uint32_t    id = 0;
    std::string file_path;
    std::string content_checksum;

    std::vector<SourceLine> source_lines;
};

struct PcSamplingData
{
    std::vector<CodeObject>     code_objects;
    std::vector<SourceFile>     source_files;
    std::vector<IsaToIsaDep>    isa_to_isa_deps;
    std::vector<IsaToSourceDep> isa_to_source_deps;
};

struct KernelInfo
{
    enum DispatchMetric
    {
        InvocationCount,
        DurationTotal,
        DurationMin,
        DurationMax,
        DurationMean,
        DurationMedian,
        NumMetrics
    };
    struct Roofline
    {
        struct Intensity
        {
            rocprofvis_controller_roofline_kernel_intensity_type_t type;
            Point                                                  position;
        };
        std::unordered_map<rocprofvis_controller_roofline_kernel_intensity_type_t,
                           Intensity>
            intensities;
    };
    uint32_t                         id;
    std::string                      name;
    std::array<uint64_t, NumMetrics> dispatch_metrics;
    Roofline                         roofline;
    PcSamplingData                   pc_sampling_data;
};

struct WorkloadInfo
{
    struct Roofline
    {
        struct Line
        {
            Point p1;
            Point p2;
        };
        struct Ceiling
        {
            rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth_type;
            rocprofvis_controller_roofline_ceiling_compute_type_t   compute_type;
            Line                                                    position;
            double                                                  throughput;
        };
        std::unordered_map<
            rocprofvis_controller_roofline_ceiling_bandwidth_type_t,
            std::unordered_map<rocprofvis_controller_roofline_ceiling_compute_type_t,
                               Ceiling>>
            ceiling_bandwidth;
        std::unordered_map<
            rocprofvis_controller_roofline_ceiling_compute_type_t,
            std::unordered_map<rocprofvis_controller_roofline_ceiling_bandwidth_type_t,
                               Ceiling>>
              ceiling_compute;
        Point max;
        Point min;
    };
    uint32_t                                 id;
    std::string                              name;
    std::vector<std::vector<std::string>>    system_info;
    std::vector<std::vector<std::string>>    profiling_config;
    AvailableMetrics                         available_metrics;
    std::unordered_map<uint32_t, KernelInfo> kernels;
    std::vector<const KernelInfo*>           ordered_kernels;  // built from map values; never null
    Roofline                                 roofline;
};

struct MetricValue 
{
    AvailableMetrics::Entry*                   entry;
    rocprofvis_controller_metric_source_type_t source_type;
    WorkloadInfo* workload;  // Valid when source_type is workload
    KernelInfo*   kernel;    // Valid when source_type is kernel
    std::unordered_map<std::string, double> values;
};

struct MetricId
{
    static constexpr unsigned table_bits    = 32;
    static constexpr uint64_t table_mask  = (1ull << table_bits) - 1;

    std::string ToString() const
    {
        return std::to_string(category_id) + "." + std::to_string(table_id) + "." +
               std::to_string(entry_id);
    }

    bool operator==(const MetricId& other) const noexcept
    {
        return category_id == other.category_id && table_id == other.table_id &&
               entry_id == other.entry_id;
    }

    bool operator<(const MetricId& other) const noexcept
    {
        if(category_id != other.category_id)
            return category_id < other.category_id;

        if(table_id != other.table_id)
            return table_id < other.table_id;

        return entry_id < other.entry_id;
    }

    static constexpr uint64_t GetTableKey(uint32_t category_id,
                                          uint32_t table_id) noexcept
    {
        return (static_cast<uint64_t>(category_id) << table_bits) |
               static_cast<uint64_t>(table_id);
    }

    constexpr uint64_t GetTableKey() const noexcept
    {
        return GetTableKey(category_id, table_id);
    }

    static constexpr uint32_t ExtractCategoryId(uint64_t key) noexcept
    {
        return static_cast<uint32_t>(key >> table_bits);
    }

    static constexpr uint32_t ExtractTableId(uint64_t key) noexcept
    {
        return static_cast<uint32_t>(key & table_mask);
    }

    uint32_t category_id = 0;
    uint32_t table_id    = 0;
    uint32_t entry_id    = 0;
};

struct MetricIdHash
{
    size_t operator()(const MetricId& id) const noexcept
    {
        uint64_t value = (static_cast<uint64_t>(id.category_id) << 48) ^
                         (static_cast<uint64_t>(id.table_id) << 16) ^
                         static_cast<uint64_t>(id.entry_id);

        return std::hash<uint64_t>{}(value);
    }
};

struct ComputeTableInfo
{
    std::shared_ptr<ComputeTableRequestParams>   table_params;
    std::vector<std::string>              table_header;
    std::vector<std::vector<std::string>> table_data;
};

}  // namespace View
}  // namespace RocProfVis
