// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_c_interface.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_data.h"

namespace RocProfVis
{
namespace Controller
{


class FlowControl : public Handle
{
public:
    FlowControl(uint64_t id, uint64_t start_timestamp, uint64_t end_timestamp, uint32_t track_id, uint32_t level, uint32_t direction, const char* category, const char* symbol);

    virtual ~FlowControl();

    rocprofvis_controller_object_type_t GetType(void) override;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) override;

private:
    uint64_t m_id;
    uint64_t m_start_timestamp;
    uint64_t m_end_timestamp;
    uint32_t m_track_id;
    uint32_t m_level;
    uint32_t m_direction;
    Data     m_symbol;
    Data     m_category;
};

}
}
