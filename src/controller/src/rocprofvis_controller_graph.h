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

class Graph : public Handle
{
    rocprofvis_result_t GenerateLOD(uint32_t lod_to_generate, double start_ts, double end_ts, std::vector<Data>& entries);
    rocprofvis_result_t GenerateLOD(uint32_t lod_to_generate, double start, double end);
    void Insert(uint32_t lod, double timestamp, uint8_t level, Handle* object);

public:
    Graph(Handle* ctx, rocprofvis_controller_graph_type_t type, uint64_t id);

    Handle* GetContext() override;

    virtual ~Graph();

    rocprofvis_result_t Fetch(uint32_t pixels, double start, double end, Array& array, uint64_t& index);

    rocprofvis_controller_object_type_t GetType(void) final;
    rocprofvis_result_t                 CombineEventNames(std::vector<Event*>& events, std::string& combined_name);

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
    std::map<uint32_t, SegmentTimeline> m_lods;
    uint64_t m_id;
    Track* m_track;
    Trace* m_ctx;
    rocprofvis_controller_graph_type_t m_type;
    std::mutex  m_mutex;
};

}
}
