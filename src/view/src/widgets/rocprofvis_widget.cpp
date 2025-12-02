// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_widget.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_core.h"
#include "rocprofvis_debug_window.h"

#include "rocprofvis_settings_manager.h"
#include "rocprofvis_utils.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

#include <algorithm>
#include <iostream>
#include <sstream>

namespace RocProfVis
{
namespace View
{

RocWidget::~RocWidget() { spdlog::info("RocWidget object destroyed"); }

void
RocWidget::Render()
{}

std::string
RocWidget::GenUniqueName(std::string name)
{
    std::ostringstream oss;
    oss << name.c_str() << std::hex << reinterpret_cast<uintptr_t>(this);
    return oss.str();
}

const std::string&
RocWidget::GetWidgetName() const
{
    return m_widget_name;
}

//------------------------------------------------------------------
RocCustomWidget::RocCustomWidget(const std::function<void()>& callback)
: m_callback(callback)
{}

void
RocCustomWidget::SetCallback(const std::function<void()>& callback)
{
    m_callback = callback;
}
void
RocCustomWidget::Render()
{
    if(m_callback)
    {
        m_callback();
    }
}

LayoutItem::Ptr
LayoutItem::CreateFromWidget(std::shared_ptr<RocWidget> widget, float w, float h)
{
    Ptr item     = std::make_shared<LayoutItem>(w, h);
    item->m_item = widget;
    return item;
}

//------------------------------------------------------------------
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
    SettingsManager& settings = SettingsManager::GetInstance();
    if(m_show_reset_button)
    {
        if(IconButton(ICON_ARROWS_CYCLE,
                      settings.GetFontManager().GetIconFont(FontType::kDefault)))
        {
            RevertToDefault();
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            settings.GetDefaultStyle().WindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                            settings.GetDefaultStyle().FrameRounding);
        if(ImGui::BeginItemTooltip())
        {
            ImGui::TextUnformatted("Revert To Default");
            ImGui::TextUnformatted(m_reset_tooltip.c_str());
            ImGui::EndTooltip();
        }
        ImGui::PopStyleVar(2);
        ImGui::SameLine();
    }
    // Draw the text as a button to avoid the background
    // and prevent clicks from being registered.
    ImGui::PushStyleColor(ImGuiCol_Button, 0);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, 0);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x -
                         ImGui::CalcTextSize(m_text.c_str()).x -
                         ImGui::GetStyle().WindowPadding.x);
    if(ImGui::Button(m_text.c_str()))
    {
        m_editing_mode           = true;
        m_request_keyboard_focus = true;
    }
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        settings.GetDefaultStyle().WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding,
                        settings.GetDefaultStyle().FrameRounding);
    if(ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted(m_tooltip_text.c_str());
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar(2);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
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
    ImGui::SetCursorPos(ImVec2(ImGui::GetContentRegionMax().x -
                                   ImGui::CalcTextSize(m_text.c_str()).x -
                                   ImGui::GetStyle().WindowPadding.x,
                               ImGui::GetCursorPosY() - 2.0f));

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

void
WithPadding(float left, float right, float top, float bottom,
            const std::function<void()>& content)
{
    if(top > 0.0f) ImGui::Dummy(ImVec2(0, top));

    // No border flags for invisible borders
    if(ImGui::BeginTable("##padding_table", 3, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("LeftPad", ImGuiTableColumnFlags_WidthFixed, left);
        ImGui::TableSetupColumn("Content", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("RightPad", ImGuiTableColumnFlags_WidthFixed, right);

        ImGui::TableNextRow();

        // Left padding
        ImGui::TableSetColumnIndex(0);
        if(left > 0.0f) ImGui::Dummy(ImVec2(left, 0));

        // Content
        ImGui::TableSetColumnIndex(1);
        ImGui::BeginGroup();
        content();
        ImGui::EndGroup();

        // Right padding
        ImGui::TableSetColumnIndex(2);
        if(right > 0.0f) ImGui::Dummy(ImVec2(right, 0));

        ImGui::EndTable();
    }

    if(bottom > 0.0f) ImGui::Dummy(ImVec2(0, bottom));
}

bool
CopyableTextUnformatted(const char* text, std::string unique_id, bool one_click_copy,
    bool context_menu)
{
    bool clicked = false;
    if(!unique_id.empty())
        ImGui::PushID(unique_id.c_str());

    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

    if(ImGui::Button(text, ImVec2(0, 0)))
    {
        clicked = true;
        if (one_click_copy)
        {
            ImGui::SetClipboardText(text);
            NotificationManager::GetInstance().Show("Cell value was copied",
                                                    NotificationLevel::Info);
        }
    }
    
    if(context_menu)
    {
        if(ImGui::BeginPopupContextItem())
        {
            if(ImGui::MenuItem("Copy cell data"))
            {
                ImGui::SetClipboardText(text);
                NotificationManager::GetInstance().Show("Cell data was copied",
                                                        NotificationLevel::Info);
            }
            ImGui::EndPopup();
        }
    }

    if(one_click_copy)
    {
        if(ImGui::IsItemHovered())
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        }
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);


    if(!unique_id.empty())
    {
        ImGui::PopID();
    }
    return clicked;
}

}  // namespace View
}
