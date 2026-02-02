// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_kernel.h"

namespace RocProfVis
{
namespace Controller
{

Kernel::Kernel()
: Handle(__kRPVControllerKernelPropertiesFirst, __kRPVControllerKernelPropertiesLast) 
, m_id(0)
{}

Kernel::~Kernel()
{}

rocprofvis_controller_object_type_t Kernel::GetType(void) 
{
    return kRPVControllerObjectTypeKernel;
}

rocprofvis_result_t Kernel::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    (void)index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Kernel);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerKernelId:
            {
                *value = (uint64_t)m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerKernelInvocationCount:
            {
                *value = (uint64_t)m_invocation_count;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerKernelDurationTotal:
            {
                *value = m_duration_total;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerKernelDurationMin:
            {
                *value = (uint64_t)m_duration_min;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerKernelDurationMax:
            {
                *value = (uint64_t)m_duration_max;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerKernelDurationMedian:
            {
                *value = (uint64_t)m_duration_median;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerKernelDurationMean:
            {
                *value = (uint64_t)m_duration_mean;
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

rocprofvis_result_t Kernel::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    (void)index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerKernelName:
            {
                result = GetStdStringImpl(value, length, m_name);
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

rocprofvis_result_t Kernel::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void)index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerKernelId:
        {
            m_id = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerKernelInvocationCount:
        {
            m_invocation_count = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerKernelDurationTotal:
        {
            m_duration_total = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerKernelDurationMin:
        {
            m_duration_min = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerKernelDurationMax:
        {
            m_duration_max = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerKernelDurationMedian:
        {
            m_duration_median = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerKernelDurationMean:
        {
            m_duration_mean = value;
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

rocprofvis_result_t Kernel::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    (void)index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerKernelName:
        {
            m_name = value;
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

}
}
