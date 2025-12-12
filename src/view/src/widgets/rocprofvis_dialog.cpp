// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_dialog.h"
#include "rocprofvis_settings_manager.h"

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
        SettingsManager& settings = SettingsManager::GetInstance();
        ImGuiStyle& style = ImGui::GetStyle();
        
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        
        ImGui::SetNextWindowSize(ImVec2(480, 0));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        if(ImGui::BeginPopupModal(m_title.c_str(), NULL, 
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
            ImGui::TextUnformatted(m_title.c_str());
            ImGui::PopStyleColor();
            
            ImGui::Spacing();

            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
            ImGui::TextUnformatted(m_message.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();

            ImGui::Spacing();
            ImGui::Spacing();

            if(m_skip_dialog_setting.has_value())
            {
                DrawCheckboxOption();
                ImGui::Spacing();
            }

            float button_width = 100.0f;
            float available_width = ImGui::GetContentRegionAvail().x;
            float button_start_x = available_width - (button_width * 2 + style.ItemSpacing.x);
            
            ImGui::SetCursorPosX(button_start_x);

            if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
            {
                if(m_on_cancel)
                {
                    m_on_cancel();
                }
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kAccentRed));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, settings.GetColor(Colors::kAccentRedHover));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, settings.GetColor(Colors::kAccentRedActive));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            
            if(ImGui::Button("OK", ImVec2(button_width, 0)))
            {
                if(m_on_confirm)
                {
                    m_on_confirm();
                }
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::PopStyleColor(4);
            ImGui::SetItemDefaultFocus();

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(5);
    }
}

void
ConfirmationDialog::DrawCheckboxOption()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    const char* cb_label = "Don't ask me again";
    
    ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
    ImGui::Checkbox(cb_label, &m_skip_dialog_setting->get());
    ImGui::PopStyleColor();
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
        SettingsManager& settings = SettingsManager::GetInstance();
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Always center this window (FIXED)
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(480, 0));

 
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);

        if(ImGui::BeginPopupModal(m_title.c_str(), NULL, 
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextMain));
            ImGui::TextUnformatted(m_title.c_str());
            ImGui::PopStyleColor();
            
            ImGui::Spacing();

            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x);
            ImGui::PushStyleColor(ImGuiCol_Text, settings.GetColor(Colors::kTextDim));
            ImGui::TextUnformatted(m_message.c_str());
            ImGui::PopStyleColor();
            ImGui::PopTextWrapPos();

            ImGui::Spacing();
            ImGui::Spacing();

            float button_width = 100.0f;
            float available_width = ImGui::GetContentRegionAvail().x;
            float button_start_x = (available_width - button_width) * 0.5f;
            
            ImGui::SetCursorPosX(button_start_x);

            // Close button styled with accent color
            ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kAccentRed));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, settings.GetColor(Colors::kAccentRedHover));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, settings.GetColor(Colors::kAccentRedActive));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            
            if(ImGui::Button("Close", ImVec2(button_width, 0)))
            {
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::PopStyleColor(4);
            ImGui::SetItemDefaultFocus();

            ImGui::EndPopup();
        }

        ImGui::PopStyleVar(5);
    }
}

}  // namespace View
}  // namespace RocProfVis
