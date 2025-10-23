// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_plot_compute.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_table_compute.h"
#include "rocprofvis_controller_compute_metrics.h"
#include "rocprofvis_core_assert.h"
#include <array>

namespace RocProfVis
{
namespace Controller
{

ComputePlot::ComputePlot(const uint64_t id, const std::string& title, const std::string& x_axis_title, const std::string& y_axis_title, const rocprofvis_controller_compute_plot_types_t type)
: Plot(id, title, x_axis_title, y_axis_title)
, m_type(type)
{
}

ComputePlot::~ComputePlot() 
{
}

rocprofvis_result_t ComputePlot::Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args)
{
    (void) dm_handle;
    (void) args;
    rocprofvis_result_t result = kRocProfVisResultSuccess;
    return result;
}

rocprofvis_result_t ComputePlot::Load(const ComputeTable* table, const std::string& series_name, const std::vector<std::string>& keys)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (table)
    {        
        uint64_t existing_values = 0;
        if (m_series.count(series_name) > 0)
        {
            // Existing series, get the number of existing values.
            result = m_series[series_name].GetUInt64(kRPVControllerPlotSeriesNumValues, 0, &existing_values);
        }
        else
        {
            // New series, set the name.
            result = m_series[series_name].SetString(kRPVControllerPlotSeriesName, 0, series_name.c_str());
        }
        if (result == kRocProfVisResultSuccess)
        {
            for (const std::string& key : keys)
            {
                // Retrieve metrics from table by searching for matching names.
                if (key.front() == '{' && key.back() == '}')
                {
                    std::vector<std::pair<std::string, Data*>> values;
                    result = table->GetMetricFuzzy(key.substr(1, key.size() - 2), values);
                    if (result == kRocProfVisResultSuccess)
                    {
                        for (uint64_t i = 0; i < values.size(); i ++)
                        {
                            result = kRocProfVisResultUnknownError;
                            double data = 0;
                            if (values[i].second->GetType() == kRPVControllerPrimitiveTypeDouble)
                            {
                                double data_double;
                                result = values[i].second->GetDouble(&data_double);
                                data = data_double;
                            }
                            else if (values[i].second->GetType() == kRPVControllerPrimitiveTypeUInt64)
                            {
                                uint64_t data_uint = 0;
                                result = values[i].second->GetUInt64(&data_uint);
                                data = static_cast<double>(data_uint);
                            }
                            if (result == kRocProfVisResultSuccess)
                            {
                                m_series[series_name].SetDouble(kRPVControllerPlotSeriesXValuesIndexed, existing_values, data);
                                m_series[series_name].SetDouble(kRPVControllerPlotSeriesYValuesIndexed, existing_values, static_cast<double>(existing_values));
                                if (series_name.empty())
                                {
                                    m_y_axis.m_tick_labels.emplace_back(values[i].first);
                                }
                                existing_values++;
                            }
                        }
                    }
                }
                else
                {
                    // Retrieve metric from table by specific name.
                    std::pair<std::string, Data*> value;
                    result = table->GetMetric(key, value);
                    double data = 0;
                    if (result == kRocProfVisResultSuccess)
                    {
                        if (value.second->GetType() == kRPVControllerPrimitiveTypeDouble)
                        {
                            result = value.second->GetDouble(&data);
                        }
                        else if (value.second->GetType() == kRPVControllerPrimitiveTypeUInt64)
                        {
                            uint64_t data_uint = 0;
                            result = value.second->GetUInt64(&data_uint);
                            data = static_cast<double>(data_uint);
                        }
                    }
                    result = kRocProfVisResultSuccess;
                    m_series[series_name].SetDouble(kRPVControllerPlotSeriesXValuesIndexed, existing_values, data);
                    m_series[series_name].SetDouble(kRPVControllerPlotSeriesYValuesIndexed, existing_values, static_cast<double>(existing_values));
                    if (series_name.empty())
                    {
                        m_y_axis.m_tick_labels.emplace_back(value.first);
                    }
                    existing_values ++;
                }
                if (result == kRocProfVisResultSuccess)
                {
                    m_series[series_name].SetUInt64(kRPVControllerPlotSeriesNumValues, 0, existing_values);
                }
            }
        }
    }   
    return result;
}

rocprofvis_result_t ComputePlot::Load(ComputeTable* counter_table, ComputeTable* benchmark_table)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (counter_table && benchmark_table)
    {   
        std::array dispatch_groups = {std::unordered_map<std::string, std::vector<std::string>> {}, std::unordered_map<std::string, std::vector<std::string>> {}};
        std::pair<std::string, Data*> metric_data;

        uint64_t num_dispatches = 0;
        result = counter_table->GetUInt64(kRPVControllerTableNumRows, 0, &num_dispatches);
        if (result == kRocProfVisResultSuccess)
        {
            ROCPROFVIS_ASSERT(num_dispatches > 0);
            uint32_t length = 0;
            result = counter_table->GetString(kRPVControllerTableColumnHeaderIndexed, 1, nullptr, &length);
            if (result == kRocProfVisResultSuccess)
            {
                ROCPROFVIS_ASSERT(length >= 0);
                std::string kernel_column_name;
                char* kernel_column_name_char = new char[length + 1];
                kernel_column_name_char[length] = '\0';
                result = counter_table->GetString(kRPVControllerTableColumnHeaderIndexed, 1, kernel_column_name_char, &length);
                if (result == kRocProfVisResultSuccess)
                {
                    kernel_column_name = kernel_column_name_char;
                    for (uint64_t dispatch = 0; dispatch < num_dispatches; dispatch ++)
                    { 
                        std::string dispatch_id = std::to_string(dispatch);
                        result = counter_table->GetMetric(dispatch_id + " " + kernel_column_name, metric_data);
                        if (result == kRocProfVisResultSuccess)
                        {
                            std::string kernel_name;
                            length = 0;
                            result = metric_data.second->GetString(nullptr, &length);
                            if (result == kRocProfVisResultSuccess)
                            {
                                ROCPROFVIS_ASSERT(length >= 0);
                                char* kernel_name_char = new char[length + 1];
                                kernel_name_char[length] = '\0';
                                result = metric_data.second->GetString(kernel_name_char, &length);
                                if (result == kRocProfVisResultSuccess)
                                {
                                    kernel_name = kernel_name_char;
                                    dispatch_groups[0]["Dispatch ID:" + dispatch_id + ":" + kernel_name] = {dispatch_id};
                                    dispatch_groups[1]["Kernel:" + kernel_name].push_back(dispatch_id);
                                }
                                delete[] kernel_name_char;
                            }
                        }
                    }
                }
                delete[] kernel_column_name_char;
            }
        }
        if (result == kRocProfVisResultSuccess)
        {
            for (std::unordered_map<std::string, std::vector<std::string>> group : dispatch_groups)
            {
                for (auto& it : group)
                {
                    const std::string& label = it.first;
                    const std::vector<std::string>& dispatch_list = it.second;
                    const int& dispatch_count = static_cast<int>(dispatch_list.size());

                    uint64_t flops_counters = 0;
                    uint64_t l2_counters = 0;
                    uint64_t l1_counters = 0;
                    uint64_t hbm_counters = 0; 
                    uint64_t dispatch_duration = 0;

                    std::array counter_array = {
                        std::make_pair(ComputeRooflineDefinition::MemoryLevelHBM, &hbm_counters), 
                        std::make_pair(ComputeRooflineDefinition::MemoryLevelL2, &l2_counters), 
                        std::make_pair(ComputeRooflineDefinition::MemoryLevelL1, &l1_counters)
                    };

                    for (const std::string& dispatch_id : dispatch_list)
                    {
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU ADD F16", metric_data));
                        flops_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU MUL F16", metric_data));
                        flops_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU FMA F16", metric_data));
                        flops_counters += 128 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU TRANS F16", metric_data));
                        flops_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU ADD F32", metric_data));
                        flops_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU MUL F32", metric_data));
                        flops_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU FMA F32", metric_data));
                        flops_counters += 128 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU TRANS F32", metric_data));
                        flops_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU ADD F64", metric_data));
                        flops_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU MUL F64", metric_data));
                        flops_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU FMA F64", metric_data));
                        flops_counters += 128 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU TRANS F64", metric_data));
                        flops_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU MFMA MOPS F16", metric_data));
                        flops_counters += 512 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU MFMA MOPS BF16", metric_data));
                        flops_counters += 512 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU MFMA MOPS F32", metric_data));
                        flops_counters += 512 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " SQ INSTS VALU MFMA MOPS F64", metric_data));
                        flops_counters += 512 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCP TCC WRITE REQ sum", metric_data));
                        l2_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCP TCC ATOMIC WITH RET REQ sum", metric_data));
                        l2_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCP TCC ATOMIC WITHOUT RET REQ sum", metric_data));
                        l2_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCP TCC READ REQ sum", metric_data));
                        l2_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCP TOTAL CACHE ACCESSES sum", metric_data));
                        l1_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCC EA0 RDREQ 32B sum", metric_data));
                        hbm_counters += 32 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCC EA0 WRREQ sum", metric_data));
                        hbm_counters += 32 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCC EA0 WRREQ 64B sum", metric_data));
                        hbm_counters -= 32 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCC EA0 WRREQ 64B sum", metric_data));
                        hbm_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCC EA0 RDREQ sum", metric_data));
                        hbm_counters += 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCC BUBBLE sum", metric_data));
                        hbm_counters -= 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCC EA0 RDREQ 32B sum", metric_data));
                        hbm_counters -= 64 * GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " TCC BUBBLE sum", metric_data));
                        hbm_counters += 128 * GetUInt64FromData(metric_data.second);                            
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " End Timestamp", metric_data));
                        dispatch_duration += GetUInt64FromData(metric_data.second);
                        ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == counter_table->GetMetric(dispatch_id + " Start Timestamp", metric_data));
                        dispatch_duration -= GetUInt64FromData(metric_data.second);
                    }
                    if (flops_counters > 0 && dispatch_duration > 0)
                    {
                        for (std::pair<ComputeRooflineDefinition::MemoryLevel, uint64_t*>& memory_level : counter_array)
                        {
                            if (*memory_level.second > 0)
                            {
                                std::string name = label + "-" + ROOFLINE_DEFINITION.m_memory_names.at(memory_level.first);
                                double x = static_cast<double>(flops_counters) / dispatch_count / *(memory_level.second) / dispatch_count;
                                double y = static_cast<double>(flops_counters) / dispatch_count / dispatch_duration / dispatch_count;

                                PlotSeries series;
                                ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetString(kRPVControllerPlotSeriesName, 0, name.c_str()));
                                ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesXValuesIndexed, 0, x));
                                ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesYValuesIndexed, 0, y));
                                m_series[name] = std::move(series);
                            }
                        }
                    }
                }
            }
            const std::string device_id = std::to_string(ROOFLINE_DEFINITION.m_device_id);
            for (const ComputeRooflineDefinition::Format& format : ROOFLINE_DEFINITION.m_plots.at(m_type).m_formats)
            {
                std::array<std::pair<ComputeRooflineDefinition::MemoryLevel, double>, ComputeRooflineDefinition::MemoryLevelCount> memory_bw;
                double max_memory_bw = 0;
                double max_ops = 0;
                    
                for (uint32_t j = ComputeRooflineDefinition::MemoryLevelHBM; j <ComputeRooflineDefinition::MemoryLevelCount; j ++)
                {
                    const ComputeRooflineDefinition::MemoryLevel level = static_cast<ComputeRooflineDefinition::MemoryLevel>(j);                        
                        
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == benchmark_table->GetMetric(device_id + " " + ROOFLINE_DEFINITION.m_memory_names.at(level) + "Bw", metric_data));
                    memory_bw[j] = std::make_pair(level, GetDoubleFromData(metric_data.second));
                    max_memory_bw = std::max(max_memory_bw, memory_bw[j].second);
                }
                for (auto& it : ROOFLINE_DEFINITION.m_format_prefix.at(format))
                {
                    const ComputeRooflineDefinition::Pipe type = it.first;
                    const std::string& prefix = it.second;
                    const std::string name = ROOFLINE_DEFINITION.m_pipe_names.at(type) + "-" + ROOFLINE_DEFINITION.m_format_names.at(format);

                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == benchmark_table->GetMetric(device_id + " " + prefix, metric_data));
                    double ops = GetDoubleFromData(metric_data.second);
                    max_ops = std::max(max_ops, ops);
                        
                    PlotSeries series;
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetString(kRPVControllerPlotSeriesName, 0, name.c_str()));
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesXValuesIndexed, 0, ops / max_memory_bw));
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesYValuesIndexed, 0, ops));
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesXValuesIndexed, 1, ROOFLINE_DEFINITION.x_axis_max));
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesYValuesIndexed, 1, ops));
                    m_series[name] = std::move(series);
                }
                for (std::pair<ComputeRooflineDefinition::MemoryLevel, double>& memory_level : memory_bw)
                {
                    ComputeRooflineDefinition::MemoryLevel& level = memory_level.first;
                    double& bw = memory_level.second;

                    std::string name = ROOFLINE_DEFINITION.m_memory_names.at(level) + "-" + ROOFLINE_DEFINITION.m_format_names.at(format);

                    PlotSeries series;
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetString(kRPVControllerPlotSeriesName, 0, name.c_str()));
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesXValuesIndexed, 0, ROOFLINE_DEFINITION.x_axis_min));
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesYValuesIndexed, 0, ROOFLINE_DEFINITION.x_axis_min * bw));
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesXValuesIndexed, 1, max_ops / bw));
                    ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == series.SetDouble(kRPVControllerPlotSeriesYValuesIndexed, 1, max_ops));
                    m_series[name] = std::move(series);
                }
            }
        }
    }  
    return result;
}

uint64_t ComputePlot::GetUInt64FromData(Data* data)
{
    uint64_t value = 0;
    switch(data->GetType())
    {
        case kRPVControllerPrimitiveTypeUInt64:
        {
            ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == data->GetUInt64(&value));
            break;
        }
        case kRPVControllerPrimitiveTypeDouble:
        {
            double value_double;
            ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == data->GetDouble(&value_double));
            value = static_cast<uint64_t>(value_double);
            break;
        }
        default:
        {
            ROCPROFVIS_ASSERT(false);
        }
    }
    return value;
}

double ComputePlot::GetDoubleFromData(Data* data)
{
    double value = 0;
    switch(data->GetType())
    {
        case kRPVControllerPrimitiveTypeUInt64:
        {
            uint64_t uint_value;
            ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == data->GetUInt64(&uint_value));
            value = static_cast<double>(uint_value);
            break;
        }
        case kRPVControllerPrimitiveTypeDouble:
        {
            ROCPROFVIS_ASSERT(kRocProfVisResultSuccess == data->GetDouble(&value));
            break;
        }
        default:
        {
            ROCPROFVIS_ASSERT(false);
        }
    }
    return value;
}

}  // namespace Controller
}
