// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_plot_compute.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_table_compute.h"

namespace RocProfVis
{
namespace Controller
{

RocProfVis::Controller::ComputePlot::ComputePlot(const uint64_t id, const std::string& title, const std::string& x_axis_title, const std::string& y_axis_title, const rocprofvis_controller_compute_plot_types_t type)
: Plot(id, title, x_axis_title, y_axis_title)
, m_type(type)
{
}

ComputePlot::~ComputePlot() 
{
}

rocprofvis_result_t ComputePlot::Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args)
{
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
            result = m_series[series_name].SetString(kRPVControllerPlotSeriesName, 0, series_name.c_str(), series_name.length());
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
                            Data* data = values[i].second;
                            if (data->GetType() == kRPVControllerPrimitiveTypeDouble)
                            {
                                double data_double;
                                data->GetDouble(&data_double);
                                m_series[series_name].SetDouble(kRPVControllerPlotSeriesXValuesIndexed, existing_values + i, data_double);
                                m_series[series_name].SetDouble(kRPVControllerPlotSeriesYValuesIndexed, existing_values + i, existing_values + i);
                                if (series_name.empty())
                                {
                                    m_y_axis.m_tick_labels.emplace_back(values[i].first);
                                }
                            }
                            else if (data->GetType() == kRPVControllerPrimitiveTypeUInt64)
                            {
                                uint64_t data_uint;
                                data->GetUInt64(&data_uint);
                                m_series[series_name].SetDouble(kRPVControllerPlotSeriesXValuesIndexed, existing_values + i, data_uint);
                                m_series[series_name].SetDouble(kRPVControllerPlotSeriesYValuesIndexed, existing_values + i, existing_values + i);
                                if (series_name.empty())
                                {
                                    m_y_axis.m_tick_labels.emplace_back(values[i].first);
                                }
                            }
                        }
                    }
                }
                else
                {
                    // Retrieve metric from table by specific name.
                    std::pair<std::string, Data*> value;
                    result = table->GetMetric(key, value);
                    if (result == kRocProfVisResultSuccess)
                    {
                        Data* data = value.second;
                        if (data->GetType() == kRPVControllerPrimitiveTypeDouble)
                        {
                            double data_double;
                            data->GetDouble(&data_double);
                            m_series[series_name].SetDouble(kRPVControllerPlotSeriesXValuesIndexed, existing_values, data_double);
                            m_series[series_name].SetDouble(kRPVControllerPlotSeriesYValuesIndexed, existing_values, existing_values);
                            if (series_name.empty())
                            {
                                m_y_axis.m_tick_labels.emplace_back(value.first);
                            }
                        }
                        else if (data->GetType() == kRPVControllerPrimitiveTypeUInt64)
                        {
                            uint64_t data_uint;
                            data->GetUInt64(&data_uint);
                            m_series[series_name].SetDouble(kRPVControllerPlotSeriesXValuesIndexed, existing_values, data_uint);
                            m_series[series_name].SetDouble(kRPVControllerPlotSeriesYValuesIndexed, existing_values, existing_values);
                            if (series_name.empty())
                            {
                                m_y_axis.m_tick_labels.emplace_back(value.first);
                            }
                        }                       
                    }
                }
            }
        }
    }   
    return result;
}

}  // namespace Controller
}
