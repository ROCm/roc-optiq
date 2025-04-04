// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "imgui.h"
#include "rocprofvis_charts.h"
#include "rocprofvis_controller_types.h"

#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class FlameChart : public Charts
{
public:
    FlameChart(int chart_id, std::string name, double zoom, double movement, double min_x,
               double max_x, double scale_x);
    void Render() override;
    void DrawBox(ImVec2 start_position, int boxplot_box_id,
                 rocprofvis_trace_event_t flame, double duration, ImDrawList* draw_list);
    void ExtractFlamePoints(rocprofvis_controller_array_t* track_data);

    std::tuple<double, double> FindMaxMinFlame();
    void        UpdateMovement(double zoom, double movement, double& min_x, double& max_x,
                               double scale_x) override;
    double       ReturnSize() override;
    int         ReturnChartID() override;
    void        SetID(int id) override;
    std::string GetName() override;
    void        SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) override;

private:
    std::vector<rocprofvis_trace_event_t> flames;
    std::string                           m_name;
    double                                 m_min_value;
    double                                 m_max_value;
    double                                 m_min_x;
    double                                 m_max_x;
    double                                 m_zoom;
    double                                 m_movement;
    double                                 m_scale_x;
    int                                   m_chart_id;
    double                                 size;
    double                                 m_sidebar_size;
    rocprofvis_color_by_value_t             color_by_value_digits;
};

}  // namespace View
}  // namespace RocProfVis
