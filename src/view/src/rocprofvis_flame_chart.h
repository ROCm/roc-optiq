// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_charts.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_view_structs.h"

#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class FlameChart : public Charts
{
public:
    FlameChart(int chart_id, std::string name, float zoom, float movement, double min_x,
               double max_x, float scale_x);
    void SetRandomColorFlag(bool set_color);
    void Render() override;
    void DrawBox(ImVec2 start_position, int boxplot_box_id,
                 rocprofvis_trace_event_t flame, float duration, ImDrawList* draw_list);

    void ExtractPointsFromData(const RawTrackEventData* event_track);

    std::tuple<double, double> FindMaxMinFlame();

    void SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) override;

    virtual bool SetRawData(const RawTrackData* raw_data);

protected:
    void RenderMetaArea() override;
    void RenderChart(float graph_width) override;

private:
    std::vector<rocprofvis_trace_event_t> flames;
    float                                 m_sidebar_size;
    rocprofvis_color_by_value_t           m_is_color_value_existant;
    const RawTrackData*                   m_raw_data;
    static std::vector<ImU32>             m_colors;
    bool                                  m_request_random_color;
};

}  // namespace View
}  // namespace RocProfVis
