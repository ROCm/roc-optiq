// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_table_compute.h"
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
    rocprofvis_result_t result = kRocProfVisResultSuccess;
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
    if (m_type == kRPVControllerComputeTableTypeSysInfo)
    {
        // Special handling for system info; all values are on one row.
        return LoadSystemInfo(csv_file);
    }

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
                std::string name = column_names[col];
                std::replace(name.begin(), name.end(), '_', ' ');
                if (csv_row[col].is_null() || csv_row[col].is_str())
                {
                    m_columns.push_back(ColumnDefintion{std::move(name), kRPVControllerPrimitiveTypeString});
                }
                else if (csv_row[col].is_float())
                {
                    m_columns.push_back(ColumnDefintion{std::move(name), kRPVControllerPrimitiveTypeDouble});
                }
                else if (csv_row[col].is_int())   
                {
                    m_columns.push_back(ColumnDefintion{std::move(name), kRPVControllerPrimitiveTypeUInt64});
                }
            }       
        }
        std::vector<Data> row_data;
        for (uint32_t col = 0; col < csv_row.size(); col ++)
        {            
            if (m_columns[col].m_type == kRPVControllerPrimitiveTypeString)
            {
                row_data.emplace_back(csv_row[col].get().c_str());
            }
            else if (m_columns[col].m_type == kRPVControllerPrimitiveTypeDouble)
            {
                if (csv_row[col].is_null())
                {
                    row_data.emplace_back(-1.0); // Rows may have empty fields.
                }
                else
                {
                    row_data.emplace_back(csv_row[col].get<double>());
                }
            }
            else if (m_columns[col].m_type == kRPVControllerPrimitiveTypeUInt64)
            {
                if (csv_row[col].is_null())
                {
                    row_data.emplace_back(static_cast<uint64_t>(-1)); // Rows may have empty fields.
                }
                else
                {
                    row_data.emplace_back(csv_row[col].get<uint64_t>());
                }               
            }
        }
        m_rows[row_count] = std::move(row_data);

        // Keep track of all numerical fields in table for plots to reference.
        // Key for any value is [row header][space][column header].
        for (int i = 1; i < m_rows[row_count].size(); i ++)
        {
            std::string row_name = csv_row[0].get();
            m_metrics_map[row_name + " " + m_columns[i].m_name] = MetricMapEntry{row_name, &m_rows[row_count][i]};
        }

        row_count ++;
    }

    result = kRocProfVisResultSuccess;
    return result;
}

rocprofvis_result_t ComputeTable::LoadSystemInfo(const std::string& csv_file)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;

    csv::CSVFormat format;
    format.delimiter(',');
    format.header_row(0);
    csv::CSVReader csv(csv_file, format);
    std::vector<std::string> column_names = csv.get_col_names();
    m_columns = {ColumnDefintion{"Field", kRPVControllerPrimitiveTypeString}, ColumnDefintion{"Value", kRPVControllerPrimitiveTypeString}};

    uint64_t row_count = 0;
    csv::CSVRow csv_row;
    if (csv.read_row(csv_row))
    {     
        for (uint32_t col = 0; col < csv_row.size(); col ++)
        {            
            Data field;
            field.SetType(kRPVControllerPrimitiveTypeString);
            std::replace(column_names[col].begin(), column_names[col].end(), '_', ' ');
            column_names[col][0] = std::toupper(column_names[col][0]);
            field.SetString(column_names[col].c_str());
            Data value;
            value.SetType(kRPVControllerPrimitiveTypeString);
            value.SetString(csv_row[col].get().c_str());
            m_rows[row_count] = {field, value};
            row_count ++;
        }
    }

    result = kRocProfVisResultSuccess;
    return result;
}

rocprofvis_result_t ComputeTable::GetMetric(const std::string& key, std::pair<std::string, Data*>& metric) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (m_metrics_map.count(key) > 0)
    {
        const MetricMapEntry& match = m_metrics_map.at(key);
        metric = std::make_pair(match.m_name, match.m_data);
        result = kRocProfVisResultSuccess;
    }

    return result;
}

rocprofvis_result_t ComputeTable::GetMetricFuzzy(const std::string& key, std::vector<std::pair<std::string, Data*>>& metrics) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    for (auto& it : m_metrics_map)
    {
        if (it.first.find(key) != std::string::npos)
        {
            const MetricMapEntry& match = m_metrics_map.at(it.first);
            metrics.emplace_back(std::make_pair(match.m_name, match.m_data)); 
            result = kRocProfVisResultSuccess;
        }
    }
    return result;
}

}  // namespace Controller
}
