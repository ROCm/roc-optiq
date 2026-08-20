// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_track_item.h"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class TimelineSelection;
class FlameTrackItem;
class TimePixelTransform;
class TimelineTrackOptions;
class EventTrackOptions;
class QueueTrackOptions;
class MeasurementController;

class FlameTrackItem : public TrackItem
{
public:
    FlameTrackItem(DataProvider& dp, uint64_t track_id,
                   TimelineTrackOptions&                  track_options,
                   std::shared_ptr<TimePixelTransform>    time_to_pixel_manager,
                   std::shared_ptr<TimelineSelection>     timeline_selection,
                   std::shared_ptr<MeasurementController> measurement);
    ~FlameTrackItem() override;

    void Update() override;
    bool ReleaseData() override;

    // Called to calculate max event label width for all flame track items.
    // Call after font size or style changes.
    static void CalculateMaxEventLabelWidth();
    bool        IsCompactMode() const override;

    friend struct FlameTrackItemTestPeer;

protected:
    void  RenderChart(float graph_width) override;
    void  RenderMetaAreaExpand() override;
    float GetMetaAreaTrailingWidth() const override;

private:
    struct ChildEventInfo
    {
        std::string name;
        size_t      name_hash;
        size_t      count;
        uint64_t    duration;
    };

    struct ChartItem
    {
        TraceEvent                  event;
        bool                        selected;
        bool                        highlighted;
        size_t                      name_hash;
        std::vector<ChildEventInfo> child_info;
    };

    void HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e);
    void HandleTimelineHighlightChanged(std::shared_ptr<RocEvent> e);
    void HandleFontSizeChanged(std::shared_ptr<RocEvent> e);

    void DrawBox(ImVec2 start_position, ChartItem& flame, float duration,
                 ImDrawList* draw_list, bool use_highlight_color);

    void ExtractPointsFromData() override;
    bool ExtractChildInfo(ChartItem& item);
    bool ParseChildInfo(const std::string& combined_name, ChildEventInfo& out_info);

    void  RenderTooltip(ChartItem& chart_item, size_t color_index);
    void  RecalculateTrackHeight();
    void  UpdateMinTrackHeight();
    void  RefreshLevelHeight();
    float DefaultTrackHeight() const;
    float ExpandedTrackHeight() const;
    float EventBoxHeight() const;
    float ComputeTextVerticalOffset(float box_height) const;
    // Font-size-dependent glyph ink center, shared/cached across all tracks.
    static float TextGlyphCenter();

    std::vector<ChartItem>                 m_chart_items;
    ImVec2                                 m_text_padding;
    float                                  m_level_height;
    // Cached per-frame vertical offset (from a box's top) for centering labels.
    float                                  m_text_vertical_offset = 0.0f;
    std::vector<uint64_t>                  m_selected_event_id;
    std::shared_ptr<MeasurementController> m_measurement;
    float                                  m_min_level;
    float                                  m_max_level;
    // Used to enforce one click handling per render cycle.
    bool                            m_deferred_click_handled;
    bool                            m_has_drawn_tool_tip;
    std::vector<ChartItem>          m_selected_chart_items;
    EventManager::SubscriptionToken m_timeline_event_selection_changed_token;
    EventManager::SubscriptionToken m_timeline_event_highlight_changed_token;
    EventManager::SubscriptionToken m_font_size_changed_token;
    ImVec2                          m_tooltip_size;

    static float             s_max_event_label_width;
    static const std::string s_child_info_separator;

    Pill* m_pill_analysis_queue;

#ifdef IMGUI_ENABLE_TEST_ENGINE
    // ID of the "FV" child window the bars are registered under; tests gather
    // bars by this parent and pick targets by width. Captured during render.
    unsigned int m_test_flame_window_id = 0;
#endif
    // User configurable options. Underlying object is shared and owned by TrackItem.
    EventTrackOptions* m_event_options;  // Always valid
    QueueTrackOptions* m_queue_options;  // Valid for queue
};

}  // namespace View
}  // namespace RocProfVis
