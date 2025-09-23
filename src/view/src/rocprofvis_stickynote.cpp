#include "rocprofvis_stickynote.h"
#include "imgui.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
namespace RocProfVis
{
namespace View
{

static int s_unique_id_counter = 0;
StickyNote::StickyNote(double time_ns, float y_offset, const ImVec2& size,
                       const std::string& text, const std::string& title,
                       const std::string& project_id, double v_min, double v_max)
: m_time_ns(time_ns)
, m_y_offset(y_offset)
, m_size(size)
, m_text(text)
, m_title(title)
, m_project_id(project_id)
, m_id(s_unique_id_counter)
, m_is_visible(true)
, m_v_min_x(v_min)
, m_v_max_x(v_max)
{
    s_unique_id_counter = s_unique_id_counter + 1;
}

double
StickyNote::GetTimeNs() const
{
    return m_time_ns;
}
float
StickyNote::GetYOffset() const
{
    return m_y_offset;
}
double
StickyNote::GetVMinX() const
{
    return m_v_min_x;
}
double
StickyNote::GetVMaxX() const
{
    return m_v_max_x;
}

ImVec2
StickyNote::GetSize() const
{
    return m_size;
}
const std::string&
StickyNote::GetText() const
{
    return m_text;
}

int
StickyNote::GetID() const
{
    return m_id;
}
const std::string&
StickyNote::GetTitle() const
{
    return m_title;
}

void
StickyNote::SetVisibility(bool visible)
{
    m_is_visible = visible;
}

bool
StickyNote::IsVisible() const
{
    return m_is_visible;
}

void
StickyNote::SetText(std::string title)
{
    m_text = title;
}
void
StickyNote::SetTitle(std::string title)
{
    m_title = title;
}
void
StickyNote::Render(ImDrawList* draw_list, const ImVec2& window_position, double v_min_x,
                   double pixels_per_ns)
{
    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            bg_color     = settings.GetColor(Colors::kFillerColor);
    ImU32            border_color = settings.GetColor(Colors::kBorderColor);
    ImU32            text_color   = settings.GetColor(Colors::kRulerTextColor);
    ImU32            shadow_color = IM_COL32(0, 0, 0, 60);

    const float rounding      = 8.0f;
    const float border_thick  = 2.0f;
    const float shadow_offset = 4.0f;
    const float margin        = 12.0f;
    const float header_height = 28.0f;
    const float edit_btn_size = 20.0f;

    float  x           = static_cast<float>((m_time_ns - v_min_x) * pixels_per_ns);
    float  y           = m_y_offset;
    ImVec2 sticky_pos  = ImVec2(window_position.x + x, window_position.y + y);
    ImVec2 sticky_size = m_size;

    // Set the cursor to the sticky note position inside the parent window
    ImGui::SetCursorScreenPos(sticky_pos);

    // Begin a child window for the sticky note
    ImGui::BeginChild(
        ("StickyNoteChild##" + std::to_string(reinterpret_cast<uintptr_t>(this))).c_str(),
        sticky_size, false,
        ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse |
            ImGuiWindowFlags_NoBackground);

    // Get the child window's draw list and position
    ImDrawList* child_draw_list = ImGui::GetWindowDrawList();
    ImVec2      child_offset    = ImGui::GetWindowPos();

    // Draw sticky note graphics (background, border, accent bar)
    child_draw_list->AddRectFilled(child_offset + ImVec2(shadow_offset, shadow_offset),
                                   child_offset + ImVec2(sticky_size.x + shadow_offset,
                                                         sticky_size.y + shadow_offset),
                                   shadow_color, rounding);

    child_draw_list->AddRectFilled(child_offset, child_offset + sticky_size, bg_color,
                                   rounding);
    child_draw_list->AddRect(child_offset, child_offset + sticky_size, border_color,
                             rounding, 0, border_thick);

    ImU32 accent_color = settings.GetColor(Colors::kHighlightChart);
    child_draw_list->AddRectFilled(child_offset,
                                   child_offset + ImVec2(sticky_size.x, header_height),
                                   accent_color, rounding, ImDrawFlags_RoundCornersTop);

    // Title (top left, clipped if too long)
    float  title_padding_x = margin;
    float  title_padding_y = 4.0f;
    float  title_max_width = sticky_size.x - 2 * margin - edit_btn_size - margin;
    ImVec2 title_pos       = ImVec2(title_padding_x, title_padding_y);
    ImVec4 title_clip =
        ImVec4(child_offset.x + title_pos.x, child_offset.y + title_pos.y,
               child_offset.x + title_pos.x + title_max_width,
               child_offset.y + title_pos.y + header_height - title_padding_y);

    child_draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                             child_offset + title_pos, text_color, m_title.c_str(),
                             nullptr, title_max_width, &title_clip);

    // Edit Button (top right, local to child window)
    ImVec2 edit_btn_local_pos = ImVec2(sticky_size.x - edit_btn_size - margin / 2,
                                       (header_height - edit_btn_size) / 2);
    ImGui::SetCursorPos(edit_btn_local_pos);

    if(ImGui::Button(
           ("EditSticky##" + std::to_string(reinterpret_cast<uintptr_t>(this))).c_str(),
           ImVec2(edit_btn_size, edit_btn_size)))
    {
        EventManager::GetInstance()->AddEvent(
            std::make_shared<StickyNoteEvent>(m_id, m_project_id));
        std::cout << "Edit Sticky Note ID: " << m_id << " Title: " << m_title
                  << std::endl;
    }

    // Draw edit button graphics over the button
    ImVec2 edit_btn_max = ImVec2(edit_btn_local_pos.x + edit_btn_size,
                                 edit_btn_local_pos.y + edit_btn_size);
    child_draw_list->AddRectFilled(child_offset + edit_btn_local_pos,
                                   child_offset + edit_btn_max,
                                   IM_COL32(220, 220, 220, 255), 6.0f);
    child_draw_list->AddLine(
        child_offset + ImVec2(edit_btn_local_pos.x + 5, edit_btn_local_pos.y + 15),
        child_offset + ImVec2(edit_btn_local_pos.x + 15, edit_btn_local_pos.y + 5),
        IM_COL32(80, 80, 80, 255), 2.0f);

    // Text Area (local to child window)
    float  text_area_x = margin;
    float  text_area_y = header_height + margin;
    float  text_area_w = sticky_size.x - 2 * margin;
    float  text_area_h = sticky_size.y - header_height - 2 * margin;
    ImVec2 text_pos    = ImVec2(text_area_x, text_area_y);

    const ImVec4 clip_rect =
        ImVec4(child_offset.x + text_area_x, child_offset.y + text_area_y,
               child_offset.x + text_area_x + text_area_w,
               child_offset.y + text_area_y + text_area_h);

    child_draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(),
                             child_offset + text_pos, text_color, m_text.c_str(), nullptr,
                             text_area_w, &clip_rect);

    ImGui::EndChild();
}

bool
StickyNote::HandleDrag(const ImVec2& window_position, double v_min_x, double v_max_x,
                       double pixels_per_ns, int& dragged_id)
{
    if(m_resizing) return false;

    float  x          = static_cast<float>((m_time_ns - v_min_x) * pixels_per_ns);
    float  y          = m_y_offset;
    ImVec2 sticky_pos = ImVec2(window_position.x + x, window_position.y + y);
    ImVec2 sticky_max = ImVec2(sticky_pos.x + m_size.x, sticky_pos.y + m_size.y);

    const float handle_size = 12.0f;
    ImVec2 handle_pos = ImVec2(sticky_max.x - handle_size, sticky_max.y - handle_size);
    ImVec2 handle_max = ImVec2(sticky_max.x, sticky_max.y);

    ImVec2 mouse_pos = ImGui::GetMousePos();

    if(ImGui::IsMouseHoveringRect(handle_pos, handle_max)) return false;

    bool mouse_down     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    // Only allow drag if no other note is being dragged or this is the one
    if((dragged_id == -1 || dragged_id == m_id) && !m_dragging &&
       ImGui::IsMouseHoveringRect(sticky_pos, sticky_max) &&
       ImGui::IsMouseClicked(ImGuiMouseButton_Left))
    {
        m_dragging    = true;
        m_drag_offset = ImVec2(mouse_pos.x - sticky_pos.x, mouse_pos.y - sticky_pos.y);
        dragged_id    = m_id;
    }

    if(m_dragging && mouse_down)
    {
        float new_x = mouse_pos.x - window_position.x - m_drag_offset.x;
        float new_y = mouse_pos.y - window_position.y - m_drag_offset.y;
        m_time_ns   = v_min_x + (new_x / pixels_per_ns);
        m_y_offset  = new_y;
        m_v_max_x   = v_max_x;
        m_v_min_x   = v_min_x;
        return true;
    }

    if(m_dragging && mouse_released)
    {
        m_dragging = false;
        dragged_id = -1;
    }

    return m_dragging;
}

bool
StickyNote::HandleResize(const ImVec2& window_position, double v_min_x, double v_max_x,
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
        m_v_max_x        = v_max_x;
        m_v_min_x        = v_min_x;
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
