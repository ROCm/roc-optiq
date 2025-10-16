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

protected:
    void RenderChart(float graph_width) override;
    void RenderMetaAreaScale() override;
    void RenderMetaAreaOptions() override;
    void RenderMetaAreaExpand() override;

private:
    // Local cache of selection state packed with event data for each event.
    struct ChartItem
    {
        rocprofvis_trace_event_t event;
        bool                     selected;
        size_t                   name_hash;
    };

    void HandleTimelineSelectionChanged(std::shared_ptr<RocEvent> e);

    void DrawBox(ImVec2 start_position, int boxplot_box_id, ChartItem& flame,
                 float duration, ImDrawList* draw_list);
    bool ExtractPointsFromData();

    std::vector<ChartItem>             m_chart_items;
    EventColorMode                     m_event_color_mode;
    ImVec2                             m_text_padding;
    float                              m_level_height;
    std::vector<uint64_t>              m_selected_event_id;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    FlameTrackProjectSettings          m_project_settings;
    float                              m_min_level;
    float                              m_max_level;
    // Used to enforce one selection change per render cycle.
    bool                            m_selection_changed;
    bool                            m_has_drawn_tool_tip;
    std::vector<ChartItem>          m_selected_chart_items;
    EventManager::SubscriptionToken m_timeline_event_selection_changed_token;
};

}  // namespace View
}  // namespace RocProfVis