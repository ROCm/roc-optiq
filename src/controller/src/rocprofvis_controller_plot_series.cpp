// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_plot_series.h"
#include "rocprofvis_controller_data.h"
#include <cstring>


namespace RocProfVis
{
namespace Controller
{

PlotSeries::PlotSeries()
: Handle(__kRPVControllerPlotSeriesPropertiesFirst, __kRPVControllerPlotSeriesPropertiesLast)
, m_name("")
{}

PlotSeries::~PlotSeries() {}

rocprofvis_controller_object_type_t PlotSeries::GetType(void)
{
    return kRPVControllerObjectTypePlotSeries;
}

rocprofvis_result_t PlotSeries::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerPlotSeriesNumValues:
            {
                *value = m_values.size();
                result = kRocProfVisResultSuccess;
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

rocprofvis_result_t PlotSeries::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
            case kRPVControllerPlotSeriesXValuesIndexed:
            {
                if (index < m_values.size())
                {
                    *value = m_values[index].first;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerPlotSeriesYValuesIndexed:
            {
                if (index < m_values.size())
                {
                    *value = m_values[index].second;
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

rocprofvis_result_t PlotSeries::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (length)
    {
        switch (property)
        {
            case kRPVControllerPlotSeriesName:
            {
                if (!value && length)
                {
                    *length = static_cast<uint32_t>(m_name.size());
                    result  = kRocProfVisResultSuccess;
                }
                else if (value && length)
                {
                    strncpy(value, m_name.c_str(), *length);
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

rocprofvis_result_t PlotSeries::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    (void) value;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch (property)
    {
        case kRPVControllerPlotSeriesNumValues:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t PlotSeries::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch (property)
    {
        case kRPVControllerPlotSeriesXValuesIndexed:
        {
            if (index >= m_values.size())
            {
                m_values.resize(index + 1);
            }
            m_values[index].first = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPlotSeriesYValuesIndexed:
        {
            if (index >= m_values.size())
            {
                m_values.resize(index + 1);
            }
            m_values[index].second = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t PlotSeries::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch (property)
    {
        case kRPVControllerPlotSeriesName:
        {
            if (value)
            {
                m_name = value;
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
    return result;
}

}  // namespace Controller
}
