// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_line_track_item.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_timeline_arrow.h"
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

class TimelineSelection;

enum class TimeFormat
{
    kTimecode,
    kNanoseconds,
};

typedef struct ViewCoords
{
    double time_offset_ns;
    double y_scroll_position;
    float  zoom;
} ViewCoords;

class TimelineView : public RocWidget
{
public:
    TimelineView(DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection);
    ~TimelineView();
    virtual void                     Render();
    void                             Update();
    void                             MakeGraphView();
    void                             ResetView();
    void                             DestroyGraphs();
    std::vector<rocprofvis_graph_t>* GetGraphs();
    void                             RenderInteractiveUI(ImVec2 screen_pos);
    void                             TimelineOptions();
    void                             ScrollToTrack(const uint64_t& track_id);
    void                             SetViewTimePosition(double time_pos_ns, bool center);
    void                             RenderGraphPoints();
    void                             RenderGridAlt();
    void                             RenderGrid();
    void                             RenderScrubber(ImVec2 screen_pos);
    void                             RenderSplitter(ImVec2 screen_pos);
    void                             RenderGraphView();
    void                             HandleTopSurfaceTouch();
    void                             CalibratePosition();
    void                             HandleNewTrackData(std::shared_ptr<RocEvent> e);
    void                             CalculateGridInterval();
    ViewCoords                       GetViewCoords() const;
    void                             SetViewCoords(const ViewCoords& coords);
    void                             RenderArrowOptionsMenu();
    void ShowTimelineContextMenu(const ImVec2& window_position);
    void RenderStickyNotes(ImDrawList* draw_list, ImVec2 window_position);

private:
    std::vector<rocprofvis_graph_t>    m_graphs;
    int                                m_ruler_height;
    ImVec2                             m_ruler_padding;
    double                             m_v_min_x;
    double                             m_v_max_x;
    double                             m_min_x;
    double                             m_max_x;
    double                             m_min_y;
    double                             m_max_y;
    float                              m_zoom;
    int                                m_sidebar_size;
    double                             m_view_time_offset_ns;
    double                             m_v_width;
    double                             m_pixels_per_ns;
    float                              m_scroll_position_y;
    float                              m_content_max_y_scroll;
    bool                               m_can_drag_to_pan;
    double                             m_previous_scroll_position;
    bool                               m_meta_map_made;
    bool                               m_resize_activity;
    double                             m_scroll_position_x;
    EventManager::SubscriptionToken    m_scroll_to_track_token;
    double                             m_last_data_req_v_width;
    double                             m_scrollbar_location_as_percentage;
    bool                               m_artifical_scrollbar_active;
    float                              m_unload_track_distance;
    double                             m_range_x;
    DataProvider&                      m_data_provider;
    std::pair<double, double>          m_highlighted_region;
    Settings&                          m_settings;
    EventManager::SubscriptionToken    m_new_track_token;
    double                             m_last_data_req_view_time_offset_ns;
    int                                m_artificial_scrollbar_height;
    ImVec2                             m_graph_size;
    TimeFormat                         m_display_time_format;
    double                             m_grid_interval_ns;
    int                                m_grid_interval_count;
    bool                               m_recalculate_grid_interval;
    ImVec2                             m_last_graph_size;
    std::map<uint64_t, float>          m_track_height_total;  // Track index to height
    TimelineArrow                      m_arrow_layer;
    bool                               m_stop_user_interaction;
    float                              m_last_zoom;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    RocProfVis::View::AnnotationsView  m_annotations_view;

    struct
    {
        bool     handled;
        uint64_t track_id;
        int      new_index;
    } m_reorder_request;
};

}  // namespace View
}  // namespace RocProfVis
