// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_stickynote.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_click_manager.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_settings_manager.h"
#include <algorithm>
#include <spdlog/spdlog.h>
#include <spdlog/spdlog.h>

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
StickyNote::Render(ImDrawList* draw_list, const ImVec2& window_position, std::shared_ptr<TimePixelTransform> tpt)
{
    if(!tpt)
    {
        spdlog::error("StickyNote::Render: conversion_manager shared_ptr is null, cannot render");
        return;
    }
    SettingsManager& settings     = SettingsManager::GetInstance();
    ImU32            bg_color     = settings.GetColor(Colors::kFillerColor);
    ImU32            border_color = settings.GetColor(Colors::kBorderColor);
    ImU32            accent_color = settings.GetColor(Colors::kStickyNote);

    const float rounding      = 8.0f;
    const float margin        = 12.0f;
    const float header_height = 32.0f;
    const float edit_btn_size = 30.0f;

    float  x           = tpt->TimeToPixel(m_time_ns);
    float  y           = m_y_offset;
    ImVec2 sticky_pos  = ImVec2(window_position.x + x, window_position.y + y);
    ImVec2 sticky_size = m_size;

    // Set the cursor to the sticky note position inside the parent window
    ImGui::SetCursorScreenPos(sticky_pos);

    // Round corners
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 8.0f);

    ImGui::SetCursorScreenPos(sticky_pos);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, bg_color);
    ImGui::PushStyleColor(ImGuiCol_Border, border_color);

    ImGui::BeginChild(
        ("StickyNoteChild##" + std::to_string(reinterpret_cast<uintptr_t>(this))).c_str(),
        sticky_size, true);

    // Header (accent bar)
    ImGui::PushStyleColor(ImGuiCol_ChildBg, accent_color);
    ImGui::BeginChild("Header", ImVec2(sticky_size.x, header_height), false);

    // Title (left)
    ImGui::SetCursorPos(ImVec2(margin, 4.0f));
    ImGui::TextUnformatted(m_title.c_str());

    // Edit button (right)
    ImGui::SetCursorPos(ImVec2(sticky_size.x - edit_btn_size - margin / 2,
                               (header_height - edit_btn_size) / 2));
    ImGui::PushFont(
        SettingsManager::GetInstance().GetFontManager().GetIconFont(FontType::kDefault));
    if(ImGui::Button((std::string(ICON_EDIT) + "##" +
                      std::to_string(reinterpret_cast<uintptr_t>(this)))
                         .c_str(),
                     ImVec2(edit_btn_size, edit_btn_size)))
    {
        EventManager::GetInstance()->AddEvent(
            std::make_shared<StickyNoteEvent>(m_id, m_project_id));
    }
    ImGui::PopFont();

    ImGui::EndChild();

    ImGui::PopStyleColor();

    // Text area
    ImGui::SetCursorPos(ImVec2(margin, header_height + margin));
    ImGui::PushTextWrapPos(sticky_size.x - margin);
    ImGui::TextUnformatted(m_text.c_str());
    ImGui::PopTextWrapPos();

    ImGui::EndChild();
    // Cover hover case for input control
    ImVec2 sticky_max =
        ImVec2(sticky_pos.x + sticky_size.x, sticky_pos.y + sticky_size.y);
    if(ImGui::IsMouseHoveringRect(sticky_pos, sticky_max))
    {
        TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kInteractiveLayer);
    }

    else
    {
        TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kNone);
    }

    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}

bool
StickyNote::HandleDrag(const ImVec2&                               window_position,
                       std::shared_ptr<TimePixelTransform> conversion_manager, int& dragged_id)
{
    if(!conversion_manager)
    {
        spdlog::error("StickyNote::HandleDrag: conversion_manager shared_ptr is null");
        return false;
    }
    if(m_resizing) return false;

    float  x          = conversion_manager->TimeToPixel(m_time_ns);
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
        TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kInteractiveLayer);
    }

    if(m_dragging && mouse_down)
    {
        float  new_x       = mouse_pos.x - window_position.x - m_drag_offset.x;
        float  new_y       = mouse_pos.y - window_position.y - m_drag_offset.y;
        ImVec2 window_size = ImGui::GetWindowSize();
        new_y =
            std::clamp(new_y, 0.0f, window_size.y - m_size.y);  // Limit to window height
        new_x =
            std::clamp(new_x, 0.0f, window_size.x - m_size.x);  // Limit to window width

        m_time_ns  = conversion_manager->PixelToTime(new_x);
        m_y_offset = new_y;
        m_v_max_x  = conversion_manager->GetVMaxX();
        m_v_min_x  = conversion_manager->GetVMinX();

        return true;
    }

    if(m_dragging && mouse_released)
    {
        m_dragging = false;
        dragged_id = -1;
        TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kNone);
    }

    return m_dragging;
}

bool
StickyNote::HandleResize(const ImVec2&       window_position,
                         std::shared_ptr<TimePixelTransform> conversion_manager)
{
    if(!conversion_manager)
    {
        spdlog::error("StickyNote::HandleResize: conversion_manager shared_ptr is null");
        return false;
    }
    // Only allow resize if not dragging
    if(m_dragging) return false;

    float  x          = conversion_manager->TimeToPixel(m_time_ns);
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
        m_v_max_x        = conversion_manager->GetVMaxX();
        m_v_min_x        = conversion_manager->GetVMinX();
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