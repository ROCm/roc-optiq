// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_stickynote.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_click_manager.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include <algorithm>
#include <spdlog/spdlog.h>

namespace RocProfVis
{
namespace View
{

static int s_unique_id_counter = 0;
StickyNote::StickyNote(double time_ns, float y_offset, const ImVec2& size,
                       const std::string& text, const std::string& title,
                       const std::string& project_id, double v_min, double v_max,
                       bool is_minimized)
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
, m_is_minimized(is_minimized)
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
bool
StickyNote::Render(ImDrawList* draw_list, const ImVec2& window_position,
                   std::shared_ptr<TimePixelTransform> tpt)
{
    if(!tpt)
    {
        spdlog::error(
            "StickyNote::Render: conversion_manager shared_ptr is null, cannot render");
        return false;
    }
    SettingsManager& settings      = SettingsManager::GetInstance();
    const bool       use_dark_mode = settings.GetUserSettings().display_settings.use_dark_mode;
    ImU32            bg_color          = use_dark_mode ? IM_COL32(58, 53, 36, 245)
                                                       : IM_COL32(255, 252, 228, 248);
    ImU32            border_color      = use_dark_mode ? IM_COL32(62, 116, 168, 220)
                                                       : IM_COL32(91, 139, 184, 205);
    ImU32            header_color      = use_dark_mode ? IM_COL32(70, 62, 40, 248)
                                                       : IM_COL32(255, 247, 204, 248);
    ImU32            shadow_color      = use_dark_mode ? IM_COL32(0, 0, 0, 85)
                                                       : IM_COL32(76, 95, 128, 35);
    ImU32            icon_hover_color =
        ApplyAlpha(settings.GetColor(Colors::kButtonHovered), 0.74f);
    ImU32 icon_active_color =
        ApplyAlpha(settings.GetColor(Colors::kButtonActive), 0.86f);
    ImU32            text_color        = use_dark_mode ? IM_COL32(238, 243, 255, 255)
                                                       : IM_COL32(25, 38, 56, 255);
    ImU32            muted_text_color  = use_dark_mode ? IM_COL32(145, 156, 174, 255)
                                                       : IM_COL32(92, 106, 126, 255);
    ImU32            accent_color      = use_dark_mode ? IM_COL32(225, 203, 78, 235)
                                                       : IM_COL32(168, 128, 0, 235);

    const float rounding      = settings.GetDefaultStyle().ChildRounding;
    const float margin        = 14.0f;
    const float header_height = 36.0f;
    const float icon_btn_gap  = 8.0f;

    float  x           = tpt->TimeToPixel(m_time_ns);
    float  y           = m_y_offset;
    ImVec2 sticky_pos  = ImVec2(window_position.x + x, window_position.y + y);
    ImVec2 sticky_size = m_size;

    // Set the cursor to the sticky note position inside the parent window
    ImGui::SetCursorScreenPos(sticky_pos);

    if(m_is_minimized)
    {
        std::string child_id = "StickyButtonArea##" + std::to_string(m_id);
        ImGui::BeginChild(child_id.c_str(), m_size, false, ImGuiWindowFlags_None);

        ImFont* icon_font = SettingsManager::GetInstance().GetFontManager().GetIconFont(
            FontType::kDefault);
        ImGui::PushFont(icon_font);
        ImVec2 icon_size = ImGui::CalcTextSize(ICON_STICKY_NOTE);
        ImVec2 padding   = ImGui::GetStyle().FramePadding;
        const float minimized_btn_size =
            std::max(icon_size.x + padding.x * 2.0f, icon_size.y + padding.y * 2.0f);
        ImVec2 btn_max = ImVec2(sticky_pos.x + minimized_btn_size,
                                sticky_pos.y + minimized_btn_size);
        if(draw_list)
        {
            draw_list->AddRectFilled(ImVec2(sticky_pos.x + 2.0f, sticky_pos.y + 3.0f),
                                     ImVec2(btn_max.x + 2.0f, btn_max.y + 3.0f),
                                     shadow_color, rounding * 0.6f);
            draw_list->AddRectFilled(sticky_pos, btn_max, bg_color, rounding * 0.6f);
            draw_list->AddRect(sticky_pos, btn_max, border_color, rounding * 0.6f);
        }
        ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, icon_hover_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, icon_active_color);
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::Button(
            (std::string(ICON_STICKY_NOTE) + "##" + std::to_string(m_id)).c_str(),
            ImVec2(minimized_btn_size, minimized_btn_size));
        if(ImGui::IsItemHovered() && IsMouseReleasedWithDragCheck(ImGuiMouseButton_Left))
        {
            m_expanded_screen_pos = ImVec2(-1, -1);
            m_is_minimized        = false;
        }
        ImGui::PopStyleColor(4);
        ImGui::PopFont();

        bool blocks_timeline_input = ImGui::IsMouseHoveringRect(sticky_pos, btn_max);
        if(blocks_timeline_input)
        {
            TimelineFocusManager::GetInstance().RequestLayerFocus(
                Layer::kInteractiveLayer);
        }
        else
        {
            TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kNone);
        }

        ImGui::EndChild();
        return blocks_timeline_input;
    }
    else
    {
        ImVec2 window_size = ImGui::GetWindowSize();
        float  adjusted_x;
        float  adjusted_y;

        if(m_expanded_screen_pos.x >= 0 && m_expanded_screen_pos.y >= 0)
        {
            adjusted_x = m_expanded_screen_pos.x;
            adjusted_y = m_expanded_screen_pos.y;
        }
        else
        {
            float note_end_x = x + sticky_size.x;
            float note_end_y = y + sticky_size.y;
            adjusted_x       = x;
            adjusted_y       = y;

            if(note_end_x > window_size.x)
            {
                adjusted_x = x - (sticky_size.x * 1.1f);
                adjusted_x = std::max(adjusted_x, 0.0f);
            }
            if(note_end_y > window_size.y)
            {
                adjusted_y = y - (sticky_size.y * 1.1f);
                adjusted_y = std::max(adjusted_y, 0.0f);
            }
        }

        sticky_pos = ImVec2(window_position.x + adjusted_x, window_position.y + adjusted_y);
        ImGui::SetCursorPos(ImVec2(adjusted_x, adjusted_y));

        if(draw_list)
        {
            draw_list->AddRectFilled(
                ImVec2(sticky_pos.x + 3.0f, sticky_pos.y + 4.0f),
                ImVec2(sticky_pos.x + sticky_size.x + 3.0f,
                       sticky_pos.y + sticky_size.y + 4.0f),
                shadow_color, rounding);
        }

        // Round corners
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, rounding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, bg_color);
        ImGui::PushStyleColor(ImGuiCol_Border, border_color);

        ImGui::BeginChild(
            ("StickyNoteChild##" + std::to_string(reinterpret_cast<uintptr_t>(this)))
                .c_str(),
            sticky_size, true);

        ImDrawList* note_draw_list = ImGui::GetWindowDrawList();
        const ImVec2 note_min      = ImGui::GetWindowPos();
        const ImVec2 header_min    = note_min;
        const ImVec2 header_max    = ImVec2(note_min.x + sticky_size.x,
                                            note_min.y + header_height);
        note_draw_list->AddRectFilled(header_min, header_max, header_color, rounding,
                                      ImDrawFlags_RoundCornersTop);
        note_draw_list->AddRectFilled(header_min,
                                      ImVec2(note_min.x + 3.0f, note_min.y + sticky_size.y),
                                      ApplyAlpha(accent_color, 0.78f), rounding,
                                      ImDrawFlags_RoundCornersLeft);
        note_draw_list->AddLine(ImVec2(note_min.x, header_max.y),
                                ImVec2(note_min.x + sticky_size.x, header_max.y),
                                ApplyAlpha(border_color, 0.42f));

        // Title (left)
        ImGui::SetCursorPos(
            ImVec2(margin, (header_height - ImGui::GetTextLineHeight()) * 0.5f));
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::TextUnformatted(m_title.c_str());
        ImGui::PopStyleColor();

        // Edit button (left of close)
        ImFont* action_icon_font = SettingsManager::GetInstance().GetFontManager().GetIconFont(
            FontType::kDefault);
        ImGui::PushFont(action_icon_font);
        ImVec2 edit_icon_size  = ImGui::CalcTextSize(ICON_EDIT);
        ImVec2 close_icon_size = ImGui::CalcTextSize(ICON_X_CIRCLED);
        const float action_btn_size =
            std::max({34.0f, edit_icon_size.x + 14.0f, close_icon_size.x + 14.0f});
        const float action_btn_y = (header_height - action_btn_size) * 0.5f;

        ImGui::SetCursorPos(ImVec2(sticky_size.x - action_btn_size * 2.0f - margin -
                                       icon_btn_gap,
                                   action_btn_y));
        ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, icon_hover_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, icon_active_color);
        ImGui::PushStyleColor(ImGuiCol_Text, muted_text_color);
        if(ImGui::Button((std::string(ICON_EDIT) + "##" +
                          std::to_string(reinterpret_cast<uintptr_t>(this)))
                             .c_str(),
                         ImVec2(action_btn_size, action_btn_size)))
        {
            EventManager::GetInstance()->AddEvent(
                std::make_shared<StickyNoteEvent>(m_id, m_project_id));
        }
        ImGui::PopStyleColor(4);

        ImGui::SetCursorPos(ImVec2(sticky_size.x - action_btn_size - margin,
                                   action_btn_y));
        ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kTransparent));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, icon_hover_color);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, icon_active_color);
        ImGui::PushStyleColor(ImGuiCol_Text, muted_text_color);
        if(ImGui::Button(
               (std::string(ICON_X_CIRCLED) + "##icon_switch_" + std::to_string(m_id))
                   .c_str(),
               ImVec2(action_btn_size, action_btn_size)))
        {
            m_is_minimized = true;
        }
        ImGui::PopStyleColor(4);
        ImGui::PopFont();

        // Scroll only the note body; the header/actions stay pinned.
        const float body_y      = header_height + margin;
        const float body_height = std::max(0.0f, sticky_size.y - body_y - margin);
        ImGui::SetCursorPos(ImVec2(margin, body_y));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kTransparent));
        ImGui::BeginChild("StickyNoteBody", ImVec2(sticky_size.x - margin * 2.0f,
                                                   body_height), false);
        ImGui::PushStyleColor(ImGuiCol_Text, text_color);
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
        ImGui::TextUnformatted(m_text.c_str());
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        ImGui::EndChild();
        // Cover hover case for input control
        ImVec2 sticky_max =
            ImVec2(sticky_pos.x + sticky_size.x, sticky_pos.y + sticky_size.y);
        bool blocks_timeline_input =
            ImGui::IsMouseHoveringRect(sticky_pos, sticky_max) &&
            !HotkeyManager::GetInstance().IsActionHeld(HotkeyActionId::kRegionSelect);
        if(blocks_timeline_input)
        {
            TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kInteractiveLayer);
        }
        else
        {
            TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kNone);
        }

        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        return blocks_timeline_input;
    }
}

bool
StickyNote::HandleDrag(const ImVec2&                       window_position,
                       std::shared_ptr<TimePixelTransform> conversion_manager,
                       int&                                dragged_id)
{
    if(!conversion_manager)
    {
        spdlog::error("StickyNote::HandleDrag: conversion_manager shared_ptr is null");
        return false;
    }
    if(m_resizing) return false;

    ImVec2 drag_pos;
    ImVec2 drag_max;
    float  drag_w;
    float  drag_h;

    float x = conversion_manager->TimeToPixel(m_time_ns);
    float y = m_y_offset;

    if(m_is_minimized)
    {
        ImVec2 icon_pos = ImVec2(window_position.x + x, window_position.y + y);

        ImFont* icon_font = SettingsManager::GetInstance().GetFontManager().GetIconFont(
            FontType::kDefault);
        ImGui::PushFont(icon_font);
        ImVec2 icon_size = ImGui::CalcTextSize(ICON_STICKY_NOTE);
        ImGui::PopFont();

        ImVec2 padding = ImGui::GetStyle().FramePadding;
        drag_w         = icon_size.x + padding.x * 2.0f;
        drag_h         = icon_size.y + padding.y * 2.0f;
        drag_pos       = icon_pos;
        drag_max       = ImVec2(icon_pos.x + drag_w, icon_pos.y + drag_h);
    }
    else
    {
        if(m_expanded_screen_pos.x >= 0 && m_expanded_screen_pos.y >= 0)
        {
            drag_pos = ImVec2(window_position.x + m_expanded_screen_pos.x,
                              window_position.y + m_expanded_screen_pos.y);
        }
        else
        {
            drag_pos = ImVec2(window_position.x + x, window_position.y + y);
        }
        // Expanded notes drag by the header so the scrollable body can own
        // scrollbar/body interactions.
        drag_w   = m_size.x;
        drag_h   = 36.0f;
        drag_max = ImVec2(drag_pos.x + drag_w, drag_pos.y + drag_h);
    }

    ImVec2 mouse_pos      = ImGui::GetMousePos();
    bool   mouse_down     = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    bool   mouse_released = ImGui::IsMouseReleased(ImGuiMouseButton_Left);

    if(!m_is_minimized)
    {
        const float handle_size = 12.0f;
        ImVec2      sticky_max  = ImVec2(drag_pos.x + m_size.x, drag_pos.y + m_size.y);
        ImVec2      handle_pos  = ImVec2(sticky_max.x - handle_size,
                                         sticky_max.y - handle_size);
        if(ImGui::IsMouseHoveringRect(handle_pos, sticky_max))
            return false;
    }

    // Only allow drag if no other note is being dragged or this is the one
    if((dragged_id == -1 || dragged_id == m_id) && !m_dragging &&
       ImGui::IsMouseHoveringRect(drag_pos, drag_max) &&
       ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !HotkeyManager::GetInstance().IsActionHeld(HotkeyActionId::kRegionSelect))
    {
        m_dragging    = true;
        m_drag_offset = ImVec2(mouse_pos.x - drag_pos.x, mouse_pos.y - drag_pos.y);
        dragged_id    = m_id;
        TimelineFocusManager::GetInstance().RequestLayerFocus(Layer::kInteractiveLayer);
    }

    if(m_dragging && mouse_down)
    {
        float  new_x       = mouse_pos.x - window_position.x - m_drag_offset.x;
        float  new_y       = mouse_pos.y - window_position.y - m_drag_offset.y;
        ImVec2 window_size = ImGui::GetWindowSize();
        new_x = std::clamp(new_x, 0.0f, window_size.x - drag_w);
        new_y = std::clamp(new_y, 0.0f, window_size.y - drag_h);

        if(m_is_minimized)
        {
            m_time_ns  = conversion_manager->PixelToTime(new_x);
            m_y_offset = new_y;
            m_v_max_x  = conversion_manager->GetVMaxX();
            m_v_min_x  = conversion_manager->GetVMinX();
        }
        else
        {
            m_expanded_screen_pos = ImVec2(new_x, new_y);
        }

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
StickyNote::HandleResize(const ImVec2&                       window_position,
                         std::shared_ptr<TimePixelTransform> conversion_manager)
{
    if(!conversion_manager)
    {
        spdlog::error("StickyNote::HandleResize: conversion_manager shared_ptr is null");
        return false;
    }
    // Only allow resize if not dragging
    if(m_dragging || m_is_minimized) return false;

    ImVec2 sticky_pos;
    if(m_expanded_screen_pos.x >= 0 && m_expanded_screen_pos.y >= 0)
    {
        sticky_pos = ImVec2(window_position.x + m_expanded_screen_pos.x,
                            window_position.y + m_expanded_screen_pos.y);
    }
    else
    {
        float x = conversion_manager->TimeToPixel(m_time_ns);
        float y = m_y_offset;
        sticky_pos = ImVec2(window_position.x + x, window_position.y + y);
    }
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
       ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !HotkeyManager::GetInstance().IsActionHeld(HotkeyActionId::kRegionSelect))
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