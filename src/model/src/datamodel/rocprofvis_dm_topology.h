// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_common_types.h"
#include "rocprofvis_dm_base.h"
#include "rocprofvis_db_query_builder.h"
#include <vector>
#include <map>
#include <variant>

namespace RocProfVis
{
namespace DataModel
{

#define INVALID_BRANCH_LEVEL -1;

// ExtData is class of container object for extended data records (ExtDataRecord)
// Can be used to store extended data records for event or for track
class TopologyNode : public DmBase {
public:
	using TopologyProperty = std::variant<uint64_t, double, std::string>;
    using TopologyNodeId = uint64_t;
	TopologyNode(rocprofvis_controller_topology_node_type_t type, TopologyNodeId id, TopologyNode* prev_node):m_type(type), m_id(id), m_prev_node(prev_node) {};
	virtual ~TopologyNode() {};

    TopologyNodeId GetId() { return m_id; };

    // Method to read Topology object property as uint64
    // @param property - property enumeration rocprofvis_dm_extdata_property_t
    // @param index - index of any indexed property
    // @param value - pointer reference to uint64_t return value
    // @return status of operation
    virtual rocprofvis_dm_result_t          GetPropertyAsUint64(rocprofvis_dm_property_t property, 
        rocprofvis_dm_property_index_t index, 
        uint64_t* value) override;  
    // Method to read Topology object property as uint64
    // @param property - property enumeration rocprofvis_dm_extdata_property_t
    // @param index - index of any indexed property
    // @param value - pointer reference to uint64_t return value
    // @return status of operation
    virtual rocprofvis_dm_result_t          GetPropertyAsDouble(rocprofvis_dm_property_t property, 
        rocprofvis_dm_property_index_t index, 
        double* value) override;  
    // Method to read Topology object property as char*
    // @param property - property enumeration rocprofvis_dm_extdata_property_t
    // @param index - index of any indexed property
    // @param value - pointer reference to char* return value
    // @return status of operation
    virtual rocprofvis_dm_result_t          GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
        rocprofvis_dm_property_index_t index, 
        char** value) override;
    // Method to read Topology object property as handle (aka void*)
    // @param property - property enumeration 
    // @param index - index of any indexed property
    // @param value - pointer reference to handle (aka void*) return value
    // @return status of operation
    virtual rocprofvis_dm_result_t GetPropertyAsHandle(rocprofvis_dm_property_t property, 
        rocprofvis_dm_property_index_t index, 
        rocprofvis_dm_handle_t* value) override;

    virtual rocprofvis_dm_result_t  AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers) { return kRocProfVisDmResultSuccess; };
    virtual TopologyNode* FindNodeMatchingIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers);
    rocprofvis_dm_result_t  SetBasicProperty(const char* name, uint64_t db_instance, rocprofvis_db_topology_data_type_t type, const char* value);
    virtual rocprofvis_dm_result_t  ProcessProperty(const char* name, rocprofvis_db_topology_data_type_t type, void* value) {return kRocProfVisDmResultSuccess;};
    rocprofvis_controller_topology_node_type_t GetType() { return m_type; };
    virtual std::string GetNodeName() = 0;
    virtual bool DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers);
    virtual rocprofvis_dm_result_t GetTrackId(uint64_t& id) { return kRocProfVisDmResultNotLoaded; }

protected:
    virtual TopologyNode* FindRelevantPropertyNode(rocprofvis_dm_track_identifiers_t* track_identifiers, std::string table_name);
    virtual TopologyNode* FindRelevantTopologyNode(rocprofvis_dm_track_identifiers_t* track_identifiers, std::string table_name);
    virtual const std::map<std::string, uint32_t>* GetPropertiesMap() { return nullptr; };
    virtual const bool IsRelevantPropertyTableName(std::string table_name) { return false; };
    virtual const bool IsRelevantTopologyTableName(std::string table_name) { return false; };
    virtual const uint32_t GetLevelId() const { return INVALID_BRANCH_LEVEL; };
    virtual const char* GetLevelTag() const { return ""; }
    TopologyNode* GetRootNode();
    
    
	rocprofvis_controller_topology_node_type_t   m_type;
    TopologyNodeId                               m_id;
	std::vector<std::unique_ptr<TopologyNode>>   m_children;
	std::map<uint32_t, TopologyProperty>         m_properties;
    TopologyNode*                                m_prev_node;
    std::string                                  m_name;
};


class TopologyNodeRoot : public TopologyNode {
public:
    TopologyNodeRoot():TopologyNode(kRPVControllerTopologyNodeRoot,0, nullptr) {};
    ~TopologyNodeRoot() {};

    rocprofvis_dm_result_t          AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    rocprofvis_dm_result_t          AddProperty(rocprofvis_dm_track_identifiers_t* track_identifiers, rocprofvis_db_topology_data_type_t type, const char* table, const char* name, void* value);
    std::string                     GetNodeName() { return "root"; };
};

class TopologyTrackRefence {
public:

    TopologyTrackRefence(uint32_t track_id) : m_track_id(track_id) {}
    virtual ~TopologyTrackRefence() {};
protected:
    uint32_t m_track_id;
};

class TopologyProcessRefence {
public:
    TopologyProcessRefence(rocprofvis_dm_process_id pid) : m_pid(pid) {}
    virtual ~TopologyProcessRefence() {};
protected:
    rocprofvis_dm_process_id m_pid;
};

class TopologyNodeSystemNode :  public TopologyNode {
public:
    TopologyNodeSystemNode(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx):
        TopologyNode(kRPVControllerTopologyNodeSytemNode, track_identifiers->id[TRACK_ID_NODE], ctx) {};
    ~TopologyNodeSystemNode() {};
    rocprofvis_dm_result_t AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    std::string GetNodeName() override;
private:
    const std::map<std::string, uint32_t>* GetPropertiesMap() override { return &s_columns_ids; }
    const bool IsRelevantPropertyTableName(std::string table_name) override { return table_name == "Node"; }
    const bool IsRelevantTopologyTableName(std::string table_name) override { return table_name == "Agent"; };
    const uint32_t GetLevelId() const override { return TRACK_ID_NODE; }
    virtual const char* GetLevelTag() const override { return Builder::NODE_ID_SERVICE_NAME; }
    inline static const std::map<std::string,uint32_t>
        s_columns_ids = {
            { "id", kRPVControllerNodeId },
            { "hostname", kRPVControllerNodeHostName },
            { "domain_name", kRPVControllerNodeDomainName },
            { "system_name", kRPVControllerNodeOSName },
            { "release", kRPVControllerNodeOSRelease },
            { "version", kRPVControllerNodeOSVersion },
            { "hardware_name", kRPVControllerNodeHardwareName },
            { "machine_id", kRPVControllerNodeMachineId },
            { "guid", kRPVControllerNodeMachineGuid },
            { "hash", kRPVControllerNodeHash },
    };
    rocprofvis_dm_track_identifiers_t temp_processor_node_idetifiers;
};

class TopologyNodeRPDSystemNode : public TopologyNodeSystemNode {
public:
    TopologyNodeRPDSystemNode(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) :
        TopologyNodeSystemNode(track_identifiers, ctx) {
    }
private:
    virtual const char* GetLevelTag() const override { return Builder::SPACESAVER_SERVICE_NAME; }
};

class TopologyNodeProcess : public TopologyNode {
public:

    TopologyNodeProcess(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) :
        TopologyNode(kRPVControllerTopologyNodeProcess, track_identifiers->id[TRACK_ID_PID], ctx) {}
    ~TopologyNodeProcess() {};
    rocprofvis_dm_result_t AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    std::string GetNodeName() override;
private:
    const std::map<std::string, uint32_t>* GetPropertiesMap() override { return &s_columns_ids; }
    const bool IsRelevantPropertyTableName(std::string table_name) override { return table_name == "Process"; }
    const uint32_t GetLevelId() const override { return TRACK_ID_PID; }
    const char* GetLevelTag() const override { return Builder::PROCESS_ID_SERVICE_NAME; }
    inline static const std::map<std::string, uint32_t> 
        s_columns_ids = {
            { "id", kRPVControllerProcessId },
            { "nid", kRPVControllerProcessNodeId },
            { "init", kRPVControllerProcessInitTime },
            { "fini", kRPVControllerProcessFinishTime },
            { "start", kRPVControllerProcessStartTime },
            { "end", kRPVControllerProcessEndTime },
            { "command", kRPVControllerProcessCommand },
            { "environment", kRPVControllerProcessEnvironment },
            { "extdata", kRPVControllerProcessExtData },
    };
};

class TopologyNodeProcessor : public TopologyNode {
public:

    TopologyNodeProcessor(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) :TopologyNode(kRPVControllerTopologyNodeProcessor, track_identifiers->id[TRACK_ID_AGENT], ctx) {}
    ~TopologyNodeProcessor() {};
    rocprofvis_dm_result_t AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    std::string GetNodeName() override;
private:
    const std::map<std::string, uint32_t>* GetPropertiesMap() override { return &s_columns_ids; }
    const bool IsRelevantPropertyTableName(std::string table_name) override { return table_name == "Agent"; }
    const uint32_t GetLevelId() const override { return TRACK_ID_AGENT; }
    const char* GetLevelTag() const override { return Builder::AGENT_ID_SERVICE_NAME; }
    inline static const std::map<std::string, uint32_t> 
        s_columns_ids = {
            { "id", kRPVControllerProcessorId },
            { "nid", kRPVControllerProcessorNodeId },
            { "type", kRPVControllerProcessorType },
            { "type_index", kRPVControllerProcessorTypeIndex },
            { "absolute_index", kRPVControllerProcessorIndex },
            { "logical_index", kRPVControllerProcessorLogicalIndex },
            { "uuid", kRPVControllerProcessorUUID },
            { "name", kRPVControllerProcessorName },
            { "model_name", kRPVControllerProcessorModelName },
            { "vendor_name", kRPVControllerProcessorVendorName },
            { "product_name", kRPVControllerProcessorProductName },
            { "user_name", kRPVControllerProcessorUserName },
            { "extdata", kRPVControllerProcessorExtData },
    };
};

class TopologyNodeThread : public TopologyNode, public TopologyTrackRefence {
public:

    TopologyNodeThread(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) :
        TopologyNode(kRPVControllerTopologyNodeThread, track_identifiers->id[TRACK_ID_TID], ctx),
        TopologyTrackRefence(track_identifiers->track_id),
        m_thread_type(track_identifiers->category == kRocProfVisDmRegionSampleTrack ? kRPVControllerThreadTypeSampled : kRPVControllerThreadTypeInstrumented) {}
    virtual ~TopologyNodeThread() {};
    virtual std::string GetNodeName() override; 
    rocprofvis_dm_result_t GetTrackId(uint64_t& id) override { id = m_track_id; return kRocProfVisDmResultSuccess;}

private:
    rocprofvis_controller_thread_type_t m_thread_type;
    const uint32_t GetLevelId() const override { return TRACK_ID_TID; }
    const char* GetLevelTag() const override { return Builder::THREAD_ID_SERVICE_NAME; }
    const std::map<std::string, uint32_t>* GetPropertiesMap() override { return &s_columns_ids; }
    const bool IsRelevantPropertyTableName(std::string table_name) override { return table_name == "Thread"; }
    inline static const std::map<std::string, uint32_t> 
        s_columns_ids = {
        { "id", kRPVControllerThreadId },
        { "nid", kRPVControllerThreadNode },
        { "pid", kRPVControllerThreadProcess },
        { "ppid", kRPVControllerThreadParentId },
        { "tid", kRPVControllerThreadTid },
        { "name", kRPVControllerThreadName },
        { "extdata", kRPVControllerThreadExtData },
        { "start", kRPVControllerThreadStartTime },
        { "end", kRPVControllerThreadEndTime },

    };
};

class TopologyNodeThreadInstrumented : public TopologyNodeThread {
public:
    TopologyNodeThreadInstrumented(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) : TopologyNodeThread(track_identifiers, ctx) {}
    ~TopologyNodeThreadInstrumented() {};
    std::string GetNodeName() override;
    bool DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    rocprofvis_dm_result_t  GetPropertyAsUint64(rocprofvis_dm_property_t property,  rocprofvis_dm_property_index_t index,  uint64_t* value) override;
};

class TopologyNodeThreadSampled : public TopologyNodeThread {
public:
    TopologyNodeThreadSampled(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) : TopologyNodeThread(track_identifiers, ctx) {}
    ~TopologyNodeThreadSampled() {};
    std::string GetNodeName() override;
    bool DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    rocprofvis_dm_result_t  GetPropertyAsUint64(rocprofvis_dm_property_t property,  rocprofvis_dm_property_index_t index,  uint64_t* value) override;
};

class TopologyNodeQueue : public TopologyNode , public TopologyTrackRefence, public TopologyProcessRefence  {
public:

    TopologyNodeQueue(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) :
        TopologyNode(kRPVControllerTopologyNodeQueue, track_identifiers->id[TRACK_ID_QUEUE], ctx),
        TopologyTrackRefence(track_identifiers->track_id),
        TopologyProcessRefence(track_identifiers->process_id) {}
    virtual ~TopologyNodeQueue() {};
    virtual std::string GetNodeName() override;
    virtual bool DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    rocprofvis_dm_result_t GetTrackId(uint64_t& id) override { id = m_track_id; return kRocProfVisDmResultSuccess;}
private:
    const uint32_t GetLevelId() const override { return TRACK_ID_QUEUE; }
    const char* GetLevelTag() const override { return Builder::QUEUE_ID_SERVICE_NAME; }
    const std::map<std::string, uint32_t>* GetPropertiesMap() override { return &s_columns_ids; }
    const bool IsRelevantPropertyTableName(std::string table_name) override { return table_name == "Queue"; }
    inline static const std::map<std::string, uint32_t> 
        s_columns_ids = {
        { "id", kRPVControllerQueueId },
        { "nid", kRPVControllerQueueNode },
        { "pid", kRPVControllerQueueProcess },
        { "name", kRPVControllerQueueName },
        { "extdata", kRPVControllerQueueExtData },
    };
};

class TopologyNodeMemoryAllocation : public TopologyNodeQueue {
public:
    TopologyNodeMemoryAllocation(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) : TopologyNodeQueue(track_identifiers, ctx) {}
    ~TopologyNodeMemoryAllocation() {};
    std::string GetNodeName() override;
    bool DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    rocprofvis_dm_result_t  GetPropertyAsUint64(rocprofvis_dm_property_t property,  rocprofvis_dm_property_index_t index,  uint64_t* value) override;
};

class TopologyNodeMemoryCopy : public TopologyNodeQueue {
public:
    TopologyNodeMemoryCopy(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) : TopologyNodeQueue(track_identifiers, ctx) {}
    ~TopologyNodeMemoryCopy() {};
    std::string GetNodeName() override;
    bool DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    rocprofvis_dm_result_t  GetPropertyAsUint64(rocprofvis_dm_property_t property,  rocprofvis_dm_property_index_t index,  uint64_t* value) override;
};

class TopologyNodeKernelDispatch : public TopologyNodeQueue {
public:
    TopologyNodeKernelDispatch(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) : TopologyNodeQueue(track_identifiers, ctx) {}
    ~TopologyNodeKernelDispatch() {};
    bool DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
};

class TopologyNodeStream : public TopologyNode, public TopologyTrackRefence {
public:

    TopologyNodeStream(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) :
        TopologyNode(kRPVControllerTopologyNodeStream, track_identifiers->id[TRACK_ID_STREAM], ctx),
        TopologyTrackRefence(track_identifiers->track_id){}
    ~TopologyNodeStream() {};
    std::string GetNodeName() override;
    rocprofvis_dm_result_t GetTrackId(uint64_t& id) override { id = m_track_id; return kRocProfVisDmResultSuccess;}
private:
    const uint32_t GetLevelId() const override { return TRACK_ID_STREAM; }
    const char* GetLevelTag() const override { return Builder::STREAM_ID_SERVICE_NAME; }
    rocprofvis_dm_result_t  ProcessProperty(const char* name, rocprofvis_db_topology_data_type_t type, void* value) override;
    const std::map<std::string, uint32_t>* GetPropertiesMap() override { return &s_columns_ids; }
    const bool IsRelevantPropertyTableName(std::string table_name) override { return table_name == "Stream"; }
    const bool IsRelevantTopologyTableName(std::string table_name) override { return table_name == "QueueRef"; };
    inline static const std::map<std::string, uint32_t> 
        s_columns_ids = {
        { "id", kRPVControllerStreamId },
        { "nid", kRPVControllerStreamNode },
        { "pid", kRPVControllerStreamProcess },
        { "name", kRPVControllerStreamName },
        { "extdata", kRPVControllerStreamExtData },
    };
};

class TopologyNodeCounter : public TopologyNode , public TopologyTrackRefence , public TopologyProcessRefence {
public:

    TopologyNodeCounter(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) :
        TopologyNode(kRPVControllerTopologyNodeCounter, 
            track_identifiers->id[TRACK_ID_COUNTER],
            ctx),
        TopologyTrackRefence(track_identifiers->track_id),
        TopologyProcessRefence(track_identifiers->process_id),
    m_tag(track_identifiers->tag[TRACK_ID_COUNTER]) { }
    ~TopologyNodeCounter() {};
    std::string GetNodeName() override;
    bool DoesThisNodeMatchIdentifiers(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
    rocprofvis_dm_result_t GetTrackId(uint64_t& id) override { id = m_track_id; return kRocProfVisDmResultSuccess;}
private:
    std::string m_tag;
    const uint32_t GetLevelId() const override { return TRACK_ID_COUNTER; }
    const char* GetLevelTag() const override { return m_tag.c_str();}
    const std::map<std::string, uint32_t>* GetPropertiesMap() override { return &s_columns_ids; }
    const bool IsRelevantPropertyTableName(std::string table_name) override { return table_name == "PMC"; }
    inline static const std::map<std::string, uint32_t> 
        s_columns_ids = {
        { "id", kRPVControllerCounterId },
        { "nid", kRPVControllerCounterNode },
        { "pid", kRPVControllerCounterProcess },
        { "agent_id", kRPVControllerCounterProcessor },
        { "name", kRPVControllerCounterName },
        { "symbol", kRPVControllerCounterSymbol },
        { "description", kRPVControllerCounterDescription },
        { "long_description", kRPVControllerCounterExtendedDesc },
        { "component", kRPVControllerCounterComponent },
        { "units", kRPVControllerCounterUnits },
        { "value_type", kRPVControllerCounterValueType },
        { "block", kRPVControllerCounterBlock },
        { "expression", kRPVControllerCounterExpression },
        { "guid", kRPVControllerCounterGuid },
        { "extdata", kRPVControllerCounterExtData },
        { "target_arch", kRPVControllerCounterTargetArch },
        { "event_code", kRPVControllerCounterEventCode },
        { "instance_id", kRPVControllerCounterInstanceId },
        { "is_constant", kRPVControllerCounterIsConstant },
        { "is_derived", kRPVControllerCounterIsDerived },
    };
};

class TopologyReferenceNode : public TopologyNode {
public:
    TopologyReferenceNode(rocprofvis_controller_topology_node_type_t type, uint64_t id, rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) 
        :TopologyNode(type, id, ctx) { downstream_track_identifiers = *track_identifiers; }
    virtual rocprofvis_dm_result_t          GetPropertyAsUint64(rocprofvis_dm_property_t property, 
        rocprofvis_dm_property_index_t index, 
        uint64_t* value) override;  
    virtual rocprofvis_dm_result_t          GetPropertyAsDouble(rocprofvis_dm_property_t property, 
        rocprofvis_dm_property_index_t index, 
        double* value) override;  
    virtual rocprofvis_dm_result_t          GetPropertyAsCharPtr(rocprofvis_dm_property_t property, 
        rocprofvis_dm_property_index_t index, 
        char** value) override;
    virtual rocprofvis_dm_result_t GetPropertyAsHandle(rocprofvis_dm_property_t property, 
        rocprofvis_dm_property_index_t index, 
        rocprofvis_dm_handle_t* value) override;
    std::string GetNodeName() override;
protected:
    TopologyNode* FindReferencedNode();
private:

    rocprofvis_dm_track_identifiers_t downstream_track_identifiers;
};

class TopologyNodeDownStreamProcessor : public TopologyReferenceNode {
public:
    TopologyNodeDownStreamProcessor(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) 
        :TopologyReferenceNode(kRPVControllerTopologyNodeProcessor, track_identifiers->id[TRACK_ID_AGENT], track_identifiers, ctx) {}
    rocprofvis_dm_result_t          AddNode(rocprofvis_dm_track_identifiers_t* track_identifiers) override;
private:
    const uint32_t GetLevelId() const override { return TRACK_ID_AGENT; }
    const char* GetLevelTag() const override { return Builder::AGENT_ID_SERVICE_NAME; }
};

class TopologyNodeDownStreamQueue : public TopologyReferenceNode, public TopologyTrackRefence {
public:
    TopologyNodeDownStreamQueue(rocprofvis_dm_track_identifiers_t* track_identifiers, TopologyNode* ctx) 
        :TopologyReferenceNode(kRPVControllerTopologyNodeQueue, track_identifiers->id[TRACK_ID_QUEUE], track_identifiers, ctx),
        TopologyTrackRefence(track_identifiers->track_id){}
    rocprofvis_dm_result_t GetTrackId(uint64_t& id) override { id = m_track_id; return kRocProfVisDmResultSuccess;}
private:
    const uint32_t GetLevelId() const override { return TRACK_ID_QUEUE; }
    const char* GetLevelTag() const override { return Builder::QUEUE_ID_SERVICE_NAME; }

};

}  // namespace DataModel
}  // namespace RocProfVis