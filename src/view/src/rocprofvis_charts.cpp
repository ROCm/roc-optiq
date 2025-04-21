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
, m_metadata_width(400.0f)
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

void
Charts::Render()
{
    ImGuiWindowFlags window_flags =
        ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoMove;

    if(ImGui::BeginChild((std::to_string(m_id)).c_str()), ImVec2(0, 0), true,
       window_flags)
    {
        ImVec2 parent_size = ImGui::GetContentRegionAvail();
        float  graph_width = parent_size.x - m_metadata_width;

        RenderMetaArea();
        ImGui::SameLine();
        RenderChart(graph_width);
        RenderResizeBar(parent_size);
    }
    ImGui::EndChild();
}

void
Charts::RenderResizeBar(const ImVec2& parent_size)
{
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("Resize Bar", ImVec2(parent_size.x, 4.0f), false);

    // Controls for graph resize.
    // ImGuiIO& io              = ImGui::GetIO();
    // bool     is_control_held = io.KeyCtrl;
    // if(is_control_held)
    {
        ImGui::Selectable(("##MovePositionLine" + std::to_string(m_id)).c_str(), false,
                          ImGuiSelectableFlags_AllowDoubleClick, ImVec2(0, 4.0f));
        if(ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        }
        if(ImGui::BeginDragDropSource(ImGuiDragDropFlags_None))
        {
            ImVec2 drag_delta = ImGui::GetMouseDragDelta(ImGuiMouseButton_Left);
            m_track_height    = m_track_height + (drag_delta.y);
            ImGui::ResetMouseDragDelta();
            ImGui::EndDragDropSource();
        }
        if(ImGui::BeginDragDropTarget())
        {
            ImGui::EndDragDropTarget();
        }
    }
    ImGui::EndChild();  // end resize handle
    ImGui::PopStyleColor();
}