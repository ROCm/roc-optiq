// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_trace_compute.h"
#include "rocprofvis_controller_compute_metrics.h"
#include "rocprofvis_controller_plot_compute.h"
#include "rocprofvis_controller_table_compute.h"
#include "rocprofvis_core_assert.h"
#include <filesystem>

namespace RocProfVis
{
namespace Controller
{

ComputeTrace::ComputeTrace()
{

}

ComputeTrace::~ComputeTrace()
{
    for (auto& it : m_tables)
    {
        ComputeTable* table = it.second;
        if (table)
        {
            delete table;
        }
    }
}

rocprofvis_result_t ComputeTrace::Load(char const* const directory)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (std::filesystem::exists(directory) && std::filesystem::is_directory(directory))
    {        
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator{directory})
        {
            if (entry.path().extension() == ".csv")
            {
                const std::string& file = entry.path().filename().string();
                if (COMPUTE_TABLE_DEFINITIONS.count(file) > 0)
                {
                    const ComputeTableDefinition& definition = COMPUTE_TABLE_DEFINITIONS.at(file);
                    ComputeTable* table = new ComputeTable(m_tables.size(), definition.m_type, definition.m_title);
                    ROCPROFVIS_ASSERT(table);
                    result = table->Load(entry.path().string());
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    m_tables[definition.m_type] = table;
                }
            }
        }
        spdlog::info("ComputeTrace::Load - {}/{} Tables", m_tables.size(), COMPUTE_TABLE_DEFINITIONS.size());
        for (const ComputePlotDefinition& definition : COMPUTE_PLOT_DEFINITIONS)
        {
            ComputePlot* plot = new ComputePlot(m_plots.size(), definition.m_title, definition.x_axis_label, definition.y_axis_label, definition.m_type);
            ROCPROFVIS_ASSERT(plot);
            for (const ComputePlotDataSeriesDefinition& series : definition.m_series)
            {
                for (const ComputePlotDataDefinition& data : series.m_values)
                {
                    if (m_tables.count(data.m_table_type) > 0)
                    {
                        result = plot->Load(m_tables[data.m_table_type], series.m_name, data.m_metric_keys);
                        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    }
                    else
                    {
                        delete plot;
                        result = kRocProfVisResultNotLoaded;
                        break;
                    }
                }
                if (result != kRocProfVisResultSuccess)
                {
                    break;
                }
            }
            if (result == kRocProfVisResultSuccess)
            {
                m_plots[definition.m_type] = plot;
            }
        }
        spdlog::info("ComputeTrace::Load - {}/{} Plots", m_plots.size(), COMPUTE_PLOT_DEFINITIONS.size());
    }
    return result;
}

rocprofvis_controller_object_type_t ComputeTrace::GetType(void) 
{
    return kRPVControllerObjectTypeComputeTrace;
}

rocprofvis_result_t ComputeTrace::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        if (kRPVControllerComputeMetricTypeL2CacheRd <= property && property < kRPVControllerComputeMetricTypeCount)
        {
            Data* metric_data = nullptr;
            result = GetMetric(static_cast<rocprofvis_controller_compute_metric_types_t>(property), &metric_data);
            if (result == kRocProfVisResultSuccess)
            {
                ROCPROFVIS_ASSERT(metric_data);
                switch(metric_data->GetType())
                {
                    case kRPVControllerPrimitiveTypeUInt64:
                    {
                        result = metric_data->GetUInt64(value);
                        break;
                    }
                    case kRPVControllerPrimitiveTypeDouble:
                    {
                        double data;
                        result = metric_data->GetDouble(&data);
                        if (result == kRocProfVisResultSuccess)
                        {
                            *value = static_cast<uint64_t>(data);
                        }
                        break;
                    }
                    default: 
                    {
                        result = kRocProfVisResultInvalidType;
                        break;
                    }
                }
            }
        }
        else
        {
            result = kRocProfVisResultInvalidEnum;
        }
    }
    return result;
}

rocprofvis_result_t ComputeTrace::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTrace::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        if (kRPVControllerComputeTableTypeKernelList <= property && property < kRPVControllerComputeTableTypeCount)
        {
            rocprofvis_controller_compute_table_types_t table_type = static_cast<rocprofvis_controller_compute_table_types_t>(property);
            if (m_tables.count(table_type) > 0)
            {
                *value = (rocprofvis_handle_t*)m_tables[static_cast<rocprofvis_controller_compute_table_types_t>(property)];
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultNotLoaded;
            }          
        }
        else if (kRPVControllerComputePlotTypeKernelDurationPercentage <= property && property < kRPVControllerComputePlotTypeCount)
        {
            rocprofvis_controller_compute_plot_types_t plot_type = static_cast<rocprofvis_controller_compute_plot_types_t>(property);
            if (m_plots.count(plot_type) > 0)
            {
                *value = (rocprofvis_handle_t*)m_plots[static_cast<rocprofvis_controller_compute_plot_types_t>(property)];
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultNotLoaded;
            }
        }
        else
        {
            result = kRocProfVisResultInvalidEnum;
        }
    }
    return result;
}


rocprofvis_result_t ComputeTrace::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTrace::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}
rocprofvis_result_t ComputeTrace::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTrace::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}
rocprofvis_result_t ComputeTrace::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t ComputeTrace::GetMetric(const rocprofvis_controller_compute_metric_types_t metric_type, Data** value)
{
    rocprofvis_result_t result = kRocProfVisResultNotLoaded;
    const ComputeMetricDefinition& metric = COMPUTE_METRIC_DEFINITIONS.at(metric_type);
    const rocprofvis_controller_compute_table_types_t& table_type = metric.m_table_type;
    if (m_tables.count(table_type))
    {
        const std::string& key = metric.m_metric_key;
        std::pair<std::string, Data*> metric_data;
        result = m_tables[table_type]->GetMetric(key, metric_data);
        if (result == kRocProfVisResultSuccess)
        {
            *value = metric_data.second;
        }
    }
    return result;
}

}
}
