// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_charts.h"
#include "rocprofvis_structs.h"
#include "rocprofvis_controller_types.h"
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

class LineChart : public Charts
{
public:
    LineChart(int id, std::string name, double zoom, double movement, double& min_x,
              double& max_x, double scale_x);
    ~LineChart();
    void   Render() override;
    void   UpdateMovement(double zoom, double movement, double& min_x, double& max_x,
                          double scale_x) override;
    ImVec2 MapToUI(rocprofvis_data_point_t& point, ImVec2& c_position, ImVec2& c_size,
                   double scale_x, double scale_y);
    std::vector<rocprofvis_data_point_t> LineChart::ExtractPointsFromData(
        rocprofvis_controller_array_t* track_data);
    std::tuple<double, double> FindMaxMin();
    double                    ReturnSize() override;
    void                     SetID(int id) override;
    int                      ReturnChartID() override;
    std::string              GetName() override;
    int                      SetSize();
    void  SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) override;
    double CalculateMissingX(double x1, double y1, double x2, double y2, double known_y);

private:
    std::vector<rocprofvis_data_point_t>   m_data;
    std::string                            m_name;
    double                                  m_min_value;
    double                                  m_max_value;
    double                                  m_zoom;
    rocprofvis_color_by_value_t              m_color_by_value_digits;
    double                                  m_movement;
    double                                  m_min_x;
    double                                  m_max_x;
    double                                  m_min_y;
    double                                  m_max_y;
    double                                  m_scale_x;
    int                                    m_id;
    double                                  size;
    bool                                   is_color_value_existant;
    std::map<int, rocprofvis_graph_map_t>* tree;
};

}  // namespace View
}  // namespace RocProfVis
