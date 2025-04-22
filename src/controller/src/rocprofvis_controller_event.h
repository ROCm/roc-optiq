// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
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

    virtual ~Event();

    rocprofvis_controller_object_type_t GetType(void) override;

    rocprofvis_result_t Fetch(rocprofvis_property_t property,  Array& array, rocprofvis_dm_trace_t dm_handle);

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
    size_t m_name;
    size_t m_category;

private:
    rocprofvis_result_t FetchDataModelFlowTraceProperty(Array& array, rocprofvis_dm_trace_t dm_trace_handle);
    rocprofvis_result_t FetchDataModelStackTraceProperty(Array& array, rocprofvis_dm_trace_t dm_trace_handle);
    rocprofvis_result_t FetchDataModelExtendedDataProperty(Array& array, rocprofvis_dm_trace_t dm_trace_handle);
};

}
}
