// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

class Arguments;
class Array;
class Future;
class ComputeTable;

class ComputeTrace : public Handle
{
public:
    ComputeTrace();

    virtual ~ComputeTrace();

    rocprofvis_result_t Load(char const* const directory);

    rocprofvis_controller_object_type_t GetType(void) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final;

private:
    std::unordered_map<rocprofvis_controller_compute_table_types_t, ComputeTable*> m_tables;

};

}
}
