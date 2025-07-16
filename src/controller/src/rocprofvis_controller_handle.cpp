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

}
}
