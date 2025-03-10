// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_structs.h"
#include <map>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class MainView
{
public:
    MainView();
    ~MainView();

    void GenerateGraphPoints(
        std::map<std::string, rocprofvis_trace_process_t>& trace_data);

    void MakeGrid();
    void MakeGraphView(std::map<std::string, rocprofvis_trace_process_t>& trace_data,
                       float                                              scale_x);
    void MakeGraphMetadataView(
        std::map<std::string, rocprofvis_trace_process_t>& trace_data);
    void HandleGraphResize(int chart_id);
    void HandleSidebarResize();
    void FindMaxMin();
    void FindMaxMinFlame();
    void RenderLineCharts(int chart_id, float scale_x);
    void RenderFlameCharts(int chart_id, float scale_x);
    void RenderGraphMetadata(int graph_id, float size, std::string type,
                             rocprofvis_meta_map_struct_t data);

    void                                  HandleTopSurfaceTouch();
    std::vector<rocprofvis_data_point_t>  ExtractPointsFromData(void* data);
    std::vector<rocprofvis_trace_event_t> ExtractFlamePoints(
        const std::vector<rocprofvis_trace_event_t>& traceEvents);

private:
    std::map<int, rocprofvis_meta_map_struct_t> m_meta_map;
    std::vector<rocprofvis_trace_event_t>       m_flame_event;
    std::vector<rocprofvis_data_point_t>        m_data_arr;
    float m_min_value;
    float m_max_value;
    float m_zoom;
    float m_movement;
    float m_scrubber_position;
    float m_v_width;
    float m_v_min_x;
    float m_v_max_x;
    float m_scale_x;
    float m_min_x;
    float m_scroll_position;
    float m_sidebar_size;
    float m_max_x;
    float m_min_y;
    float m_max_y;
    bool  m_ran_once;
    bool  m_user_adjusting_graph_height;
    bool  m_meta_map_made;
    bool  m_has_zoom_happened;
};

}  // namespace View
}  // namespace RocProfVis


