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
, m_preview_font_size(-1)
{}

SettingsPanel::~SettingsPanel() {}

void
SettingsPanel::Render()
{
    const ImVec2 fixed_size(420, 480);
    const float  margin = 20.0f;
    ImGui::SetNextWindowSize(fixed_size, ImGuiCond_Always);

    ImGui::OpenPopup("Settings");

    auto&     settings     = Settings::GetInstance();
    auto&     font_manager = settings.GetFontManager();
    int       theme        = settings.IsDarkMode() ? 0 : 1;
    int       font_size    = font_manager.GetCurrentFontSizeIndex();
    const int num_sizes    = static_cast<int>(font_manager.GetFontSizes().size());

    if(ImGui::BeginPopupModal("Settings", nullptr,
                              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImGui::ColorConvertU32ToFloat4(
                                  settings.GetColor(RocProfVis::View::Colors::kBgPanel)));

        ImGui::SetCursorPosX(margin);
        ImVec2 child_size = ImVec2(fixed_size.x - 2 * margin, fixed_size.y - margin - 60);
        ImGui::BeginChild("SettingsContent", child_size, true,
                          ImGuiWindowFlags_HorizontalScrollbar);

        // For space at the top
        ImGui::Dummy(ImVec2(0, 16));

        // Section: Appearance
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(16, 16));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                               settings.GetColor(RocProfVis::View::Colors::kTextMain)),
                           "Appearance");
        ImGui::PopStyleVar(2);

        ImGui::Spacing();
        ImGui::Separator();

        // Theme selection
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::TextUnformatted("Theme");
        ImGui::SameLine(120);
        ImGui::RadioButton("Dark", &theme, 0);
        ImGui::SameLine();
        ImGui::RadioButton("Light", &theme, 1);
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Switch between dark and light UI themes.");
        ImGui::PopStyleVar();

        if(theme != (settings.IsDarkMode() ? 0 : 1))
        {
            if(theme == 0)
                settings.DarkMode();
            else
                settings.LightMode();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // DPI-based scaling toggle
        bool dpi_scaling = settings.IsDPIBasedScaling();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::Checkbox("DPI-based Font Scaling", &dpi_scaling);
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Automatically scale font size based on display DPI. "
                              "Unchecked if you adjust font size manually.");
        settings.SetDPIBasedScaling(dpi_scaling);
        ImGui::PopStyleVar();

        ImGui::Spacing();
        ImGui::Separator();

        // Font size section
        if(m_preview_font_size < 0) m_preview_font_size = font_size;

        ImGui::TextUnformatted("Font Size");
        ImGui::SameLine(120);

        ImGui::BeginDisabled(dpi_scaling);  // Disable font controls if DPI scaling is on

        // font buttons
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
        if(ImGui::Button("-", ImVec2(28, 0)) && m_preview_font_size > 0)
        {
            m_preview_font_size--;
            settings.SetDPIBasedScaling(false);
        }
        ImGui::PopStyleVar(2);

        ImGui::SameLine();

        // font slider
        int slider_min = 0;
        int slider_max = num_sizes - 1;
        ImGui::SetNextItemWidth(100);
        if(ImGui::SliderInt("##FontSizeSlider", &m_preview_font_size, slider_min,
                            slider_max, "%d"))
        {
            settings.SetDPIBasedScaling(false);
        }

        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 12.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
        if(ImGui::Button("+", ImVec2(28, 0)) && m_preview_font_size < num_sizes - 1)
        {
            m_preview_font_size++;
            settings.SetDPIBasedScaling(false);
        }
        ImGui::PopStyleVar(2);

        ImGui::EndDisabled();  // End disabling font controls

        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Increase or decrease the font size for the UI.");

        // Font preview
        ImFont* preview_font = font_manager.GetFontByIndex(m_preview_font_size);
        if(preview_font)
        {
            ImGui::Spacing();
            ImGui::PushFont(preview_font);
            ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                                   settings.GetColor(RocProfVis::View::Colors::kTextDim)),
                               "AMD ROCm Visualizer");
            ImGui::PopFont();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Section: Actions
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                               settings.GetColor(RocProfVis::View::Colors::kTextMain)),
                           "Actions");
        ImGui::Spacing();

        // Restore to Default Settings
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImGui::ColorConvertU32ToFloat4(
                                  settings.GetColor(RocProfVis::View::Colors::kButton)));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImGui::ColorConvertU32ToFloat4(settings.GetColor(
                                  RocProfVis::View::Colors::kButtonHovered)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImGui::ColorConvertU32ToFloat4(settings.GetColor(
                                  RocProfVis::View::Colors::kButtonActive)));
        if(ImGui::Button("Restore to Default Settings", ImVec2(-1, 0)))
        {
            m_is_open = false;
            settings.SetDPIBasedScaling(true);
            settings.LightMode();

            //Set font based on DPI again
            settings.GetFontManager().SetFontSize(
                settings.GetFontManager().GetFontSizeIndexForDPI(settings.GetDPI()));
            m_preview_font_size = -1;
        }
        ImGui::PopStyleColor(3);

        ImGui::EndChild();
        ImGui::PopStyleColor();

        //  Bottom bar for Save/Close
        ImGui::SetCursorPosY(fixed_size.y - 60 - margin);
        ImGui::SetCursorPosX(margin);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImGui::ColorConvertU32ToFloat4(
                                  settings.GetColor(RocProfVis::View::Colors::kBgPanel)));
        ImGui::BeginChild("SettingsBottomBar", ImVec2(fixed_size.x - 2 * margin, 60),
                          true);

        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        float button_width = 100.0f;
        float spacing      = 16.0f;
        float total_width  = button_width * 2 + spacing;
        float x            = ((fixed_size.x - 2 * margin) - total_width) * 0.5f;
        ImGui::SetCursorPosX(x);

        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImGui::ColorConvertU32ToFloat4(
                                  settings.GetColor(RocProfVis::View::Colors::kButton)));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImGui::ColorConvertU32ToFloat4(settings.GetColor(
                                  RocProfVis::View::Colors::kButtonHovered)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImGui::ColorConvertU32ToFloat4(settings.GetColor(
                                  RocProfVis::View::Colors::kButtonActive)));

        if(ImGui::Button("Close", ImVec2(button_width, 0)))
        {
            m_is_open           = false;
            m_preview_font_size = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine(0, spacing);
        if(ImGui::Button("Save", ImVec2(button_width, 0)))
        {
            m_is_open = false;
            font_manager.SetFontSize(m_preview_font_size);
            m_preview_font_size = -1;
            ImGui::CloseCurrentPopup();
        }
        ImGui::PopStyleColor(3);

        ImGui::PopStyleVar();
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::Spacing();
        ImGui::EndPopup();
    }
}

}  // namespace View
}  // namespace RocProfVis
