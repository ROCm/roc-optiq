// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_segment.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <map>
#include <string>
#include <memory>

namespace RocProfVis
{
namespace Controller
{

class Array;
class Thread;
class Queue;
class Stream;
class Trace;
class Counter;
class Future;

class Track : public Handle
{
public:
    Track(rocprofvis_controller_track_type_t type, uint64_t id, rocprofvis_dm_track_t dm_handle, Trace* ctx);

    virtual ~Track();

    rocprofvis_result_t FetchSegments(double start, double end, void* user_ptr, Future* future, FetchSegmentsFunc func);
    rocprofvis_result_t Fetch(double start, double end, Array& array, uint64_t& index, Future* future);

    rocprofvis_controller_object_type_t GetType(void) final;
    rocprofvis_dm_track_t GetDmHandle(void);
    Handle* GetContext(void) override;
    SegmentTimeline* GetSegments();

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
    uint64_t m_id;
    uint64_t m_node;
    uint64_t m_num_entries;
    rocprofvis_controller_track_type_t m_type;
    SegmentTimeline m_segments;
    double m_start_timestamp;
    double m_end_timestamp;
    double m_min_value;
    double m_max_value;
     std::string m_name;
    rocprofvis_dm_track_t m_dm_handle;
    Thread* m_thread;
    Queue* m_queue;
    Stream* m_stream;
    Counter* m_counter;
    Trace* m_ctx;

    std::condition_variable_any  m_cv;


private:
    rocprofvis_result_t FetchFromDataModel(double start, double end, Future* future);

    inline static std::atomic<uint32_t> s_data_model_load;
};

}
}
