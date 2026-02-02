// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include <string>

namespace RocProfVis
{
namespace Controller
{

class Kernel : public Handle
{
public:
    Kernel();

    virtual ~Kernel();

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value) final;

private:
    uint32_t m_id;
    std::string m_name;
    uint32_t m_invocation_count;
    uint64_t m_duration_total;
    uint32_t m_duration_min;
    uint32_t m_duration_max;
    uint32_t m_duration_median;
    uint32_t m_duration_mean;
};

}
}
