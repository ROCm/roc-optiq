// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) override;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) override;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) override;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) override;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) override;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value, uint32_t length) override;

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
