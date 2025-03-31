// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Array;
class Future;
class Track;
class Graph;
class Timeline;

class Trace : public Handle
{
public:
    Trace();

    virtual ~Trace();

    rocprofvis_result_t Load(char const* const filename, Future& future);

    rocprofvis_result_t AsyncFetch(Track& track, Future& future, Array& array,
                                   double start, double end);

    rocprofvis_result_t AsyncFetch(Graph& graph, Future& future, Array& array,
                                   double start, double end, uint32_t pixels);

    rocprofvis_controller_object_type_t GetType(void) final;

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
    std::vector<Track*> m_tracks;
    uint64_t m_id;
    Timeline* m_timeline;
};

}
}
