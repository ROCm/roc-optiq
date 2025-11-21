// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include <cstddef>
#include <string>
#include <vector>
#include "rocprofvis_c_interface.h"

namespace RocProfVis
{
namespace Controller
{

class Array;
class Track;
class Callstack;
class FlowControl;

class Event : public Handle
{
public:
    Event(uint64_t id, double start_ts, double end_ts);

    Event& operator=(Event&& other);

    virtual ~Event();

    rocprofvis_controller_object_type_t GetType(void) override;

    rocprofvis_result_t Fetch(rocprofvis_property_t property,  Array& array, rocprofvis_dm_trace_t dm_handle);

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) override;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) override;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) override;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) override;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) override;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value) override;
    bool                IsDeletable() override;
    void                IncreaseRetainCounter()  override;

private:
    Array* m_children;
    uint64_t m_id;
    double m_start_timestamp;
    double m_end_timestamp;
    size_t m_name;
    size_t m_category;
    size_t m_combined_top_name;
    uint8_t m_level;
    uint8_t m_retain_counter;

public:
    static rocprofvis_result_t FetchDataModelFlowTraceProperty(uint64_t event_id, Array& array, rocprofvis_dm_trace_t dm_trace_handle);
    static rocprofvis_result_t FetchDataModelStackTraceProperty(uint64_t event_id, Array& array, rocprofvis_dm_trace_t dm_trace_handle);
    static rocprofvis_result_t FetchDataModelExtendedDataProperty(uint64_t event_id, Array& array, rocprofvis_dm_trace_t dm_trace_handle);
};

}
}
