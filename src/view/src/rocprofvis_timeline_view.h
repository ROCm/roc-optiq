// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_line_track_item.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_structs.h"
#include "rocprofvis_track_item.h"
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

class TimelineView : public RocWidget
{
public:
    TimelineView(DataProvider& dp);
    ~TimelineView();
    virtual void                           Render();
    void                                   Update();
    void                                   MakeGraphView();
    void                                   ResetView();
    void                                   DestroyGraphs();
    std::map<int, rocprofvis_graph_map_t>* GetGraphMap();

private:
    void RenderGraphPoints();
    void RenderGrid();
    void RenderScrubber(ImVec2 screen_pos);
    void RenderSplitter(ImVec2 screen_pos);
    void RenderGraphView();
    void HandleTopSurfaceTouch();
    void CalibratePosition();
    void HandleNewTrackData(std::shared_ptr<RocEvent> e);

private:
    std::map<int, rocprofvis_graph_map_t> m_graph_map;
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
    double                                m_pixels_per_ns;
    double                                m_original_v_max_x;
    double                                m_scroll_position;
    double                                m_content_max_y_scoll;
    bool                                  m_can_drag_to_pan;
    double                                m_previous_scroll_position;
    bool                                  m_user_adjusting_graph_height;
    bool                                  m_meta_map_made;
    bool                                  m_has_zoom_happened;
    bool                                  m_show_graph_customization_window;
    bool                                  m_is_control_held;
    bool                                  m_resize_activity;
    double                                m_scroll_position_x;
    double                                m_v_past_width;
    bool                                  m_stop_zooming;
    double                                m_scrollbar_location_as_percentage;
    bool                                  m_artifical_scrollbar_active;
    float                                 m_unload_track_distance;
    float                                 m_universal_content_size;
    double                                m_range_x;
    DataProvider&                         m_data_provider;
    std::pair<double, double>             m_highlighted_region;
    Settings&                             m_settings;
    EventManager::SubscriptionToken       m_new_track_token;
    double                                m_viewport_past_position;
    int                                   m_artificial_scrollbar_size;
    ImVec2                                m_graph_size;
    bool                                  m_region_selection_changed;
};

}  // namespace View
}  // namespace RocProfVis
