// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_segment.h"

namespace RocProfVis
{
namespace Controller
{

class Array;
class Track;
class Trace;
class Future;

class Graph : public Handle
{
    rocprofvis_result_t GenerateLOD(uint32_t lod_to_generate, double start_ts, double end_ts, std::vector<Data>& entries, Future* future);
    rocprofvis_result_t GenerateLOD(uint32_t lod_to_generate, double start, double end, Future* future);
    void Insert(uint32_t lod, double timestamp, uint8_t level, Handle* object);

public:
    Graph(Handle* ctx, rocprofvis_controller_graph_type_t type, uint64_t id);

    Handle* GetContext() override;

    virtual ~Graph();

    rocprofvis_result_t Fetch(uint32_t pixels, double start, double end, Array& array, uint64_t& index, Future* future);

    rocprofvis_controller_object_type_t GetType(void) final;
    rocprofvis_result_t                 CombineEventInfo(std::vector<Event*>& events, std::string& combined_name, size_t &max_duration_str_index);
    rocprofvis_result_t                 GenerateLODEvent(std::vector<Event*>& events,
                                                         uint32_t lod_to_generate, uint32_t level,
                                                         double event_min, double event_max);

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;

private:
    std::map<uint32_t, SegmentTimeline> m_lods;
    uint64_t m_id;
    Track* m_track;
    Trace* m_ctx;
    rocprofvis_controller_graph_type_t m_type;
    std::condition_variable_any  m_cv;
};

}
}
