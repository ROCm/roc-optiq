// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

struct AvailableMetrics
{
    struct Entry {
        size_t index;
        uint32_t category_id;
        uint32_t table_id;
        uint32_t id;
        std::string name;
        std::string description;
        std::string unit;
    }; 
    struct Table {
        uint32_t id;
        std::string name;
        std::unordered_map<uint32_t, Entry&> entries;
    };
    struct Category {
        uint32_t id;
        std::string name;
        std::unordered_map<uint32_t, Table> tables;
    };
    std::vector<Entry> list;
    std::unordered_map<uint32_t, Category> tree;
};

struct KernelInfo
{
    uint32_t id;
    std::string name;
    uint32_t invocation_count;
    uint64_t duration_total;
    uint32_t duration_min;
    uint32_t duration_max;
    uint32_t duration_mean;
    uint32_t duration_median;
};

struct WorkloadInfo
{
    uint32_t id;
    std::string name;
    std::vector<std::vector<std::string>> system_info;
    std::vector<std::vector<std::string>> profiling_config;
    AvailableMetrics available_metrics;
    std::unordered_map<uint32_t, KernelInfo> kernels;
};

struct MetricValue {
    std::string name;
    double value;
    AvailableMetrics::Entry& entry;
    KernelInfo& kernel;
};

}  // namespace View
}  // namespace RocProfVis
