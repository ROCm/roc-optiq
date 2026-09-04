// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_table.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_trace.h"

namespace RocProfVis
{
namespace Controller
{

Table::Table(uint64_t id, uint32_t first_prop_index, uint32_t last_prop_index)
: Handle(first_prop_index, last_prop_index)
, m_num_items(0)
, m_id(id)
, m_sort_column(0)
, m_sort_order(kRPVControllerSortOrderAscending)
{}

Table::~Table() {}

rocprofvis_controller_object_type_t Table::GetType(void)
{
	return kRPVControllerObjectTypeTable;
}

rocprofvis_result_t
Table::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTableId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTableNumColumns:
            {
                *value = m_columns.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTableNumRows:
            {
                *value = m_num_items;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTableColumnTypeIndexed:
            {
                if(index < m_columns.size())
                {
                    *value = m_columns[index].m_type;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Table::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerTableColumnHeaderIndexed:
            {
                if(index < m_columns.size())
                {
                    result = GetStdStringImpl(value, length, m_columns[index].m_name);
                }
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

void
Table::Reset()
{
    m_num_items = 0;
    m_sort_column = 0;
    m_sort_order = kRPVControllerSortOrderAscending;
    m_columns.clear();
    m_rows.clear();
}

rocprofvis_result_t
Table::SetupAndFetch(Trace& controller, Arguments& args, Array& array, Future* future)
{
    rocprofvis_result_t result = Setup(controller.GetDMHandle(), args, future);
    if(result == kRocProfVisResultSuccess)
    {
        uint64_t start_index = 0;
        uint64_t start_count = 0;
        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetUInt64(kRPVControllerTableArgsStartIndex, 0, &start_index);
        }
        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetUInt64(kRPVControllerTableArgsStartCount, 0, &start_count);
        }
        result = Fetch(controller.GetDMHandle(), start_index, start_count, array, future);
    }
    return result;
}

rocprofvis_result_t
Table::UnpackArguments(Arguments& args, TableArguments*& out) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(!out)
    {
        out = new TableArguments();
    }
    uint64_t sort_column = 0;
    uint64_t sort_order  = 0;
    result = args.GetUInt64(kRPVControllerTableArgsSortColumn, 0, &sort_column);
    if(result == kRocProfVisResultSuccess)
    {
        result = args.GetUInt64(kRPVControllerTableArgsSortOrder, 0, &sort_order);
    }
    if(result == kRocProfVisResultSuccess)
    {
        out->m_sort_column = sort_column;
        out->m_sort_order = (rocprofvis_controller_sort_order_t)sort_order;
    }
    return result;
}

void
Table::GetCurrentArguments(TableArguments*& out) const
{
    if(!out)
    {
        out = new TableArguments();
    }
    out->m_sort_column = m_sort_column;
    out->m_sort_order = m_sort_order;
}

void
Table::SetCurrentArguments(TableArguments& in)
{
    m_sort_column = in.m_sort_column;
    m_sort_order = in.m_sort_order;
}

}
}
