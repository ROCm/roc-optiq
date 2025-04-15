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
    FlameChart(int chart_id, std::string name, float zoom, float movement, float min_x,
               float max_x, float scale_x);
    void Render() override;
    void DrawBox(ImVec2 start_position, int boxplot_box_id,
                 rocprofvis_trace_event_t flame, float duration, ImDrawList* draw_list);

    void ExtractPointsFromData(const RawTrackEventData* event_track);

    std::tuple<float, float> GetMinMax();
    std::tuple<float, float> FindMaxMinFlame();
    void  UpdateMovement(float zoom, float movement, float& min_x, float& max_x,
                         float scale_x, float m_scroll_position) override;
    float GetTrackHeight() override;
    int   ReturnChartID() override;
    void  SetID(int id) override;
    const std::string& GetName() override;
    void  SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) override;
    bool  GetVisibility() override;
    float GetMovement() override;

    virtual bool SetRawData(const RawTrackData* raw_data);

private:
    std::vector<rocprofvis_trace_event_t> flames;
    std::string                           m_name;
    float                                 m_min_x;
    float                                 m_max_x;
    float                                 m_zoom;
    float                                 m_movement;
    float                                 m_scale_x;
    int                                   m_chart_id;
    float                                 m_track_height;
    float                                 m_sidebar_size;
    rocprofvis_color_by_value_t           m_is_color_value_existant;
    bool                                  m_is_chart_visible;
    float                                 m_movement_since_unload;
    float                                 m_y_movement;
    const RawTrackData*                   m_raw_data;
};

}  // namespace View
}  // namespace RocProfVis
