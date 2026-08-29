// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_track_topology.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_settings_manager.h"

#include <unordered_set>

namespace RocProfVis
{
namespace View
{

namespace
{

// The lut entries point into the model vector, so they can only be taken once
// that vector has stopped growing.
void
BuildLut(std::vector<IterableModel>&                   models,
         std::unordered_map<uint64_t, IterableModel*>& lut)
{
    lut.clear();
    for(IterableModel& model : models)
    {
        lut[model.info->GetId()] = &model;
    }
}

TreeNode*
AddBranchNode(TreeNode* parent, NodeType type, const std::string& label,
              bool collapsable = true, bool show_eye_button = true,
              bool framed = false)
{
    (void) framed;
    auto node             = std::make_unique<TreeNode>(type, label, collapsable);
    node->show_eye_button = show_eye_button;
    return parent->AddChild(std::move(node));
}

LeafNode*
AddLeafNode(TreeNode* parent, const std::vector<const TrackInfo*>& track_list,
            uint64_t graph_index, const std::string& fallback_label,
            bool render_children_inline = false)
{
    uint64_t    track_id = graph_index;
    std::string label    = fallback_label;

    if(graph_index < track_list.size() && track_list[graph_index])
    {
        track_id = track_list[graph_index]->id;
        if(label.empty())
        {
            label = track_list[graph_index]->main_name;
        }
    }

    auto leaf                    = std::make_unique<LeafNode>(label, graph_index, track_id);
    leaf->render_children_inline = render_children_inline;
    LeafNode* raw = leaf.get();
    parent->AddChild(std::move(leaf));
    return raw;
}

template<typename Model>
void
BuildLeafList(TreeNode* parent, NodeType type, const std::string& label,
              const std::vector<Model>& items,
              const std::vector<const TrackInfo*>& track_list,
              bool show_list_header = true)
{
    if(items.empty())
    {
        return;
    }

    TreeNode* target = parent;
    if(show_list_header)
    {
        target = AddBranchNode(parent, type, label, true, true, true);
    }
    for(const auto& item : items)
    {
        if(item.info)
        {
            AddLeafNode(target, track_list, item.graph_index, item.info->GetName());
        }
    }
}

/*
 * Builds a processor subtree.  When the processor appears inline beneath a
 * stream leaf, show_controls is false: no eye buttons and no intermediate
 * list headers are rendered, and breaks_visibility_chain isolates the
 * subtree from ancestor bulk-visibility toggles.
 */
void
BuildProcessorTree(TreeNode* parent, const ProcessorModel& processor,
                   const std::vector<const TrackInfo*>& track_list,
                   bool show_controls, bool breaks_chain = false)
{
    if(!processor.info)
    {
        return;
    }

    TreeNode* node = AddBranchNode(parent, NodeType::kProcessor,
                                   processor.header, true, show_controls, false);
    node->breaks_visibility_chain = breaks_chain;
    node->show_lead_arrow         = !show_controls;
    BuildLeafList(node, NodeType::kQueueList, processor.queue_header,
                  processor.queues, track_list, show_controls);
    BuildLeafList(node, NodeType::kCounterList, processor.counter_header,
                  processor.counters, track_list, show_controls);
}

void
BuildProcessTree(TreeNode* parent, const ProcessModel& process,
                 const std::vector<const TrackInfo*>& track_list)
{
    if(!process.info)
    {
        return;
    }

    TreeNode* process_node = AddBranchNode(parent, NodeType::kProcess,
                                           process.header, true, true, false);

    if(!process.streams.empty())
    {
        TreeNode* stream_list = AddBranchNode(process_node, NodeType::kStreamList,
                                              process.stream_header, true, true, true);
        for(const auto& stream : process.streams)
        {
            if(!stream.info)
            {
                continue;
            }

            bool      has_processors = !stream.processors.empty();
            LeafNode* stream_leaf =
                AddLeafNode(stream_list, track_list, stream.graph_index,
                            stream.info->GetName(), has_processors);
            for(const auto& processor : stream.processors)
            {
                BuildProcessorTree(stream_leaf, processor, track_list,
                                   false, true);
            }
        }
    }

    BuildLeafList(process_node, NodeType::kInstrumentedThreadList,
                  process.instrumented_thread_header,
                  process.instrumented_threads, track_list);
    BuildLeafList(process_node, NodeType::kSampledThreadList,
                  process.sampled_thread_header,
                  process.sampled_threads, track_list);
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
, m_topology_dirty(true)
, m_graphs_dirty(true)
, m_metadata_changed_event_token(EventManager::InvalidSubscriptionToken)
, m_format_changed_token(EventManager::InvalidSubscriptionToken)
{
    auto metadata_changed_event_handler = [this](std::shared_ptr<RocEvent> event) {
        if(event) {
            if(m_data_provider.GetTraceFilePath() == event->GetSourceId()) {
                m_topology_dirty = true;
                m_graphs_dirty = true;
            }
        }
    };
    m_metadata_changed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        metadata_changed_event_handler);

    //subscribe to time format changed event
    auto format_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        (void) e;
        // Reformat time columns
        FormatCells();
    };

    m_format_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), format_changed_handler);    
}

TrackTopology::~TrackTopology()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        m_metadata_changed_event_token);

    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), m_format_changed_token);
}

void
TrackTopology::Update()
{
    UpdateTopology();
    UpdateGraphs();
}

bool
TrackTopology::Dirty()
{
    return m_topology_dirty || m_graphs_dirty;
}

const TopologyModel&
TrackTopology::GetTopology() const
{
    return m_topology;
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
TrackTopology::UpdateTopology()
{
    if(m_topology_dirty && m_data_provider.GetState() == ProviderState::kReady)
    {
        const TopologyTree& tree = m_data_provider.DataModel().GetTopology();

        m_topology.nodes.clear();
        m_topology.node_lut.clear();

        const std::vector<TopologyNode*>& nodes = tree.GetNodes();
        m_topology.nodes.resize(nodes.size());
        m_topology.node_header = "Nodes (" + std::to_string(nodes.size()) + ")";

        for(size_t i = 0; i < nodes.size(); i++)
        {
            const NodeInfo& node_info  = static_cast<const NodeInfo&>(*nodes[i]);
            NodeModel&      node_model = m_topology.nodes[i];

            m_topology.node_lut[node_info.GetId()] = &node_model;
            node_model.info                        = &node_info;
            node_model.info_table                  = InfoTable{
                { { InfoTable::Cell{ "OS", false },
                    InfoTable::Cell{ node_info.os_name, false } },
                  { InfoTable::Cell{ "OS Release", false },
                    InfoTable::Cell{ node_info.os_release, false } },
                  { InfoTable::Cell{ "OS Version", false },
                    InfoTable::Cell{ node_info.os_version, false } } }
            };

            BuildProcessorModels(node_model, node_info);
            BuildProcessModels(node_model, node_info);
        }
        FormatCells();
        m_topology_dirty = false;
        EventManager::GetInstance()->AddEvent(
            std::make_shared<RocEvent>(static_cast<int>(RocEvents::kTopologyChanged),
                                       m_data_provider.GetTraceFilePath()));
    }
}

void
TrackTopology::BuildProcessorModels(NodeModel& node_model, const NodeInfo& node_info)
{
    const std::vector<TopologyNode*>& processors =
        node_info.GetChildren(TopologyNodeType::kProcessor);

    node_model.processors.resize(processors.size());
    node_model.processor_header = "Processors (" + std::to_string(processors.size()) + ")";

    for(size_t i = 0; i < processors.size(); i++)
    {
        const ProcessorInfo& processor_info =
            static_cast<const ProcessorInfo&>(*processors[i]);
        ProcessorModel& processor_model = node_model.processors[i];

        node_model.processor_lut[processor_info.GetId()] = &processor_model;
        processor_model.info                             = &processor_info;
        processor_model.info_table                       = MakeProcessorTable(processor_info);
        processor_model.header =
            "[" + std::to_string(processor_info.GetTopologyId().fields.id) + "] " +
            DeviceTypeString(processor_info.type) +
            std::to_string(processor_info.type_index) + ": " +
            processor_info.product_name;

        const std::vector<TopologyNode*>& queues =
            processor_info.GetChildren(TopologyNodeType::kQueue);
        processor_model.queues.resize(queues.size());
        processor_model.queue_header = "Queues (" + std::to_string(queues.size()) + ")";
        for(size_t j = 0; j < queues.size(); j++)
        {
            processor_model.queue_lut[queues[j]->GetId()] = &processor_model.queues[j];
            processor_model.queues[j].info                = queues[j];
            processor_model.queues[j].info_table          = MakeQueueTable(processor_info);
        }

        const std::vector<TopologyNode*>& counters =
            processor_info.GetChildren(TopologyNodeType::kCounter);
        processor_model.counters.resize(counters.size());
        processor_model.counter_header =
            "Counters (" + std::to_string(counters.size()) + ")";
        for(size_t j = 0; j < counters.size(); j++)
        {
            const CounterInfo& counter_info =
                static_cast<const CounterInfo&>(*counters[j]);
            processor_model.counter_lut[counter_info.GetId()] =
                &processor_model.counters[j];
            processor_model.counters[j].info       = &counter_info;
            processor_model.counters[j].info_table = InfoTable{
                { { InfoTable::Cell{ "Description", false },
                    InfoTable::Cell{ counter_info.description, false } },
                  { InfoTable::Cell{ "Value Type", false },
                    InfoTable::Cell{ counter_info.value_type, false } } }
            };
        }
    }
}

void
TrackTopology::BuildProcessModels(NodeModel& node_model, const NodeInfo& node_info)
{
    const std::vector<TopologyNode*>& processes =
        node_info.GetChildren(TopologyNodeType::kProcess);

    node_model.processes.resize(processes.size());
    node_model.process_header = "Processes (" + std::to_string(processes.size()) + ")";

    for(size_t i = 0; i < processes.size(); i++)
    {
        const ProcessInfo& process_info = static_cast<const ProcessInfo&>(*processes[i]);
        ProcessModel&      process_model = node_model.processes[i];

        node_model.process_lut[process_info.GetId()] = &process_model;
        process_model.info                           = &process_info;
        process_model.info_table                     = InfoTable{
            { { InfoTable::Cell{ "Start Time", false },
                MakeTimeCell(process_info.start_time) },
              { InfoTable::Cell{ "End Time", false },
                MakeTimeCell(process_info.end_time) },
              { InfoTable::Cell{ "Command", false },
                InfoTable::Cell{ process_info.command, false } },
              { InfoTable::Cell{ "Environment", false },
                InfoTable::Cell{ process_info.environment, false } } }
        };
        process_model.header =
            process_info.command + " (" + std::to_string(process_info.GetId()) + ")";

        BuildStreamModels(process_model, process_info);
        BuildThreadModels(process_model, process_info);
    }
}

void
TrackTopology::BuildStreamModels(ProcessModel&      process_model,
                                 const ProcessInfo& process_info)
{
    const std::vector<TopologyNode*>& streams =
        process_info.GetChildren(TopologyNodeType::kStream);

    process_model.streams.resize(streams.size());
    process_model.stream_header = "Streams (" + std::to_string(streams.size()) + ")";

    const TimelineModel& timeline = m_data_provider.DataModel().GetTimeline();

    for(size_t i = 0; i < streams.size(); i++)
    {
        const StreamInfo& stream_info  = static_cast<const StreamInfo&>(*streams[i]);
        StreamModel&      stream_model = process_model.streams[i];

        process_model.stream_lut[stream_info.GetId()] = &stream_model;
        stream_model.info                             = &stream_info;

        // A stream shows the processors it dispatches to, and under each one
        // only the queues that stream actually used.
        const std::vector<TopologyNode*>& stream_processors =
            stream_info.GetLinkedChildren(TopologyNodeType::kProcessor);
        const std::vector<TopologyNode*>& stream_queues =
            stream_info.GetLinkedChildren(TopologyNodeType::kQueue);

        stream_model.processors.resize(stream_processors.size());
        for(size_t j = 0; j < stream_processors.size(); j++)
        {
            const ProcessorInfo& processor_info =
                static_cast<const ProcessorInfo&>(*stream_processors[j]);
            ProcessorModel& processor_model = stream_model.processors[j];

            stream_model.processor_lut[processor_info.GetId()] = &processor_model;
            processor_model.info                               = &processor_info;
            processor_model.info_table = MakeProcessorTable(processor_info);
            processor_model.header     = DeviceTypeString(processor_info.type) +
                                     std::to_string(processor_info.type_index);

            for(TopologyNode* queue : stream_queues)
            {
                if(queue->GetParent() != &processor_info)
                {
                    continue;
                }
                processor_model.queues.push_back(IterableModel{});
                IterableModel& queue_model = processor_model.queues.back();
                queue_model.info           = queue;
                queue_model.info_table     = MakeQueueTable(processor_info);

                const TrackInfo* track = timeline.GetTrack(queue->GetTrackId());
                if(track)
                {
                    queue_model.graph_index = track->index;
                }
            }
            BuildLut(processor_model.queues, processor_model.queue_lut);
        }
    }
}

void
TrackTopology::BuildThreadModels(ProcessModel&      process_model,
                                 const ProcessInfo& process_info)
{
    const std::vector<TopologyNode*>& threads =
        process_info.GetChildren(TopologyNodeType::kThread);

    for(TopologyNode* node : threads)
    {
        const ThreadInfo& thread_info = static_cast<const ThreadInfo&>(*node);
        const bool        instrumented =
            thread_info.kind == ThreadInfo::Kind::kInstrumented;

        std::vector<IterableModel>& models = instrumented
                                                 ? process_model.instrumented_threads
                                                 : process_model.sampled_threads;

        models.push_back(IterableModel{});
        IterableModel& thread_model = models.back();
        thread_model.info           = &thread_info;
        thread_model.info_table     = InfoTable{
            { { InfoTable::Cell{ "Start Time", false },
                MakeTimeCell(thread_info.start_time) },
              { InfoTable::Cell{ "End Time", false },
                MakeTimeCell(thread_info.end_time) } }
        };
    }

    BuildLut(process_model.instrumented_threads, process_model.instrumented_thread_lut);
    BuildLut(process_model.sampled_threads, process_model.sampled_thread_lut);

    process_model.instrumented_thread_header =
        "Threads (" + std::to_string(process_model.instrumented_threads.size()) + ")";
    process_model.sampled_thread_header =
        "Sampled Threads (" + std::to_string(process_model.sampled_threads.size()) + ")";
}

InfoTable
TrackTopology::MakeProcessorTable(const ProcessorInfo& processor_info) const
{
    return InfoTable{
        { { InfoTable::Cell{ "Processor type", false },
            InfoTable::Cell{ DeviceTypeString(processor_info.type), false } },
          { InfoTable::Cell{ "Processor index", false },
            InfoTable::Cell{ std::to_string(processor_info.type_index), false } },
          { InfoTable::Cell{ "Product name", false },
            InfoTable::Cell{ processor_info.product_name, false } } }
    };
}

InfoTable
TrackTopology::MakeQueueTable(const ProcessorInfo& processor_info) const
{
    return InfoTable{
        { { InfoTable::Cell{ DeviceTypeString(processor_info.type) + " " +
                                 std::to_string(processor_info.type_index),
                             false },
            InfoTable::Cell{ processor_info.product_name, false } } }
    };
}

InfoTable::Cell
TrackTopology::MakeTimeCell(double timestamp)
{
    return InfoTable::Cell{
        std::to_string(timestamp), false, true,
        [this](const std::string& raw, std::string& formatted_out) {
            return FormatTimeCell(raw, formatted_out);
        }
    };
}

void
TrackTopology::FormatCells()
{
    for(auto& node : m_topology.nodes)
    {
        for (auto& processor : node.processors)
        {
            for(auto& q : processor.queues)
            {
                for(auto& row : q.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
            for(auto& c : processor.counters)
            {
                for(auto& row : c.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
        }
        for(auto& process : node.processes)
        {
            // Format process table
            for(auto& row : process.info_table.cells)
            {
                for(auto& cell : row)
                {
                    if(cell.needs_format && cell.formatter)
                    {
                        cell.formatter(cell.data, cell.formatted);
                    }
                }
            }
            // Format child tables
            for(auto& t : process.instrumented_threads)
            {
                for(auto& row : t.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
            for(auto& t : process.sampled_threads)
            {
                for(auto& row : t.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
            for(auto& s : process.streams)
            {
                for(auto& row : s.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
        }
    }
}

bool
TrackTopology::FormatTimeCell(const std::string& raw, std::string& formatted_out)
{
    SettingsManager& settings    = SettingsManager::GetInstance();
    auto             time_format = settings.GetUserSettings().unit_settings.time_format;

    formatted_out = nanosecond_to_formatted_str(std::stod(raw), time_format, true);
    return true;
}

std::string
TrackTopology::DeviceTypeString(
    const rocprofvis_controller_processor_type_t& device_type) const
{
    switch(device_type)
    {
        case kRPVControllerProcessorTypeGPU:
        {
            return "GPU";
        }
        case kRPVControllerProcessorTypeCPU:
        {
            return "CPU";
        }
        case kRPVControllerProcessorTypeNIC:
        {
            return "NIC";
        }
        default:
        {
            return "Undefined";
        }
    }
}

void
TrackTopology::UpdateGraphs()
{
    if(m_graphs_dirty && m_data_provider.GetState() == ProviderState::kReady)
    {
        m_topology.uncategorized_graph_indices.clear();
        const auto& track_list = m_data_provider.DataModel().GetTimeline().GetTrackList();
        for(const TrackInfo* track : track_list)
        {
            if(track)
            {
                const uint64_t& node_id      = track->topology.node_id;
                const uint64_t& process_id   = track->topology.process_id;
                const uint64_t& processor_id = track->topology.device_id;
                const uint64_t& id           = track->topology.id.value;
                const uint64_t& index        = track->index;
                switch(track->topology.type)
                {
                    case TrackInfo::TrackType::Queue:
                    {
                        m_topology.node_lut[node_id]
                            ->processor_lut[processor_id]
                            ->queue_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    case TrackInfo::TrackType::Stream:
                    {
                        m_topology.node_lut[node_id]
                            ->process_lut[process_id]
                            ->stream_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    case TrackInfo::TrackType::InstrumentedThread:
                    {
                        m_topology.node_lut[node_id]
                            ->process_lut[process_id]
                            ->instrumented_thread_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    case TrackInfo::TrackType::SampledThread:
                    {
                        m_topology.node_lut[node_id]
                            ->process_lut[process_id]
                            ->sampled_thread_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    case TrackInfo::TrackType::Counter:
                    {
                        m_topology.node_lut[node_id]
                            ->processor_lut[processor_id]
                            ->counter_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    default:
                    {
                        m_topology.uncategorized_graph_indices.push_back(index);
                        break;
                    }
                }
            }
        }
        BuildSidebarTree();
        m_graphs_dirty = false;
    }
}

void
TrackTopology::BuildSidebarTree()
{
    m_sidebar_tree = {};

    auto root = std::make_unique<TreeNode>(NodeType::kRoot, "Project", true);
    root->show_eye_button = false;

    TreeNode* root_node = root.get();
    const auto& track_list = m_data_provider.DataModel().GetTimeline().GetTrackList();

    if(!m_topology.nodes.empty())
    {
        TreeNode* node_list = AddBranchNode(root_node, NodeType::kNodeList,
            m_topology.node_header, true, true, false);
        for(const auto& node : m_topology.nodes)
        {
            if(!node.info)
            {
                continue;
            }

            TopologyTree& tdm        = m_data_provider.DataModel().GetTopology();
            const bool    multi_node = tdm.NodeCount() > 1;
            const size_t  node_index = tdm.GetNodeDisplayIndex(node.info->GetId());
            const std::string  node_label =
                (multi_node && node_index > 0)
                    ? "[" + std::to_string(node_index) + "] " + node.info->host_name
                    : node.info->host_name;
            TreeNode* node_branch =
                AddBranchNode(node_list, NodeType::kNode, node_label, true, true, false);
            if(multi_node && node_index > 0)
            {
                const size_t wheel_size =
                    SettingsManager::GetInstance().GetColorWheel().size();
                node_branch->show_color_swatch = true;
                node_branch->color_index = wheel_size ? (node_index - 1) % wheel_size : 0;
            }

            if(!node.processors.empty())
            {
                TreeNode* processor_list = AddBranchNode(node_branch,
                    NodeType::kProcessorList, node.processor_header, true, true, false);
                for(const auto& processor : node.processors)
                {
                    BuildProcessorTree(processor_list, processor, track_list, true);
                }
            }

            if(!node.processes.empty())
            {
                TreeNode* process_list = AddBranchNode(node_branch, NodeType::kProcessList,
                    node.process_header, true, true, false);
                for(const auto& process : node.processes)
                {
                    BuildProcessTree(process_list, process, track_list);
                }
            }
        }
    }

    if(!m_topology.uncategorized_graph_indices.empty())
    {
        TreeNode* uncategorized = AddBranchNode(root_node, NodeType::kUncategorizedList,
            "Uncategorized", !m_topology.nodes.empty(), false, false);
        for(const auto& graph_index : m_topology.uncategorized_graph_indices)
        {
            AddLeafNode(uncategorized, track_list, graph_index, "");
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
