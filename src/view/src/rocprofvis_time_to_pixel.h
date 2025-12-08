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
    double m_min_x;
    double m_max_x;
    double m_v_min_x;
    double m_v_max_x;
    double m_view_time_offset_ns;
    double m_v_width;
    float  m_zoom;
    double m_range_x;
    float  m_graph_size_x;
    double m_pixels_per_ns;
    bool   m_has_changed;
};
}  // namespace View
}  // namespace RocProfVis
