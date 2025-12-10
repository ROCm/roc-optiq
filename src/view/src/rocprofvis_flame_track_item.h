// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_track_item.h"
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
    FlameTrackItem(DataProvider&                      dp,
                   std::shared_ptr<TimelineSelection> timeline_selection, uint64_t chart_id,
                   std::string name, float level_min,
                   float level_max, TimePixelTransform* m_time_to_pixel_manager);
    ~FlameTrackItem();

    bool ReleaseData() override;

    // Called to calculate max event label width for all flame track items.
    // Call after font size or style changes.
    static void CalculateMaxEventLabelWidth();
    bool        IsCompactMode() const override { return m_compact_mode; }

protected:
    void RenderChart(float graph_width) override;
    void RenderMetaAreaScale() override;
    void RenderMetaAreaOptions() override;
    void RenderMetaAreaExpand() override;

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
        rocprofvis_trace_event_t    event;
        bool                        selected;
        size_t                      name_hash;
        std::vector<ChildEventInfo> child_info;
    };

    void HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e);

    void DrawBox(ImVec2 start_position, int boxplot_box_id, ChartItem& flame,
                 float duration, ImDrawList* draw_list);

    bool ExtractPointsFromData();
    bool ExtractChildInfo(ChartItem& item);
    bool ParseChildInfo(const std::string& combined_name, ChildEventInfo& out_info);

    void RenderTooltip(ChartItem& chart_item, int color_index);
    void RecalculateTrackHeight();

    std::vector<ChartItem>             m_chart_items;
    EventColorMode                     m_event_color_mode;
    ImVec2                             m_text_padding;
    float                              m_level_height;
    std::vector<uint64_t>              m_selected_event_id;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    FlameTrackProjectSettings          m_flame_track_project_settings;
    float                              m_min_level;
    float                              m_max_level;
    // Used to enforce one click handling per render cycle.
    bool                            m_deferred_click_handled;
    bool                            m_has_drawn_tool_tip;
    std::vector<ChartItem>          m_selected_chart_items;
    EventManager::SubscriptionToken m_timeline_event_selection_changed_token;
    ImVec2                          m_tooltip_size;

    static float             s_max_event_label_width;
    static const std::string s_child_info_separator;
    TimePixelTransform*      m_tpt;
    bool        m_is_expanded;
    bool        m_compact_mode;
};

}  // namespace View
}  // namespace RocProfVis