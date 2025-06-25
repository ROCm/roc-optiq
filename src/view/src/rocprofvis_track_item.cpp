#include "rocprofvis_track_item.h"
#include "rocprofvis_settings.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_debug_window.h"
#include "widgets/rocprofvis_gui_helpers.h"

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
, m_track_content_height(0.0f)
, m_min_track_height(10.0f)
, m_is_in_view_vertical(false)
, m_metadata_padding(ImVec2(4.0f, 4.0f))
, m_resize_grip_thickness(4.0f)
, m_request_state(TrackDataRequestState::kIdle)
, m_is_resize(false)
, m_meta_area_clicked(false)
, m_meta_area_scale_width(0.0f)
, m_settings(Settings::GetInstance())
, m_selected(false)
{}

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
    s_metadata_width = static_cast<float>(sidebar_size);
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

bool
TrackItem::IsSelected() const
{
    return m_selected;
}

void
TrackItem::SetSelected(bool selected)
{
    m_selected = selected;
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
    (void) y_scroll_position;
}

void
TrackItem::Render(float width)
{
    ImGui::BeginGroup();

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          m_selected ? m_settings.GetColor(Colors::kMetaDataColorSelected)
                                     : m_settings.GetColor(Colors::kMetaDataColor));
    ImGui::BeginChild(("MetaData_" + std::to_string(m_id)).c_str(),
                      ImVec2(s_metadata_width, m_track_height), true,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderMetaArea();
    ImGui::EndChild();
    ImGui::PopStyleColor();
    ImGui::SameLine();

    ImGui::BeginChild(("Chart_" + std::to_string(m_id)).c_str(),
                      ImVec2(width, m_track_height), false,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    RenderChart(width);
    RenderResizeBar(ImVec2(width, m_track_height));
    ImGui::EndChild();

    ImGui::EndGroup();
}

void
TrackItem::RenderMetaArea()
{
    // Shrink the meta data content area by one unit in the vertical direction so that the
    // borders rendered by the parent are visible other wise the bg fill will cover them
    // up.
    ImVec2 m_metadata_shrink_padding(0.0f, 1.0f);
    ImVec2 outer_container_size = ImGui::GetContentRegionAvail();
    m_track_content_height      = m_track_height - m_metadata_shrink_padding.y * 2.0f;

    ImGui::SetCursorPos(m_metadata_shrink_padding);
    if(ImGui::BeginChild("MetaData Area",
                         ImVec2(s_metadata_width, outer_container_size.y -
                                                      m_metadata_shrink_padding.y * 2.0f),
                         ImGuiChildFlags_None))
    {
        ImVec2 content_size = ImGui::GetContentRegionAvail();

        // handle mouse click
        ImVec2 container_pos  = ImGui::GetWindowPos();
        ImVec2 container_size = ImGui::GetWindowSize();

        bool is_mouse_inside = ImGui::IsMouseHoveringRect(
            container_pos, ImVec2(container_pos.x + container_size.x,
                                  container_pos.y + container_size.y));

        m_meta_area_clicked = false;
        if(is_mouse_inside)
        {
            if(ImGui::IsMouseClicked(ImGuiMouseButton_Left))
            {
                m_meta_area_clicked = true;
            }
        }

        // Set padding for the child window (Note this done using SetCursorPos
        // because ImGuiStyleVar_WindowPadding has no effect on child windows without
        // borders)
        ImGui::SetCursorPos(m_metadata_padding);
        // Adjust content size to account for padding
        content_size.x -= m_metadata_padding.x * 2;
        content_size.y -= m_metadata_padding.x * 2;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 4.0f));
        if(ImGui::BeginChild(
               "MetaData Content",
               ImVec2(content_size.x - m_meta_area_scale_width, content_size.y),
               ImGuiChildFlags_None))
        {
            ImGui::Text(m_name.c_str());

            if(m_request_state != TrackDataRequestState::kIdle)
            {
                ImGuiStyle& style = ImGui::GetStyle();

                float  dot_radius  = 10.0f;
                int    num_dots    = 3;
                float  dot_spacing = 5.0f;
                float  anim_speed  = 7.0f;
                ImVec2 dot_size =
                    MeasureLoadingIndicatorDots(dot_radius, num_dots, dot_spacing);

                ImVec2 cursor_pos = ImGui::GetCursorPos();
                ImGui::SetCursorPos(
                    ImVec2(cursor_pos.x + (content_size.x - dot_size.x) * 0.5f,
                           cursor_pos.y + style.ItemSpacing.y));

                RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing,
                                           m_settings.GetColor(Colors::kScrollBarColor),
                                           anim_speed);
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();

        ImGui::SameLine();

        ImVec2 scale_container_size(m_meta_area_scale_width, content_size.y);
        RenderMetaAreaScale(scale_container_size);
    }
    ImGui::EndChild();  // end metadata area
}

void
TrackItem::RenderMetaAreaScale(ImVec2& container_size)
{
    (void) container_size;
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
        if(m_track_height < m_min_track_height)
        {
            m_track_height = m_min_track_height;
        }

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
TrackItem::RequestData(double min, double max, float width)
{
    if(m_request_state == TrackDataRequestState::kIdle)
    {
        m_request_state = TrackDataRequestState::kRequesting;
        bool result =
            m_data_provider.FetchTrack(m_id, min, max, static_cast<uint32_t>(width));
        if(!result)
        {
            m_request_state = TrackDataRequestState::kIdle;
            spdlog::warn("Failed to request data for track {} from {} to {}", m_id,
                         min - m_data_provider.GetStartTime(),
                         max - m_data_provider.GetStartTime());
        }
        else
        {
            spdlog::debug("Fetching from {} to {} ( {} ) at zoom {} for track {}",
                          min - m_data_provider.GetStartTime(),
                          max - m_data_provider.GetStartTime(), max - min, m_zoom, m_id);
        }
    }
    else
    {
        spdlog::warn("Fetch request rejected for track {}, already pending...", m_id);
    }
}
