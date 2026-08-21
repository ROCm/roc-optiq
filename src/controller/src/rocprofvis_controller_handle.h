// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include <string_view>

namespace RocProfVis
{
namespace Controller
{

// All controller handles inherit from this and implement this API
class Handle
{
public:
    Handle(uint32_t first_prop_index, uint32_t last_prop_index);
    virtual ~Handle() {}

    virtual rocprofvis_controller_object_type_t GetType(void) = 0;

    // Generic property accessors — these form the public C API surface and must stay public
    // on Handle so the API dispatch (rocprofvis_controller.cpp) can reach them via the base
    // pointer. Overrides in derived classes should be declared PRIVATE: they are implementation
    // details of the API bridge, not intended for use by internal C++ code. To access object
    // state from within the codebase, add a dedicated typed getter/setter to the derived class.
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
    rocprofvis_result_t GetStdStringImpl(char* value, uint32_t* length, std::string_view data);
    rocprofvis_result_t UnhandledProperty(rocprofvis_property_t property);

    uint32_t m_first_prop_index;
    uint32_t m_last_prop_index;
};

}
}
