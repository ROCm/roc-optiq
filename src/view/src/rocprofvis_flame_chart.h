// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_structs.h"
#include <vector>

namespace RocProfVis
{
namespace View
{

class FlameChart
{
public:
    FlameChart(int chart_id, float min_value, float max_value, float zoom, float movement,
               float min_x, float max_x,
               const std::vector<rocprofvis_trace_event_t>& data_arr, float scale_x);
    void render();
    void DrawBox(ImVec2 start_position, int boxplot_box_id,
                 rocprofvis_trace_event_t flame, float duration, ImDrawList* draw_list);

private:
    std::vector<rocprofvis_trace_event_t> flames;
    float m_min_value;
    float m_max_value;
    float m_min_x;
    float m_max_x;
    float m_min_start_time;
    float m_zoom;
    float m_movement;
    float m_scale_x;
    int   m_chart_id;
};

}  // namespace View
}  // namespace RocProfVis

