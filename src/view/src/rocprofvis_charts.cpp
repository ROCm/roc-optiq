#include "rocprofvis_charts.h"

using namespace RocProfVis::View;

Charts::Charts(int id, std::string name, float zoom, float movement, double& min_x,
               double& max_x, float scale_x)
: m_id(id)
, m_zoom(zoom)
, m_movement(movement)
, m_min_x(min_x)
, m_max_x(max_x)
, m_scale_x(scale_x)
, m_name(name)
, m_track_height(75.0f)
, m_is_chart_visible(true)
{}

float
Charts::GetTrackHeight()
{
    return m_track_height;
}

const std::string&
Charts::GetName()
{
    return m_name;
}

int
Charts::ReturnChartID()
{
    return m_id;
}

bool
Charts::GetVisibility()
{
    return m_is_chart_visible;
}

float
Charts::GetMovement()
{
    return 0;  // m_movement_since_unload - m_y_movement;
}

void
Charts::SetID(int id)
{
    m_id = id;
}

std::tuple<double, double>
Charts::GetMinMax()
{
    return std::make_tuple(m_min_x, m_max_x);
}
