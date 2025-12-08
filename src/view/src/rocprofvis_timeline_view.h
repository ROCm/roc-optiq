// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_project.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_time_to_pixel.h"
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
class TimelineView;
 
typedef struct ViewCoords
{
    double y;
    float  z;
    double v_min_x;
    double v_max_x;

} ViewCoords;

class TimelineViewProjectSettings : public ProjectSetting
{
public:
    TimelineViewProjectSettings(const std::string& project_id,
                                TimelineView&      timeline_view);
    ~TimelineViewProjectSettings() override;
    void ToJson() override;
    bool Valid() const override;

    uint64_t TrackID(int index) const;
    bool     DisplayTrack(uint64_t track_id) const;

private:
    TimelineView& m_timeline_view;
};

class TimelineView : public RocWidget
{
    friend TimelineViewProjectSettings;

public:
    TimelineView(DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection,
                 std::shared_ptr<AnnotationsManager> annotations);
    ~TimelineView();
    virtual void                                     Render() override;
    void                                             Update() override;
    void                                             MakeGraphView();
    void                                             ResetView();
    void                                             DestroyGraphs();
    std::shared_ptr<std::vector<rocprofvis_graph_t>> GetGraphs();
    void                                             RenderInteractiveUI();
    void ScrollToTrack(const uint64_t& track_id);
    void SetViewTimePosition(double time_pos_ns, bool center);
    void SetViewableRangeNS(double start_ns, double end_ns);
    void MoveToPosition(double start_ns, double end_ns, double y_position, bool center);
    void RenderGraphPoints();
    void RenderHistogram();
    void RenderTraceView();

    void           RenderGrid();
    float          GetScrollPosition();
    void           RenderScrubber(ImVec2 screen_pos);
    void           RenderSplitter();
    void           RenderGraphView();
    void           HandleTopSurfaceTouch();
    void           HandleHistogramTouch();
    void           HandleNewTrackData(std::shared_ptr<RocEvent> e);
    void           CalculateGridInterval();
    ImVec2         GetGraphSize();
    void           RenderAnnotations(ImDrawList* draw_list, ImVec2 window_position);
    ViewCoords     GetViewCoords() const;
    void           RenderTimelineViewOptionsMenu(ImVec2 window_position);
    TimelineArrow& GetArrowLayer();

private:
    void                            UpdateMaxMetaAreaSize(float new_size);
    void                            CalculateMaxMetaAreaSize();
    void                            UpdateAllMaxMetaAreaSizes();
    EventManager::SubscriptionToken m_scroll_to_track_token;
    EventManager::SubscriptionToken m_navigation_token;

    EventManager::SubscriptionToken m_new_track_token;
    EventManager::SubscriptionToken m_font_changed_token;
    EventManager::SubscriptionToken m_set_view_range_token;
    int                             m_dragged_sticky_id;
    const std::vector<double>*      m_histogram;
    float                           m_ruler_height;
    float                           m_ruler_padding;
    double                              m_min_y;
    double                              m_max_y;
    float                               m_sidebar_size;
    float                               m_scroll_position_y;
    float                               m_content_max_y_scroll;
    bool                                m_can_drag_to_pan;
    double                              m_previous_scroll_position;
    bool                                m_meta_map_made;
    bool                                m_resize_activity;
    double                              m_last_data_req_v_width;
    float                               m_unload_track_distance;
    DataProvider&                       m_data_provider;
    std::pair<double, double>           m_highlighted_region;
    SettingsManager&                    m_settings;
    double                              m_last_data_req_view_time_offset_ns;
    float                               m_artificial_scrollbar_height;
    double                              m_grid_interval_ns;
    int                                 m_grid_interval_count;
    bool                                m_recalculate_grid_interval;
    ImVec2                              m_last_graph_size;
    std::map<uint64_t, float>           m_track_height_total;  // Track index to height
    TimelineArrow                       m_arrow_layer;
    bool                                m_stop_user_interaction;
    float                               m_last_zoom;
    std::unordered_map<uint64_t, float> m_track_position_y;  // Track index to height
    float                               m_track_height_sum;
    std::shared_ptr<TimelineSelection>  m_timeline_selection;
    std::shared_ptr<AnnotationsManager> m_annotations;
    bool                                m_pseudo_focus;
    bool                                m_histogram_pseudo_focus;
    float                               m_max_meta_area_size;
    std::shared_ptr<std::vector<rocprofvis_graph_t>> m_graphs;
    TimeToPixelManager                               m_time_to_pixel_manager;
    struct
    {
        bool     handled;
        uint64_t track_id;
        int      new_index;
    } m_reorder_request;
    TimelineViewProjectSettings m_project_settings;
};

}  // namespace View
}  // namespace RocProfVis