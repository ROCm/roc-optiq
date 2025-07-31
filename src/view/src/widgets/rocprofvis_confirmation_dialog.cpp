// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.
#include "rocprofvis_confirmation_dialog.h"

namespace RocProfVis
{
namespace View
{

void
ConfirmationDialog::Show(const std::string& title, const std::string& message,
                         std::function<void()> on_confirm_callback)
{
    m_title       = title;
    m_message     = message;
    m_on_confirm  = on_confirm_callback;
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

    // Always center this window when appearing
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

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
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}  // namespace View
}  // namespace RocProfVis
