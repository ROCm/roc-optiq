// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings_panel.h"
#include <imgui.h>
#include <imgui_internal.h>
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
    if(open)
    {
        // If open save current display settings
        m_display_settings_initial = Settings::GetInstance().GetCurrentDisplaySettings();
    }
}
SettingsPanel::SettingsPanel()
: m_is_open(false)
, m_preview_font_size(-1)
, m_display_settings_initial()
{}

SettingsPanel::~SettingsPanel() {}
void
SettingsPanel::Render()
{
    // Layout constants
    static constexpr ImVec2 kWindowSize(420, 480);
    static constexpr float  kMargin                  = 20.0f;
    static constexpr float  kTopSpace                = 16.0f;
    static constexpr float  kBottomBarHeight         = 60.0f;
    static constexpr float  kButtonWidth             = 100.0f;
    static constexpr float  kButtonSpacing           = 16.0f;
    static constexpr float  kThemeRadioSpacing       = 120.0f;
    static constexpr float  kFontButtonWidth         = 28.0f;
    static constexpr float  kFontSliderWidth         = 100.0f;
    static constexpr float  kFontButtonRounding      = 12.0f;
    static constexpr float  kCheckboxRounding        = 6.0f;
    static constexpr float  kCheckboxBorderThickness = 2.0f;

    ImGui::SetNextWindowSize(kWindowSize, ImGuiCond_Always);
    ImGui::OpenPopup("Settings");

    auto& settings     = Settings::GetInstance();
    auto& font_manager = settings.GetFontManager();
    int   theme        = settings.IsDarkMode() ? 0 : 1;
    int   font_size    = font_manager.GetCurrentFontSizeIndex();
    int   num_sizes    = static_cast<int>(font_manager.GetFontSizes().size());

    if(ImGui::BeginPopupModal("Settings", nullptr,
                              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImGui::ColorConvertU32ToFloat4(
                                  settings.GetColor(RocProfVis::View::Colors::kBgPanel)));

        ImGui::SetCursorPosX(kMargin);
        ImVec2 child_size = ImVec2(kWindowSize.x - 2 * kMargin,
                                   kWindowSize.y - kMargin - kBottomBarHeight);
        ImGui::BeginChild("SettingsContent", child_size, true,
                          ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::Dummy(ImVec2(0, kTopSpace));

        // Appearance Section
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 8));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(16, 16));
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                               settings.GetColor(RocProfVis::View::Colors::kTextMain)),
                           "Appearance");
        ImGui::PopStyleVar(2);

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Dummy(ImVec2(0, 10));

        // Theme selection
        ImGui::TextUnformatted("Theme");
        ImGui::SameLine(kThemeRadioSpacing);

        // Push a light grey color for the radio button circle
        ImGui::PushStyleColor(ImGuiCol_FrameBg,
                              ImGui::ColorConvertU32ToFloat4(
                                  settings.GetColor(RocProfVis::View::Colors::kButton)));

        int theme_radio = theme;
        if(ImGui::RadioButton("Dark", theme_radio == 0)) theme_radio = 0;
        ImGui::SameLine();
        if(ImGui::RadioButton("Light", theme_radio == 1)) theme_radio = 1;

        ImGui::PopStyleColor();  // Restore previous color

        ImGui::Dummy(ImVec2(0, 20));

        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Switch between dark and light UI themes.");

        if(theme_radio != theme)
        {
            if(theme_radio == 0)
                settings.DarkMode();
            else
                settings.LightMode();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // DPI-based scaling toggle
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kCheckboxRounding);
        bool dpi_scaling = settings.IsDPIBasedScaling();
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Dummy(ImVec2(4, 0));
        ImGui::SameLine();
        if(ImGui::Checkbox("DPI-based Font Scaling", &dpi_scaling))
            settings.SetDPIBasedScaling(dpi_scaling);

        // Draw a border around the checkbox square only
        ImVec2 box_min = ImGui::GetItemRectMin();
        float  box_sz  = ImGui::GetFrameHeight();
        ImVec2 box_max = ImVec2(box_min.x + box_sz, box_min.y + box_sz);

        ImU32 border_col = settings.GetColor(RocProfVis::View::Colors::kAccentRedHover);
        ImGui::GetWindowDrawList()->AddRect(
            box_min, box_max, border_col, kCheckboxRounding, 0, kCheckboxBorderThickness);

        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Automatically scale font size based on display DPI. "
                              "Unchecked if you adjust font size manually.");
        ImGui::PopStyleVar();
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Spacing();
        ImGui::Separator();

        // Font size section
        if(m_preview_font_size < 0) m_preview_font_size = font_size;

        ImGui::TextUnformatted("Font Size");
        ImGui::SameLine(kThemeRadioSpacing);

        ImGui::BeginDisabled(dpi_scaling);

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kFontButtonRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
        if(ImGui::Button("-", ImVec2(kFontButtonWidth, 0)) && m_preview_font_size > 0)
        {
            m_preview_font_size--;
            settings.SetDPIBasedScaling(false);
        }
        ImGui::PopStyleVar(2);

        ImGui::SameLine();

        ImGui::SetNextItemWidth(kFontSliderWidth);
        int slider_min = 0;
        int slider_max = num_sizes - 1;
        if(ImGui::SliderInt("##FontSizeSlider", &m_preview_font_size, slider_min,
                            slider_max, "%d"))
        {
            settings.SetDPIBasedScaling(false);
        }

        ImGui::SameLine();

        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, kFontButtonRounding);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
        if(ImGui::Button("+", ImVec2(kFontButtonWidth, 0)) &&
           m_preview_font_size < num_sizes - 1)
        {
            m_preview_font_size++;
            settings.SetDPIBasedScaling(false);
        }
        ImGui::PopStyleVar(2);

        ImGui::EndDisabled();

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

        // Actions Section
        ImGui::TextColored(ImGui::ColorConvertU32ToFloat4(
                               settings.GetColor(RocProfVis::View::Colors::kTextMain)),
                           "Actions");
        ImGui::Dummy(ImVec2(0, 10));
        ImGui::PushStyleColor(ImGuiCol_Button,
                              ImGui::ColorConvertU32ToFloat4(
                                  settings.GetColor(RocProfVis::View::Colors::kButton)));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImGui::ColorConvertU32ToFloat4(settings.GetColor(
                                  RocProfVis::View::Colors::kButtonHovered)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              ImGui::ColorConvertU32ToFloat4(settings.GetColor(
                                  RocProfVis::View::Colors::kButtonActive)));

        if(ImGui::SmallButton("Restore Defaults"))
        {
            m_is_open = false;
            settings.RestoreDisplaySettings(settings.GetInitialDisplaySettings());

            m_preview_font_size = -1;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Restore all settings to their default values.");

        ImGui::PopStyleColor(3);

        ImGui::EndChild();
        ImGui::PopStyleColor();

        // Bottom bar for Save/Close
        ImGui::SetCursorPosY(kWindowSize.y - kBottomBarHeight - kMargin);
        ImGui::SetCursorPosX(kMargin);
        ImGui::PushStyleColor(ImGuiCol_ChildBg,
                              ImGui::ColorConvertU32ToFloat4(
                                  settings.GetColor(RocProfVis::View::Colors::kBgPanel)));
        ImGui::BeginChild("SettingsBottomBar",
                          ImVec2(kWindowSize.x - 2 * kMargin, kBottomBarHeight), true);

        ImGui::Separator();
        ImGui::Spacing();
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);

        float total_width = kButtonWidth * 2 + kButtonSpacing;
        float x           = ((kWindowSize.x - 2 * kMargin) - total_width) * 0.5f;
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

        if(ImGui::Button("Close", ImVec2(kButtonWidth, 0)))
        {
            m_is_open           = false;
            m_preview_font_size = -1;
            ImGui::CloseCurrentPopup();

            settings.RestoreDisplaySettings(m_display_settings_initial);
        }
        ImGui::SameLine(0, kButtonSpacing);
        if(ImGui::Button("Save", ImVec2(kButtonWidth, 0)))
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
