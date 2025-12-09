// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_plot.h"
#include "rocprofvis_c_interface.h"

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class ComputeTable;

class ComputePlot : public Plot
{
public:
    ComputePlot(const uint64_t id, const std::string& title, const std::string& x_axis_title, const std::string& y_axis_title, const rocprofvis_controller_compute_plot_types_t type);

    virtual ~ComputePlot();

    rocprofvis_result_t Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args) final;
    rocprofvis_result_t Load(const ComputeTable* table, const std::string& series_name, const std::vector<std::string>& keys);
    rocprofvis_result_t Load(ComputeTable* counter_table, ComputeTable* benchmark_table);

private:
    uint64_t GetUInt64FromData(Data* data);
    double GetDoubleFromData(Data* data);

    rocprofvis_controller_compute_plot_types_t m_type;

};

}
}
