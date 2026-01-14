// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_trace.h"
#include "rocprofvis_c_interface.h"
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;
class Future;
class ComputeTable;
class Plot;
class ComputePlot;

class ComputeTrace : public Trace
{
public:
    ComputeTrace(char const* const filename);

    virtual ~ComputeTrace();

    virtual rocprofvis_result_t Init() final;

    virtual rocprofvis_result_t Load(Future& future) final;

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;

    rocprofvis_result_t AsyncFetch(Plot& plot, Arguments& args, Future& future, Array& array);

private:
    rocprofvis_result_t LoadCSV();
    rocprofvis_result_t LoadRocpd();

    rocprofvis_result_t GetMetric(const rocprofvis_controller_compute_metric_types_t metric_type, Data** value);
    /*DebugComputeTable function is for debugging purposes only. Feel free to refactor it or remove it */
    rocprofvis_result_t DebugComputeTable(rocprofvis_dm_table_id_t table_id, std::string query, std::string description);
    
    std::unordered_map<rocprofvis_controller_compute_table_types_t, ComputeTable*> m_tables;
    std::unordered_map<rocprofvis_controller_compute_plot_types_t, ComputePlot*> m_plots;

};

}
}
