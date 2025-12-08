// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_time_to_pixel.h"
#include <algorithm>
namespace RocProfVis
{
namespace View
{
TimeToPixelManager::TimeToPixelManager()
: m_min_x(std::numeric_limits<double>::max())
, m_max_x(std::numeric_limits<double>::lowest())
, m_v_min_x(0.0)
, m_v_max_x(0.0)
, m_view_time_offset_ns(0.0)
, m_v_width(0.0)
, m_zoom(1.0f)
, m_range_x(0.0)
, m_graph_size_x(0.0f)
, m_pixels_per_ns(0.0)
, m_has_changed(false)
{}
TimeToPixelManager::~TimeToPixelManager() {}

void
TimeToPixelManager::ComputePixelMapping()
{
    // Check if the user has actually changed anything if not nothing to compute.
    if(m_has_changed)
    {
        // Before doing any computation validate the data is correct
        m_view_time_offset_ns =
            std::clamp(m_view_time_offset_ns, 0.0, std::max(0.0, m_range_x - m_v_width));

        // Compute
        m_v_width       = (m_range_x) / m_zoom;
        m_v_min_x       = m_min_x + m_view_time_offset_ns;
        m_v_max_x       = m_v_min_x + m_v_width;
        m_pixels_per_ns = (m_graph_size_x) / (m_v_max_x - m_v_min_x);
    }
    else
    {
        return;
    }
}

void
TimeToPixelManager::Reset()
{
    m_zoom                = 1.0f;
    m_view_time_offset_ns = 0.0f;
    m_min_x               = std::numeric_limits<double>::max();
    m_max_x               = std::numeric_limits<double>::lowest();
    m_v_min_x             = 0.0f;
    m_v_max_x             = 0.0f;
    m_v_width             = 0.0f;
    m_range_x             = 0.0f;
    m_graph_size_x        = 0.0f;
    m_pixels_per_ns       = 0.0f;
}

double
TimeToPixelManager::GetMinX() const
{
    return m_min_x;
}
double
TimeToPixelManager::GetMaxX() const
{
    return m_max_x;
}
double
TimeToPixelManager::GetVMinX() const
{
    return m_v_min_x;
}
double
TimeToPixelManager::GetVMaxX() const
{
    return m_v_max_x;
}
double
TimeToPixelManager::GetViewTimeOffsetNs() const
{
    return m_view_time_offset_ns;
}
double
TimeToPixelManager::GetVWidth() const
{
    return m_v_width;
}
float
TimeToPixelManager::GetZoom() const
{
    return m_zoom;
}
double
TimeToPixelManager::GetRangeX() const
{
    return m_range_x;
}
float
TimeToPixelManager::GetGraphSizeX() const
{
    return m_graph_size_x;
}
double
TimeToPixelManager::GetPixelsPerNs() const
{
    return m_pixels_per_ns;
}

void
TimeToPixelManager::SetMinMaxX(double min_x, double max_x)
{
    m_min_x       = min_x;
    m_max_x       = max_x;
    m_range_x     = m_max_x - m_min_x;
    m_has_changed = true;
}

void
TimeToPixelManager::SetViewTimeOffsetNs(double value)
{
    m_view_time_offset_ns = value;
    m_has_changed         = true;
}

void
TimeToPixelManager::SetZoom(float value)
{
    m_zoom        = value;
    m_has_changed = true;
}

void
TimeToPixelManager::SetGraphSizeX(float value)
{
    m_graph_size_x = value;
    m_has_changed  = true;
}

}  // namespace View
}  // namespace RocProfVis
