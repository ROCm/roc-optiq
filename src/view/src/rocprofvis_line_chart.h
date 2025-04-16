// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_charts.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_view_structs.h"
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
    LineChart(int id, std::string name, float zoom, float movement, double& min_x,
              double& max_x, float scale_x);
    ~LineChart();
    void   Render() override;
    void   UpdateMovement(float zoom, float movement, double& min_x, double& max_x,
                          float scale_x, float m_scroll_position) override;
    ImVec2 MapToUI(rocprofvis_data_point_t& point, ImVec2& c_position, ImVec2& c_size,
                   float scale_x, float scale_y);

    void ExtractPointsFromData(const RawTrackSampleData* track_data);

    std::tuple<double, double> GetMinMax();
    std::tuple<double, double> FindMaxMin();
    float                    GetTrackHeight() override;
    void                     SetID(int id) override;
    int                      ReturnChartID() override;
    const std::string&       GetName() override;
    int                      SetSize();
    void  SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) override;
    float CalculateMissingX(float x1, float y1, float x2, float y2, float known_y);
    bool  GetVisibility() override;
    float GetMovement() override;

    virtual bool SetRawData(const RawTrackData* raw_data);

private:
    std::vector<rocprofvis_data_point_t> m_data;
    std::string                          m_name;
    float                                m_zoom;
    rocprofvis_color_by_value_t          m_color_by_value_digits;
    double                               m_movement;
    double                               m_min_x;
    double                               m_max_x;
    double                               m_min_y;
    double                               m_max_y;
    float                                m_scale_x;
    int                                  m_id;
    float                                m_track_height;
    bool                                 m_is_color_value_existant;
    bool                                 m_is_chart_visible;
    float                                m_movement_since_unload;
    float                                m_y_movement;
    const RawTrackData*                  m_raw_data;
};

}  // namespace View
}  // namespace RocProfVis
