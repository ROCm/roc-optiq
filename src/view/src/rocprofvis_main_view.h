// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_charts.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_flame_chart.h"
#include "rocprofvis_grid.h"
#include "rocprofvis_line_chart.h"
#include "rocprofvis_structs.h"
#include "rocprofvis_view_structs.h"
#include "widgets/rocprofvis_widget.h"
#include <map>
#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

class MainView : public RocWidget
{
public:
    MainView(DataProvider& dp);
    ~MainView();

    virtual void Render();
    void         Update();
    void         MakeGraphView();

    void ResetView();
    void DestroyGraphs();

    std::map<int, rocprofvis_graph_map_t>* GetGraphMap();

private:
    void RenderGraphPoints();
    void RenderGrid();
    void RenderScrubber(ImVec2 screen_pos);
    void RenderSplitter(ImVec2 screen_pos);

    void RenderGraphView();
    void HandleGraphResize(int chart_id);
    void FindMaxMinFlame(std::vector<rocprofvis_trace_event_t> m_flame_event);
    void RenderFlameCharts(int chart_id, float scale_x);
    void RenderGraphCustomizationWindow(int graph_number);
    void HandleTopSurfaceTouch();

    void HandleNewTrackData(std::shared_ptr<RocEvent> e);

private:
    std::map<int, rocprofvis_graph_map_t> m_graph_map;
    Grid                                  m_grid;
    int                                   m_grid_size;
    double                                m_v_min_x;
    double                                m_v_max_x;
    double                                m_min_x;
    double                                m_max_x;
    double                                m_min_y;
    double                                m_max_y;
    float                                 m_zoom;
    int                                   m_sidebar_size;
    double                                m_movement;
    double                                m_scrubber_position;
    double                                m_v_width;
    double                                m_scale_x;
    double                                m_original_v_max_x;
    bool                                  m_capture_og_v_max_x;
    double                                m_scroll_position;
    double                                m_content_max_y_scoll;
    double                                m_previous_scroll_position;
    bool                                  m_user_adjusting_graph_height;
    bool                                  m_meta_map_made;
    bool                                  m_has_zoom_happened;
    bool                                  m_show_graph_customization_window;
    bool                                  m_is_control_held;
    bool                                  m_can_drag_to_pan;
    bool                                  m_resize_activity;

    EventManager::EventHandler m_new_track_data_handler;
    DataProvider&              m_data_provider;
    float                      m_unload_track_distance;
};

}  // namespace View
}  // namespace RocProfVis
