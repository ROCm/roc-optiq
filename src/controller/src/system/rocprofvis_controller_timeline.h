// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include <optional>
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

    [[nodiscard]] rocprofvis_result_t AsyncFetch(Track& track, Future& future, Array& array,
                                                 double start, double end);

    [[nodiscard]] rocprofvis_result_t AsyncFetch(Graph& track, Future& future, Array& array,
                                                 double start, double end, uint32_t pixels);

    rocprofvis_controller_object_type_t GetType(void) final
    {
        return kRPVControllerObjectTypeTimeline;
    }
    uint64_t GetId() const noexcept { return m_id; }
    uint64_t GetNumberOfGraphs() const noexcept { return m_graphs.size(); }
    uint64_t GetExclusiveMemoryUsage() const noexcept
    {
        return sizeof(Timeline) + m_graphs.size() * sizeof(Graph*);
    }
    [[nodiscard]] rocprofvis_result_t GetInclusiveMemoryUsage(uint64_t* value) const;
    [[nodiscard]] rocprofvis_result_t GetMinTimestamp(double* value) const;
    [[nodiscard]] rocprofvis_result_t GetMaxTimestamp(double* value) const;
    [[nodiscard]] rocprofvis_result_t GetGraph(uint64_t index, Graph** value) const;
    [[nodiscard]] rocprofvis_result_t GetGraphById(uint64_t id, Graph** value) const;
    [[nodiscard]] rocprofvis_result_t SetNumberOfGraphs(uint64_t count);
    // Ownership transfers on success; replacement deletes the previous graph.
    // An existing graph is moved to the requested final index.
    [[nodiscard]] rocprofvis_result_t SetGraph(uint64_t index, Graph* graph);

private:
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

    void RecomputeBounds();

private:
    uint64_t                m_id;
    std::vector<Graph*>     m_graphs;
    std::optional<double>   m_min_ts;
    std::optional<double>   m_max_ts;
};

}
}
