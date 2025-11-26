// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "rocprofvis_dialog.h"

namespace RocProfVis
{
namespace View
{

void
ConfirmationDialog::Show(const std::string& title, const std::string& message,
                         std::function<void()> on_confirm_callback, std::function<void()> on_cancel_callback)
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

    if (ImGui::IsPopupOpen(m_title.c_str(), ImGuiPopupFlags_None))
    {
        // Always center this window when appearing
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        // Todo: get rid of magic numbers
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));        

        if(ImGui::BeginPopupModal(m_title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::NewLine();
            ImGui::TextUnformatted(m_message.c_str());
            ImGui::NewLine();
            ImGui::Separator();

            if(ImGui::Button("OK"))
            {
                if(m_on_confirm)
                {
                    m_on_confirm();  // Execute the action
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
            ImGui::SameLine();
            ImGui::Checkbox("Do not ask me again", &m_setting_option);

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(2);  // Pop ImGuiStyleVar_WindowPadding, ImGuiStyleVar_ItemSpacing
    }
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

    if (ImGui::IsPopupOpen(m_title.c_str(), ImGuiPopupFlags_None))
    {
        // Always center this window when appearing
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(4, 4));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 2));        

        if(ImGui::BeginPopupModal(m_title.c_str(), NULL, ImGuiWindowFlags_AlwaysAutoResize))
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

        ImGui::PopStyleVar(2);  // Pop ImGuiStyleVar_WindowPadding, ImGuiStyleVar_ItemSpacing
    }
}

}  // namespace View
}  // namespace RocProfVis
