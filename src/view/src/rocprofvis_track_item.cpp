#include "rocprofvis_track_item.h"
#include "rocprofvis_settings.h"
using namespace RocProfVis::View;

float TrackItem::s_metadata_width = 400.0f;

TrackItem::TrackItem(DataProvider& dp, int id, std::string name, float zoom,
                     float movement, double& min_x, double& max_x, float scale_x)
: m_data_provider(dp)
, m_id(id)
, m_zoom(zoom)
, m_movement(movement)
, m_min_x(min_x)
, m_max_x(max_x)
, m_scale_x(scale_x)
, m_name(name)
, m_track_height(75.0f)
, m_is_in_view_vertical(false)
, m_metadata_bg_color()
, m_metadata_padding(ImVec2(4.0f, 4.0f))
, m_resize_grip_thickness(4.0f)
, m_request_state(TrackDataRequestState::kIdle)
, m_is_resize(false)
, m_meta_area_clicked(false)
, m_settings(Settings::GetInstance())
{
    m_metadata_bg_color = m_settings.GetColor(static_cast<int>(Colors::kMetaDataColor));
}

bool
TrackItem::GetResizeStatus()
{
    return m_is_resize;
}
float
TrackItem::GetTrackHeight()
{
    return m_track_height;
}

const std::string&
TrackItem::GetName()
{
    return m_name;
}

int
TrackItem::GetID()
{
    return m_id;
}

void
TrackItem::SetSidebarSize(int sidebar_size)
{
    s_metadata_width = sidebar_size;
}

bool
TrackItem::IsInViewVertical()
{
    return m_is_in_view_vertical;
}

void
TrackItem::SetDistanceToView(float distance)
{
    m_distance_to_view_y = distance;
}

float
TrackItem::GetDistanceToView()
{
    return m_distance_to_view_y;
}

void
TrackItem::SetInViewVertical(bool in_view)
{
    m_is_in_view_vertical = in_view;
}

void
TrackItem::SetID(int id)
{
    m_id = id;
}

std::tuple<double, double>
TrackItem::GetMinMax()
{
    return std::make_tuple(m_min_x, m_max_x);
}

void
TrackItem::UpdateMovement(float zoom, float movement, double& min_x, double& max_x,
                          float scale_x, float y_scroll_position)
{
    m_zoom     = zoom;
    m_movement = movement;
    m_scale_x  = scale_x;
    m_min_x    = min_x;
    m_max_x    = max_x;
}

void
TrackItem::Render()
{
    m_metadata_bg_color = m_settings.GetColor(static_cast<int>(Colors::kMetaDataColor));
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(m_id)).c_str()), ImVec2(0, 0), true,
       window_flags)
    {
        ImVec2 parent_size = ImGui::GetContentRegionAvail();
        float  graph_width = parent_size.x - s_metadata_width;

        RenderMetaArea();
        ImGui::SameLine();

        RenderChart(graph_width);
        RenderResizeBar(parent_size);
    }
    ImGui::EndChild();
}

void
TrackItem::RenderResizeBar(const ImVec2& parent_size)
{
    m_is_resize = false;

    ImGui::SetCursorPos(ImVec2(0, parent_size.y - m_resize_grip_thickness));
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_settings.GetColor(static_cast<int>(Colors::kTransparent)));
    ImGui::BeginChild("Resize Bar", ImVec2(parent_size.x, m_resize_grip_thickness),
                      false);

    ImGui::Selectable(("##MovePositionLine" + std::to_string(m_id)).c_str(), false,
                      ImGuiSelectableFlags_AllowDoubleClick,
                      ImVec2(0, m_resize_grip_thickness));
    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        m_is_resize = true;
    }

    if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
    {
        ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
        m_track_height    = m_track_height + (drag_delta.y);
        ImGui::ResetMouseDragDelta();
        ImGui::EndDragDropSource();
        m_is_resize = true;
    }
    if(ImGui::BeginDragDropTarget())
    {
        ImGui::EndDragDropTarget();
    }
    ImGui::EndChild();  // end resize handle
    ImGui::PopStyleColor();
}

void
TrackItem::RequestData()
{
    if(m_request_state == TrackDataRequestState::kIdle)
    {
        m_request_state = TrackDataRequestState::kRequesting;
        m_data_provider.FetchTrack(m_id, m_data_provider.GetStartTime(),
                                   m_data_provider.GetEndTime(), 1000, 0);
    }
}
