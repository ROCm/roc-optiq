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
    virtual rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value);
    virtual rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value);
    virtual rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value);
    virtual rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length);

    virtual rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value);
    virtual rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value);
    virtual rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value);
    virtual rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value);

    virtual Handle* GetContext() { return nullptr; }
    virtual bool    IsDeletable() { return true; }
    virtual void    IncreaseRetainCounter() {};

protected:
    rocprofvis_result_t GetStringImpl(char* value, uint32_t* length, char const* data, uint32_t data_len);
    rocprofvis_result_t UnhandledProperty(rocprofvis_property_t property);

    uint32_t m_first_prop_index;
    uint32_t m_last_prop_index;
};

}
}

#define GetStdStringImpl(value, length, data) GetStringImpl(value, length, data.c_str(), static_cast<uint32_t>(data.length()))
