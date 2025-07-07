// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_processor.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

Processor::Processor() {}

Processor::~Processor() {}

rocprofvis_controller_object_type_t
Processor::GetType(void)
{
    return kRPVControllerObjectTypeProcessor;
}

rocprofvis_result_t
Processor::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessorId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorTypeIndex:
            {
                *value = m_type_index;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorName:
            case kRPVControllerProcessorModelName:
            case kRPVControllerProcessorUserName:
            case kRPVControllerProcessorVendorName:
            case kRPVControllerProcessorProductName:
            case kRPVControllerProcessorExtData:
            case kRPVControllerProcessorUUID:
            case kRPVControllerProcessorType:
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

rocprofvis_result_t
Processor::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData:
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t
Processor::GetObject(rocprofvis_property_t property, uint64_t index,
                     rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData:
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t
Processor::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                     uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorName:
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
        case kRPVControllerProcessorModelName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_model_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_model_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorUserName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_user_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_user_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorVendorName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_vendor_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_vendor_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorProductName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_product_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_product_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorExtData:
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
        case kRPVControllerProcessorUUID:
        {
            if(value && length && *length)
            {
                strncpy(value, m_uuid.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_uuid.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorType:
        {
            if(value && length && *length)
            {
                strncpy(value, m_type.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_type.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t
Processor::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        {
            m_id   = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorTypeIndex:
        {
            m_type_index = value;
            result       = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorNodeId:
        {
            m_node_id = value;
            result    = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData:
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
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

rocprofvis_result_t
Processor::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData:
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t
Processor::SetObject(rocprofvis_property_t property, uint64_t index,
                     rocprofvis_handle_t* value)

{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        case kRPVControllerProcessorName:
        case kRPVControllerProcessorModelName:
        case kRPVControllerProcessorUserName:
        case kRPVControllerProcessorVendorName:
        case kRPVControllerProcessorProductName:
        case kRPVControllerProcessorExtData:
        case kRPVControllerProcessorUUID:
        case kRPVControllerProcessorType:
        case kRPVControllerProcessorTypeIndex:
        case kRPVControllerProcessorNodeId:
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

rocprofvis_result_t
Processor::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                     uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && length)
    {
        switch(property)
        {
            case kRPVControllerProcessorName:
            {
                m_name = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorModelName:
            {
                m_model_name = value;
                result       = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorUserName:
            {
                m_user_name = value;
                result      = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorVendorName:
            {
                m_vendor_name = value;
                result        = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorProductName:
            {
                m_product_name = value;
                result         = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorExtData:
            {
                m_ext_data = value;
                result     = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorUUID:
            {
                m_uuid = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorType:
            {
                m_type = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorId:
            case kRPVControllerProcessorTypeIndex:
            case kRPVControllerProcessorNodeId:
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

}
}
