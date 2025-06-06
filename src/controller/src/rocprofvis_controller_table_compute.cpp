// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_table_compute.h"
#include "rocprofvis_controller_compute_metrics.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_array.h"
#include "csv.hpp"
#include <filesystem>

// Define in CSV parser lib conflicts with clashes with our Handle's GetObject.
#ifdef GetObject
#undef GetObject
#endif

namespace RocProfVis
{
namespace Controller
{

ComputeTable::ComputeTable(const uint64_t id, const rocprofvis_controller_compute_table_types_t type, const std::string& title)
: Table(id)
, m_type(type)
, m_title(title)
{
}

ComputeTable::~ComputeTable() 
{
}

rocprofvis_result_t ComputeTable::Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count, Array& array)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;    
    result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, m_rows.size());
    if (result == kRocProfVisResultSuccess)
    {
        for (uint32_t r = 0; r < m_rows.size(); r ++)
        {
            Array* row_array = new Array();
            ROCPROFVIS_ASSERT(row_array);
            std::vector<Data>& row_data = row_array->GetVector();
            row_data.resize(m_rows[r].size());
            for (uint32_t c = 0; c < m_rows[r].size(); c ++)
            {
                row_data[c].SetType(m_rows[r][c].GetType());
                row_data[c] = m_rows[r][c];
            }
            result = array.SetObject(kRPVControllerArrayEntryIndexed, r, (rocprofvis_handle_t*)row_array);
        }
    }
    return result;
}

rocprofvis_result_t ComputeTable::Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    uint64_t sort_column = 0;
    uint64_t sort_order  = 0;
    result = args.GetUInt64(kRPVControllerTableArgsSortColumn, 0, &sort_column);
    if (result == kRocProfVisResultSuccess)
    {
        result = args.GetUInt64(kRPVControllerTableArgsSortOrder, 0, &sort_order);
        if (result == kRocProfVisResultSuccess)
        {
            m_sort_column = sort_column;
            m_sort_order = static_cast<rocprofvis_controller_sort_order_t>(sort_order);
        }
    }
    return result;
}

rocprofvis_result_t ComputeTable::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
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
                *value = m_rows.size();
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
            case kRPVControllerTableColumnHeaderIndexed:
            case kRPVControllerTableRowHeaderIndexed:
            case kRPVControllerTableRowIndexed:
            case kRPVControllerTableTitle:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t ComputeTable::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTable::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTableId:
            case kRPVControllerTableNumColumns:
            case kRPVControllerTableNumRows:
            case kRPVControllerTableColumnTypeIndexed:
            case kRPVControllerTableColumnHeaderIndexed:
            case kRPVControllerTableRowHeaderIndexed:
            case kRPVControllerTableRowIndexed:
            case kRPVControllerTableTitle:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t ComputeTable::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerTableColumnHeaderIndexed:
            {
                if (index < m_columns.size())
                {
                    if(!value && length)
                    {
                        *length = m_columns[index].m_name.size();
                        result  = kRocProfVisResultSuccess;
                    }
                    else if (value && length)
                    {
                        strncpy(value, m_columns[index].m_name.c_str(), *length);
                        result = kRocProfVisResultSuccess;
                    }
                }
                break;
            }
            case kRPVControllerTableTitle:
            {
                if(!value && length)
                {
                    *length = m_title.size();
                    result  = kRocProfVisResultSuccess;
                }
                else if(value && length)
                {
                    strncpy(value, m_title.c_str(), *length);
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerTableId:
            case kRPVControllerTableNumColumns:
            case kRPVControllerTableNumRows:
            case kRPVControllerTableColumnTypeIndexed:
            case kRPVControllerTableRowHeaderIndexed:
            case kRPVControllerTableRowIndexed:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t ComputeTable::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTable::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTable::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTable::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTable::Load(const std::string& csv_file)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;

    csv::CSVFormat format;
    format.delimiter(',');
    format.header_row(0);
    csv::CSVReader csv(csv_file, format);
    std::vector<std::string> column_names = csv.get_col_names();
    uint64_t row_count = 0;
    for (csv::CSVRow& csv_row : csv)
    {
        if (csv.n_rows() == 1)
        {
            for (uint32_t col = 0; col < csv_row.size(); col ++)
            {
                if (csv_row[col].is_null() || csv_row[col].is_str())
                {
                    m_columns.push_back(ColumnDefintion{column_names[col], kRPVControllerPrimitiveTypeString});
                }
                else if (csv_row[col].is_float())
                {
                    m_columns.push_back(ColumnDefintion{column_names[col], kRPVControllerPrimitiveTypeDouble});
                }
                else if (csv_row[col].is_int())   
                {
                    m_columns.push_back(ColumnDefintion{column_names[col], kRPVControllerPrimitiveTypeUInt64});
                }
            }       
        }
        std::vector<Data> row_data;
        for (uint32_t col = 0; col < csv_row.size(); col ++)
        {            
            Data data;
            data.SetType(m_columns[col].m_type);
            if (m_columns[col].m_type == kRPVControllerPrimitiveTypeString)
            {
                data.SetString(csv_row[col].get().c_str());
            }
            else if (m_columns[col].m_type == kRPVControllerPrimitiveTypeDouble)
            {
                if (csv_row[col].is_null())
                {
                    data.SetDouble(-1);
                }
                else
                {
                    data.SetDouble(csv_row[col].get<double>());
                }
            }
            else if (m_columns[col].m_type == kRPVControllerPrimitiveTypeUInt64)
            {
                if (csv_row[col].is_null())
                {
                    data.SetUInt64(-1);
                }
                else
                {
                    data.SetUInt64(csv_row[col].get<uint64_t>());
                }               
            }
            row_data.push_back(data);
        }
        m_rows[row_count] = row_data;
        row_count ++;
    }

    result = kRocProfVisResultSuccess;
    return result;
}

bool ComputeTable::RowSorter(std::vector<Data>& a, std::vector<Data>& b)
{
    bool result = false;
    ROCPROFVIS_ASSERT(m_sort_column < m_columns.size());
    switch (m_columns[m_sort_column].m_type)
    {
        case kRPVControllerPrimitiveTypeString:
        {
            uint32_t length = -1;
            ROCPROFVIS_ASSERT(a[m_sort_column].GetString(nullptr, &length) == kRocProfVisResultSuccess);
            ROCPROFVIS_ASSERT(length >= 0);
            char* data_a = new char[length + 1];
            data_a[length] = '\0';
            ROCPROFVIS_ASSERT(a[m_sort_column].GetString(data_a, &length) == kRocProfVisResultSuccess);
            ROCPROFVIS_ASSERT(b[m_sort_column].GetString(nullptr, &length) == kRocProfVisResultSuccess);
            ROCPROFVIS_ASSERT(length >= 0);
            char* data_b = new char[length + 1];
            data_b[length] = '\0';
            ROCPROFVIS_ASSERT(a[m_sort_column].GetString(data_b, &length) == kRocProfVisResultSuccess);
            result = (m_sort_order == kRPVControllerSortOrderAscending) ? (strcmp(data_a, data_b) < 0) : (strcmp(data_a, data_b) > 0);
            delete[] data_a;
            delete[] data_b;
        }
        case kRPVControllerPrimitiveTypeDouble:
        {
            double data_a;
            ROCPROFVIS_ASSERT(a[m_sort_column].GetDouble(&data_a) == kRocProfVisResultSuccess);
            double data_b;
            ROCPROFVIS_ASSERT(b[m_sort_column].GetDouble(&data_b) == kRocProfVisResultSuccess);
            result = (m_sort_order == kRPVControllerSortOrderAscending) ? (data_a < data_b) : (data_a > data_b);
            break;
        }
        case kRPVControllerPrimitiveTypeUInt64:
        {
            uint64_t data_a;
            ROCPROFVIS_ASSERT(a[m_sort_column].GetUInt64(&data_a) == kRocProfVisResultSuccess);
            uint64_t data_b;
            ROCPROFVIS_ASSERT(b[m_sort_column].GetUInt64(&data_b) == kRocProfVisResultSuccess);
            result = (m_sort_order == kRPVControllerSortOrderAscending) ? (data_a < data_b) : (data_a > data_b);
            break;
        }
    }
    return result;
}

}  // namespace Controller
}
