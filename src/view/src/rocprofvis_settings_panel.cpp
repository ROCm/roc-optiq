// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings_panel.h"
#include <imgui.h>
#include <rocprofvis_settings.h>

namespace RocProfVis
{
namespace View
{
bool
SettingsPanel::IsOpen()
{
    return m_is_open;
}
void
SettingsPanel::SetOpen(bool open)
{
    m_is_open = open;
}
SettingsPanel::SettingsPanel()
: m_is_open(false)
{}

SettingsPanel::~SettingsPanel() {}
void
SettingsPanel::Render()
{
    ImGui::OpenPopup("Settings");

    auto&     settings     = Settings::GetInstance();
    auto&     font_manager = settings.GetFontManager();
    int       theme        = settings.IsDarkMode() ? 0 : 1;
    int       font_size    = font_manager.GetCurrentFontSizeIndex();
    const int num_sizes    = static_cast<int>(font_manager.GetFontSizes().size());

    if(ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("Appearance");
        ImGui::Separator();

        // Theme selection
        ImGui::TextUnformatted("Theme");
        ImGui::SameLine();
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Switch between dark and light UI themes.");

        bool theme_changed = false;
        if(ImGui::RadioButton("Dark", theme == 0))
        {
            if(theme != 0)
            {
                theme         = 0;
                theme_changed = true;
            }
        }
        ImGui::SameLine();
        if(ImGui::RadioButton("Light", theme == 1))
        {
            if(theme != 1)
            {
                theme         = 1;
                theme_changed = true;
            }
        }

        if(theme_changed)
        {
            if(theme == 0)
                settings.DarkMode();
            else
                settings.LightMode();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Font size selection
        ImGui::TextUnformatted("Font Size");
        ImGui::SameLine();
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Increase or decrease the font size for the UI.");

        if(ImGui::ArrowButton("##FontSizeMinus", ImGuiDir_Left) && font_size > 0)
        {
            font_size--;
            font_manager.SetFontSize(font_size);
        }
        ImGui::SameLine();
        ImGui::Text("%d", font_size);
        ImGui::SameLine();
        if(ImGui::ArrowButton("##FontSizePlus", ImGuiDir_Right) &&
           font_size < num_sizes - 1)
        {
            font_size++;
            font_manager.SetFontSize(font_size);
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Font size selection
        ImGui::TextUnformatted("Restore to Default Settings");
        if(ImGui::Button("Restore"))
        {
            m_is_open = false;
        }
        ImGui::Spacing();
        ImGui::Separator();

        if(ImGui::Button("Close"))
        {
            m_is_open = false;

            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();

        if(ImGui::Button("Save"))
        {
            m_is_open = false;

            ImGui::CloseCurrentPopup();
        }

        ImGui::EndPopup();
    }
}

}  // namespace View
}  // namespace RocProfVis
