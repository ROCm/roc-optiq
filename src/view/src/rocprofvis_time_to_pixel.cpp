// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_time_to_pixel.h"
#include <algorithm>
#include <limits>

namespace RocProfVis
{
namespace View
{

constexpr float  MAX_ZOOM_OUT_EXTENT = 1.0f;       // Maximum zoom out extent
constexpr float  MIN_ZOOM_DIVISOR    = 0.000001f;  // Prevent division by zero
constexpr double MIN_VIEW_WIDTH      = 10.0;       // Minimum viewable width in ns

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
    accurate to the data and is in screen ns.

    IMPORTANT: x_position must account for any offsets such as sidebar width.

    Return : Time in ns corresponding to the input x_position in pixels.
    */
    return (m_view_time_offset_ns + ((x_position / m_graph_size_x) * m_v_width_ns));
}

bool
TimePixelTransform::ValidateAndFixState()
{
    /*
    Validates all state variables and fixes any inconsistencies.
    Returns true if state is valid, false if state cannot be fixed.
    */

    // Validate min/max relationship
    if(m_max_x_ns <= m_min_x_ns)
    {
        return false;
    }

    m_range_x_ns = m_max_x_ns - m_min_x_ns;

    // Validate zoom
    m_zoom = std::max(m_zoom, MAX_ZOOM_OUT_EXTENT);
    m_zoom = std::max(m_zoom, MIN_ZOOM_DIVISOR);

    // Validate graph sizes
    if(m_graph_size_x <= 0.0f)
    {
        m_graph_size_x = 1.0f;  // Set to minimum valid value
    }
    if(m_graph_size_y <= 0.0f)
    {
        m_graph_size_y = 1.0f;  // Set to minimum valid value
    }

    // Compute view width to validate offset bounds
    if(m_range_x_ns > 0.0 && m_zoom > 0.0f)
    {
        double computed_v_width = m_range_x_ns / m_zoom;

        // Ensure minimum view width
        if(computed_v_width < MIN_VIEW_WIDTH)
        {
            computed_v_width = MIN_VIEW_WIDTH;
            // Adjust zoom to accommodate minimum width
            if(m_range_x_ns > 0.0)
            {
                m_zoom = static_cast<float>(m_range_x_ns / computed_v_width);
            }
        }

        // Clamp view offset to valid range
        double max_offset     = std::max(0.0, m_range_x_ns - computed_v_width);
        m_view_time_offset_ns = std::clamp(m_view_time_offset_ns, 0.0, max_offset);
    }
    else
    {
        // No valid range, reset offset
        m_view_time_offset_ns = 0.0;
    }

    return true;
}

void
TimePixelTransform::ComputePixelMapping()
{
    /*
    This function is used to compute the critical variables needed for the UI to operate.
    It is ultimately attempting to calculate m_pixels_per_ns to enable time to pixel
    conversion.
    */

    // Check if the user has actually changed anything if not nothing to compute.
    if(m_has_changed)
    {
        // Before doing any computation validate the data is correct
        if(!ValidateAndFixState())
        {
            // Invalid state, cannot compute - reset to safe defaults
            m_v_width_ns    = 0.0;
            m_v_min_x_ns    = m_min_x_ns;
            m_v_max_x_ns    = m_min_x_ns;
            m_pixels_per_ns = 0.0;
            m_has_changed   = false;
            return;
        }

        // Compute - ValidateAndFixState() has ensured all inputs are valid
        m_v_width_ns = (m_range_x_ns) / m_zoom;
        m_v_min_x_ns = m_min_x_ns + m_view_time_offset_ns;
        m_v_max_x_ns = m_v_min_x_ns + m_v_width_ns;

        // Compute pixels per nanosecond (view_span should always be > 0 after
        // ValidateAndFixState)
        double view_span = m_v_max_x_ns - m_v_min_x_ns;
        if(view_span > 0.0 && m_graph_size_x > 0.0f)
        {
            m_pixels_per_ns = m_graph_size_x / view_span;
        }
        else
        {
            m_pixels_per_ns = 0.0;
        }

        m_has_changed = false;
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
ImVec2
TimePixelTransform::GetGraphSize() const
{
    return ImVec2(m_graph_size_x, m_graph_size_y);
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

double
TimePixelTransform::NormalizeTime(double time_ns)
{
    /*
    This function normalizes input time_ns to UI time (relative to min_x).
    */
    return time_ns - m_min_x_ns;
}

void
TimePixelTransform::SetMinMaxX(double min_x, double max_x)
{
    // Validate: max must be greater than min
    if(max_x <= min_x)
    {
        // Invalid range, ignore the update
        return;
    }

    if(min_x != m_min_x_ns || max_x != m_max_x_ns)
    {
        m_min_x_ns   = min_x;
        m_max_x_ns   = max_x;
        m_range_x_ns = m_max_x_ns - m_min_x_ns;

        // Ensure view offset stays within new bounds
        m_view_time_offset_ns = std::clamp(m_view_time_offset_ns, 0.0,
                                           std::max(0.0, m_range_x_ns - m_v_width_ns));

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
    // Validate: zoom must be at least MIN_ZOOM and greater than MIN_ZOOM_DIVISOR
    // to prevent division by zero in calculations
    float validated_zoom = std::max(zoom, MAX_ZOOM_OUT_EXTENT);
    validated_zoom       = std::max(validated_zoom, MIN_ZOOM_DIVISOR);

    if(validated_zoom != m_zoom)
    {
        m_zoom        = validated_zoom;
        m_has_changed = true;
    }
}

void
TimePixelTransform::SetGraphSizeX(float graph_size_x, float graph_size_y)
{
    if(graph_size_x <= 0.0f || graph_size_y <= 0.0f)
    {
        return;
    }

    if(graph_size_x != m_graph_size_x || graph_size_y != m_graph_size_y)
    {
        m_graph_size_x = graph_size_x;
        m_graph_size_y = graph_size_y;
        m_has_changed  = true;
    }
}

}  // namespace View
}  // namespace RocProfVis
