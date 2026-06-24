// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_track_item.h"
#include <memory>
#include <string>
#include <vector>
#include "rocprofvis_time_to_pixel.h"

namespace RocProfVis
{
namespace View
{

class TimelineSelection;
class FlameTrackItem;
class TimePixelTransform;
class MeasurementController;

enum class EventColorMode
{
    kNone,
    kByEventName,
    kByTimeLevel,
    __kCount
};

class FlameTrackProjectSettings : public ProjectSetting
{
public:
    FlameTrackProjectSettings(const std::string& project_id, FlameTrackItem& track_item);
    ~FlameTrackProjectSettings() override;
    void ToJson() override;
    bool Valid() const override;

    EventColorMode ColorEvents() const;
    bool           CompactMode() const;

private:
    FlameTrackItem& m_track_item;
};

class FlameTrackItem : public TrackItem
{
    friend FlameTrackProjectSettings;

public:
    FlameTrackItem(DataProvider&                          dp,
                   std::shared_ptr<TimelineSelection>     timeline_selection,
                   std::shared_ptr<MeasurementController> measurement,
                   uint64_t                               track_id,
                   std::shared_ptr<TimePixelTransform>    time_to_pixel_manager);
    ~FlameTrackItem();

    bool ReleaseData() override;

    // Called to calculate max event label width for all flame track items.
    // Call after font size or style changes.
    static void CalculateMaxEventLabelWidth();
    bool        IsCompactMode() const override { return m_compact_mode; }

#ifdef IMGUI_ENABLE_TEST_ENGINE
    bool GetFirstEventScreenCenterForTest(ImVec2& out_center) const
    {
        if(!m_first_event_rect_valid_for_test) return false;
        out_center = ImVec2(
            (m_first_event_rect_min_for_test.x + m_first_event_rect_max_for_test.x) * 0.5f,
            (m_first_event_rect_min_for_test.y + m_first_event_rect_max_for_test.y) * 0.5f);
        return true;
    }
    bool GetSecondEventScreenCenterForTest(ImVec2& out_center) const
    {
        if(!m_second_event_rect_valid_for_test) return false;
        out_center = ImVec2(
            (m_second_event_rect_min_for_test.x + m_second_event_rect_max_for_test.x) * 0.5f,
            (m_second_event_rect_min_for_test.y + m_second_event_rect_max_for_test.y) * 0.5f);
        return true;
    }
    // Drive Compact Mode through the same side-effecting path the gear-menu
    // checkbox uses (the checkbox lives in a popup with no stable widget id).
    void SetCompactModeForTest(bool on)
    {
        m_compact_mode = on;
        ApplyCompactMode();
    }
    float GetLevelHeightForTest() const { return m_level_height; }
    // Event color mode is set by the gear-menu radio buttons (a plain field
    // assignment); expose it so a test can drive and read it without the popup.
    EventColorMode GetEventColorModeForTest() const { return m_event_color_mode; }
    void SetEventColorModeForTest(EventColorMode mode) { m_event_color_mode = mode; }
#endif

protected:
    void  RenderChart(float graph_width) override;
    void  RenderMetaAreaOptions() override;
    void  RenderMetaAreaExpand() override;
    void  RenderSecondaryMetaPill(const ImVec2& content_size) override;

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
        TraceEvent    event;
        bool                        selected;
        bool                        highlighted;
        size_t                      name_hash;
        std::vector<ChildEventInfo> child_info;
    };

    void HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e);
    void HandleTimelineHighlightChanged(std::shared_ptr<RocEvent> e);

    void DrawBox(ImVec2 start_position, int boxplot_box_id, ChartItem& flame,
                 float duration, ImDrawList* draw_list, bool use_highlight_color);

    bool ExtractPointsFromData();
    bool ExtractChildInfo(ChartItem& item);
    bool ParseChildInfo(const std::string& combined_name, ChildEventInfo& out_info);

    void RenderTooltip(ChartItem& chart_item, int color_index);
    void RecalculateTrackHeight();
    void ApplyCompactMode();

    void RequestAnalysis() override;

    std::vector<ChartItem>             m_chart_items;
    EventColorMode                     m_event_color_mode;
    ImVec2                             m_text_padding;
    float                              m_level_height;
    std::vector<uint64_t>              m_selected_event_id;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    std::shared_ptr<MeasurementController> m_measurement;
    FlameTrackProjectSettings          m_flame_track_project_settings;
    float                              m_min_level;
    float                              m_max_level;
    // Used to enforce one click handling per render cycle.
    bool                            m_deferred_click_handled;
    bool                            m_has_drawn_tool_tip;
    std::vector<ChartItem>          m_selected_chart_items;
    EventManager::SubscriptionToken m_timeline_event_selection_changed_token;
    EventManager::SubscriptionToken m_timeline_event_highlight_changed_token;
    ImVec2                          m_tooltip_size;

    static float             s_max_event_label_width;
    static const std::string s_child_info_separator;
    bool                     m_is_expanded;
    bool                     m_compact_mode;

    const AnalysisQueueUtilization* m_queue_utilization;
    Pill                            m_queue_utilization_pill;

#ifdef IMGUI_ENABLE_TEST_ENGINE
    // Reset at the top of RenderChart, updated per DrawBox.
    bool   m_first_event_rect_valid_for_test = false;
    ImVec2 m_first_event_rect_min_for_test{ 0.0f, 0.0f };
    ImVec2 m_first_event_rect_max_for_test{ 0.0f, 0.0f };
    bool   m_second_event_rect_valid_for_test = false;
    ImVec2 m_second_event_rect_min_for_test{ 0.0f, 0.0f };
    ImVec2 m_second_event_rect_max_for_test{ 0.0f, 0.0f };
#endif
};

}  // namespace View
}  // namespace RocProfVis