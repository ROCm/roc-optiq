// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

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
        std::vector<std::string>             values_names; //TODO
    };
    struct Category
    {
        uint32_t                            id;
        std::string                         name;
        std::unordered_map<uint32_t, Table> tables;
    };
    std::vector<Entry>                     list;
    std::unordered_map<uint32_t, Category> tree;
};

struct Point
{
    double x;
    double y;
};

struct KernelInfo
{
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
    uint32_t    id;
    std::string name;
    uint32_t    invocation_count;
    uint64_t    duration_total;
    uint32_t    duration_min;
    uint32_t    duration_max;
    uint32_t    duration_mean;
    uint32_t    duration_median;
    Roofline    roofline;
};

struct ChartData
{
    std::vector<const char*> m_labels;
    std::vector<double>      m_x_values;
    std::vector<double>      m_y_values;
    std::vector<double>      m_fractions;  // TODO: needed only for Pie
    double                   m_x_max;      // TODO: needed only for BAR
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
    };
    uint32_t                                 id;
    std::string                              name;
    std::vector<std::vector<std::string>>    system_info;
    std::vector<std::vector<std::string>>    profiling_config;
    AvailableMetrics                         available_metrics;
    std::unordered_map<uint32_t, KernelInfo> kernels;
    Roofline                                 roofline;
    ChartData                                GenerateChartData() const
    {
        ChartData data;
        double    total = 0.0;
        for(const auto& kernel : kernels)
        {
            data.m_labels.push_back(kernel.second.name.c_str());
            data.m_x_values.push_back(static_cast<double>(kernel.second.duration_total));
            data.m_y_values.push_back(static_cast<double>(data.m_y_values.size()));
            data.m_x_max = std::max(data.m_x_max, data.m_x_values.back());
            total += data.m_x_values.back();
        }

        data.m_fractions.resize(
            data.m_x_values
                .size());  // TODO: It need only for PieChart thing how to read of there
        for(size_t i = 0; i < data.m_x_values.size(); ++i)
        {
            data.m_fractions[i] = data.m_x_values[i] / total;
        }

        return data;
    };  // TODO: generate diferent data depend by params
 
    };

struct MetricValue 
{
    AvailableMetrics::Entry* entry;
    KernelInfo*              kernel;
    std::unordered_map<std::string, double> values;
};

union MetricKey
{
    struct
    {
        uint64_t category_id : 16;
        uint64_t table_id : 16;
        uint64_t entry_id : 32;
    } fields;
    uint64_t id;
};

union TableKey
{
    struct
    {
        uint64_t category_id : 16;
        uint64_t table_id : 48;
    } fields;
    uint64_t id;
};

struct ComputeTableInfo
{
    std::shared_ptr<ComputeTableRequestParams>   table_params;
    std::vector<std::string>              table_header;
    std::vector<std::vector<std::string>> table_data;
};

}  // namespace View
}  // namespace RocProfVis
