// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_plot.h"
#include "rocprofvis_controller_array.h"
#include <cstring>

namespace RocProfVis
{
namespace Controller
{

Plot::Plot(uint64_t id, std::string title, std::string x_axis_title, std::string y_axis_title)
: m_id(id)
, m_title(title)
{
	m_x_axis.m_title = x_axis_title;
	m_y_axis.m_title = y_axis_title;
}

Plot::~Plot()
{
}

rocprofvis_controller_object_type_t Plot::GetType(void)
{
	return kRPVControllerObjectTypePlot;
}

rocprofvis_result_t Plot::Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count, Array& array)
{
    (void) dm_handle;
    (void) count;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, m_series.size());
    if (result == kRocProfVisResultSuccess)
    {
        index = 0;
        std::vector<Data>& array_data = array.GetVector();
        array_data.resize(m_series.size());
        for (auto& it : m_series)
        {
            result = array.SetObject(kRPVControllerArrayEntryIndexed, index, (rocprofvis_handle_t*)&it.second);
            index ++;
        }
    }
    return result;
}

rocprofvis_result_t Plot::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerPlotId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPlotNumSeries:
            {
                *value = m_series.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPlotNumXAxisLabels:
            {
                *value = m_x_axis.m_tick_labels.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPlotNumYAxisLabels:
            {
                *value = m_y_axis.m_tick_labels.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPlotXAxisLabelsIndexed:
            case kRPVControllerPlotYAxisLabelsIndexed:
            case kRPVControllerPlotXAxisTitle:
            case kRPVControllerPlotYAxisTitle:
            case kRPVControllerPlotTitle:
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

rocprofvis_result_t Plot::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerPlotId:
            case kRPVControllerPlotNumSeries:
            case kRPVControllerPlotNumXAxisLabels:
            case kRPVControllerPlotNumYAxisLabels:
            case kRPVControllerPlotXAxisLabelsIndexed:
            case kRPVControllerPlotYAxisLabelsIndexed:
            case kRPVControllerPlotXAxisTitle:
            case kRPVControllerPlotYAxisTitle:
            case kRPVControllerPlotTitle:
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

rocprofvis_result_t Plot::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerPlotId:
            case kRPVControllerPlotNumSeries:
            case kRPVControllerPlotNumXAxisLabels:
            case kRPVControllerPlotNumYAxisLabels:
            case kRPVControllerPlotXAxisLabelsIndexed:
            case kRPVControllerPlotYAxisLabelsIndexed:
            case kRPVControllerPlotXAxisTitle:
            case kRPVControllerPlotYAxisTitle:
            case kRPVControllerPlotTitle:
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

rocprofvis_result_t Plot::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (length)
    {
        switch (property)
        {
            case kRPVControllerPlotId:
            case kRPVControllerPlotNumSeries:
            case kRPVControllerPlotNumXAxisLabels:
            case kRPVControllerPlotNumYAxisLabels:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            case kRPVControllerPlotXAxisLabelsIndexed:
            {
                if (index < m_x_axis.m_tick_labels.size())
                {
                    if (!value && length)
                    {
                        *length = static_cast<uint32_t>(m_x_axis.m_tick_labels[index].size());
                        result  = kRocProfVisResultSuccess;
                    }
                    else if (value && length)
                    {
                        strncpy(value, m_x_axis.m_tick_labels[index].c_str(), *length);
                        result = kRocProfVisResultSuccess;
                    }
                }
                break;
            }
            case kRPVControllerPlotYAxisLabelsIndexed:
            {
                if (index < m_y_axis.m_tick_labels.size())
                {
                    if (!value && length)
                    {
                        *length = static_cast<uint32_t>(m_y_axis.m_tick_labels[index].size());
                        result  = kRocProfVisResultSuccess;
                    }
                    else if (value && length)
                    {
                        strncpy(value, m_y_axis.m_tick_labels[index].c_str(), *length);
                        result = kRocProfVisResultSuccess;
                    }
                }                
                break;
            }
            case kRPVControllerPlotXAxisTitle:
            {
                if (!value && length)
                {
                    *length = static_cast<uint32_t>(m_x_axis.m_title.size());
                    result  = kRocProfVisResultSuccess;
                }
                else if (value && length)
                {
                    strncpy(value, m_x_axis.m_title.c_str(), *length);
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPlotYAxisTitle:
            {
                if (!value && length)
                {
                    *length = static_cast<uint32_t>(m_y_axis.m_title.size());
                    result  = kRocProfVisResultSuccess;
                }
                else if (value && length)
                {
                    strncpy(value, m_y_axis.m_title.c_str(), *length);
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPlotTitle:
            {
                if (!value && length)
                {
                    *length = static_cast<uint32_t>(m_title.size());
                    result  = kRocProfVisResultSuccess;
                }
                else if (value && length)
                {
                    strncpy(value, m_title.c_str(), *length);
                    result = kRocProfVisResultSuccess;
                }
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

rocprofvis_result_t Plot::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t Plot::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t Plot::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t Plot::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

}
}
