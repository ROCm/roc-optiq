// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller_types.h"
#include "rocprofvis_raw_track_data.h"
#include "rocprofvis_track_item.h"
#include "rocprofvis_view_structs.h"
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

class LineTrackItem : public TrackItem
{
public:
    LineTrackItem(DataProvider& dp, int id, std::string name, float zoom,
                  double time_offset_ns, double& min_x, double& max_x, double scale_x);
    ~LineTrackItem();

    ImVec2 MapToUI(rocprofvis_data_point_t& point, ImVec2& c_position, ImVec2& c_size,
                   float scale_x, float scale_y);
    bool   HandleTrackDataChanged() override;
    bool   ExtractPointsFromData();

    std::tuple<double, double> FindMaxMin();
    void  SetColorByValue(rocprofvis_color_by_value_t color_by_value_digits) override;
    float CalculateMissingX(float x1, float y1, float x2, float y2, float known_y);
    void  LineTrackRender(float graph_width);
    void  BoxPlotRender(float graph_width);
    bool  HasData() override;
    void  ReleaseData() override;
    void  SetShowBoxplot(bool show_boxplot);

protected:
    virtual void RenderMetaAreaScale() override;
    virtual void RenderChart(float graph_width) override;
    virtual void RenderMetaAreaOptions() override;

private:
    std::vector<rocprofvis_data_point_t> m_data;
    rocprofvis_color_by_value_t          m_color_by_value_digits;
    double                               m_min_y;
    double                               m_max_y;
    std::string                          m_min_y_str;
    std::string                          m_max_y_str;
    bool                                 m_is_color_value_existant;
    DataProvider&                        m_dp;
    bool                                 m_show_boxplot;
};

}  // namespace View
}  // namespace RocProfVis
