// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"

namespace RocProfVis
{
namespace Controller
{

class Data
{
    void Reset();

public:
    Data();

    Data(Data const& other);

    Data(Data&& other);

    Data& operator=(Data const& other);

    Data& operator=(Data&& other);

    Data(rocprofvis_controller_primitive_type_t type);

    Data(double val);

    Data(uint64_t val);

    Data(rocprofvis_handle_t* object);

    Data(char const* const string);

    ~Data();

    rocprofvis_controller_primitive_type_t GetType(void) const;

    void SetType(rocprofvis_controller_primitive_type_t type);

    rocprofvis_result_t GetObject(rocprofvis_handle_t** object);

    rocprofvis_result_t SetObject(rocprofvis_handle_t* object);

    rocprofvis_result_t GetString(char* string, uint32_t* length);

    rocprofvis_result_t SetString(char const* string);

    rocprofvis_result_t GetUInt64(uint64_t* value);

    rocprofvis_result_t SetUInt64(uint64_t value);

    rocprofvis_result_t GetDouble(double* value);

    rocprofvis_result_t SetDouble(double value);

private:
    rocprofvis_controller_primitive_type_t m_type;
    union
    {
        rocprofvis_handle_t* m_object;
        char* m_string;
        uint64_t m_uint64;
        double m_double;
    };
};

}
}
