// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_topology_model.h"
#include "rocprofvis_common_defs.h"

#include <algorithm>
#include <sstream>

namespace RocProfVis
{
namespace View
{

TopologyNode::TopologyNode(TopologyNodeType type, uint64_t id)
: m_type(type)
, m_id(id)
, m_parent(nullptr)
, m_track_id(INVALID_UINT64_INDEX)
{}

const std::vector<TopologyNode*>&
TopologyNode::EmptyChildList()
{
    static const std::vector<TopologyNode*> empty;
    return empty;
}

TopologyNode*
TopologyNode::GetParent(TopologyNodeType type) const
{
    TopologyNode* ancestor = m_parent;
    while(ancestor && ancestor->GetNodeType() != type)
    {
        ancestor = ancestor->GetParent();
    }
    return ancestor;
}

const std::vector<TopologyNode*>&
TopologyNode::GetChildren(TopologyNodeType type) const
{
    std::map<TopologyNodeType, std::vector<TopologyNode*>>::const_iterator it =
        m_children.find(type);
    return (it != m_children.end()) ? it->second : EmptyChildList();
}

size_t
TopologyNode::GetChildCount(TopologyNodeType type) const
{
    return GetChildren(type).size();
}

const std::vector<TopologyNode*>&
TopologyNode::GetLinkedChildren(TopologyNodeType type) const
{
    std::map<TopologyNodeType, std::vector<TopologyNode*>>::const_iterator it =
        m_linked_children.find(type);
    return (it != m_linked_children.end()) ? it->second : EmptyChildList();
}

bool
TopologyNode::HasTrack() const
{
    return m_track_id != INVALID_UINT64_INDEX;
}

uint64_t
QueueInfo::GetProcessorId() const
{
    const TopologyNode* processor = GetParent();
    return processor ? processor->GetId() : INVALID_UINT64_INDEX;
}

uint64_t
CounterInfo::GetProcessorId() const
{
    const TopologyNode* processor = GetParent();
    return processor ? processor->GetId() : INVALID_UINT64_INDEX;
}

TopologyTree::TopologyTree()
: m_root(nullptr)
{
    Clear();
}

void
TopologyTree::Attach(TopologyNode* parent, TopologyNode* child)
{
    child->m_parent = parent;
    parent->m_children[child->GetNodeType()].push_back(child);
}

NodeInfo*
TopologyTree::AddNode(uint64_t node_id)
{
    std::unique_ptr<NodeInfo> owned = std::make_unique<NodeInfo>(node_id);
    NodeInfo*                 node  = owned.get();
    m_storage.push_back(std::move(owned));
    Attach(m_root, node);
    m_node_index[node_id] = node;
    return node;
}

void
TopologyTree::AssignNodeDisplayIndices()
{
    std::vector<uint64_t> ids;
    ids.reserve(m_node_index.size());
    for(const std::pair<const uint64_t, NodeInfo*>& entry : m_node_index)
    {
        ids.push_back(entry.first);
    }
    std::sort(ids.begin(), ids.end());
    for(size_t rank = 0; rank < ids.size(); rank++)
    {
        m_node_index[ids[rank]]->display_index = rank + 1;
    }
}

size_t
TopologyTree::GetNodeDisplayIndex(uint64_t node_id) const
{
    const NodeInfo* node = GetNode(node_id);
    return node ? node->display_index : 0;
}

ProcessorInfo*
TopologyTree::AddProcessor(NodeInfo* parent, TopologyId processor_id)
{
    std::unique_ptr<ProcessorInfo> owned =
        std::make_unique<ProcessorInfo>(processor_id.value);
    ProcessorInfo* processor = owned.get();
    m_storage.push_back(std::move(owned));
    Attach(parent, processor);
    m_processor_index[processor_id.value] = processor;
    return processor;
}

ProcessInfo*
TopologyTree::AddProcess(NodeInfo* parent, uint64_t process_id)
{
    std::unique_ptr<ProcessInfo> owned = std::make_unique<ProcessInfo>(process_id);
    ProcessInfo*                 process = owned.get();
    m_storage.push_back(std::move(owned));
    Attach(parent, process);
    m_process_index[process_id] = process;
    return process;
}

ThreadInfo*
TopologyTree::AddThread(ProcessInfo* parent, uint64_t thread_id, ThreadInfo::Kind kind)
{
    std::unique_ptr<ThreadInfo> owned = std::make_unique<ThreadInfo>(thread_id, kind);
    ThreadInfo*                 thread = owned.get();
    m_storage.push_back(std::move(owned));
    Attach(parent, thread);
    if(kind == ThreadInfo::Kind::kInstrumented)
    {
        m_instrumented_thread_index[thread_id] = thread;
    }
    else
    {
        m_sampled_thread_index[thread_id] = thread;
    }
    return thread;
}

StreamInfo*
TopologyTree::AddStream(ProcessInfo* parent, uint64_t stream_id)
{
    std::unique_ptr<StreamInfo> owned  = std::make_unique<StreamInfo>(stream_id);
    StreamInfo*                 stream = owned.get();
    m_storage.push_back(std::move(owned));
    Attach(parent, stream);
    m_stream_index[stream_id] = stream;
    return stream;
}

QueueInfo*
TopologyTree::AddQueue(ProcessorInfo* parent, uint64_t queue_id)
{
    std::unique_ptr<QueueInfo> owned = std::make_unique<QueueInfo>(queue_id);
    QueueInfo*                 queue = owned.get();
    m_storage.push_back(std::move(owned));
    Attach(parent, queue);
    m_queue_index[{ queue_id, parent->GetId() }] = queue;
    return queue;
}

CounterInfo*
TopologyTree::AddCounter(ProcessorInfo* parent, uint64_t counter_id)
{
    std::unique_ptr<CounterInfo> owned   = std::make_unique<CounterInfo>(counter_id);
    CounterInfo*                 counter = owned.get();
    m_storage.push_back(std::move(owned));
    Attach(parent, counter);
    m_counter_index[counter_id] = counter;
    return counter;
}

void
TopologyTree::LinkStreamProcessor(StreamInfo* stream, ProcessorInfo* processor)
{
    if(!stream || !processor)
    {
        return;
    }
    std::vector<TopologyNode*>& linked =
        stream->m_linked_children[TopologyNodeType::kProcessor];
    if(std::find(linked.begin(), linked.end(), processor) == linked.end())
    {
        linked.push_back(processor);
    }
}

void
TopologyTree::LinkStreamQueue(StreamInfo* stream, QueueInfo* queue)
{
    if(!stream || !queue)
    {
        return;
    }
    std::vector<TopologyNode*>& linked =
        stream->m_linked_children[TopologyNodeType::kQueue];
    if(std::find(linked.begin(), linked.end(), queue) == linked.end())
    {
        linked.push_back(queue);
        queue->m_secondary_parents.push_back(stream);
    }
}

void
TopologyTree::BindTrack(TopologyNode* node, uint64_t track_id)
{
    if(!node || track_id == INVALID_UINT64_INDEX)
    {
        return;
    }
    node->m_track_id     = track_id;
    m_track_index[track_id] = node;
}

const NodeInfo*
TopologyTree::GetNode(uint64_t node_id) const
{
    std::unordered_map<uint64_t, NodeInfo*>::const_iterator it = m_node_index.find(node_id);
    return (it != m_node_index.end()) ? it->second : nullptr;
}

const ProcessorInfo*
TopologyTree::GetProcessor(uint64_t processor_id) const
{
    return GetProcessorMutable(processor_id);
}

ProcessorInfo*
TopologyTree::GetProcessorMutable(uint64_t processor_id) const
{
    std::unordered_map<uint64_t, ProcessorInfo*>::const_iterator it =
        m_processor_index.find(processor_id);
    return (it != m_processor_index.end()) ? it->second : nullptr;
}

const ProcessInfo*
TopologyTree::GetProcess(uint64_t process_id) const
{
    std::unordered_map<uint64_t, ProcessInfo*>::const_iterator it =
        m_process_index.find(process_id);
    return (it != m_process_index.end()) ? it->second : nullptr;
}

const QueueInfo*
TopologyTree::GetQueue(uint64_t queue_id, uint64_t processor_id) const
{
    return GetQueueMutable(queue_id, processor_id);
}

QueueInfo*
TopologyTree::GetQueueMutable(uint64_t queue_id, uint64_t processor_id) const
{
    std::map<std::pair<uint64_t, uint64_t>, QueueInfo*>::const_iterator it =
        m_queue_index.find({ queue_id, processor_id });
    return (it != m_queue_index.end()) ? it->second : nullptr;
}

const StreamInfo*
TopologyTree::GetStream(uint64_t stream_id) const
{
    std::unordered_map<uint64_t, StreamInfo*>::const_iterator it =
        m_stream_index.find(stream_id);
    return (it != m_stream_index.end()) ? it->second : nullptr;
}

const CounterInfo*
TopologyTree::GetCounter(uint64_t counter_id) const
{
    std::unordered_map<uint64_t, CounterInfo*>::const_iterator it =
        m_counter_index.find(counter_id);
    return (it != m_counter_index.end()) ? it->second : nullptr;
}

const ThreadInfo*
TopologyTree::GetInstrumentedThread(uint64_t thread_id) const
{
    std::unordered_map<uint64_t, ThreadInfo*>::const_iterator it =
        m_instrumented_thread_index.find(thread_id);
    return (it != m_instrumented_thread_index.end()) ? it->second : nullptr;
}

const ThreadInfo*
TopologyTree::GetSampledThread(uint64_t thread_id) const
{
    std::unordered_map<uint64_t, ThreadInfo*>::const_iterator it =
        m_sampled_thread_index.find(thread_id);
    return (it != m_sampled_thread_index.end()) ? it->second : nullptr;
}

const TopologyNode*
TopologyTree::FindByTrackId(uint64_t track_id) const
{
    std::unordered_map<uint64_t, TopologyNode*>::const_iterator it =
        m_track_index.find(track_id);
    return (it != m_track_index.end()) ? it->second : nullptr;
}

const std::vector<TopologyNode*>&
TopologyTree::GetNodes() const
{
    return m_root->GetChildren(TopologyNodeType::kNode);
}

bool
TopologyTree::GetProcessorTypeLabel(const ProcessorInfo& processor_info,
                                    std::string&         label_out) const
{
    switch(processor_info.type)
    {
        case kRPVControllerProcessorTypeCPU:
            label_out = "CPU" + std::to_string(processor_info.type_index);
            return true;
        case kRPVControllerProcessorTypeGPU:
            label_out = "GPU" + std::to_string(processor_info.type_index);
            return true;
        case kRPVControllerProcessorTypeNIC:
            label_out = "NIC" + std::to_string(processor_info.type_index);
            return true;
        default: return false;
    }
}

void
TopologyTree::Clear()
{
    m_node_index.clear();
    m_processor_index.clear();
    m_process_index.clear();
    m_stream_index.clear();
    m_counter_index.clear();
    m_instrumented_thread_index.clear();
    m_sampled_thread_index.clear();
    m_queue_index.clear();
    m_track_index.clear();
    m_storage.clear();

    std::unique_ptr<TopologyNode> root =
        std::make_unique<TopologyNode>(TopologyNodeType::kRoot, 0);
    m_root = root.get();
    m_storage.push_back(std::move(root));
}

std::string
TopologyTree::ToString() const
{
    return ToString(*m_root, 0);
}

std::string
TopologyTree::ToString(const TopologyNode& node, int indent) const
{
    static const std::map<TopologyNodeType, const char*> TYPE_NAMES = {
        { TopologyNodeType::kRoot, "Root" },
        { TopologyNodeType::kNode, "Node" },
        { TopologyNodeType::kProcessor, "Processor" },
        { TopologyNodeType::kProcess, "Process" },
        { TopologyNodeType::kThread, "Thread" },
        { TopologyNodeType::kStream, "Stream" },
        { TopologyNodeType::kQueue, "Queue" },
        { TopologyNodeType::kCounter, "Counter" }
    };

    std::ostringstream ss;
    ss << std::string(indent, ' ') << TYPE_NAMES.at(node.GetNodeType()) << " "
       << node.GetId();
    if(!node.GetName().empty())
    {
        ss << " (" << node.GetName() << ")";
    }
    if(node.HasTrack())
    {
        ss << " track=" << node.GetTrackId();
    }
    ss << std::endl;

    for(const std::pair<const TopologyNodeType, std::vector<TopologyNode*>>& group :
        node.m_children)
    {
        for(const TopologyNode* child : group.second)
        {
            ss << ToString(*child, indent + 2);
        }
    }
    return ss.str();
}

}  // namespace View
}  // namespace RocProfVis
