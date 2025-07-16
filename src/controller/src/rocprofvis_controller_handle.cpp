// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_handle.h"
#include <cstring>

namespace RocProfVis
{
namespace Controller
{

rocprofvis_result_t Handle::GetStringImpl(char* value, uint32_t* length, char const* data, uint32_t data_len)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && length && *length)
    {
        strncpy(value, data, *length);
        result = kRocProfVisResultSuccess;
    }
    else if(length)
    {
        *length = data_len;
        result  = kRocProfVisResultSuccess;
    }
    return result;
}

}
}
