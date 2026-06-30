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
#include "widgets/rocprofvis_widget.h"
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <chrono>
 
namespace RocProfVis
{
namespace View
{

class TimelineSelection;
class TimelineView;
class MeasurementController;
 
typedef struct ViewCoords
{
    double y;
    float  z;
    double v_min_x;
    double v_max_x;

} ViewCoords;

class LoadingTimer
{
public:
    LoadingTimer(uint64_t delay);
    ~LoadingTimer() = default;

    void Start();
    bool IsStarted() const { return m_started; }
    // Started and still counting down. Advances only via Tick() each frame, so
    // callers keep rendering until it expires instead of stalling on it.
    bool IsRunning() const { return m_started && m_timer < m_delay; }
    bool IsExpired();
    void Restart();
    void Tick();
private:
    std::chrono::time_point<std::chrono::steady_clock> m_last_tick;
    std::chrono::milliseconds                          m_timer;
    std::chrono::milliseconds                          m_delay;
    bool                                               m_started;
};

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
                 std::shared_ptr<MeasurementController> measurement,
                 std::shared_ptr<AnnotationsManager>   annotations);
    ~TimelineView();
    virtual void                                     Render() override;
    void                                             Update() override;
    bool                                             WantsContinuousRender() const;
    void                                             MakeGraphView();
    void                                             ResetView();
    void                                             DestroyGraphs();
    std::shared_ptr<std::vector<TrackGraph>> GetGraphs();
    void                                             RenderInteractiveUI();
    void ScrollToTrack(const uint64_t& track_id);
    void SetViewableRangeNS(double start_ns, double end_ns);
    void MoveToPosition(double start_ns, double end_ns, double y_position, bool center);
    void RenderGraphPoints();
    void RenderHistogram();
    void RenderTraceView();

    void           RenderGrid();
    float          GetScrollPosition();
    void           TimelineDragShimmy(int shimmy_amount);
    double         CalculateHighlightTimeWithShimmy(float mouse_x, float origin_x);
    void           RenderScrubber(ImVec2 screen_pos);
    void           RenderSplitter();
    void           RenderGraphView();
    void           HandleTopSurfaceTouch();
    void           HandleHistogramTouch();
    void           HandleNewTrackData(std::shared_ptr<RocEvent> e);
    void           CalculateGridInterval();
    ImVec2         GetGraphSize();
    void           RenderAnnotations(ImDrawList* draw_list, ImVec2 window_position);
    void           RenderMeasurement(ImDrawList* draw_list, ImVec2 window_position);
    void           CopyMeasurement(int index);
    int            MeasurementAtPixel(float rel_x) const;
    void           ResyncMeasurementHighlights();
    ViewCoords                          GetViewCoords() const;
    std::shared_ptr<TimePixelTransform> GetTransform() const;
    float          GetTotalTrackHeight() const;
    float          GetTrackViewportHeight() const;
    void           GetVisibleTrackFractions(float& start_fraction, float& end_fraction) const;
    void           RenderTimelineViewOptionsMenu(ImVec2 window_position);
    TimelineArrow& GetArrowLayer();

private:
    enum class MeasurementRulerDragTarget
    {
        kNone,
        kStart,
        kEnd
    };

    // Per-type track counts shown in the histogram header strip. The label
    // strings are built once when the trace is loaded (CalculateTrackCounts),
    // not rebuilt every frame in RenderTrackStats.
    struct TrackTypeCounts
    {
        uint64_t total                = 0;
        uint64_t instrumented_threads = 0;
        uint64_t sampled_threads      = 0;
        uint64_t queues               = 0;
        uint64_t streams              = 0;
        uint64_t counters             = 0;
        uint64_t other                = 0;

        std::string              total_label;    // e.g. "12 Tracks"
        std::string              breakdown;      // e.g. "3 threads, 2 queues"
        std::vector<std::string> tooltip_lines;  // e.g. "Queues: 2" per non-zero type
    };

    void CalculateTrackCounts();
    void BuildTrackCountLabels();
    void RenderTrackStats(float available_width);

    void UpdateMaxMetaAreaSize(bool update_tracks = false);

    void RenderTrack(int track_index, bool request_data, ImGuiWindowFlags window_flags,
                     ImVec2 container_size);
    bool IsRequestDataNeeded();
    void RequestDataIfEmpty(TrackItem* track_item, bool request_data);
    void RenderNormalTrack(TrackGraph& track_graph, int track_index, ImGuiWindowFlags window_flags,
                   bool is_reordering);
    void RenderTimeRangeSelectionFill(ImDrawList* draw_list, ImVec2 lane_min,
                                      ImVec2 lane_max);
    void RenderEmptyTrack(TrackItem* track_item);
    void RenderReorderingTrack(TrackItem* track_item, ImVec2 container_size);

    void                            ClearTimeRangeSelection();
    void                            CopySelectedEventNames();
    void                            CopySelectedEventDetails();
    EventManager::SubscriptionToken m_scroll_to_track_token;
    EventManager::SubscriptionToken m_navigation_token;
    EventManager::SubscriptionToken m_new_track_token;
    EventManager::SubscriptionToken m_font_changed_token;
    EventManager::SubscriptionToken m_set_view_range_token;
    EventManager::SubscriptionToken m_timeline_time_range_changed_token;

    int                                 m_dragged_sticky_id;
    const std::vector<double>*          m_histogram;
    float                               m_ruler_height;
    float                               m_ruler_padding;
    float                               m_sidebar_size;
    float                               m_scroll_position_y;
    float                               m_content_max_y_scroll;
    bool                                m_can_drag_to_pan;
    double                              m_previous_scroll_position;
    bool                                m_meta_map_made;
    bool                                m_resize_activity;
    bool                                m_reorder_auto_scrolling;
    double                              m_last_data_req_v_width;
    float                               m_unload_track_distance;
    DataProvider&                       m_data_provider;
    std::pair<double, double>           m_highlighted_region;
    SettingsManager&                    m_settings;
    double                              m_last_data_req_view_time_offset_ns;
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
    std::shared_ptr<MeasurementController> m_measurement;
    std::shared_ptr<AnnotationsManager> m_annotations;
    bool                                m_pseudo_focus;
    bool                                m_histogram_pseudo_focus;
    float                               m_max_meta_scale_area_size;
    std::shared_ptr<std::vector<TrackGraph>> m_graphs;
    std::shared_ptr<TimePixelTransform>               m_tpt;
    struct
    {
        bool     handled;
        uint64_t track_id;
        int      new_index;
    } m_reorder_request;

    bool m_dragging_selection_start;
    bool m_dragging_selection_end;
    bool m_is_selecting_region;
    MeasurementRulerDragTarget m_dragging_measurement_ruler;
    // Measurement targeted by the timeline right-click context menu (-1 = none).
    int                        m_context_menu_measurement;

    TimelineViewProjectSettings m_project_settings;
    LoadingTimer                m_loading_timer;
    TrackTypeCounts             m_track_counts;
};

}  // namespace View
}  // namespace RocProfVis