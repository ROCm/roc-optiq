// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_view_structs.h"
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class FlameTrackItem : public TrackItem
{
public:
    FlameTrackItem(DataProvider& dp, int chart_id, std::string name, float zoom,
                   float movement, double min_x, double max_x, float scale_x);
    void SetRandomColorFlag(bool set_color);
    void Render(double width) override;
    void DrawBox(ImVec2 start_position, int boxplot_box_id,
                 rocprofvis_trace_event_t flame, float duration, ImDrawList* draw_list);

    bool                       HandleTrackDataChanged() override;
    bool                       ExtractPointsFromData();
    std::tuple<double, double> FindMaxMinFlame();

    void SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) override;

    bool HasData() override;
    void ReleaseData() override;

protected:
    void RenderChart(float graph_width) override;

private:
    std::vector<rocprofvis_trace_event_t> m_flames;
    bool                                  m_request_random_color;
    ImVec2                                m_text_padding;
    float                                 m_flame_height;
    uint64_t                              m_selected_event_id;
    DataProvider&                         m_dp;
};

}  // namespace View
}  // namespace RocProfVis
