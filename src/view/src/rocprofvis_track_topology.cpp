// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_track_topology.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"

#include <unordered_set>

namespace RocProfVis
{
namespace View
{

namespace
{

TreeNode*
AddBranchNode(TreeNode* parent, NodeType type, const std::string& label,
              bool collapsable = true, bool show_eye_button = true)
{
    auto node             = std::make_unique<TreeNode>(type, label, collapsable);
    node->show_eye_button = show_eye_button;
    return parent->AddChild(std::move(node));
}

/*
 * Adds a row for one track. The topology name wins when it has one; tracks
 * that only exist on the timeline (the uncategorized list) fall back to the
 * name the timeline gave them.
 */
LeafNode*
AddLeafNode(TreeNode* parent, uint64_t track_id, const std::string& topology_label,
            const TimelineModel& timeline, bool render_children_inline = false)
{
    std::string label = topology_label;
    if(label.empty())
    {
        const TrackInfo* track = timeline.GetTrack(track_id);
        if(track)
        {
            label = track->main_name;
        }
    }

    auto leaf                    = std::make_unique<LeafNode>(label, track_id);
    leaf->render_children_inline = render_children_inline;
    LeafNode* raw                = leaf.get();
    parent->AddChild(std::move(leaf));
    return raw;
}

void
BuildLeafList(TreeNode* parent, NodeType type, const std::string& label,
              const std::vector<TopologyNode*>& items, const TimelineModel& timeline,
              bool show_list_header)
{
    if(items.empty())
    {
        return;
    }

    TreeNode* target = parent;
    if(show_list_header)
    {
        target = AddBranchNode(parent, type, label);
    }
    for(const TopologyNode* item : items)
    {
        AddLeafNode(target, item->GetTrackId(), item->GetName(), timeline);
    }
}

/*
 * Builds a processor subtree. Queues are passed in rather than read from the
 * processor because a processor shown under a stream lists only the queues that
 * stream dispatched to.
 *
 * In that inline position show_controls is false: the row becomes a plain
 * "GPU0" lead-in with no eye button and no intermediate list headers, and
 * breaks_visibility_chain isolates it from ancestor bulk-visibility toggles.
 */
void
BuildProcessorBranch(TreeNode* parent, const ProcessorInfo& processor,
                     const std::vector<TopologyNode*>& queues,
                     const std::vector<TopologyNode*>& counters,
                     const TimelineModel& timeline, bool show_controls,
                     bool breaks_chain = false)
{
    const std::string label =
        show_controls ? processor.GetHeader()
                      : TopologyTree::GetProcessorTypeName(processor.type) +
                            std::to_string(processor.type_index);

    TreeNode* node =
        AddBranchNode(parent, NodeType::kProcessor, label, true, show_controls);
    node->breaks_visibility_chain = breaks_chain;
    node->show_lead_arrow         = !show_controls;

    BuildLeafList(node, NodeType::kQueueList,
                  "Queues (" + std::to_string(queues.size()) + ")", queues, timeline,
                  show_controls);
    BuildLeafList(node, NodeType::kCounterList,
                  "Counters (" + std::to_string(counters.size()) + ")", counters,
                  timeline, show_controls);
}

void
BuildProcessBranch(TreeNode* parent, const ProcessInfo& process,
                   const TimelineModel& timeline)
{
    TreeNode* process_node =
        AddBranchNode(parent, NodeType::kProcess, process.GetHeader());

    const std::vector<TopologyNode*>& streams =
        process.GetChildren(TopologyNodeType::kStream);
    if(!streams.empty())
    {
        TreeNode* stream_list =
            AddBranchNode(process_node, NodeType::kStreamList,
                          "Streams (" + std::to_string(streams.size()) + ")");
        for(const TopologyNode* stream : streams)
        {
            const std::vector<TopologyNode*>& stream_processors =
                stream->GetLinkedChildren(TopologyNodeType::kProcessor);
            const std::vector<TopologyNode*>& stream_queues =
                stream->GetLinkedChildren(TopologyNodeType::kQueue);

            LeafNode* stream_leaf =
                AddLeafNode(stream_list, stream->GetTrackId(), stream->GetName(),
                            timeline, !stream_processors.empty());

            for(const TopologyNode* node : stream_processors)
            {
                const ProcessorInfo& processor =
                    static_cast<const ProcessorInfo&>(*node);
                std::vector<TopologyNode*> queues;
                for(TopologyNode* queue : stream_queues)
                {
                    if(queue->GetParent() == &processor)
                    {
                        queues.push_back(queue);
                    }
                }
                BuildProcessorBranch(stream_leaf, processor, queues, {}, timeline,
                                     false, true);
            }
        }
    }

    std::vector<TopologyNode*> instrumented;
    std::vector<TopologyNode*> sampled;
    for(TopologyNode* node : process.GetChildren(TopologyNodeType::kThread))
    {
        const ThreadInfo& thread = static_cast<const ThreadInfo&>(*node);
        if(thread.kind == ThreadInfo::Kind::kInstrumented)
        {
            instrumented.push_back(node);
        }
        else
        {
            sampled.push_back(node);
        }
    }

    BuildLeafList(process_node, NodeType::kInstrumentedThreadList,
                  "Threads (" + std::to_string(instrumented.size()) + ")", instrumented,
                  timeline, true);
    BuildLeafList(process_node, NodeType::kSampledThreadList,
                  "Sampled Threads (" + std::to_string(sampled.size()) + ")", sampled,
                  timeline, true);
}

// Flattens leaf track ids in tree order, deduping repeats (a track can appear more
// than once, e.g. a device's queues under both the processor list and a stream).
void
CollectLeafTrackIds(const TreeNode* node, std::vector<uint64_t>& out,
                    std::unordered_set<uint64_t>& seen)
{
    if(!node)
    {
        return;
    }
    if(node->IsLeaf())
    {
        const uint64_t track_id = static_cast<const LeafNode*>(node)->track_id;
        if(seen.insert(track_id).second)
        {
            out.push_back(track_id);
        }
    }
    for(const auto& child : node->children)
    {
        CollectLeafTrackIds(child.get(), out, seen);
    }
}

}  // namespace

TrackTopology::TrackTopology(DataProvider& dp)
: m_data_provider(dp)
, m_dirty(true)
, m_metadata_changed_event_token(EventManager::InvalidSubscriptionToken)
{
    // Track names and the set of timeline-only tracks both come from metadata,
    // so the projection is rebuilt when it changes. The topology tree itself is
    // untouched, which is what keeps a reorder from feeding back into the
    // topology order the timeline sorts by.
    auto metadata_changed_event_handler = [this](std::shared_ptr<RocEvent> event) {
        if(event) {
            if(m_data_provider.GetTraceFilePath() == event->GetSourceId()) {
                m_dirty = true;
            }
        }
    };
    m_metadata_changed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        metadata_changed_event_handler);
}

TrackTopology::~TrackTopology()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        m_metadata_changed_event_token);
}

void
TrackTopology::Update()
{
    if(m_dirty && m_data_provider.GetState() == ProviderState::kReady)
    {
        BuildSidebarTree();
        m_dirty = false;
        EventManager::GetInstance()->AddEvent(
            std::make_shared<RocEvent>(static_cast<int>(RocEvents::kTopologyChanged),
                                       m_data_provider.GetTraceFilePath()));
    }
}

bool
TrackTopology::Dirty()
{
    return m_dirty;
}

const SidebarTree&
TrackTopology::GetSidebarTree() const
{
    return m_sidebar_tree;
}

const std::vector<uint64_t>&
TrackTopology::GetTrackIdsInTreeOrder() const
{
    return m_track_order;
}

void
TrackTopology::BuildSidebarTree()
{
    m_sidebar_tree = {};

    auto root             = std::make_unique<TreeNode>(NodeType::kRoot, "Project", true);
    root->show_eye_button = false;

    TreeNode*            root_node = root.get();
    const TopologyTree&  topology  = m_data_provider.DataModel().GetTopology();
    const TimelineModel& timeline  = m_data_provider.DataModel().GetTimeline();

    const std::vector<TopologyNode*>& nodes = topology.GetNodes();
    if(!nodes.empty())
    {
        TreeNode* node_list =
            AddBranchNode(root_node, NodeType::kNodeList,
                          "Nodes (" + std::to_string(nodes.size()) + ")");
        const bool multi_node = nodes.size() > 1;

        for(const TopologyNode* node : nodes)
        {
            const NodeInfo& node_info = static_cast<const NodeInfo&>(*node);
            const size_t    node_index = node_info.display_index;
            const std::string node_label =
                (multi_node && node_index > 0)
                    ? "[" + std::to_string(node_index) + "] " + node_info.host_name
                    : node_info.host_name;

            TreeNode* node_branch =
                AddBranchNode(node_list, NodeType::kNode, node_label);
            if(multi_node && node_index > 0)
            {
                const size_t wheel_size =
                    SettingsManager::GetInstance().GetColorWheel().size();
                node_branch->show_color_swatch = true;
                node_branch->color_index = wheel_size ? (node_index - 1) % wheel_size : 0;
            }

            const std::vector<TopologyNode*>& processors =
                node_info.GetChildren(TopologyNodeType::kProcessor);
            if(!processors.empty())
            {
                TreeNode* processor_list =
                    AddBranchNode(node_branch, NodeType::kProcessorList,
                                  "Processors (" + std::to_string(processors.size()) +
                                      ")");
                for(const TopologyNode* processor : processors)
                {
                    BuildProcessorBranch(
                        processor_list, static_cast<const ProcessorInfo&>(*processor),
                        processor->GetChildren(TopologyNodeType::kQueue),
                        processor->GetChildren(TopologyNodeType::kCounter), timeline,
                        true);
                }
            }

            const std::vector<TopologyNode*>& processes =
                node_info.GetChildren(TopologyNodeType::kProcess);
            if(!processes.empty())
            {
                TreeNode* process_list =
                    AddBranchNode(node_branch, NodeType::kProcessList,
                                  "Processes (" + std::to_string(processes.size()) + ")");
                for(const TopologyNode* process : processes)
                {
                    BuildProcessBranch(process_list,
                                       static_cast<const ProcessInfo&>(*process),
                                       timeline);
                }
            }
        }
    }

    // Tracks the controller never tied to a topology node still need a home.
    std::vector<const TrackInfo*> uncategorized;
    for(const TrackInfo* track : timeline.GetTrackList())
    {
        if(track && track->topology.type == TrackInfo::TrackType::Unknown)
        {
            uncategorized.push_back(track);
        }
    }
    if(!uncategorized.empty())
    {
        TreeNode* uncategorized_list =
            AddBranchNode(root_node, NodeType::kUncategorizedList, "Uncategorized",
                          !nodes.empty(), false);
        for(const TrackInfo* track : uncategorized)
        {
            AddLeafNode(uncategorized_list, track->id, "", timeline);
        }
    }

    m_sidebar_tree.root = std::move(root);

    // Cache the flattened, deduped leaf order so "Sort by topology" is an O(1)
    // lookup of a clean permutation rather than a fresh tree walk each time.
    m_track_order.clear();
    std::unordered_set<uint64_t> seen;
    CollectLeafTrackIds(m_sidebar_tree.root.get(), m_track_order, seen);
}

}  // namespace View
}  // namespace RocProfVis
