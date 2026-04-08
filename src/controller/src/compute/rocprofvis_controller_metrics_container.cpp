// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_metrics_container.h"
#include "rocprofvis_controller_string_table.h"

namespace RocProfVis
{
namespace Controller
{

MetricsContainer::MetricsContainer() 
: Handle(__kRPVControllerMetricsContainerPropertiesFirst, __kRPVControllerMetricsContainerPropertiesLast)
{}

MetricsContainer::~MetricsContainer() 
{}

rocprofvis_controller_object_type_t MetricsContainer::GetType(void) 
{
    return kRPVControllerObjectTypeMetricsContainer;
}

rocprofvis_result_t MetricsContainer::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(MetricsContainer);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerMetricsContainerNumMetrics:
            {
                *value = m_container.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerMetricsContainerKernelIdIndexed:
            {
                if(index < m_container.size())
                {
                    *value = m_container[index].kernel_id;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
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

rocprofvis_result_t MetricsContainer::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerMetricsContainerMetricValueValueIndexed:
            {
                if(index < m_container.size())
                {
                    *value = m_container[index].value;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
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

rocprofvis_result_t MetricsContainer::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerMetricsContainerMetricIdIndexed:
            {
                if(index < m_container.size())
                {
                    char const* str = StringTable::Get().GetString(m_container[index].id_idx);
                    result = GetStringImpl(value, length, str, static_cast<uint32_t>(strlen(str)));
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerMetricsContainerMetricNameIndexed:
            {
                if(index < m_container.size())
                {
                    char const* str = StringTable::Get().GetString(m_container[index].name_idx);
                    result = GetStringImpl(value, length, str, static_cast<uint32_t>(strlen(str)));
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerMetricsContainerMetricValueNameIndexed:
            {
                if(index < m_container.size())
                {
                    char const* str = StringTable::Get().GetString(m_container[index].value_name_idx);
                    result = GetStringImpl(value, length, str, static_cast<uint32_t>(strlen(str)));
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
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

rocprofvis_result_t MetricsContainer::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerMetricsContainerNumMetrics:
        {
            m_container.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerMetricsContainerKernelIdIndexed:
        {
            m_container[index].kernel_id = (uint32_t)value;
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

rocprofvis_result_t MetricsContainer::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerMetricsContainerMetricValueValueIndexed:
        {
            m_container[index].value = value;
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
rocprofvis_result_t MetricsContainer::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerMetricsContainerMetricIdIndexed:
        {
            m_container[index].id_idx = StringTable::Get().AddString(value, true);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerMetricsContainerMetricNameIndexed:
        {
            m_container[index].name_idx = StringTable::Get().AddString(value, true);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerMetricsContainerMetricValueNameIndexed:
        {
            m_container[index].value_name_idx = StringTable::Get().AddString(value, true);
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

bool MetricsContainer::QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_property_t& property, rocprofvis_controller_primitive_type_t& type) const
{
    bool valid = true;
    switch(in)
    {
        case kRPVComputeColumnMetricId:
        {
            property = kRPVControllerMetricsContainerMetricIdIndexed;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnMetricName:
        {
            property = kRPVControllerMetricsContainerMetricNameIndexed;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnKernelUUID:
        {
            property = kRPVControllerMetricsContainerKernelIdIndexed;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnMetricValueName:
        {
            property = kRPVControllerMetricsContainerMetricValueNameIndexed;
            type = kRPVControllerPrimitiveTypeString;
            break;
        }
        case kRPVComputeColumnMetricValue:
        {
            property = kRPVControllerMetricsContainerMetricValueValueIndexed;
            type = kRPVControllerPrimitiveTypeDouble;
            break;
        }
        default:
        {
            valid = false;
            break;
        }
    }
    return valid;
}

}  // namespace Controller
}
