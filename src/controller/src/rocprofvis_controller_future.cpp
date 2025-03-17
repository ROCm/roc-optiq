// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_future.h"

#include <cassert>

namespace RocProfVis
{
namespace Controller
{

Future::Future()
{
}

Future::~Future()
{
}

rocprofvis_controller_object_type_t Future::GetType(void) 
{
    return kRPVControllerObjectTypeFuture;
}

void Future::Set(std::future<rocprofvis_result_t>&& future)
{
    assert(future.valid() && !m_future.valid());
    m_future = std::move(future);
}

bool Future::IsValid() const
{
    return m_future.valid();
}

rocprofvis_result_t Future::Wait(float timeout)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (timeout == FLT_MAX)
    {
        m_future.wait();
        result = kRocProfVisResultSuccess;
    }
    else
    {
        std::chrono::microseconds time =
            std::chrono::microseconds(uint64_t(timeout * 1000000.0));
        std::future_status   status = m_future.wait_for(time);
        if (status == std::future_status::timeout || status == std::future_status::deferred)
        {
            result = kRocProfVisResultTimeout;
        }
        else
        {
            result = kRocProfVisResultSuccess;
        }
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
            case kRPVControllerFutureResult:
            {
                if(m_future.valid())
                {
                    *value = m_future.get();
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
    assert(0);
    return kRocProfVisResultReadOnlyError;
}
rocprofvis_result_t Future::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}
rocprofvis_result_t Future::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}
rocprofvis_result_t Future::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}

}
}
