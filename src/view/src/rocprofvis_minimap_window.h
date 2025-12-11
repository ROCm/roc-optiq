// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_track_topology.h"
#include "imgui.h"
#include <vector>
#include <map>
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

struct TopologyNode
{
    ImVec2 position;
    float  radius;
    double activity_value;  // Normalized activity from minimap data
    uint64_t node_id;
    std::vector<ImVec2> process_positions;  // Positions of processes within this node
    std::vector<uint64_t> process_ids;
    std::vector<double> process_activities;
};

class MinimapWindow
{
public:
    MinimapWindow(DataProvider& data_provider, std::shared_ptr<TrackTopology> topology);
    ~MinimapWindow();

    void Render();
    void Toggle();
    bool IsVisible() const { return m_visible; }

private:
    void UpdateTopologyLayout();
    void CalculateNodePositions();
    void CalculateProcessPositions();
    double GetNodeActivity(uint64_t node_id);
    double GetProcessActivity(uint64_t process_id);
    ImU32 GetActivityColor(double normalized_value);
    void DrawTopologyMap(ImDrawList* draw_list, ImVec2 canvas_pos, ImVec2 canvas_size);

    DataProvider& m_data_provider;
    std::shared_ptr<TrackTopology> m_topology;
    bool m_visible;
    
    // Topology layout data
    std::vector<TopologyNode> m_topology_nodes;
    std::unordered_map<uint64_t, double> m_track_activities;  // track_id -> normalized activity
    
    // Activity value range
    double m_min_activity;
    double m_max_activity;
    
    static constexpr float NODE_RADIUS = 40.0f;
    static constexpr float PROCESS_RADIUS = 12.0f;
    static constexpr float NODE_SPACING = 150.0f;
    static constexpr float PROCESS_SPACING = 30.0f;
};

}  // namespace View
}  // namespace RocProfVis

