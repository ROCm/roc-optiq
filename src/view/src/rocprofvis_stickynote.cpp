#include "imgui.h"
#include "rocprofvis_stickynote.h"

namespace RocProfVis
{
namespace View
{

StickyNote::StickyNote(double time_ns, float y_offset, const ImVec2& size,
                       const std::string& text)
: m_time_ns(time_ns)
, m_y_offset(y_offset)
, m_size(size)
, m_text(text)
{}

void
StickyNote::Render(ImDrawList* draw_list, const ImVec2& window_position, double v_min_x,
                   double pixels_per_ns)
{
    float  x          = static_cast<float>((m_time_ns - v_min_x) * pixels_per_ns);
    float  y          = m_y_offset;
    ImVec2 sticky_pos = ImVec2(window_position.x + x, window_position.y + y);
    ImVec2 sticky_max = ImVec2(sticky_pos.x + m_size.x, sticky_pos.y + m_size.y);

    draw_list->AddRectFilled(sticky_pos, sticky_max, IM_COL32(255, 255, 150, 220), 6.0f);
    draw_list->AddRect(sticky_pos, sticky_max, IM_COL32(200, 200, 80, 255), 6.0f, 0,
                       2.0f);
    ImVec2 text_pos = ImVec2(sticky_pos.x + 8, sticky_pos.y + 8);
    draw_list->AddText(text_pos, IM_COL32(60, 60, 60, 255), m_text.c_str());
}
bool
StickyNote::HandleDrag(const ImVec2& window_position, double v_min_x,
                       double pixels_per_ns)
{
    // Only allow drag if not resizing
    if(m_resizing) return false;

    float  x          = static_cast<float>((m_time_ns - v_min_x) * pixels_per_ns);
    float  y          = m_y_offset;
    ImVec2 sticky_pos = ImVec2(window_position.x + x, window_position.y + y);
    ImVec2 sticky_max = ImVec2(sticky_pos.x + m_size.x, sticky_pos.y + m_size.y);

    // --- Resize handle region ---
    const float handle_size = 12.0f;
    ImVec2 handle_pos = ImVec2(sticky_max.x - handle_size, sticky_max.y - handle_size);
    ImVec2 handle_max = ImVec2(sticky_max.x, sticky_max.y);

    ImVec2 mouse_pos = ImGui::GetMousePos();

    // If mouse is over resize handle, do not start drag
    if(ImGui::IsMouseHoveringRect(handle_pos, handle_max)) return false;

    bool mouse_down     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    if(!m_dragging && ImGui::IsMouseHoveringRect(sticky_pos, sticky_max) &&
       ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_dragging    = true;
        m_drag_offset = ImVec2(mouse_pos.x - sticky_pos.x, mouse_pos.y - sticky_pos.y);
    }

    if(m_dragging && mouse_down)
    {
        float new_x = mouse_pos.x - window_position.x - m_drag_offset.x;
        float new_y = mouse_pos.y - window_position.y - m_drag_offset.y;
        m_time_ns   = v_min_x + (new_x / pixels_per_ns);
        m_y_offset  = new_y;
        return true;
    }

    if(m_dragging && mouse_released)
    {
        m_dragging = false;
    }

    return m_dragging;
}

bool
StickyNote::HandleResize(const ImVec2& window_position, double v_min_x,
                         double pixels_per_ns)
{
    // Only allow resize if not dragging
    if(m_dragging) return false;

    float  x          = static_cast<float>((m_time_ns - v_min_x) * pixels_per_ns);
    float  y          = m_y_offset;
    ImVec2 sticky_pos = ImVec2(window_position.x + x, window_position.y + y);
    ImVec2 sticky_max = ImVec2(sticky_pos.x + m_size.x, sticky_pos.y + m_size.y);

    const float handle_size = 12.0f;
    ImVec2 handle_pos = ImVec2(sticky_max.x - handle_size, sticky_max.y - handle_size);
    ImVec2 handle_max = ImVec2(sticky_max.x, sticky_max.y);

    ImVec2 mouse_pos      = ImGui::GetMousePos();
    bool   mouse_down     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool   mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    // Draw resize handle (should be done in Render, but safe here for logic)
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32       handle_color =
        m_resizing ? IM_COL32(200, 100, 100, 255) : IM_COL32(200, 200, 80, 255);
    draw_list->AddRectFilled(handle_pos, handle_max, handle_color, 3.0f);

    if(!m_resizing && ImGui::IsMouseHoveringRect(handle_pos, handle_max) &&
       ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_resizing      = true;
        m_resize_offset = ImVec2(mouse_pos.x - sticky_max.x, mouse_pos.y - sticky_max.y);
    }

    if(m_resizing && mouse_down)
    {
        float new_width  = mouse_pos.x - sticky_pos.x - m_resize_offset.x;
        float new_height = mouse_pos.y - sticky_pos.y - m_resize_offset.y;
        m_size.x         = std::max(new_width, 60.0f);
        m_size.y         = std::max(new_height, 40.0f);
        return true;
    }

    if(m_resizing && mouse_released)
    {
        m_resizing = false;
    }

    return m_resizing;
}

}  // namespace View
}  // namespace RocProfVis
