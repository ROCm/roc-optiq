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

#include <cfloat>

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

rocprofvis_controller_object_type_t Timeline::GetType(void) 
{
    return kRPVControllerObjectTypeTimeline;
}

rocprofvis_result_t Timeline::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                *value = sizeof(Timeline);
                *value += m_graphs.size() * sizeof(Graph*);
                result = kRocProfVisResultSuccess;
                for (auto& graph : m_graphs)
                {
                    uint64_t entry_size = 0;
                    result = graph->GetUInt64(property, 0, &entry_size);
                    if (result == kRocProfVisResultSuccess)
                    {
                        *value += entry_size;
                    }
                    else
                    {
                        break;
                    }
                }
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Timeline);
                *value += m_graphs.size() * sizeof(Graph*);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTimelineId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTimelineNumGraphs:
            {
                *value = m_graphs.size();
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
rocprofvis_result_t Timeline::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTimelineMinTimestamp:
            {
                if(m_graphs.size())
                {
                    *value = m_min_ts;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerTimelineMaxTimestamp:
            {
                if(m_graphs.size())
                {
                    *value = m_max_ts;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
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
rocprofvis_result_t Timeline::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTimelineGraphById:
            {
                result = kRocProfVisResultOutOfRange;
                for (auto* graph : m_graphs)
                {
                    uint64_t graph_id = 0;
                    if (graph->GetUInt64(kRPVControllerGraphId, 0, &graph_id) == kRocProfVisResultSuccess && graph_id == index)
                    {
                        *value = (rocprofvis_handle_t*)graph;
                        result = kRocProfVisResultSuccess;
                        break;
                    }
                }
                break;
            }
            case kRPVControllerTimelineGraphIndexed:
            {
                if(index < m_graphs.size())
                {
                    *value = (rocprofvis_handle_t*) m_graphs[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
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

rocprofvis_result_t Timeline::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
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
            if(m_graphs.size() != value)
            {
                for (uint64_t i = value; i < m_graphs.size(); i++)
                {
                    delete m_graphs[i];
                    m_graphs[i] = nullptr;
                }
                m_graphs.resize(value);
                result = (m_graphs.size() == value) ? kRocProfVisResultSuccess : kRocProfVisResultMemoryAllocError;
            }
            else
            {
                result = kRocProfVisResultSuccess;
            }
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

rocprofvis_result_t Timeline::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTimelineGraphIndexed:
        {
            GraphRef graph(value);
            if(graph.IsValid())
            {
                if(index < m_graphs.size())
                {
                    bool is_replace = false;
                    for (uint32_t i = 0; i < m_graphs.size(); i++)
                    {
                        if (m_graphs[i] == graph.Get())
                        {
                            m_graphs.erase(m_graphs.begin() + i);
                            is_replace = true;
                            break;
                        }
                    }

                    double min_ts = 0;
                    double max_ts = 0;
                    if((graph.Get()->GetDouble(kRPVControllerGraphStartTimestamp, 0,
                                               &min_ts) == kRocProfVisResultSuccess) &&
                       (graph.Get()->GetDouble(kRPVControllerGraphEndTimestamp, 0,
                                               &max_ts) == kRocProfVisResultSuccess))
                    {
                        m_min_ts = std::min(min_ts, m_min_ts);
                        m_max_ts = std::max(max_ts, m_max_ts);
                        if(is_replace)
                        {
                            m_graphs.insert(m_graphs.begin() + index, graph.Get());
                        }
                        else
                        {
                            m_graphs[index] = graph.Get();
                        }
                        result          = kRocProfVisResultSuccess;
                    }
                }
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
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

}
}
