// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_plot_series.h"
#include "rocprofvis_c_interface.h"
#include <vector>
#include <map>
#include <string>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;

class Plot : public Handle
{
public:
    Plot(uint64_t id, std::string title, std::string x_axis_title, std::string y_axis_title);

    virtual ~Plot();

    virtual rocprofvis_controller_object_type_t GetType(void);

    virtual rocprofvis_result_t Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args) = 0;
    virtual rocprofvis_result_t Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count, Array& array);

    virtual rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value);
    virtual rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value);
    virtual rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value);
    virtual rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length);

    virtual rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value);
    virtual rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value);
    virtual rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value);
    virtual rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length);

protected:
    struct AxisDefintion
    {
        std::string m_title;
        std::vector<std::string> m_tick_labels;
    };
    uint64_t m_id;
    std::string m_title;
    AxisDefintion m_x_axis;
    AxisDefintion m_y_axis;
    std::map<std::string, PlotSeries> m_series;
};

}
}
