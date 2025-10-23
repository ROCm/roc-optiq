// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;
class Future;
class Data;
class ComputeTable;
class ComputePlot;

class ComputeTrace : public Handle
{
public:
    ComputeTrace();

    virtual ~ComputeTrace();

    rocprofvis_result_t Load(char const* const directory);

    rocprofvis_controller_object_type_t GetType(void) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;

private:
    rocprofvis_result_t GetMetric(const rocprofvis_controller_compute_metric_types_t metric_type, Data** value);

    std::unordered_map<rocprofvis_controller_compute_table_types_t, ComputeTable*> m_tables;
    std::unordered_map<rocprofvis_controller_compute_plot_types_t, ComputePlot*> m_plots;
};

}
}
