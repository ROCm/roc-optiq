// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_c_interface.h"
#include <vector>
#include <map>
#include <deque>
#include <string>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;
class Track;

class Table : public Handle
{
public:
    Table(uint64_t id);

    virtual ~Table();

    rocprofvis_controller_object_type_t GetType(void) final;

    void Reset();

    rocprofvis_result_t Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args);
    rocprofvis_result_t Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index,
                              uint64_t count, Array& array);

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
    struct ColumnDefintion
    {
        std::string m_name;
        rocprofvis_controller_primitive_type_t m_type;
    };

    std::vector<uint32_t> m_tracks;
    std::vector<ColumnDefintion> m_columns;
    std::map<uint64_t, std::vector<Data>> m_rows;
    std::deque<uint64_t> m_lru;
    double m_start_ts;
    double m_end_ts;
    uint64_t m_num_items;
    uint64_t m_id;
    uint64_t m_sort_column;
    rocprofvis_controller_sort_order_t m_sort_order;
    rocprofvis_controller_track_type_t m_track_type;
};

}
}
