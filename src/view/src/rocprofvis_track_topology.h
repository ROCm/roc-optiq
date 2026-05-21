// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_tree_node.h"
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace View
{

struct InfoTable
{
    struct Cell
    {
        std::string data;
        bool        expand = false;
        bool        needs_format = false;
        std::function<bool(const std::string& raw, std::string& formatted_out)> formatter{};
        std::string formatted{};
    };
    std::vector<std::vector<Cell>> cells;
};

struct IterableModel
{
    const IterableInfo*    info;
    InfoTable              info_table;
    uint64_t               graph_index;
};

struct ProcessorModel;

struct StreamModel
{
    const StreamInfo*                           info;
    InfoTable                                   info_table;
    std::string                                 header;
    std::string                                 processor_header;
    std::vector<ProcessorModel>                   processors;
    std::unordered_map<uint64_t, ProcessorModel*> processor_lut;
    uint64_t                                    graph_index;
};

struct ProcessModel
{
    const ProcessInfo*                           info;
    InfoTable                                    info_table;
    std::string                                  header;
    std::string                                  stream_header;
    std::vector<StreamModel>                     streams;
    std::unordered_map<uint64_t, StreamModel*>   stream_lut;
    std::string                                  instrumented_thread_header;
    std::vector<IterableModel>                   instrumented_threads;
    std::unordered_map<uint64_t, IterableModel*> instrumented_thread_lut;
    std::string                                  sampled_thread_header;
    std::vector<IterableModel>                   sampled_threads;
    std::unordered_map<uint64_t, IterableModel*> sampled_thread_lut;
};
struct ProcessorModel
{
    const DeviceInfo*                           info;
    InfoTable                                    info_table;
    std::string                                  header;
    std::string                                  queue_header;
    std::vector<IterableModel>                   queues;
    std::unordered_map<uint64_t, IterableModel*> queue_lut;
    std::string                                  counter_header;
    std::vector<IterableModel>                   counters;
    std::unordered_map<uint64_t, IterableModel*> counter_lut;
};
struct NodeModel
{
    const NodeInfo*                             info;
    InfoTable                                   info_table;
    std::string                                 process_header;
    std::vector<ProcessModel>                   processes;
    std::unordered_map<uint64_t, ProcessModel*> process_lut;
    std::string                                 processor_header;
    std::vector<ProcessorModel>                   processors;
    std::unordered_map<uint64_t, ProcessorModel*> processor_lut;
};
struct TopologyModel
{
    std::vector<NodeModel>                   nodes;
    std::string                              node_header;
    std::unordered_map<uint64_t, NodeModel*> node_lut;
    std::vector<uint64_t>                    uncategorized_graph_indices;
};

class TrackTopology
{
public:
    TrackTopology(DataProvider& dp);
    ~TrackTopology();
    void Update();

    bool                 Dirty();
    const TopologyModel& GetTopology() const;
    const SidebarTree&   GetSidebarTree() const;
    void FormatCells();

private:
    /*
     * Generates topology tree and builds UI strings.
     * Runs once on trace to load to avoid repeating operations on static data.
     */
    void UpdateTopology();
    /*
     * Binds tree nodes to a timeline view graph.
     * Runs on graph index change.
     */
    void UpdateGraphs();
    void BuildSidebarTree();

 
    bool FormatTimeCell(const std::string& raw, std::string& formatted_out);

    std::string DeviceTypeString(
        const rocprofvis_controller_processor_type_t& device_type) const;

    DataProvider&                   m_data_provider;
    bool                            m_topology_dirty;
    bool                            m_graphs_dirty;
    EventManager::SubscriptionToken m_metadata_changed_event_token;
    EventManager::SubscriptionToken m_format_changed_token;

    TopologyModel m_topology;
    SidebarTree   m_sidebar_tree;
};

}  // namespace View
}  // namespace RocProfVis