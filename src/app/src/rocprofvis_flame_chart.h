// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "structs.h"
#include <string>
#include <vector>

class FlameChart
{
public:
    FlameChart(int chart_id, double min_value, double max_value, float zoom,
               float movement, double min_x, double max_x,
               const std::vector<rocprofvis_trace_event_t>& data_arr, float scale_x);
    void render();
    void DrawBox(ImVec2 start_position, int boxplot_box_id,
                 rocprofvis_trace_event_t flame, float duration, ImDrawList* draw_list);

private:
    int graph_depth;
    std::vector<rocprofvis_trace_event_t> flames;
    double                                m_min_value;
    double                                m_max_value;
    float                                 m_zoom;
    float                                 m_movement;
    double                                m_min_x;
    double                                m_max_x;
    double                                m_min_start_time;
    int                                   m_chart_id;
    float                                 m_scale_x;
};

