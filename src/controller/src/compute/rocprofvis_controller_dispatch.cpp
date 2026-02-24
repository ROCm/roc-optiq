// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_dispatch.h"

namespace RocProfVis
{
namespace Controller
{

Dispatch::Dispatch()
: Handle(__kRPVControllerDispatchPropertiesFirst, __kRPVControllerDispatchPropertiesLast)
, m_dispatch_uuid(0)
, m_kernel_uuid(0)
, m_dispatch_id(0)
, m_gpu_id(0)
, m_start_timestamp(0)
, m_end_timestamp(0)
{}

Dispatch::~Dispatch()
{}

rocprofvis_controller_object_type_t Dispatch::GetType(void)
{
    return kRPVControllerObjectTypeDispatch;
}

rocprofvis_result_t Dispatch::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
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
                *value = sizeof(Dispatch);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerDispatchUUID:
            {
                *value = (uint64_t)m_dispatch_uuid;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerDispatchKernelUUID:
            {
                *value = (uint64_t)m_kernel_uuid;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerDispatchId:
            {
                *value = (uint64_t)m_dispatch_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerDispatchGpuId:
            {
                *value = (uint64_t)m_gpu_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerDispatchStartTimestamp:
            {
                *value = m_start_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerDispatchEndTimestamp:
            {
                *value = m_end_timestamp;
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

rocprofvis_result_t Dispatch::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void)index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerDispatchUUID:
        {
            m_dispatch_uuid = (uint32_t)value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerDispatchKernelUUID:
        {
            m_kernel_uuid = (uint32_t)value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerDispatchId:
        {
            m_dispatch_id = (uint32_t)value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerDispatchGpuId:
        {
            m_gpu_id = (uint32_t)value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerDispatchStartTimestamp:
        {
            m_start_timestamp = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerDispatchEndTimestamp:
        {
            m_end_timestamp = value;
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

bool Dispatch::QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_property_t& property, rocprofvis_controller_primitive_type_t& type) const
{
    bool valid = true;
    switch(in)
    {
        case kRPVComputeColumnDispatchUUID:
        {
            property = kRPVControllerDispatchUUID;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnDispatchKernelUUID:
        {
            property = kRPVControllerDispatchKernelUUID;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnDispatchId:
        {
            property = kRPVControllerDispatchId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnDispatchGpuId:
        {
            property = kRPVControllerDispatchGpuId;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnDispatchStartTimestamp:
        {
            property = kRPVControllerDispatchStartTimestamp;
            type = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVComputeColumnDispatchEndTimestamp:
        {
            property = kRPVControllerDispatchEndTimestamp;
            type = kRPVControllerPrimitiveTypeUInt64;
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

}
}
