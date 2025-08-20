// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings_panel.h"
#include "json.h"
#include <filesystem>
#include <fstream>
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
        // Save current display settings and initialize preview/font index
        m_display_settings_initial  = Settings::GetInstance().GetCurrentDisplaySettings();
        m_preview_font_size         = m_display_settings_initial.font_size_index;
        m_display_settings_modified = m_display_settings_initial;
    }
}
SettingsPanel::SettingsPanel()
: m_is_open(false)
, m_preview_font_size(-1)
, m_display_settings_initial()
, m_display_settings_modified()
{}

SettingsPanel::~SettingsPanel() {}
void
SettingsPanel::Render()
{
    // Layout constants
    static constexpr ImVec2 kWindowSize(420, 480);
    static constexpr float  kMargin            = 20.0f;
    static constexpr float  kTopSpace          = 16.0f;
    static constexpr float  kBottomBarHeight   = 60.0f;
    static constexpr float  kButtonWidth       = 100.0f;
    static constexpr float  kButtonSpacing     = 16.0f;
    static constexpr float  kThemeRadioSpacing = 120.0f;
    static constexpr float  kFontButtonWidth   = 28.0f;
    static constexpr float  kFontSliderWidth   = 100.0f;

    ImGui::SetNextWindowSize(kWindowSize, ImGuiCond_Always);
    ImGui::OpenPopup("Settings");

    auto& settings     = Settings::GetInstance();
    auto& font_manager = settings.GetFontManager();
    int   theme        = settings.IsDarkMode() ? 0 : 1;
    int   num_sizes    = static_cast<int>(font_manager.GetFontSizes().size());

    if(ImGui::BeginPopupModal("Settings", nullptr,
                              ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse))
    {
        ImGui::SetCursorPosX(kMargin);
        ImVec2 child_size = ImVec2(kWindowSize.x - 2 * kMargin,
                                   kWindowSize.y - kMargin - kBottomBarHeight);
        ImGui::BeginChild("SettingsContent", child_size, true,
                          ImGuiWindowFlags_HorizontalScrollbar);

        ImGui::Dummy(ImVec2(0, kTopSpace));

        // Appearance Section
        ImGui::Text("Appearance");

        ImGui::Spacing();
        ImGui::Separator();

        ImGui::Dummy(ImVec2(0, 10));

        // Theme selection
        ImGui::TextUnformatted("Theme");
        ImGui::SameLine(kThemeRadioSpacing);

        int theme_radio = theme;
        if(ImGui::RadioButton("Dark", theme_radio == 0)) theme_radio = 0;
        ImGui::SameLine();
        if(ImGui::RadioButton("Light", theme_radio == 1)) theme_radio = 1;

        ImGui::Dummy(ImVec2(0, 20));

        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Switch between dark and light UI themes.");

        if(theme_radio != theme)
        {
            if(theme_radio == 0)
            {
                m_display_settings_modified.use_dark_mode = true;
            }
            else
            {
                m_display_settings_modified.use_dark_mode = false;
            }
            settings.SetDisplaySettings(m_display_settings_modified);
        }

        ImGui::Spacing();
        ImGui::Separator();

        // DPI-based scaling toggle
        bool dpi_scaling = settings.IsDPIBasedScaling();
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Dummy(ImVec2(4, 0));
        ImGui::SameLine();
        if(ImGui::Checkbox("DPI-based Font Scaling", &dpi_scaling))
        {
            m_display_settings_modified.dpi_based_scaling = dpi_scaling;
            settings.SetDisplaySettings(m_display_settings_modified);
        }

        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Automatically scale font size based on display DPI. "
                              "Unchecked if you adjust font size manually.");
        ImGui::Dummy(ImVec2(0, 6));
        ImGui::Spacing();
        ImGui::Separator();

        // Font size section
        ImGui::TextUnformatted("Font Size");
        ImGui::SameLine(kThemeRadioSpacing);

        ImGui::BeginDisabled(dpi_scaling);

        // Only update font_size_index when user interacts
        if(ImGui::Button("-", ImVec2(kFontButtonWidth, 0)) && m_preview_font_size > 0)
        {
            m_preview_font_size--;
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(kFontSliderWidth);
        int slider_min = 0;
        int slider_max = num_sizes - 1;
        if(ImGui::SliderInt("##FontSizeSlider", &m_preview_font_size, slider_min,
                            slider_max, "%d"))
        {
        }

        ImGui::SameLine();

        if(ImGui::Button("+", ImVec2(kFontButtonWidth, 0)) &&
           m_preview_font_size < num_sizes - 1)
        {
            m_preview_font_size++;
        }

        ImGui::EndDisabled();

        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Increase or decrease the font size for the UI.");

        // Font preview
        ImFont* preview_font = font_manager.GetFontByIndex(m_preview_font_size);
        if(preview_font)
        {
            ImGui::Spacing();
            ImGui::PushFont(preview_font);
            ImGui::Text("AMD ROCm Visualizer");
            ImGui::PopFont();
        }

        ImGui::Spacing();
        ImGui::Separator();

        // Actions Section
        ImGui::Text("Actions");
        ImGui::Dummy(ImVec2(0, 10));

        if(ImGui::SmallButton("Restore Defaults"))
        {
            m_display_settings_modified = settings.GetInitialDisplaySettings();
            settings.SetDisplaySettings(m_display_settings_modified);

            m_preview_font_size = m_display_settings_modified.font_size_index;
        }
        if(ImGui::IsItemHovered())
            ImGui::SetTooltip("Restore all settings to their default values.");

        ImGui::EndChild();

        // Bottom bar for Save/Close
        ImGui::SetCursorPosY(kWindowSize.y - kBottomBarHeight - kMargin);
        ImGui::SetCursorPosX(kMargin);
        ImGui::BeginChild("SettingsBottomBar",
                          ImVec2(kWindowSize.x - 2 * kMargin, kBottomBarHeight), true);

        ImGui::Separator();
        ImGui::Spacing();

        float total_width = kButtonWidth * 2 + kButtonSpacing;
        float x           = ((kWindowSize.x - 2 * kMargin) - total_width) * 0.5f;
        ImGui::SetCursorPosX(x);

        if(ImGui::Button("Cancel", ImVec2(kButtonWidth, 0)))
        {
            m_is_open           = false;
            m_preview_font_size = -1;
            ImGui::CloseCurrentPopup();

            settings.RestoreDisplaySettings(m_display_settings_initial);
        }
        ImGui::SameLine(0, kButtonSpacing);
        if(ImGui::Button("Ok", ImVec2(kButtonWidth, 0)))
        {
            m_is_open = false;
            // Always use a valid font size index
            if(!m_display_settings_modified.dpi_based_scaling) {
                m_display_settings_modified.font_size_index = m_preview_font_size;
            }
            settings.SetDisplaySettings(m_display_settings_modified);
            m_preview_font_size = -1;
            settings.SaveSettings("settings_application.json",
                                  m_display_settings_modified);
            ImGui::CloseCurrentPopup();
        }

        ImGui::EndChild();

        ImGui::Spacing();
        ImGui::EndPopup();
    }
}

}  // namespace View
}  // namespace RocProfVis
