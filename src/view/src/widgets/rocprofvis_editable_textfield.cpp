// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_editable_textfield.h"
#include "rocprofvis_settings_manager.h"
#include "icons/rocprovfis_icon_defines.h"

namespace RocProfVis
{
namespace View
{

EditableTextField::EditableTextField(std::string id)
: m_id(std::move(id))
{}

void
EditableTextField::Render()
{
    ImGui::PushID(m_id.c_str());
    if(m_editing_mode)
    {
        DrawEditingText();
    }
    else
    {
        DrawPlainText();
    }
    ImGui::PopID();
}

void
EditableTextField::RevertToDefault()
{
    // FIXME: Assuming default is an empty string
    // Think about best way to handle revert button pressed
    m_text   = "";
    if(m_on_text_commit)
    {
        m_on_text_commit(m_text);
    }
}

void
EditableTextField::ShowResetButton(bool is_button_shown)
{
    m_show_reset_button = is_button_shown;
}

void
EditableTextField::DrawPlainText()
{
    // Draw the text as a button to avoid the background
    // and prevent clicks from being registered.
    ImGui::PushStyleColor(ImGuiCol_Button, 0);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    bool clicked = ImGui::Button(m_text.c_str());
    if(ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted(m_tooltip_text.c_str());
        ImGui::EndTooltip();
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);

    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if(clicked)
    {
        m_editing_mode = true;
        m_request_keyboard_focus = true;
    }
    if(m_show_reset_button)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        ImFont* icon_font = SettingsManager::GetInstance().GetFontManager().GetIconFont(
            FontType::kDefault);
        ImGui::PushFont(icon_font);

        ImVec2 icon_size = ImGui::CalcTextSize(ICON_ARROWS_CYCLE);
        float  button_w  = icon_size.x + style.FramePadding.x * 2.0f;
        float  button_h  = icon_size.y + style.FramePadding.y * 2.0f;

        ImVec2 prev_min      = ImGui::GetItemRectMin();
        ImVec2 prev_max      = ImGui::GetItemRectMax();
        float  prev_center_y = (prev_min.y + prev_max.y) * 0.5f;

        ImVec2 window_pos      = ImGui::GetWindowPos();
        ImVec2 content_max     = ImGui::GetContentRegionMax();
        float  right_screen    = window_pos.x + content_max.x;
        float  target_x_screen = right_screen - button_w - style.FramePadding.x * 4.0f;

        float target_y_screen = prev_center_y - (button_h * 0.5f);
        ImVec2 prev_screen_cursor = ImGui::GetCursorScreenPos();
        ImGui::SetCursorScreenPos(ImVec2(target_x_screen, target_y_screen));
        if(ImGui::Button(ICON_ARROWS_CYCLE, ImVec2(button_w, button_h)))
        {
            RevertToDefault();
        }
        // restore previous cursor so layout continues as expected
        ImGui::SetCursorScreenPos(prev_screen_cursor);
        ImGui::PopFont();
        if(ImGui::BeginItemTooltip())
        {
            ImGui::TextUnformatted("Revert To Default");
            ImGui::TextUnformatted(m_reset_tooltip.c_str());
            ImGui::EndTooltip();
        }
    }
}

void
EditableTextField::DrawEditingText()
{
    if(m_edit_buf != m_text)
    {
        m_edit_buf = m_text;
    }

    auto resize_callback = [](ImGuiInputTextCallbackData* data) -> int {
        if(data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            std::string* str = static_cast<std::string*>(data->UserData);
            str->resize(static_cast<size_t>(data->BufTextLen));
            data->Buf = str->data();
        }
        return 0;
    };

    float width = ImGui::CalcTextSize(m_edit_buf.c_str()).x;
    ImGui::SetNextItemWidth(width);

    // make edit field on the same level as the textfield
    ImVec2 prev_cursor = ImGui::GetCursorPos();
    ImGui::SetCursorPos(ImVec2(prev_cursor.x, prev_cursor.y - 2.0f));

    if(m_request_keyboard_focus)
    {
        ImGui::SetKeyboardFocusHere();
        m_request_keyboard_focus = false;
    }

    bool enter_pressed = ImGui::InputText(
        ("##" + m_id).c_str(), m_edit_buf.data(), m_edit_buf.capacity() + 1,
        ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue |
            ImGuiInputTextFlags_CallbackResize,
        resize_callback, static_cast<void*>(&m_edit_buf));

    if(enter_pressed)
    {
        AcceptEdit();
    }
    if(ImGui::IsMouseClicked(ImGuiMouseButton_Left)
       && !ImGui::IsItemHovered())
    {
        AcceptEdit();
    }
    if(ImGui::IsKeyPressed(ImGuiKey_Escape))
    {
        m_editing_mode = false;
    }
}

void
EditableTextField::SetText(std::string text, std::string tooltip, std::string reset_tooltip)
{
    m_text = std::move(text);
    m_tooltip_text = std::move(tooltip);
    m_reset_tooltip = std::move(reset_tooltip);
}

float
EditableTextField::ButtonSize() const
{
    ImFont* icon_font =
        SettingsManager::GetInstance().GetFontManager().GetIconFont(FontType::kDefault);
    ImGui::PushFont(icon_font);
    float size = ImGui::CalcTextSize(ICON_ARROWS_CYCLE).x;
    ImGui::PopFont();
    return size;
}

void
EditableTextField::AcceptEdit()
{
    m_editing_mode = false;
    m_text         = m_edit_buf;
    if(m_on_text_commit)
    {
        m_on_text_commit(m_text);
    }
}
void
EditableTextField::SetOnTextCommit(const std::function<void(const std::string&)>& cb)
{
    m_on_text_commit = cb;
}

}
}