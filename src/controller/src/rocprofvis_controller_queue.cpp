// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_queue.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

Queue::Queue() {

}

Queue::~Queue() {

}

rocprofvis_controller_object_type_t Queue::GetType(void)
{
    return kRPVControllerObjectTypeQueue;
}

rocprofvis_result_t Queue::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerQueueId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerQueueNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerQueueProcessId:
            {
                *value = m_process_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerQueueName:
            case kRPVControllerQueueExtData:
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

rocprofvis_result_t Queue::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerQueueId:
            case kRPVControllerQueueNodeId:
            case kRPVControllerQueueProcessId:
            case kRPVControllerQueueName:
            case kRPVControllerQueueExtData:
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

rocprofvis_result_t Queue::GetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
        case kRPVControllerQueueName:
        case kRPVControllerQueueExtData:
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
    return result;
}

rocprofvis_result_t Queue::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                  uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerQueueExtData:
        {
            if(value && length && *length)
            {
                strncpy(value, m_ext_data.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_ext_data.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
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
    return result;
}

rocprofvis_result_t Queue::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueId:
        {
            m_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueNodeId:
        {
            m_node_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueProcessId:
        {
            m_process_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueName:
        case kRPVControllerQueueExtData:
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
    return result;
}

rocprofvis_result_t Queue::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
        case kRPVControllerQueueName:
        case kRPVControllerQueueExtData:
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
    return result;
}

rocprofvis_result_t Queue::SetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
        case kRPVControllerQueueName:
        case kRPVControllerQueueExtData:
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
    return result;
}

rocprofvis_result_t Queue::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                  uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueName:
        {
            m_name = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueExtData:
        {
            m_ext_data = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueId:
        case kRPVControllerQueueNodeId:
        case kRPVControllerQueueProcessId:
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
    return result;
}

}
}
