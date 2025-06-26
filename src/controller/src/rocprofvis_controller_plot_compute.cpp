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
                            result = kRocProfVisResultUnknownError;
                            double data;
                            if (values[i].second->GetType() == kRPVControllerPrimitiveTypeDouble)
                            {
                                double data_double;
                                result = values[i].second->GetDouble(&data_double);
                                data = data_double;
                            }
                            else if (values[i].second->GetType() == kRPVControllerPrimitiveTypeUInt64)
                            {
                                uint64_t data_uint;
                                result = values[i].second->GetUInt64(&data_uint);
                                data = data_uint;
                            }
                            if (result == kRocProfVisResultSuccess)
                            {
                                m_series[series_name].SetDouble(kRPVControllerPlotSeriesXValuesIndexed, existing_values, data);
                                m_series[series_name].SetDouble(kRPVControllerPlotSeriesYValuesIndexed, existing_values, existing_values);
                                if (series_name.empty())
                                {
                                    m_y_axis.m_tick_labels.emplace_back(values[i].first);
                                }
                                existing_values ++;
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
                            uint64_t data_uint;
                            result = value.second->GetUInt64(&data_uint);
                            data = data_uint;
                        }
                    }
                    result = kRocProfVisResultSuccess;
                    m_series[series_name].SetDouble(kRPVControllerPlotSeriesXValuesIndexed, existing_values, data);
                    m_series[series_name].SetDouble(kRPVControllerPlotSeriesYValuesIndexed, existing_values, existing_values);
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

}  // namespace Controller
}
