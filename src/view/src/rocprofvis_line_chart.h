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
    void Render() override;

    ImVec2 MapToUI(rocprofvis_data_point_t& point, ImVec2& c_position, ImVec2& c_size,
                   float scale_x, float scale_y);

    void ExtractPointsFromData(const RawTrackSampleData* track_data);

    std::tuple<double, double> FindMaxMin();
    void  SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) override;
    float CalculateMissingX(float x1, float y1, float x2, float y2, float known_y);

    virtual bool SetRawData(const RawTrackData* raw_data);

protected:
    virtual void RenderMetaArea() override;
    virtual void RenderChart(float graph_width) override;

private:
    std::vector<rocprofvis_data_point_t> m_data;
    rocprofvis_color_by_value_t          m_color_by_value_digits;
    double                               m_movement;
    double                               m_min_y;
    double                               m_max_y;
    bool                                 m_is_color_value_existant;
    const RawTrackData*                  m_raw_data;
};

}  // namespace View
}  // namespace RocProfVis
