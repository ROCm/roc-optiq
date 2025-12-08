// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

namespace RocProfVis
{
namespace View
{
class TimeToPixelManager
{
public:
    TimeToPixelManager();
    ~TimeToPixelManager();

    void   ComputePixelMapping();
    double GetMinX() const;
    double GetMaxX() const;
    double GetVMinX() const;
    double GetVMaxX() const;
    double GetViewTimeOffsetNs() const;
    double GetVWidth() const;
    float  GetZoom() const;
    double GetRangeX() const;
    float  GetGraphSizeX() const;
    double GetPixelsPerNs() const;
    void   SetMinMaxX(double min_x, double max_x);
    void   SetViewTimeOffsetNs(double value);
    void   SetZoom(float value);
    void   SetGraphSizeX(float value);
    void   Reset();

private:
    double m_min_x;                // This value is the min value globally across data.
    double m_max_x;                // This value is the max value globally across data.
    double m_v_min_x;              // This value is the minimum value in the current view.
    double m_v_max_x;              // This value is the maximum value in the current view.
    double m_view_time_offset_ns;  // This value is how much the user has panned in ns.
    double m_v_width;  // This value is the width of the current view in timeline units.
    float  m_zoom;     // This value is the current zoom level.
    double m_range_x;  // This value is the total range of the data (max_x - min_x).
    float  m_graph_size_x;   // This value is the size of the graph area in pixels.
    double m_pixels_per_ns;  // This value is how many pixels represent 1 ns in the
                             // current view.
    bool m_has_changed;  // This value indicates if the mapping needs to be recomputed.
};
}  // namespace View
}  // namespace RocProfVis
