// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <string>
#include <vector>
#include <map>

namespace RocProfVis
{
namespace Controller
{

class Trace;
class Track;

class TopologyNode : public Handle
{
public:
    TopologyNode(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx, TopologyNode* parent);
    virtual ~TopologyNode();

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) final;
    TopologyNode*       GetParent();
    TopologyNode*       GetParent(rocprofvis_controller_object_type_t type);
private:

    rocprofvis_dm_topology_node                m_dm_topology_node;
    rocprofvis_controller_topology_node_type_t m_node_type;
    const char*                                m_name;                                  
    std::map<rocprofvis_controller_topology_node_type_t, std::vector<TopologyNode*>>    
                                               m_children;
    TopologyNode*                              m_parent;
    Trace*                                     m_ctx;
    Track*                                     m_track;
};

class TopologyRoot : public TopologyNode
{
public:
    TopologyRoot(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx) :TopologyNode(dm_topology_node, ctx, nullptr) {};
    virtual ~TopologyRoot() {};
    rocprofvis_controller_object_type_t GetType(void) override { return kRPVControllerObjectTypeTopologyNode; };
};

class Node : public TopologyNode
{
public:
    Node(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx, TopologyNode* parent) :TopologyNode(dm_topology_node, ctx, parent) {};
    virtual ~Node() {};
    rocprofvis_controller_object_type_t GetType(void) override { return kRPVControllerObjectTypeNode; };
};

class Process : public TopologyNode
{
public:
    Process(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx, TopologyNode* parent) :TopologyNode(dm_topology_node, ctx, parent) {};
    virtual ~Process() {};
    rocprofvis_controller_object_type_t GetType(void) override { return kRPVControllerObjectTypeProcess; };
};

class Processor : public TopologyNode
{
public:
    Processor(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx, TopologyNode* parent) :TopologyNode(dm_topology_node, ctx, parent) {};
    virtual ~Processor() {};
    rocprofvis_controller_object_type_t GetType(void) override { return kRPVControllerObjectTypeProcessor; };
};

class Thread : public TopologyNode
{
public:
    Thread(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx, TopologyNode* parent) :TopologyNode(dm_topology_node, ctx, parent) {};
    virtual ~Thread() {};
    rocprofvis_controller_object_type_t GetType(void) override { return kRPVControllerObjectTypeThread; };
};

class Queue : public TopologyNode
{
public:
    Queue(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx, TopologyNode* parent) :TopologyNode(dm_topology_node, ctx, parent) {};
    virtual ~Queue() {};
    rocprofvis_controller_object_type_t GetType(void) override { return kRPVControllerObjectTypeQueue; };
};

class Stream : public TopologyNode
{
public:
    Stream(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx, TopologyNode* parent) :TopologyNode(dm_topology_node, ctx, parent) {};
    virtual ~Stream() {};
    rocprofvis_controller_object_type_t GetType(void) override { return kRPVControllerObjectTypeStream; };
};

class Counter : public TopologyNode
{
public:
    Counter(rocprofvis_dm_topology_node dm_topology_node, Trace* ctx, TopologyNode* parent) :TopologyNode(dm_topology_node, ctx, parent) {};
    virtual ~Counter() {};
    rocprofvis_controller_object_type_t GetType(void) override { return kRPVControllerObjectTypeCounter; };
};

}
}
