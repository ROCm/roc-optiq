// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_data.h"
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
class Future;

class Table : public Handle
{
public:
    Table(uint64_t id, uint32_t first_prop_index, uint32_t last_prop_index);

    virtual ~Table();

    virtual rocprofvis_controller_object_type_t GetType(void);

    virtual rocprofvis_result_t Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future) = 0;
    virtual rocprofvis_result_t Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count, Array& array, Future* future) = 0;
    virtual rocprofvis_result_t ExportCSV(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future, const char* path) const = 0;

protected:
    struct ColumnDefintion
    {
        std::string m_name;
        rocprofvis_controller_primitive_type_t m_type;
    };

    std::vector<ColumnDefintion> m_columns;
    std::map<uint64_t, std::vector<Data>> m_rows;
    std::string m_filter;
    std::string m_group;
    std::string m_group_cols;
    uint64_t m_num_items;
    uint64_t m_id;
    uint64_t m_sort_column;
    rocprofvis_controller_sort_order_t m_sort_order;
};

}
}
