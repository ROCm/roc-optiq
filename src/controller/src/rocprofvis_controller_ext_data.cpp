// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_ext_data.h"
#include "rocprofvis_controller_reference.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_ext_data_t, ExtData, kRPVControllerObjectTypeExtData> TrackRef;

ExtData::ExtData(const char* category, const char* name, const char* value)
: m_category(category)
, m_name(name)
, m_value(value)
{
}

ExtData::~ExtData()
{
}

rocprofvis_controller_object_type_t ExtData::GetType(void) 
{
    return kRPVControllerObjectTypeExtData;
}

rocprofvis_result_t ExtData::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                uint64_t cat_size = 0;
                uint64_t name_size = 0;
                uint64_t value_size = 0;
                
                *value = sizeof(ExtData);
                result = kRocProfVisResultSuccess;

                if (result == kRocProfVisResultSuccess)
                {
                    result = m_category.GetUInt64(&cat_size);
                }
                
                if (result == kRocProfVisResultSuccess)
                {
                    result = m_name.GetUInt64(&name_size);
                }
                
                if (result == kRocProfVisResultSuccess)
                {
                    result = m_value.GetUInt64(&value_size);
                }

                if (result == kRocProfVisResultSuccess)
                {
                    *value += cat_size;
                    *value += name_size;
                    *value += value_size;
                }
                break;
            }
            case kRPVControllerExtDataCategory:
            case kRPVControllerExtDataName:
            case kRPVControllerExtDataValue:
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
rocprofvis_result_t ExtData::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerExtDataCategory:
            case kRPVControllerExtDataName:
            case kRPVControllerExtDataValue:
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
rocprofvis_result_t ExtData::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerExtDataCategory:
            case kRPVControllerExtDataName:
            case kRPVControllerExtDataValue:
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
rocprofvis_result_t ExtData::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        switch(property)
        {
            case kRPVControllerExtDataCategory:
                result = m_category.GetString(value, length);
                break;
            case kRPVControllerExtDataName:
                result = m_name.GetString(value, length);
                break;
            case kRPVControllerExtDataValue:
                result = m_value.GetString(value, length);
                break;
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t ExtData::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        switch(property)
        {
            case kRPVControllerExtDataCategory:
            case kRPVControllerExtDataName:
            case kRPVControllerExtDataValue:
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
rocprofvis_result_t ExtData::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        switch(property)
        {
            case kRPVControllerExtDataCategory:
            case kRPVControllerExtDataName:
            case kRPVControllerExtDataValue:
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
rocprofvis_result_t ExtData::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerExtDataCategory:
            case kRPVControllerExtDataName:
            case kRPVControllerExtDataValue:
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
rocprofvis_result_t ExtData::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerExtDataCategory:
            case kRPVControllerExtDataName:
            case kRPVControllerExtDataValue:
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
