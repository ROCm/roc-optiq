// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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

private:
    rocprofvis_controller_compute_plot_types_t m_type;

};

}
}
