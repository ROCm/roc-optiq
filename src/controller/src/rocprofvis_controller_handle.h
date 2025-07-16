// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include <cstring>

namespace RocProfVis
{
namespace Controller
{

// All controller handles inherit from this and implement this API
class Handle
{
public:
    Handle() {}
    virtual ~Handle() {}

    virtual rocprofvis_controller_object_type_t GetType(void) = 0;

    // Handlers for getters.
    virtual rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) = 0;
    virtual rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) = 0;
    virtual rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) = 0;
    virtual rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) = 0;

    virtual rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) = 0;
    virtual rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) = 0;
    virtual rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) = 0;
    virtual rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) = 0;

    virtual Handle* GetContext() { return nullptr; }

protected:
    rocprofvis_result_t GetStringImpl(char* value, uint32_t* length, char const* data, uint32_t data_len);
};

}
}

#define GetStdStringImpl(value, length, data) GetStringImpl(value, length, data.c_str(), data.length())
