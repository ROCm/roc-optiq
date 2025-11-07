// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_track_item.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class TimelineSelection;
class FlameTrackItem;

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

private:
    FlameTrackItem& m_track_item;
};

class FlameTrackItem : public TrackItem
{
    friend FlameTrackProjectSettings;

public:
    FlameTrackItem(DataProvider&                      dp,
                   std::shared_ptr<TimelineSelection> timeline_selection, int chart_id,
                   std::string name, float zoom, double movement, double min_x,
                   double max_x, double scale_x, float level_min, float level_max);
    ~FlameTrackItem();

    bool ReleaseData() override;

    // Called to calculate max event label width for all flame track items.
    // Call after font size or style changes.
    static void CalculateMaxEventLabelWidth();

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

    std::vector<ChartItem>             m_chart_items;
    EventColorMode                     m_event_color_mode;
    ImVec2                             m_text_padding;
    float                              m_level_height;
    std::vector<uint64_t>              m_selected_event_id;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    FlameTrackProjectSettings          m_project_settings;
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
};

}  // namespace View
}  // namespace RocProfVis