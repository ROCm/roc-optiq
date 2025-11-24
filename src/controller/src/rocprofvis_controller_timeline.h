// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

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
class Graph;
class Track;

class Timeline : public Handle
{
public:
    Timeline(uint64_t id);

    ~Timeline();

    rocprofvis_result_t AsyncFetch(Track& track, Future& future, Array& array,
                                   double start, double end);

    rocprofvis_result_t AsyncFetch(Graph& track, Future& future, Array& array,
                                   double start, double end, uint32_t pixels);

    rocprofvis_controller_object_type_t GetType(void) final;

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) final;

private:
    uint64_t            m_id;
    std::vector<Graph*> m_graphs;
    double m_min_ts;
    double m_max_ts;
};

}
}
