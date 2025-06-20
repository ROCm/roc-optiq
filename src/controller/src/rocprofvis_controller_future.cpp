// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_future.h"
#include "rocprofvis_core_assert.h"

#include <cfloat>

namespace RocProfVis
{
namespace Controller
{

Future::Future()
: m_job(false)
{
}

Future::~Future()
{ 
    delete m_job;
}

rocprofvis_controller_object_type_t Future::GetType(void) 
{
    return kRPVControllerObjectTypeFuture;
}

void Future::Set(Job* job)
{
    ROCPROFVIS_ASSERT(job);
    m_job = job;
}

bool Future::IsValid() const
{
    return m_job;
}

rocprofvis_result_t Future::Wait(float timeout)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (m_job)
    {
        result = m_job->Wait(timeout);
    }
    return result;
}

rocprofvis_result_t Future::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value && (property == kRPVControllerFutureResult))
    {
        switch (property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Future);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerFutureResult:
            {
                if(m_job)
                {
                    *value = m_job->GetResult();
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultUnknownError;
                }
                break;
            }
            case kRPVControllerFutureType:
            {
                *value = m_object.GetType();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerFutureObject:
            {
                result = m_object.GetUInt64(value);
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
rocprofvis_result_t Future::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && (property == kRPVControllerFutureResult))
    {
        switch(property)
        {
            case kRPVControllerFutureObject:
            {
                result = m_object.GetDouble(value);
                break;
            }
            case kRPVControllerFutureResult:
            case kRPVControllerFutureType:
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
rocprofvis_result_t Future::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && (property == kRPVControllerFutureResult))
    {
        switch(property)
        {
            case kRPVControllerFutureObject:
            {
                result = m_object.GetObject(value);
                break;
            }
            case kRPVControllerFutureResult:
            case kRPVControllerFutureType:
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
rocprofvis_result_t Future::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && (property == kRPVControllerFutureResult))
    {
        switch(property)
        {
            case kRPVControllerFutureObject:
            {
                result = m_object.GetString(value, length);
                break;
            }
            case kRPVControllerFutureResult:
            case kRPVControllerFutureType:
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

rocprofvis_result_t Future::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    switch(property)
    {
        case kRPVControllerFutureObject:
        case kRPVControllerFutureResult:
        case kRPVControllerFutureType:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}
rocprofvis_result_t Future::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    switch(property)
    {
        case kRPVControllerFutureObject:
        case kRPVControllerFutureResult:
        case kRPVControllerFutureType:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}
rocprofvis_result_t Future::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    switch(property)
    {
        case kRPVControllerFutureObject:
        case kRPVControllerFutureResult:
        case kRPVControllerFutureType:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}
rocprofvis_result_t Future::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    switch(property)
    {
        case kRPVControllerFutureObject:
        case kRPVControllerFutureResult:
        case kRPVControllerFutureType:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}

}
}
