// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Track;
class Callstack;
class FlowControl;

class Event : public Handle
{
public:
    Event(uint64_t id, double start_ts, double end_ts);

    virtual ~Event();

    rocprofvis_controller_object_type_t GetType(void) override;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) override;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) override;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) override;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) override;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) override;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) override;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) override;

private:
    Track* m_track;
    uint64_t m_id;
    double m_start_timestamp;
    double m_end_timestamp;
    std::string m_name;
    std::vector<Callstack*> m_callstack;
    std::vector<FlowControl*> m_input_flow_control;
    std::vector<FlowControl*> m_output_flow_control;
};

}
}
