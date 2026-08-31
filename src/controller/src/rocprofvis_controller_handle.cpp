// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_handle.h"
#include <algorithm>
#include <cstring>

namespace RocProfVis
{
namespace Controller
{

Handle::Handle(uint32_t first_prop_index, uint32_t last_prop_index)
: m_first_prop_index(first_prop_index)
, m_last_prop_index(last_prop_index)
{}

rocprofvis_result_t Handle::GetStdStringImpl(char* value, uint32_t* length, std::string_view data)
{
    if (!length)
        return kRocProfVisResultInvalidArgument;

    const auto data_len = static_cast<uint32_t>(data.size());
    if (!value || *length == 0)
    {
        *length = data_len;
        return kRocProfVisResultSuccess;
    }

    const size_t copy = std::min(static_cast<size_t>(data_len), static_cast<size_t>(*length));
    if (copy > 0) std::memcpy(value, data.data(), copy);
    return kRocProfVisResultSuccess;
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
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t Handle::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t Handle::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t Handle::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) {
    (void) property;
    (void) index;
    (void) value;
    (void) length; 
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t Handle::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t Handle::SetDouble(rocprofvis_property_t property, uint64_t index, double value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t Handle::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t Handle::SetString(rocprofvis_property_t property, uint64_t index, char const* value) {
    (void) property;
    (void) index;
    (void) value;    
    return kRocProfVisResultNotSupported;
}

}
}
