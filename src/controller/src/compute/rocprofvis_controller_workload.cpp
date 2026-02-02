// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_workload.h"
#include "rocprofvis_controller_kernel.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_string_table.h"
#include "json.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_kernel_t, Kernel, kRPVControllerObjectTypeKernel> KernelRef;

Workload::Workload()
: Handle(__kRPVControllerWorkloadPropertiesFirst, __kRPVControllerWorkloadPropertiesLast)
, m_id(0)
{}

Workload::~Workload()
{
    for(Kernel* kernel : m_kernels)
    {
        delete kernel;
    }
}

rocprofvis_controller_object_type_t Workload::GetType(void) 
{
    return kRPVControllerObjectTypeWorkload;
}

rocprofvis_result_t Workload::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                uint64_t size = 0;
                for(Kernel* kernel : m_kernels)
                {
                    size += kernel->GetUInt64(kRPVControllerCommonMemoryUsageInclusive, 0, &size);
                }
                size += sizeof(Workload);
                *value = size;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Workload);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerWorkloadId:
            {
                *value = (uint64_t)m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerWorkloadSystemInfoNumEntries:
            {
                *value = m_system_info.keys.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerWorkloadConfigurationNumEntries:
            {
                *value = m_profiling_config.keys.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerWorkloadNumAvailableMetrics:
            {
                *value = m_available_metrics.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerWorkloadAvailableMetricCategoryIdIndexed:
            {
                if(index < m_available_metrics.size())
                {
                    *value = m_available_metrics[index].category_id;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadAvailableMetricTableIdIndexed:
            {
                if(index < m_available_metrics.size())
                {
                    *value = m_available_metrics[index].table_id;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadNumKernels:
            {
                *value = m_kernels.size();
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

rocprofvis_result_t Workload::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerWorkloadName:
            {
                result = GetStdStringImpl(value, length, m_name);
                break;
            }
            case kRPVControllerWorkloadSystemInfoEntryNameIndexed:
            {
                if(index < m_system_info.keys.size())
                {
                    result = GetStdStringImpl(value, length, m_system_info.keys[index]);
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadSystemInfoEntryValueIndexed:
            {
                if(index < m_system_info.values.size())
                {
                    result = GetStdStringImpl(value, length, m_system_info.values[index]);
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadConfigurationEntryNameIndexed:
            {
                if(index < m_profiling_config.keys.size())
                {
                    result = GetStdStringImpl(value, length, m_profiling_config.keys[index]);
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadConfigurationEntryValueIndexed:
            {
                if(index < m_profiling_config.values.size())
                {
                    result = GetStdStringImpl(value, length, m_profiling_config.values[index]);
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadAvailableMetricCategoryNameIndexed:
            {
                if(index < m_available_metrics.size())
                {
                    char const* str = StringTable::Get().GetString(m_available_metrics[index].category_name_idx);
                    result = GetStringImpl(value, length, str, static_cast<uint32_t>(strlen(str)));
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadAvailableMetricTableNameIndexed:
            {
                if(index < m_available_metrics.size())
                {
                    char const* str = StringTable::Get().GetString(m_available_metrics[index].table_name_idx);
                    result = GetStringImpl(value, length, str, static_cast<uint32_t>(strlen(str)));
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadAvailableMetricNameIndexed:
            {
                if(index < m_available_metrics.size())
                {
                    char const* str = StringTable::Get().GetString(m_available_metrics[index].name_idx);
                    result = GetStringImpl(value, length, str, static_cast<uint32_t>(strlen(str)));
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadAvailableMetricDescriptionIndexed:
            {
                if(index < m_available_metrics.size())
                {
                    char const* str = StringTable::Get().GetString(m_available_metrics[index].description_idx);
                    result = GetStringImpl(value, length, str, static_cast<uint32_t>(strlen(str)));
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerWorkloadAvailableMetricUnitIndexed:
            {
                if(index < m_available_metrics.size())
                {
                    char const* str = StringTable::Get().GetString(m_available_metrics[index].unit_idx);
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

rocprofvis_result_t Workload::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerWorkloadKernelIndexed:
            {
                if(index < m_kernels.size())
                {
                    *value = (rocprofvis_handle_t*)m_kernels[index];
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

rocprofvis_result_t Workload::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void)index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerWorkloadId:
        {
            m_id = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerWorkloadSystemInfoNumEntries:
        {
            m_system_info.keys.resize(value);
            m_system_info.values.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerWorkloadConfigurationNumEntries:
        {
            m_profiling_config.keys.resize(value);
            m_profiling_config.values.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerWorkloadNumAvailableMetrics:
        {
            m_available_metrics.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerWorkloadAvailableMetricCategoryIdIndexed:
        {
            if(index < m_available_metrics.size())
            {
                m_available_metrics[index].category_id = (uint32_t)value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadAvailableMetricTableIdIndexed:
        {
            if(index < m_available_metrics.size())
            {
                m_available_metrics[index].table_id = (uint32_t)value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadNumKernels:
        {
            m_kernels.resize(value);
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

rocprofvis_result_t Workload::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerWorkloadKernelIndexed:
        {
            if(m_kernels.size() > index)
            {
                KernelRef ref(value);
                if(ref.IsValid())
                {
                    m_kernels[index] = ref.Get();
                    result = kRocProfVisResultSuccess;
                }
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
    return result;
}

rocprofvis_result_t Workload::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerWorkloadName:
        {
            m_name = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerWorkloadSystemInfoEntryNameIndexed:
        {
            if(index < m_system_info.keys.size())
            {
                m_system_info.keys[index] = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadSystemInfoEntryValueIndexed:
        {
            if(index < m_system_info.values.size())
            {
                m_system_info.values[index] = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadConfigurationEntryNameIndexed:
        {
            if(index < m_profiling_config.keys.size())
            {
                m_profiling_config.keys[index] = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadConfigurationEntryValueIndexed:
        {
            if(index < m_profiling_config.values.size())
            {
                m_profiling_config.values[index] = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadAvailableMetricCategoryNameIndexed:
        {
            if(index < m_available_metrics.size())
            {
                m_available_metrics[index].category_name_idx = StringTable::Get().AddString(value, true);
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadAvailableMetricTableNameIndexed:
        {
            if(index < m_available_metrics.size())
            {
                m_available_metrics[index].table_name_idx = StringTable::Get().AddString(value, true);
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadAvailableMetricNameIndexed:
        {
            if(index < m_available_metrics.size())
            {
                m_available_metrics[index].name_idx = StringTable::Get().AddString(value, true);
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadAvailableMetricDescriptionIndexed:
        {
            if(index < m_available_metrics.size())
            {
                m_available_metrics[index].description_idx = StringTable::Get().AddString(value, true);
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerWorkloadAvailableMetricUnitIndexed:
        {
            if(index < m_available_metrics.size())
            {
                m_available_metrics[index].unit_idx = StringTable::Get().AddString(value, true);
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
    return result;
}

}
}
