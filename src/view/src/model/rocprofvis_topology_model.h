// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_model_types.h"

#include <map>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

enum class TopologyNodeType : uint32_t
{
    kRoot,
    kNode,
    kProcessor,
    kProcess,
    kThread,
    kStream,
    kQueue,
    kCounter
};

class TopologyTree;

/**
 * @brief Base for every level of the system topology.
 *
 * Nodes are owned by the TopologyTree arena, never by their parent. That is
 * what lets a node be reached from more than one place: a queue sits under its
 * processor (the structural parent) and is also referenced by every stream
 * that dispatches to it (a secondary parent).
 */
class TopologyNode
{
public:
    TopologyNode(TopologyNodeType type, uint64_t id);
    virtual ~TopologyNode() = default;

    TopologyNodeType   GetNodeType() const { return m_type; }
    uint64_t           GetId() const { return m_id; }
    const std::string& GetName() const { return m_name; }
    void               SetName(const std::string& name) { m_name = name; }

    // Display string built once at load; the sidebar reads it every frame.
    const std::string& GetHeader() const { return m_header; }
    void               SetHeader(const std::string& header) { m_header = header; }

    TopologyNode* GetParent() const { return m_parent; }
    // Nearest ancestor of the given type, walking structural parents only.
    TopologyNode* GetParent(TopologyNodeType type) const;

    // Streams that dispatch to this queue. Empty at every other level.
    const std::vector<TopologyNode*>& GetSecondaryParents() const
    {
        return m_secondary_parents;
    }

    const std::vector<TopologyNode*>& GetChildren(TopologyNodeType type) const;

    // Referenced but not owned: the processors and queues a stream dispatches to.
    const std::vector<TopologyNode*>& GetLinkedChildren(TopologyNodeType type) const;

    /*
     * Timeline identity of the track this node draws as, or INVALID_UINT64_INDEX
     * for interior nodes. Only the id is stored: the track's *position* on the
     * timeline changes on every sort/drag, so it is resolved through
     * TimelineModel at the point of use instead of being cached here.
     */
    bool     HasTrack() const;
    uint64_t GetTrackId() const { return m_track_id; }

private:
    friend class TopologyTree;  // wires edges and binds tracks

    static const std::vector<TopologyNode*>& EmptyChildList();

    TopologyNodeType                                       m_type;
    uint64_t                                               m_id;
    std::string                                            m_name;
    std::string                                            m_header;
    TopologyNode*                                          m_parent;
    std::map<TopologyNodeType, std::vector<TopologyNode*>> m_children;
    std::map<TopologyNodeType, std::vector<TopologyNode*>> m_linked_children;
    std::vector<TopologyNode*>                             m_secondary_parents;
    uint64_t                                               m_track_id;
};

struct NodeInfo : public TopologyNode
{
    explicit NodeInfo(uint64_t id)
    : TopologyNode(TopologyNodeType::kNode, id)
    , display_index(0)
    {}

    std::string host_name;
    std::string os_name;
    std::string os_release;
    std::string os_version;
    // 1-based position in controller order, used for the node color wheel and
    // the "[2] hostname" labels. Assigned once while the tree is built.
    size_t display_index;
};

struct ProcessorInfo : public TopologyNode
{
    explicit ProcessorInfo(uint64_t id)
    : TopologyNode(TopologyNodeType::kProcessor, id)
    , type(kRPVControllerProcessorTypeUndefined)
    , type_index(0)
    {}

    // The id carries the source-instance bits for multinode/compare traces.
    TopologyId GetTopologyId() const
    {
        TopologyId id;
        id.value = GetId();
        return id;
    }

    std::string                            product_name;
    rocprofvis_controller_processor_type_t type;
    uint64_t                               type_index;  // GPU0, GPU1, ...
};

struct ProcessInfo : public TopologyNode
{
    explicit ProcessInfo(uint64_t id)
    : TopologyNode(TopologyNodeType::kProcess, id)
    , start_time(0.0)
    , end_time(0.0)
    {}

    double      start_time;
    double      end_time;
    std::string command;
    std::string environment;
};

struct ThreadInfo : public TopologyNode
{
    enum class Kind
    {
        kInstrumented,
        kSampled
    };

    ThreadInfo(uint64_t id, Kind thread_kind)
    : TopologyNode(TopologyNodeType::kThread, id)
    , kind(thread_kind)
    , tid(0)
    , start_time(0.0)
    , end_time(0.0)
    {}

    Kind     kind;
    uint64_t tid;
    double   start_time;
    double   end_time;
};

struct QueueInfo : public TopologyNode
{
    explicit QueueInfo(uint64_t id)
    : TopologyNode(TopologyNodeType::kQueue, id)
    {}

    // Owning processor id. Queue ids are only unique within a processor, so
    // this is half of the queue lookup key.
    uint64_t GetProcessorId() const;
};

struct StreamInfo : public TopologyNode
{
    explicit StreamInfo(uint64_t id)
    : TopologyNode(TopologyNodeType::kStream, id)
    {}

    // Processors and queues reached through GetLinkedChildren(); the stream
    // does not own them.
};

struct CounterInfo : public TopologyNode
{
    explicit CounterInfo(uint64_t id)
    : TopologyNode(TopologyNodeType::kCounter, id)
    {}

    uint64_t GetProcessorId() const;

    std::string description;
    std::string units;
    std::string value_type;
};

/**
 * @brief The system topology, as a tree.
 *
 * Built once when a trace loads by walking the controller's topology, which is
 * already a parent-linked tree. The arena owns every node; the per-type maps
 * are lookup shortcuts over that tree, not structure.
 */
class TopologyTree
{
public:
    TopologyTree();
    ~TopologyTree() = default;

    // Construction. Each factory allocates into the arena and wires the
    // structural edge, so a node can never exist unparented.
    NodeInfo*      AddNode(uint64_t node_id);
    ProcessorInfo* AddProcessor(NodeInfo* parent, TopologyId processor_id);
    ProcessInfo*   AddProcess(NodeInfo* parent, uint64_t process_id);
    ThreadInfo*    AddThread(ProcessInfo* parent, uint64_t thread_id, ThreadInfo::Kind kind);
    StreamInfo*    AddStream(ProcessInfo* parent, uint64_t stream_id);
    QueueInfo*     AddQueue(ProcessorInfo* parent, uint64_t queue_id);
    CounterInfo*   AddCounter(ProcessorInfo* parent, uint64_t counter_id);

    /*
     * Second edge: the controller repeats a stream's processors and queues as
     * its own subtree. Link the existing nodes instead of duplicating them.
     * Both are idempotent.
     */
    void LinkStreamProcessor(StreamInfo* stream, ProcessorInfo* processor);
    void LinkStreamQueue(StreamInfo* stream, QueueInfo* queue);

    void BindTrack(TopologyNode* node, uint64_t track_id);

    /*
     * Derives everything that depends on the finished tree (node display ranks,
     * track order) and bumps the revision. Call once when the load walk is done.
     */
    void Finalize();

    /*
     * Changes each time the tree is rebuilt. Consumers that project the tree
     * (the sidebar, the details pane) remember the revision they last built
     * against instead of sharing a dirty flag, so a rebuild of one does not
     * force a rebuild of the other.
     */
    uint64_t GetRevision() const { return m_revision; }

    /*
     * Track ids in topology order: per node, each processor's queues then
     * counters, then each process's streams and threads. Deduped, so a queue
     * reached from both its processor and a stream appears once. Drives the
     * timeline's "sort by topology"; tracks the controller never tied to a
     * topology node are not in here.
     */
    const std::vector<uint64_t>& GetTrackOrder() const { return m_track_order; }

    // Lookup shortcuts.
    const NodeInfo*      GetNode(uint64_t node_id) const;
    const ProcessorInfo* GetProcessor(uint64_t processor_id) const;
    const ProcessInfo*   GetProcess(uint64_t process_id) const;
    const QueueInfo*     GetQueue(uint64_t queue_id, uint64_t processor_id) const;
    const CounterInfo*   GetCounter(uint64_t counter_id) const;
    const ThreadInfo*    GetThread(uint64_t thread_id, ThreadInfo::Kind kind) const;

    ProcessorInfo* GetProcessorMutable(uint64_t processor_id) const;
    QueueInfo*     GetQueueMutable(uint64_t queue_id, uint64_t processor_id) const;

    const TopologyNode* FindByTrackId(uint64_t track_id) const;

    const std::vector<TopologyNode*>& GetNodes() const;
    size_t                            NodeCount() const { return m_node_index.size(); }
    size_t                            ProcessCount() const { return m_process_index.size(); }
    // 1-based display rank, or 0 if the node id is unknown.
    size_t GetNodeDisplayIndex(uint64_t node_id) const;

    // "GPU0", "CPU1". False (and label_out untouched) for an undefined type.
    bool GetProcessorTypeLabel(const ProcessorInfo& processor_info,
                               std::string&         label_out) const;
    // "GPU", "CPU", "NIC", or "Undefined".
    static const char* GetProcessorTypeName(
        rocprofvis_controller_processor_type_t processor_type);

    void Clear();

    // Debug
    std::string ToString() const;

private:
    void        Attach(TopologyNode* parent, TopologyNode* child);
    std::string ToString(const TopologyNode& node, int indent) const;
    /*
     * Ranks nodes by ascending id and stores the 1-based rank on each NodeInfo.
     * The rank drives the node color wheel and the "[2] hostname" labels, which
     * must stay stable across sessions.
     */
    void AssignNodeDisplayIndices();
    void BuildTrackOrder();

    // The arena owns every node; all parent/child edges are non-owning.
    std::vector<std::unique_ptr<TopologyNode>>          m_storage;
    TopologyNode*                                       m_root;
    uint64_t                                            m_revision;
    std::vector<uint64_t>                               m_track_order;
    std::unordered_map<uint64_t, NodeInfo*>             m_node_index;
    std::unordered_map<uint64_t, ProcessorInfo*>        m_processor_index;
    std::unordered_map<uint64_t, ProcessInfo*>          m_process_index;
    std::unordered_map<uint64_t, CounterInfo*>          m_counter_index;
    std::unordered_map<uint64_t, ThreadInfo*>           m_instrumented_thread_index;
    std::unordered_map<uint64_t, ThreadInfo*>           m_sampled_thread_index;
    std::map<std::pair<uint64_t, uint64_t>, QueueInfo*> m_queue_index;
    std::unordered_map<uint64_t, TopologyNode*>         m_track_index;
};

}  // namespace View
}  // namespace RocProfVis
