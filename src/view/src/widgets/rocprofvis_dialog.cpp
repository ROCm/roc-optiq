// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_dialog.h"
#include "rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

void
ConfirmationDialog::Show(const std::string& title, const std::string& message,
                         std::function<void()> on_confirm_callback,
                         std::function<void()> on_cancel_callback)
{
    m_title       = title;
    m_message     = message;
    m_on_confirm  = on_confirm_callback;
    m_on_cancel   = on_cancel_callback;
    m_should_open = true;
}

void
ConfirmationDialog::Render()
{
    if(m_should_open)
    {
        ImGui::OpenPopup(m_title.c_str());
        m_should_open = false;
    }

    if(ImGui::IsPopupOpen(m_title.c_str(),
                          ImGuiPopupFlags_None))
    {
        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup();
        ImGui::SetNextWindowSize(ImVec2(580, 0));

        if(ImGui::BeginPopupModal(m_title.c_str(), NULL,
                                  ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoMove))
        {
            ImGui::NewLine();

            // Add message text with padding
            ImGui::Dummy(ImVec2(5.0f, 0.0f));
            ImGui::SameLine();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() +
                                   ImGui::GetContentRegionAvail().x);
            ImGui::TextUnformatted(m_message.c_str());
            ImGui::PopTextWrapPos();
            ImGui::SameLine();
            ImGui::Dummy(ImVec2(5.0f, 0.0f));

            ImGui::NewLine();
            ImGui::Separator();

            if(ImGui::Button("OK"))
            {
                if(m_on_confirm)
                {
                    m_on_confirm();
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SetItemDefaultFocus();
            ImGui::SameLine();

            if(ImGui::Button("Cancel"))
            {
                if(m_on_cancel)
                {
                    m_on_cancel();
                }
                ImGui::CloseCurrentPopup();
            }

            if(m_skip_dialog_setting.has_value())
            {
                ImGui::SameLine();
                DrawCheckboxOption();
            }

            ImGui::EndPopup();
        }
        popup_style.PopStyles();
    }
}

void
ConfirmationDialog::DrawCheckboxOption()
{
    const char* cb_label        = "Don't ask me again";
    ImGuiStyle& style           = ImGui::GetStyle();
    float       window_width    = ImGui::GetWindowSize().x;
    float       text_width      = ImGui::CalcTextSize(cb_label).x;
    float       checkbox_square = ImGui::GetFrameHeight();
    float       total_width =
        checkbox_square + style.ItemInnerSpacing.x + text_width + style.FramePadding.x;

    float pos_x = window_width - style.WindowPadding.x - total_width;
    ImGui::SetCursorPosX(pos_x);
    ImGui::Checkbox(cb_label, &m_skip_dialog_setting->get());
}

void
MessageDialog::Show(const std::string& title, const std::string& message)
{
    m_title       = title;
    m_message     = message;
    m_should_open = true;
}

void
MessageDialog::Render()
{
    if(m_should_open)
    {
        ImGui::OpenPopup(m_title.c_str());
        m_should_open = false;
    }

    if(ImGui::IsPopupOpen(m_title.c_str(), ImGuiPopupFlags_None))
    {
        // Always center this window when appearing
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));

        if(ImGui::BeginPopupModal(m_title.c_str(), NULL,
                                  ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::NewLine();
            ImGui::TextUnformatted(m_message.c_str());
            ImGui::NewLine();
            if(ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(
            2);  // Pop ImGuiStyleVar_WindowPadding, ImGuiStyleVar_ItemSpacing
    }
}

}  // namespace View
}  // namespace RocProfVis