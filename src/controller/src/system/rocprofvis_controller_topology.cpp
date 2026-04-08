// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_topology.h"
#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_trace_system.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_topology_node_t, TopologyNode,
                  kRPVControllerObjectTypeTopologyNode> TopologyNodeRef;


TopologyNode::TopologyNode(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx, TopologyNode* parent)
: Handle(__kRPVControllerNodePropertiesFirst, __kRPVControllerNodePropertiesLast)
, m_dm_topology_node(dm_topology_node)
, m_ctx(ctx)
, m_parent(parent)
, m_track(nullptr)
{
    m_name = rocprofvis_dm_get_property_as_charptr(m_dm_topology_node, kRPVControllerTopologyNodeName, 0);
    m_node_type = (rocprofvis_controller_topology_node_type_t)rocprofvis_dm_get_property_as_uint64(m_dm_topology_node, kRPVControllerTopologyNodeType, 0);
    uint64_t track_id;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_uint64(m_dm_topology_node, kRPVControllerTopologyNodeTrack, 0, &track_id))
    {
        if (kRocProfVisResultSuccess != m_ctx->GetObject(kRPVControllerSystemTrackIndexed, track_id, (rocprofvis_handle_t**)&m_track))
        {
            m_track = nullptr;
        }
    }

    uint64_t num_children =
        rocprofvis_dm_get_property_as_uint64(
            dm_topology_node, kRPVControllerTopologyNodeNumChildren, 0);

    for (int i = 0; i < num_children; i++)
    {
        rocprofvis_dm_topology_node dm_child_node =
            rocprofvis_dm_get_property_as_handle(
                dm_topology_node, kRPVControllerTopologyNodeChildHandleIndexed, i);
        if (dm_child_node)
        {
            rocprofvis_controller_topology_node_type_t child_node_type = (rocprofvis_controller_topology_node_type_t)rocprofvis_dm_get_property_as_uint64(dm_child_node, kRPVControllerTopologyNodeType, 0);
            TopologyNode* child_node = nullptr;
            Track* child_track;
            switch (child_node_type)
            {
            case kRPVControllerTopologyNodeSytemNode:
                child_node = new Node(dm_child_node, ctx, this);
                break;
            case kRPVControllerTopologyNodeProcess: 
                child_node = new Process(dm_child_node, ctx, this);
                break;
            case kRPVControllerTopologyNodeProcessor: 
                child_node = new Processor(dm_child_node, ctx, this);
                break;
            case kRPVControllerTopologyNodeThread:
                child_node = new Thread(dm_child_node, ctx, this);
                child_node->GetObject(kRPVControllerThreadTrack, 0, (rocprofvis_handle_t**)& child_track);
                if (child_track)
                {
                    child_track->SetObject(kRPVControllerTrackThread, 0, (rocprofvis_handle_t*)child_node);
                }
                break;
            case kRPVControllerTopologyNodeQueue: 
                child_node = new Queue(dm_child_node, ctx, this);
                child_node->GetObject(kRPVControllerQueueTrack, 0, (rocprofvis_handle_t**)& child_track);
                if (child_track)
                {
                    child_track->SetObject(kRPVControllerTrackQueue, 0, (rocprofvis_handle_t*)child_node);
                }
                break;
            case kRPVControllerTopologyNodeStream: 
                child_node = new Stream(dm_child_node, ctx, this);
                child_node->GetObject(kRPVControllerStreamTrack, 0, (rocprofvis_handle_t**)& child_track);
                if (child_track)
                {
                    child_track->SetObject(kRPVControllerTrackStream, 0, (rocprofvis_handle_t*)child_node);
                }
                break;
            case kRPVControllerTopologyNodeCounter: 
                child_node = new Counter(dm_child_node, ctx, this);
                child_node->GetObject(kRPVControllerCounterTrack, 0, (rocprofvis_handle_t**)& child_track);
                if (child_track)
                {
                    child_track->SetObject(kRPVControllerTrackCounter, 0, (rocprofvis_handle_t*)child_node);
                }
                break;
            }   
            if (child_node)
            {
                m_children[child_node_type].push_back(child_node);
            }
        }
    }   
}

TopologyNode::~TopologyNode()
{
    for (auto [type, children] : m_children)
    {
        for (auto* node : children)
        {
            delete node;
        }
    }
}

TopologyNode* TopologyNode::GetParent() {
    return m_parent;
}

TopologyNode* TopologyNode::GetParent(rocprofvis_controller_object_type_t type) {
    if (m_parent && m_parent->GetType() == type)
    {
        return m_parent;
    }
    else
    {
        return m_parent->GetParent(type);
    }
}

rocprofvis_result_t
TopologyNode::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch (property)
        {
        case kRPVControllerSystemNumNodes:
            *value = m_children[kRPVControllerTopologyNodeSytemNode].size();
            result = kRocProfVisResultSuccess;
            break;
        case kRPVControllerNodeNumProcessors:
            *value = m_children[kRPVControllerTopologyNodeProcessor].size();
            result = kRocProfVisResultSuccess;
            break;
        case kRPVControllerNodeNumProcesses:
            *value = m_children[kRPVControllerTopologyNodeProcess].size();
            result = kRocProfVisResultSuccess;
            break;
        case kRPVControllerProcessNumThreads:
            *value = m_children[kRPVControllerTopologyNodeThread].size();
            result = kRocProfVisResultSuccess;
            break;
        case kRPVControllerProcessNumStreams:
            *value = m_children[kRPVControllerTopologyNodeStream].size();
            result = kRocProfVisResultSuccess;
            break;
        case kRPVControllerProcessorNumQueues:
            *value = m_children[kRPVControllerTopologyNodeQueue].size();
            result = kRocProfVisResultSuccess;
            break;
        case kRPVControllerProcessorNumCounters:
            *value = m_children[kRPVControllerTopologyNodeCounter].size();
            result = kRocProfVisResultSuccess;
            break;
        case kRPVControllerStreamNumProcessors:
            *value = m_children[kRPVControllerTopologyNodeProcess].size();
            result = kRocProfVisResultSuccess;
            break;
        case kRPVControllerProcessNumQueues:
        case kRPVControllerProcessNumCounters:
            *value = 0;
            result = kRocProfVisResultSuccess;
            break;
        case kRPVControllerProcessorType:
        {
            char* prop_string;
            if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_charptr(m_dm_topology_node, kRPVControllerTopologyNodePropertyValueKeyed, property, &prop_string))
            {
                if (strcmp(prop_string, "GPU") == 0)
                {
                    *value = kRPVControllerProcessorTypeGPU;
                    result = kRocProfVisResultSuccess;
                }
                else if (strcmp(prop_string, "CPU") == 0)
                {
                    *value = kRPVControllerProcessorTypeCPU;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultUnknownError;
                }
            }
            else
            {
                result = kRocProfVisResultUnknownError;
            }
            break;
        }
        default:
        {
            rocprofvis_dm_result_t dm_result = rocprofvis_dm_get_property_as_uint64(m_dm_topology_node, kRPVControllerTopologyNodePropertyValueKeyed, property, value);
            if (kRocProfVisDmResultSuccess == dm_result)
            {
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultUnknownError;
            }
            break;
        }
        }
    }
    return result;
}

rocprofvis_result_t
TopologyNode::GetObject(rocprofvis_property_t property, uint64_t index,
                rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch (property)
        {
        case kRPVControllerSystemNodeIndexed:
            *value = (rocprofvis_handle_t*)m_children[kRPVControllerTopologyNodeSytemNode][index];
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerNodeProcessorIndexed:
            *value = (rocprofvis_handle_t*)m_children[kRPVControllerTopologyNodeProcessor][index];
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerNodeProcessIndexed:
            *value = (rocprofvis_handle_t*)m_children[kRPVControllerTopologyNodeProcess][index];
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerProcessThreadIndexed:
            *value = (rocprofvis_handle_t*)m_children[kRPVControllerTopologyNodeThread][index];
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerProcessStreamIndexed:
            *value = (rocprofvis_handle_t*)m_children[kRPVControllerTopologyNodeStream][index];
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerProcessorQueueIndexed:
            *value = (rocprofvis_handle_t*)m_children[kRPVControllerTopologyNodeQueue][index];
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerProcessorCounterIndexed:
            *value = (rocprofvis_handle_t*)m_children[kRPVControllerTopologyNodeCounter][index];
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerStreamProcessorIndexed:
            *value = (rocprofvis_handle_t*)m_children[kRPVControllerTopologyNodeProcessor][index];
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerStreamProcessor:
            *value = (rocprofvis_handle_t*)m_children[kRPVControllerTopologyNodeProcessor][0];
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerThreadTrack:
        case kRPVControllerQueueTrack:
        case kRPVControllerStreamTrack:
        case kRPVControllerCounterTrack:
            *value = (rocprofvis_handle_t*)m_track;
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerQueueNode:
        case kRPVControllerThreadNode:
        case kRPVControllerStreamNode:
        case kRPVControllerCounterNode:
            *value = (rocprofvis_handle_t*)GetParent(kRPVControllerObjectTypeNode);
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerThreadProcess:
        case kRPVControllerStreamProcess:
            *value = (rocprofvis_handle_t*)GetParent(kRPVControllerObjectTypeProcess);
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerQueueProcessor:
        case kRPVControllerCounterProcessor:
            *value = (rocprofvis_handle_t*)GetParent(kRPVControllerObjectTypeProcessor);
            result = *value ? kRocProfVisResultSuccess : kRocProfVisResultNotLoaded;
            break;
        case kRPVControllerQueueProcess: 
        {
            result = kRocProfVisResultNotLoaded;
            *value = nullptr;
            uint64_t queue_pid;
            if (kRocProfVisResultSuccess == GetUInt64(kRPVControllerQueueProcess, 0, &queue_pid))
            {
                TopologyNode* system_node = GetParent(kRPVControllerObjectTypeNode);
                for (auto process : system_node->m_children[kRPVControllerTopologyNodeProcess])
                {
                    uint64_t process_id;
                    if (kRocProfVisResultSuccess == process->GetUInt64(kRPVControllerProcessId, 0, &process_id))
                    {
                        if (process_id == queue_pid)
                        {
                            *value = (rocprofvis_handle_t*)process;
                            result = kRocProfVisResultSuccess;
                            break;
                        }
                    }
                }
            }
        }
        break;
        case kRPVControllerCounterProcess:
        {
            result = kRocProfVisResultNotLoaded;
            *value = nullptr;
            uint64_t counter_pid;
            if (kRocProfVisResultSuccess == GetUInt64(kRPVControllerCounterProcess, 0, &counter_pid))
            {
                TopologyNode* system_node = GetParent(kRPVControllerObjectTypeNode);
                for (auto process : system_node->m_children[kRPVControllerTopologyNodeProcess])
                {
                    uint64_t process_id;
                    if (kRocProfVisResultSuccess == process->GetUInt64(kRPVControllerProcessId, 0, &process_id))
                    {
                        if (process_id == counter_pid)
                        {
                            *value = (rocprofvis_handle_t*)process;
                            result = kRocProfVisResultSuccess;
                            break;
                        }
                    }
                }
            }
        }
        break;
        default:
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t
TopologyNode::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                uint32_t* length)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    char* str;
    if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_charptr(m_dm_topology_node, kRPVControllerTopologyNodePropertyValueKeyed, property, &str))
    {
        Data d(str);
        result = d.GetString(value, length);
    }
    return result;
}

rocprofvis_result_t 
TopologyNode::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) {
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    switch (property)
    {
    case kRPVControllerProcessStartTime:
    case kRPVControllerProcessEndTime:
    case kRPVControllerThreadStartTime:
    case kRPVControllerThreadEndTime:
    {
        uint64_t dm_value;
        if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_uint64(m_dm_topology_node, kRPVControllerTopologyNodePropertyValueKeyed, property, &dm_value))
        {
            *value = dm_value;
            result = kRocProfVisResultSuccess;
        }
        break;
    }
    default:
        if (kRocProfVisDmResultSuccess == rocprofvis_dm_get_property_as_double(m_dm_topology_node, kRPVControllerTopologyNodePropertyValueKeyed, property, value))
        {
            result = kRocProfVisResultSuccess;
        }
        break;
    }
    return result;
}


}
}
