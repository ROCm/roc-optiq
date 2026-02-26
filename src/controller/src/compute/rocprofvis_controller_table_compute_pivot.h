// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_table.h"
#include "rocprofvis_c_interface.h"
#include <vector>
#include <string>
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;

class ComputePivotTable : public Table
{
public:
    ComputePivotTable(const uint64_t id);

    virtual ~ComputePivotTable();

    rocprofvis_result_t Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future) final;
    rocprofvis_result_t Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count, Array& array, Future* future) final;
    rocprofvis_result_t ExportCSV(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future, const char* path) const final;

    // Handlers for getters
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

private:
    // Stored arguments from Setup()
    uint64_t m_workload_id;
    std::vector<std::string> m_metric_selectors;  // Format: "metric_id:value_name"
};

}
}
