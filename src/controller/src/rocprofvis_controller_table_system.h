// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_table.h"
#include "rocprofvis_c_interface.h"
#include <vector>
#include <deque>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;
class Track;

class SystemTable : public Table
{
public:
    SystemTable(uint64_t id);

    virtual ~SystemTable();

    void Reset();

    rocprofvis_result_t Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args) final;
    rocprofvis_result_t Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index,
                              uint64_t count, Array& array) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final;

private:
    std::vector<uint32_t> m_tracks;
    double m_start_ts;
    double m_end_ts;
};

}
}
