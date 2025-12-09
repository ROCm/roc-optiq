// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_time_to_pixel.h"
#include <algorithm>
#include <limits>

namespace RocProfVis
{
namespace View
{
TimePixelTransform::TimePixelTransform()
: m_min_x_ns(std::numeric_limits<double>::max())
, m_max_x_ns(std::numeric_limits<double>::lowest())
, m_v_min_x_ns(0.0)
, m_v_max_x_ns(0.0)
, m_view_time_offset_ns(0.0)
, m_v_width_ns(0.0)
, m_zoom(1.0f)
, m_range_x_ns(0.0)
, m_graph_size_x(0.0f)
, m_pixels_per_ns(0.0)
, m_has_changed(false)
{}
TimePixelTransform::~TimePixelTransform() {}

float
TimePixelTransform::TimeToPixel(double time_ns)
{
    /*
    The following function uses UI time (normalized to 0).

    Return: Pixel position in the graph area corresponding to the input time_ns.
    */
    return (time_ns - m_view_time_offset_ns) * m_pixels_per_ns;
}

float
TimePixelTransform::RawTimeToPixel(double time_ns)
{
    /*
    The following function uses UI time (normalized to 0)

    Return: Pixel position in the graph area corresponding to the input time_ns.
    */
    double ui_time_ns = time_ns - m_min_x_ns;
    return TimeToPixel(ui_time_ns);
}

double
TimePixelTransform::PixelToTime(float x_position)
{
    /*
    This function gets raw time (in ns) from a pixel position in the graph area. It is not
    accuret to the data and is in screen ns.

    IMPORTANT: x_position must account for any offsets such as sidebar width.

    Return : Time in ns corresponding to the input x_position in pixels.
    */
    return (m_view_time_offset_ns + ((x_position / m_graph_size_x) * m_v_width_ns));
}

void
TimePixelTransform::ComputePixelMapping()
{
    /*
    This function is used to compute the critical variables needed for the UI to operate.
    It is ultimately attempting to calculate m_pixels_per_ns toe enable time to pixel
    conversion.
    */

    // Check if the user has actually changed anything if not nothing to compute.
    if(m_has_changed)
    {
        // Before doing any computation validate the data is correct
        m_view_time_offset_ns = std::clamp(m_view_time_offset_ns, 0.0,
                                           std::max(0.0, m_range_x_ns - m_v_width_ns));

        // Compute
        m_v_width_ns    = (m_range_x_ns) / m_zoom;
        m_v_min_x_ns    = m_min_x_ns + m_view_time_offset_ns;
        m_v_max_x_ns    = m_v_min_x_ns + m_v_width_ns;
        m_pixels_per_ns = (m_graph_size_x) / (m_v_max_x_ns - m_v_min_x_ns);
    }
    else
    {
        return;
    }
}

void
TimePixelTransform::Reset()
{
    m_zoom                = 1.0f;
    m_view_time_offset_ns = 0.0f;
    m_min_x_ns            = std::numeric_limits<double>::max();
    m_max_x_ns            = std::numeric_limits<double>::lowest();
    m_v_min_x_ns          = 0.0f;
    m_v_max_x_ns          = 0.0f;
    m_v_width_ns          = 0.0f;
    m_range_x_ns          = 0.0f;
    m_graph_size_x        = 0.0f;
    m_pixels_per_ns       = 0.0f;
}

double
TimePixelTransform::GetMinX() const
{
    return m_min_x_ns;
}
double
TimePixelTransform::GetMaxX() const
{
    return m_max_x_ns;
}
double
TimePixelTransform::GetVMinX() const
{
    return m_v_min_x_ns;
}
double
TimePixelTransform::GetVMaxX() const
{
    return m_v_max_x_ns;
}
double
TimePixelTransform::GetViewTimeOffsetNs() const
{
    return m_view_time_offset_ns;
}
double
TimePixelTransform::GetVWidth() const
{
    return m_v_width_ns;
}
float
TimePixelTransform::GetZoom() const
{
    return m_zoom;
}
double
TimePixelTransform::GetRangeX() const
{
    return m_range_x_ns;
}
float
TimePixelTransform::GetGraphSizeX() const
{
    return m_graph_size_x;
}
float
TimePixelTransform::GetGraphSizeY() const
{
    return m_graph_size_y;
}
double
TimePixelTransform::GetPixelsPerNs() const
{
    return m_pixels_per_ns;
}

void
TimePixelTransform::SetMinMaxX(double min_x, double max_x)
{
    if(min_x != m_min_x_ns || max_x != m_max_x_ns)
    {
        m_min_x_ns    = min_x;
        m_max_x_ns    = max_x;
        m_range_x_ns  = m_max_x_ns - m_min_x_ns;
        m_has_changed = true;
    }
}

void
TimePixelTransform::SetViewTimeOffsetNs(double view_time_offset_ns)
{
    if(view_time_offset_ns != m_view_time_offset_ns)
    {
        m_view_time_offset_ns = view_time_offset_ns;
        m_has_changed         = true;
    }
}

void
TimePixelTransform::SetZoom(float zoom)
{
    if(zoom != m_zoom)
    {
        m_zoom        = zoom;
        m_has_changed = true;
    }
}

void
TimePixelTransform::SetGraphSizeX(float graph_size_x, float graph_size_y)
{
    if(graph_size_x != m_graph_size_x || graph_size_y != m_graph_size_y)
    {
        m_graph_size_x = graph_size_x;
        m_graph_size_y = graph_size_y;
        m_has_changed  = true;
    }
}

}  // namespace View
}  // namespace RocProfVis
