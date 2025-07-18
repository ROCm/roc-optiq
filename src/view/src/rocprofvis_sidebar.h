// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_view_structs.h"
#include "widgets/rocprofvis_widget.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

class SideBar : public RocWidget
{
public:
    SideBar(DataProvider& dp, std::vector<rocprofvis_graph_t>* graphs);
    ~SideBar();
    virtual void Render();
    void         Update();

private:
    struct InfoTable
    {
        struct Cell
        {
            std::string data;
            bool        expand;
        };
        std::vector<std::vector<Cell>> cells;
    };
    struct Queue
    {
        const queue_info_t* info;
        InfoTable           info_table;
        uint64_t            graph_index;
    };
    struct Thread
    {
        const thread_info_t* info;
        InfoTable            info_table;
        uint64_t             graph_index;
    };
    struct Counter
    {
        const counter_info_t* info;
        InfoTable             info_table;
        uint64_t              graph_index;
    };
    struct Process
    {
        const process_info_t*                  info;
        InfoTable                              info_table;
        std::string                            header;
        std::string                            queue_header;
        std::vector<Queue>                     queues;
        std::unordered_map<uint64_t, Queue*>   queue_lut;
        std::string                            thread_header;
        std::vector<Thread>                    threads;
        std::unordered_map<uint64_t, Thread*>  thread_lut;
        std::string                            counter_header;
        std::vector<Counter>                   counters;
        std::unordered_map<uint64_t, Counter*> counter_lut;
    };
    struct Node
    {
        const node_info_t*                     info;
        InfoTable                              info_table;
        std::string                            process_header;
        std::vector<Process>                   processes;
        std::unordered_map<uint64_t, Process*> process_lut;
    };
    struct Project
    {
        std::string                         header;
        std::string                         node_header;
        std::vector<Node>                   nodes;
        std::unordered_map<uint64_t, Node*> node_lut;
        std::vector<uint64_t>               uncategorized_graph_indices;
    };

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

    void RenderTable(InfoTable& table);
    void RenderTrackItem(const int& index);

    std::vector<rocprofvis_graph_t>* m_graphs;

    Settings&                       m_settings;
    DataProvider&                   m_data_provider;
    Project                         m_project;
    bool                            m_topology_dirty;
    bool                            m_graphs_dirty;
    EventManager::SubscriptionToken m_metadata_changed_event_token;
};

}  // namespace View
}  // namespace RocProfVis