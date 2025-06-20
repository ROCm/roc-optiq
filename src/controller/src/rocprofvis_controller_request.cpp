// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_request.h"
#include "rocprofvis_core_assert.h"

#include <cfloat>

namespace RocProfVis
{
namespace Controller
{

Request::Request()
: m_job(false)
{
}

Request::~Request()
{ 
    delete m_job;
}

rocprofvis_controller_object_type_t Request::GetType(void) 
{
    return kRPVControllerObjectTypeRequest;
}

void Request::Set(Job* job)
{
    ROCPROFVIS_ASSERT(job);
    m_job = job;
}

bool Request::IsValid() const
{
    return m_job;
}

rocprofvis_result_t Request::Wait(float timeout)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (m_job)
    {
        result = m_job->Wait(timeout);
    }
    return result;
}

rocprofvis_result_t Request::Cancel()
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(m_job)
    {
        result = JobSystem::Get().CancelJob(m_job);
    }
    return result;
}

rocprofvis_result_t Request::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value && (property == kRPVControllerRequestResult))
    {
        switch (property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Request);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerRequestResult:
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
            case kRPVControllerRequestType:
            {
                *value = m_object.GetType();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerRequestObject:
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
rocprofvis_result_t Request::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && (property == kRPVControllerRequestResult))
    {
        switch(property)
        {
            case kRPVControllerRequestObject:
            {
                result = m_object.GetDouble(value);
                break;
            }
            case kRPVControllerRequestResult:
            case kRPVControllerRequestType:
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
rocprofvis_result_t Request::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && (property == kRPVControllerRequestResult))
    {
        switch(property)
        {
            case kRPVControllerRequestObject:
            {
                result = m_object.GetObject(value);
                break;
            }
            case kRPVControllerRequestResult:
            case kRPVControllerRequestType:
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
rocprofvis_result_t Request::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && (property == kRPVControllerRequestResult))
    {
        switch(property)
        {
            case kRPVControllerRequestObject:
            {
                result = m_object.GetString(value, length);
                break;
            }
            case kRPVControllerRequestResult:
            case kRPVControllerRequestType:
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

rocprofvis_result_t Request::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    switch(property)
    {
        case kRPVControllerRequestObject:
        case kRPVControllerRequestResult:
        case kRPVControllerRequestType:
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
rocprofvis_result_t Request::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    switch(property)
    {
        case kRPVControllerRequestObject:
        case kRPVControllerRequestResult:
        case kRPVControllerRequestType:
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
rocprofvis_result_t Request::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    switch(property)
    {
        case kRPVControllerRequestObject:
        case kRPVControllerRequestResult:
        case kRPVControllerRequestType:
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
rocprofvis_result_t Request::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    switch(property)
    {
        case kRPVControllerRequestObject:
        case kRPVControllerRequestResult:
        case kRPVControllerRequestType:
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
