// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_minimap_window.h"
#include "rocprofvis_settings_manager.h"
#include "spdlog/spdlog.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace RocProfVis
{
namespace View
{

MinimapWindow::MinimapWindow(DataProvider& data_provider, std::shared_ptr<TrackTopology> topology)
: m_data_provider(data_provider)
, m_topology(topology)
, m_visible(false)
, m_min_activity(0.0)
, m_max_activity(1.0)
{
}

MinimapWindow::~MinimapWindow() {}

void
MinimapWindow::Toggle()
{
    m_visible = !m_visible;
    if(m_visible)
    {
        UpdateTopologyLayout();
    }
}

double
MinimapWindow::GetNodeActivity(uint64_t node_id)
{
    // Aggregate activity from all tracks belonging to this node
    double total_activity = 0.0;
    int    track_count    = 0;

    const auto& minimap_data = m_data_provider.GetMiniMap();
    const auto& track_list   = m_data_provider.GetTrackInfoList();

    for(const track_info_t* track_info : track_list)
    {
        if(track_info && track_info->topology.node_id == node_id)
        {
            auto it = minimap_data.find(track_info->id);
            if(it != minimap_data.end())
            {
                const auto& [data, is_visible] = it->second;
                if(is_visible && !data.empty())
                {
                    // Average the data values for this track
                    double track_avg = std::accumulate(data.begin(), data.end(), 0.0) /
                                       static_cast<double>(data.size());
                    total_activity += track_avg;
                    track_count++;
                }
            }
        }
    }

    return track_count > 0 ? total_activity / track_count : 0.0;
}

double
MinimapWindow::GetProcessActivity(uint64_t process_id)
{
    // Aggregate activity from all tracks belonging to this process
    double total_activity = 0.0;
    int    track_count    = 0;

    const auto& minimap_data = m_data_provider.GetMiniMap();
    const auto& track_list   = m_data_provider.GetTrackInfoList();

    for(const track_info_t* track_info : track_list)
    {
        if(track_info && track_info->topology.process_id == process_id)
        {
            auto it = minimap_data.find(track_info->id);
            if(it != minimap_data.end())
            {
                const auto& [data, is_visible] = it->second;
                if(is_visible && !data.empty())
                {
                    double track_avg = std::accumulate(data.begin(), data.end(), 0.0) /
                                       static_cast<double>(data.size());
                    total_activity += track_avg;
                    track_count++;
                }
            }
        }
    }

    return track_count > 0 ? total_activity / track_count : 0.0;
}

void
MinimapWindow::UpdateTopologyLayout()
{
    m_topology_nodes.clear();
    m_track_activities.clear();

    if(!m_topology || m_data_provider.GetState() != ProviderState::kReady)
    {
        return;
    }

    const auto& topology_model = m_topology->GetTopology();
    const auto& minimap_data    = m_data_provider.GetMiniMap();

    // Calculate activity for all tracks
    m_min_activity = std::numeric_limits<double>::max();
    m_max_activity = std::numeric_limits<double>::lowest();

    for(const auto& [track_id, track_tuple] : minimap_data)
    {
        const auto& [data, is_visible] = track_tuple;
        if(is_visible && !data.empty())
        {
            double avg_activity =
                std::accumulate(data.begin(), data.end(), 0.0) /
                static_cast<double>(data.size());
            m_track_activities[track_id] = avg_activity;
            m_min_activity                = std::min(m_min_activity, avg_activity);
            m_max_activity                = std::max(m_max_activity, avg_activity);
        }
    }

    // Normalize activities
    double range = m_max_activity - m_min_activity;
    if(range > 0.0)
    {
        for(auto& [track_id, activity] : m_track_activities)
        {
            activity = (activity - m_min_activity) / range;
        }
    }
    else
    {
        m_min_activity = 0.0;
        m_max_activity = 1.0;
    }

    // Build topology nodes
    for(const auto& node_model : topology_model.nodes)
    {
        if(!node_model.info) continue;

        TopologyNode topo_node;
        topo_node.node_id        = node_model.info->id;
        topo_node.activity_value = GetNodeActivity(node_model.info->id);

        // Collect processes for this node
        for(const auto& process_model : node_model.processes)
        {
            if(process_model.info)
            {
                topo_node.process_ids.push_back(process_model.info->id);
                topo_node.process_activities.push_back(
                    GetProcessActivity(process_model.info->id));
            }
        }

        m_topology_nodes.push_back(topo_node);
    }

    CalculateNodePositions();
    CalculateProcessPositions();
}

void
MinimapWindow::CalculateNodePositions()
{
    if(m_topology_nodes.empty()) return;

    // Simple grid layout for nodes
    int num_nodes = static_cast<int>(m_topology_nodes.size());
    int cols      = static_cast<int>(std::ceil(std::sqrt(num_nodes)));
    int rows      = (num_nodes + cols - 1) / cols;

    float start_x = NODE_SPACING;
    float start_y = NODE_SPACING;

    for(size_t i = 0; i < m_topology_nodes.size(); ++i)
    {
        int row = static_cast<int>(i) / cols;
        int col = static_cast<int>(i) % cols;

        m_topology_nodes[i].position =
            ImVec2(start_x + col * NODE_SPACING, start_y + row * NODE_SPACING);
        m_topology_nodes[i].radius = NODE_RADIUS;
    }
}

void
MinimapWindow::CalculateProcessPositions()
{
    // Arrange processes in a circle around their parent node
    for(auto& node : m_topology_nodes)
    {
        node.process_positions.clear();
        int num_processes = static_cast<int>(node.process_ids.size());

        if(num_processes == 0) continue;

        float angle_step = (2.0f * 3.14159f) / num_processes;
        float radius     = NODE_RADIUS + PROCESS_SPACING;

        for(int i = 0; i < num_processes; ++i)
        {
            float angle = i * angle_step;
            ImVec2 offset(radius * std::cos(angle), radius * std::sin(angle));
            node.process_positions.push_back(node.position + offset);
        }
    }
}

ImU32
MinimapWindow::GetActivityColor(double normalized_value)
{
    // Color scheme: dark blue (low) -> cyan -> green -> yellow -> red (high)
    normalized_value = std::clamp(normalized_value, 0.0, 1.0);

    unsigned char r, g, b;
    if(normalized_value < 0.25)
    {
        // Dark blue to Cyan
        double t = normalized_value / 0.25;
        r         = 0;
        g         = static_cast<unsigned char>(t * 200);
        b         = static_cast<unsigned char>(100 + t * 155);
    }
    else if(normalized_value < 0.5)
    {
        // Cyan to Green
        double t = (normalized_value - 0.25) / 0.25;
        r         = 0;
        g         = static_cast<unsigned char>(200 + t * 55);
        b         = static_cast<unsigned char>(255 - t * 255);
    }
    else if(normalized_value < 0.75)
    {
        // Green to Yellow
        double t = (normalized_value - 0.5) / 0.25;
        r         = static_cast<unsigned char>(t * 255);
        g         = 255;
        b         = 0;
    }
    else
    {
        // Yellow to Red
        double t = (normalized_value - 0.75) / 0.25;
        r         = 255;
        g         = static_cast<unsigned char>(255 - t * 255);
        b         = 0;
    }

    return IM_COL32(r, g, b, 255);
}

void
MinimapWindow::DrawTopologyMap(ImDrawList* draw_list, ImVec2 canvas_pos, ImVec2 canvas_size)
{
    // Draw connections first (so they appear behind nodes)
    for(const auto& node : m_topology_nodes)
    {
        // Draw lines from node to its processes
        for(const auto& process_pos : node.process_positions)
        {
            draw_list->AddLine(node.position + canvas_pos, process_pos + canvas_pos,
                              IM_COL32(100, 100, 100, 150), 1.0f);
        }
    }

    // Draw processes
    for(const auto& node : m_topology_nodes)
    {
        for(size_t i = 0; i < node.process_positions.size(); ++i)
        {
            ImVec2  pos     = node.process_positions[i] + canvas_pos;
            double  activity = i < node.process_activities.size()
                                  ? node.process_activities[i]
                                  : 0.0;
            ImU32   color   = GetActivityColor(activity);
            ImU32   border  = IM_COL32(255, 255, 255, 200);

            draw_list->AddCircleFilled(pos, PROCESS_RADIUS, color);
            draw_list->AddCircle(pos, PROCESS_RADIUS, border, 0, 1.5f);
        }
    }

    // Draw nodes
    for(const auto& node : m_topology_nodes)
    {
        ImVec2 pos    = node.position + canvas_pos;
        ImU32  color  = GetActivityColor(node.activity_value);
        ImU32  border = IM_COL32(255, 255, 255, 255);

        draw_list->AddCircleFilled(pos, node.radius, color);
        draw_list->AddCircle(pos, node.radius, border, 0, 2.0f);

        // Draw node label (simplified - just show node ID)
        char label[32];
        snprintf(label, sizeof(label), "N%llu", static_cast<unsigned long long>(node.node_id));
        ImVec2 text_size = ImGui::CalcTextSize(label);
        ImVec2 text_pos(pos.x - text_size.x * 0.5f, pos.y - text_size.y * 0.5f);
        draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), label);
    }
}

void
MinimapWindow::Render()
{
    if(!m_visible) return;

    // Update layout if topology changed
    if(m_topology && m_topology->Dirty())
    {
        UpdateTopologyLayout();
    }

    ImGui::SetNextWindowSize(ImVec2(800, 600), ImGuiCond_FirstUseEver);
    if(ImGui::Begin("Topology Map", &m_visible, ImGuiWindowFlags_None))
    {
        ImDrawList* draw_list = ImGui::GetWindowDrawList();
        ImVec2      canvas_pos = ImGui::GetCursorScreenPos();
        ImVec2      canvas_size = ImGui::GetContentRegionAvail();

        // Calculate bounds of topology layout
        float min_x = std::numeric_limits<float>::max();
        float max_x = std::numeric_limits<float>::lowest();
        float min_y = std::numeric_limits<float>::max();
        float max_y = std::numeric_limits<float>::lowest();

        for(const auto& node : m_topology_nodes)
        {
            min_x = std::min(min_x, node.position.x - node.radius);
            max_x = std::max(max_x, node.position.x + node.radius);
            min_y = std::min(min_y, node.position.y - node.radius);
            max_y = std::max(max_y, node.position.y + node.radius);

            for(const auto& proc_pos : node.process_positions)
            {
                min_x = std::min(min_x, proc_pos.x - PROCESS_RADIUS);
                max_x = std::max(max_x, proc_pos.x + PROCESS_RADIUS);
                min_y = std::min(min_y, proc_pos.y - PROCESS_RADIUS);
                max_y = std::max(max_y, proc_pos.y + PROCESS_RADIUS);
            }
        }

        if(max_x > min_x && max_y > min_y)
        {
            // Scale and center the topology map
            float layout_width  = max_x - min_x + 2 * NODE_SPACING;
            float layout_height = max_y - min_y + 2 * NODE_SPACING;
            float scale_x       = canvas_size.x / layout_width;
            float scale_y       = canvas_size.y / layout_height;
            float scale         = std::min(scale_x, scale_y) * 0.9f;  // 90% to add padding

            float offset_x = (canvas_size.x - layout_width * scale) * 0.5f;
            float offset_y = (canvas_size.y - layout_height * scale) * 0.5f;

            // Draw with scaling
            ImDrawList* scaled_draw_list = draw_list;
            ImVec2      scaled_canvas_pos(canvas_pos.x + offset_x, canvas_pos.y + offset_y);

            // Create a temporary transformation by adjusting positions
            for(auto& node : m_topology_nodes)
            {
                ImVec2 original_pos = node.position;
                node.position       = ImVec2((original_pos.x - min_x + NODE_SPACING) * scale,
                                         (original_pos.y - min_y + NODE_SPACING) * scale);

                for(size_t i = 0; i < node.process_positions.size(); ++i)
                {
                    ImVec2 original_proc_pos = node.process_positions[i];
                    node.process_positions[i] =
                        ImVec2((original_proc_pos.x - min_x + NODE_SPACING) * scale,
                               (original_proc_pos.y - min_y + NODE_SPACING) * scale);
                }
            }

            DrawTopologyMap(scaled_draw_list, scaled_canvas_pos, canvas_size);

            // Restore original positions
            CalculateNodePositions();
            CalculateProcessPositions();
        }
        else
        {
            // No topology data yet
            ImVec2 text_pos(canvas_pos.x + canvas_size.x * 0.5f - 100,
                           canvas_pos.y + canvas_size.y * 0.5f);
            draw_list->AddText(text_pos, IM_COL32(200, 200, 200, 255),
                              "No topology data available");
        }

        ImGui::Dummy(canvas_size);
    }
    ImGui::End();
}

}  // namespace View
}  // namespace RocProfVis
