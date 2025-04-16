// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_charts.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_structs.h"
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

class BoxPlot : public Charts
{
public:
    BoxPlot(int id, std::string name, float zoom, float movement, float& min_x,
            float& max_x, float scale_x);
    ~BoxPlot();
    void   Render() override;
    void   UpdateMovement(float zoom, float movement, double& min_x, double& max_x,
                          float scale_x, float y_scroll_position) override;
    ImVec2 MapToUI(rocprofvis_data_point_t& point, ImVec2& c_position, ImVec2& c_size,
                   float scale_x, float scale_y);
    std::vector<rocprofvis_data_point_t> BoxPlot::ExtractPointsFromData(
        rocprofvis_controller_array_t* track_data);
    std::tuple<float, float> FindMaxMin();
    float                    GetTrackHeight() override;
    void                     SetID(int id) override;
    int                      ReturnChartID() override;
    std::string&             GetName() override;
    int                      SetSize();
    void  SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) override;
    float CalculateMissingX(float x1, float y1, float x2, float y2, float known_y);

private:
    std::vector<rocprofvis_data_point_t> m_data;
    std::string                          m_name;
    float                                m_zoom;
    rocprofvis_color_by_value_t          m_color_by_value_digits;
    float                                m_movement;
    float                                m_min_x;
    float                                m_max_x;
    float                                m_min_y;
    float                                m_max_y;
    float                                m_scale_x;
    int                                  m_id;
    float                                m_track_height;
    bool                                 m_is_color_value_existant;
};

}  // namespace View
}  // namespace RocProfVis
