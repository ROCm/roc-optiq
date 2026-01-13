// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Data;

class PlotSeries : public Handle
{
public:
    PlotSeries();

    virtual ~PlotSeries();

    rocprofvis_controller_object_type_t GetType(void) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value) final;

private:
    std::string m_name;
    std::vector<std::pair<double, double>> m_values; //x, y pairs

};

}
}
