// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include <string>

namespace RocProfVis
{
namespace Controller
{

class ExtData : public Handle
{
public:
    ExtData(const char* category, const char* name, const char* value,
            rocprofvis_db_data_type_t type, uint64_t category_enum);

    virtual ~ExtData();

    rocprofvis_controller_object_type_t GetType(void) override;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) override;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) override;

private:
    Data m_category;
    Data m_name;
    Data m_value;
    uint64_t m_enum;
};

class ArgumentData : public ExtData
{
public:
    ArgumentData(const char* category, const char* name, const char* value,
        rocprofvis_db_data_type_t type, uint64_t category_enum, size_t position, const char* arg_type);

    virtual ~ArgumentData();

    rocprofvis_controller_object_type_t GetType(void) override;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
        uint64_t* value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
        char* value, uint32_t* length) override;
private:
    Data m_position;
    Data m_arg_type;
};

}
}
