// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_ext_data.h"
#include "rocprofvis_controller_reference.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_ext_data_t, ExtData, kRPVControllerObjectTypeExtData> TrackRef;

ExtData::ExtData(const char* category, const char* name, const char* value,
                 rocprofvis_db_data_type_t type, uint64_t category_enum)
: Handle(__kRPVControllerExtDataPropertiesFirst, __kRPVControllerExtDataPropertiesLast)
, m_category(category)
, m_name(name)
, m_enum(category_enum)
{
    switch (type)
    {
        case kRPVDataTypeInt: 
            m_value = Data((uint64_t)std::atoll(value));
            break;
        case kRPVDataTypeDouble: 
            m_value = Data(std::atof(value));
            break;
        default: 
            m_value = Data(value);
            break;

    }
}

ExtData::~ExtData() {}

rocprofvis_controller_object_type_t ExtData::GetType(void) 
{
    return kRPVControllerObjectTypeExtData;
}

rocprofvis_result_t ExtData::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerEventArgumentPosition:
                *value = 0;
                break;
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
                    *value += sizeof(uint64_t);
                }
                break;
            }
            case kRPVControllerExtDataCategoryEnum:
            {
                *value = m_enum;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerExtDataType:
            {
                *value = m_value.GetType();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerExtDataValue:
            {
                if (m_value.GetType() == kRPVControllerPrimitiveTypeUInt64)
                {
                    m_value.GetUInt64(value);
                    result = kRocProfVisResultSuccess;
                    break;
                }
                result = kRocProfVisResultInvalidType;
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
rocprofvis_result_t ExtData::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerExtDataValue:
            {
                if(m_value.GetType() == kRPVControllerPrimitiveTypeDouble)
                {
                    m_value.GetDouble(value);
                    result = kRocProfVisResultSuccess;
                    break;
                }
                result = kRocProfVisResultInvalidType;
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

rocprofvis_result_t ExtData::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventArgumentType:
        {
            Data data = Data("");
            result = data.GetString(value, length);
            break;
        }
        case kRPVControllerExtDataCategory:
            result = m_category.GetString(value, length);
            break;
        case kRPVControllerExtDataName:
            result = m_name.GetString(value, length);
            break;
        case kRPVControllerExtDataValue:
            switch(m_value.GetType())
            {
                case kRPVControllerPrimitiveTypeString:
                    result = m_value.GetString(value, length);
                    break;
                case kRPVControllerPrimitiveTypeUInt64:
                {
                    uint64_t int_data = 0;
                    if(kRocProfVisResultSuccess == m_value.GetUInt64(&int_data))
                    {
                        Data data = Data(std::to_string(int_data).c_str());
                        result    = data.GetString(value, length);
                    }
                    break;
                }
                case kRPVControllerPrimitiveTypeDouble:
                {
                    double double_data = 0;
                    if(kRocProfVisResultSuccess == m_value.GetDouble(&double_data))
                    {
                        Data data = Data(std::to_string(double_data).c_str());
                        result    = data.GetString(value, length);
                    }
                    break;
                }
                default: 
                    result = kRocProfVisResultInvalidType;
                    break;
            }           
            break;
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}


ArgumentData::ArgumentData(const char* category, const char* name, const char* value,
    rocprofvis_db_data_type_t type, uint64_t category_enum, size_t position, const char* arg_type) 
    :ExtData(category,name,value, type, category_enum)
    ,m_position(static_cast<uint64_t>(position))
    ,m_arg_type(arg_type) { }

ArgumentData::~ArgumentData() {}

rocprofvis_controller_object_type_t ArgumentData::GetType(void) 
{
    return kRPVControllerObjectTypeEventArgument;
}

rocprofvis_result_t ArgumentData::GetUInt64(rocprofvis_property_t property, uint64_t index,
    uint64_t* value) {
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch (property)
        {
        case kRPVControllerEventArgumentPosition:
            result = m_position.GetUInt64(value);
            break;
        default:
            result = ExtData::GetUInt64(property, index, value);
            break;
        }
    }
    return result;
}

rocprofvis_result_t ArgumentData::GetString(rocprofvis_property_t property, uint64_t index,
    char* value, uint32_t* length) {
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch (property)
    {
    case kRPVControllerEventArgumentType:
        result = m_arg_type.GetString(value, length);
        break;
    default:
        result = ExtData::GetString(property, index, value, length);
        break;
    }
    return result;
}

}
}
