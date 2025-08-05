// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_view_structs.h"
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
        bool        expand;
    };
    std::vector<std::vector<Cell>> cells;
};
struct QueueModel
{
    const queue_info_t* info;
    InfoTable           info_table;
    uint64_t            graph_index;
};
struct StreamModel
{
    const stream_info_t* info;
    InfoTable           info_table;
    uint64_t            graph_index;
};
struct ThreadModel
{
    const thread_info_t* info;
    InfoTable            info_table;
    uint64_t             graph_index;
};
struct CounterModel
{
    const counter_info_t* info;
    InfoTable             info_table;
    uint64_t              graph_index;
};
struct ProcessModel
{
    const process_info_t*                       info;
    InfoTable                                   info_table;
    std::string                                 header;
    std::string                                 queue_header;
    std::vector<QueueModel>                     queues;
    std::unordered_map<uint64_t, QueueModel*>   queue_lut;
    std::string                                 stream_header;
    std::vector<StreamModel>                     streams;
    std::unordered_map<uint64_t, StreamModel*>   stream_lut;
    std::string                                 thread_header;
    std::vector<ThreadModel>                    threads;
    std::unordered_map<uint64_t, ThreadModel*>  thread_lut;
    std::string                                 counter_header;
    std::vector<CounterModel>                   counters;
    std::unordered_map<uint64_t, CounterModel*> counter_lut;
};
struct NodeModel
{
    const node_info_t*                          info;
    InfoTable                                   info_table;
    std::string                                 process_header;
    std::vector<ProcessModel>                   processes;
    std::unordered_map<uint64_t, ProcessModel*> process_lut;
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

    DataProvider&                   m_data_provider;
    bool                            m_topology_dirty;
    bool                            m_graphs_dirty;
    EventManager::SubscriptionToken m_metadata_changed_event_token;

    TopologyModel m_topology;
};

}  // namespace View
}  // namespace RocProfVis