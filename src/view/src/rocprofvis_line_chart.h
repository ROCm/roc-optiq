// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "imgui.h"
#include "rocprofvis_structs.h"
#include <tuple>
#include <vector>
#include <string>
#include "rocprofvis_charts.h"

namespace RocProfVis
{
namespace View
{

class LineChart : public Charts
{
public:
    LineChart(int id,std::string name,  float zoom, float movement, float& min_x, float& max_x,
              float scale_x, void* datap);
    ~LineChart();
    void   Render() override;   
    void   UpdateMovement(float zoom, float movement, float& min_x, float& max_x,
                          float scale_x) override;
    ImVec2 MapToUI(rocprofvis_data_point_t& point, ImVec2& c_position, ImVec2& c_size,
                   float scale_x, float scale_y);
    std::vector<rocprofvis_data_point_t> ExtractPointsFromData();
    std::tuple<float, float>             FindMaxMin();
    float ReturnSize() override;
    void                                 SetID(int id) override;
    int                                  ReturnChartID() override;

private:
    std::vector<rocprofvis_data_point_t> m_data;
    std::string                          m_name;
    float                                m_min_value;
    float                                m_max_value;
    float                                m_zoom;
    float                                m_movement;
    float                                m_min_x;
    float                                m_max_x;
    float                                m_min_y;
    float                                m_max_y;
    float                                m_scale_x;
    int                                  m_id;
    void*                                datap;
    float                                size;
};

}  // namespace View
}  // namespace RocProfVis
