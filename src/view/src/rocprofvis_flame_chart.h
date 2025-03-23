// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_charts.h"
#include "rocprofvis_structs.h"
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
               float max_x, float scale_x,
               std::vector<rocprofvis_trace_event_t>& raw_flame);
    void                     Render() override;
    void                     DrawBox(ImVec2 start_position, int boxplot_box_id,
                                     rocprofvis_trace_event_t flame, float duration, ImDrawList* draw_list);
    void                     ExtractFlamePoints();
    std::tuple<float, float> FindMaxMinFlame();
    void        UpdateMovement(float zoom, float movement, float& min_x, float& max_x,
                               float scale_x) override;
    float       ReturnSize() override;
    int         ReturnChartID() override;
    void        SetID(int id) override;
    std::string GetName() override;
 
private:
    std::vector<rocprofvis_trace_event_t>  flames;
    std::string                            m_name;
    float                                  m_min_value;
    float                                  m_max_value;
    float                                  m_min_x;
    float                                  m_max_x;
    float                                  m_zoom;
    float                                  m_movement;
    float                                  m_scale_x;
    int                                    m_chart_id;
    std::vector<rocprofvis_trace_event_t>& m_raw_flame;
    float                                  size;
    float                                  m_sidebar_size;
 };

}  // namespace View
}  // namespace RocProfVis
