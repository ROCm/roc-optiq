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

class FlameTrackItem : public TrackItem
{
public:
    FlameTrackItem(DataProvider&                      dp,
                   std::shared_ptr<TimelineSelection> timeline_selection, int chart_id,
                   std::string name, double zoom, double movement, double min_x,
                   double max_x, double scale_x);

    bool HandleTrackDataChanged() override;
    void ReleaseData() override;
    void Update() override;

protected:
    void RenderChart(float graph_width) override;
    void RenderMetaAreaScale() override;
    void RenderMetaAreaOptions() override;

private:
    // Local cache of selection state packed with event data for each event.
    struct ChartItem
    {
        rocprofvis_trace_event_t event;
        bool                     selected;
    };

    void HandleTimelineSelectionChanged();

    void DrawBox(ImVec2 start_position, int boxplot_box_id, ChartItem& flame,
                 double duration, ImDrawList* draw_list);
    bool ExtractPointsFromData();

    std::vector<ChartItem>             m_chart_items;
    bool                               m_request_random_color;
    ImVec2                             m_text_padding;
    float                              m_level_height;
    std::vector<uint64_t>              m_selected_event_id;
    std::shared_ptr<TimelineSelection> m_timeline_selection;

    // Used to enforce one selection change per render cycle.
    bool m_selection_changed;

    EventManager::SubscriptionToken m_timeline_event_selection_changed_token;
};

}  // namespace View
}  // namespace RocProfVis