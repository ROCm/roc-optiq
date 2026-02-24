// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface_types.h"

namespace RocProfVis
{
namespace Controller
{

class Dispatch : public Handle
{
public:
    Dispatch();

    virtual ~Dispatch();

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;

    bool QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_property_t& property, rocprofvis_controller_primitive_type_t& type) const;

private:
    uint32_t m_dispatch_uuid;
    uint32_t m_kernel_uuid;
    uint32_t m_dispatch_id;
    uint32_t m_gpu_id;
    uint64_t m_start_timestamp;
    uint64_t m_end_timestamp;
};

}
}
