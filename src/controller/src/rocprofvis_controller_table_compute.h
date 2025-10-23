// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_table.h"
#include "rocprofvis_c_interface.h"
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;

class ComputeTable : public Table
{
public:
    ComputeTable(const uint64_t id, const rocprofvis_controller_compute_table_types_t type, const std::string& title);

    virtual ~ComputeTable();

    rocprofvis_result_t Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future) final;
    rocprofvis_result_t Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index,
                              uint64_t count, Array& array, Future* future) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t Load(const std::string& csv_file);
    rocprofvis_result_t GetMetric(const std::string& key, std::pair<std::string, Data*>& metric) const;
    rocprofvis_result_t GetMetricFuzzy(const std::string& key, std::vector<std::pair<std::string, Data*>>& metrics) const;

private:
    rocprofvis_result_t LoadSystemInfo(const std::string& csv_file);

    struct MetricMapEntry {
        std::string m_name;
        Data* m_data;
    };
    rocprofvis_controller_compute_table_types_t m_type;
    std::string m_title;
    std::unordered_map<std::string, MetricMapEntry> m_metrics_map;
};

}
}
