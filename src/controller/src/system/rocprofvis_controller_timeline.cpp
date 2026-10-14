// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_timeline.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_graph.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_id.h"
#include "rocprofvis_core_assert.h"

#include <algorithm>
#include <cfloat>
#include <exception>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_graph_t, Graph, kRPVControllerObjectTypeGraph> GraphRef;

Timeline::Timeline(uint64_t id)
: Handle(__kRPVControllerTimelinePropertiesFirst, __kRPVControllerTimelinePropertiesLast)
, m_id(id)
, m_min_ts(DBL_MAX)
, m_max_ts(DBL_MIN)
{}

Timeline::~Timeline()
{
    for(Graph* graph : m_graphs)
    {
        delete graph;
    }
}

rocprofvis_result_t Timeline::AsyncFetch(Graph& graph, Future& future, Array& array,
                                double start, double end, uint32_t pixels)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;

    future.Set(JobSystem::Get().IssueJob([&graph, &array, start, end, pixels](Future* future) -> rocprofvis_result_t {
            rocprofvis_result_t result = kRocProfVisResultUnknownError;
            uint64_t            index  = 0;
            result                     = graph.Fetch(pixels, start, end, array, index, future);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess ||
                              result == kRocProfVisResultOutOfRange ||
                              result == kRocProfVisResultCancelled);
            return result;
        }, &future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }
    ROCPROFVIS_ASSERT(error == kRocProfVisResultSuccess);

    return error;
}

rocprofvis_result_t Timeline::AsyncFetch(Track& track, Future& future, Array& array,
                                double start, double end)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;

    future.Set(JobSystem::Get().IssueJob([&track, &array, start, end](Future* future) -> rocprofvis_result_t {
            rocprofvis_result_t result = kRocProfVisResultUnknownError;
            uint64_t            index  = 0;
            result = track.Fetch(start, end, array, index, future);
            return result;
        }, &future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t
Timeline::GetInclusiveMemoryUsage(uint64_t* value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        *value = GetExclusiveMemoryUsage();
        result = kRocProfVisResultSuccess;
        for(const Graph* graph : m_graphs)
        {
            if(graph)
            {
                uint64_t graph_size = 0;
                result              = graph->GetInclusiveMemoryUsage(&graph_size);
                if(result != kRocProfVisResultSuccess)
                {
                    break;
                }
                *value += graph_size;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::GetMinTimestamp(double* value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        result = kRocProfVisResultOutOfRange;
        if(!m_graphs.empty())
        {
            *value = m_min_ts;
            result = kRocProfVisResultSuccess;
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::GetMaxTimestamp(double* value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        result = kRocProfVisResultOutOfRange;
        if(!m_graphs.empty())
        {
            *value = m_max_ts;
            result = kRocProfVisResultSuccess;
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::GetGraph(uint64_t index, Graph** value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        result = kRocProfVisResultOutOfRange;
        if(index < m_graphs.size())
        {
            *value = m_graphs[index];
            result = kRocProfVisResultSuccess;
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::GetGraphById(uint64_t id, Graph** value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        result = kRocProfVisResultOutOfRange;
        for(Graph* graph : m_graphs)
        {
            if(graph && graph->GetId() == id)
            {
                *value = graph;
                result = kRocProfVisResultSuccess;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::SetNumberOfGraphs(uint64_t count)
{
    rocprofvis_result_t result = kRocProfVisResultMemoryAllocError;
    try
    {
        for(uint64_t index = count; index < m_graphs.size(); index++)
        {
            delete m_graphs[index];
            m_graphs[index] = nullptr;
        }
        m_graphs.resize(count);
        result = kRocProfVisResultSuccess;
    } catch(std::exception const&)
    {
        result = kRocProfVisResultMemoryAllocError;
    }
    return result;
}

rocprofvis_result_t
Timeline::SetGraph(uint64_t index, Graph* graph)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(graph && index >= m_graphs.size())
    {
        result = kRocProfVisResultOutOfRange;
    }
    else if(graph)
    {
        double min_ts = 0;
        double max_ts = 0;
        if(graph->GetStartTimestamp(&min_ts) == kRocProfVisResultSuccess &&
           graph->GetEndTimestamp(&max_ts) == kRocProfVisResultSuccess)
        {
            bool is_replace = false;
            for(auto it = m_graphs.begin(); it != m_graphs.end(); ++it)
            {
                if(*it == graph)
                {
                    m_graphs.erase(it);
                    is_replace = true;
                    break;
                }
            }
            m_min_ts = std::min(min_ts, m_min_ts);
            m_max_ts = std::max(max_ts, m_max_ts);
            if(is_replace)
            {
                m_graphs.insert(m_graphs.begin() + index, graph);
            }
            else
            {
                delete m_graphs[index];
                m_graphs[index] = graph;
            }
            result = kRocProfVisResultSuccess;
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                result = GetInclusiveMemoryUsage(value);
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = GetExclusiveMemoryUsage();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTimelineId:
            {
                *value = GetId();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTimelineNumGraphs:
            {
                *value = GetNumberOfGraphs();
                result = kRocProfVisResultSuccess;
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTimelineMinTimestamp:
            {
                result = GetMinTimestamp(value);
                break;
            }
            case kRPVControllerTimelineMaxTimestamp:
            {
                result = GetMaxTimestamp(value);
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::GetObject(rocprofvis_property_t property, uint64_t index,
                    rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        Graph* graph = nullptr;
        switch(property)
        {
            case kRPVControllerTimelineGraphById:
            {
                result = GetGraphById(index, &graph);
                break;
            }
            case kRPVControllerTimelineGraphIndexed:
            {
                result = GetGraph(index, &graph);
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
        if(result == kRocProfVisResultSuccess)
        {
            *value = reinterpret_cast<rocprofvis_handle_t*>(graph);
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTimelineId:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerTimelineNumGraphs:
        {
            result = SetNumberOfGraphs(value);
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t
Timeline::SetObject(rocprofvis_property_t property, uint64_t index,
                    rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTimelineGraphIndexed:
        {
            GraphRef graph(value);
            result = graph.IsValid() ? SetGraph(index, graph.Get())
                                     : kRocProfVisResultInvalidArgument;
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

}  // namespace Controller
}  // namespace RocProfVis
