// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_handle.h"
#include <cstring>
#include <algorithm>

namespace RocProfVis
{
namespace Controller
{

rocprofvis_result_t Handle::GetStringImpl(char* value, uint32_t* length, char const* data, uint32_t data_len)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length && (!value || *length == 0))
    {
        *length = data_len;
        result  = kRocProfVisResultSuccess;
    }
    else if(value && length && *length > 0)
    {
        strncpy(value, data, std::min(*length, data_len+1));
        result = kRocProfVisResultSuccess;
    }
    return result;
}

rocprofvis_result_t Handle::UnhandledProperty(rocprofvis_property_t property)
{
    if(property >= m_first_prop_index && property < m_last_prop_index)
    {
        return kRocProfVisResultInvalidType;
    }
    return kRocProfVisResultInvalidEnum;
}

rocprofvis_result_t Handle::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) {
    (void) property;
    (void) index;
    (void) value;
    return kRocProfVisResultUnsupported;
}

rocprofvis_result_t Handle::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultUnsupported;
}

rocprofvis_result_t Handle::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultUnsupported;
}

rocprofvis_result_t Handle::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) {
    (void) property;
    (void) index;
    (void) value;
    (void) length; 
    return kRocProfVisResultUnsupported;
}

rocprofvis_result_t Handle::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultUnsupported;
}

rocprofvis_result_t Handle::SetDouble(rocprofvis_property_t property, uint64_t index, double value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultUnsupported;
}

rocprofvis_result_t Handle::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultUnsupported;
}

rocprofvis_result_t Handle::SetString(rocprofvis_property_t property, uint64_t index, char const* value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultUnsupported;
}

}
}
