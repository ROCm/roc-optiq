// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_settings_panel.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"

// Layout constants
constexpr float kCategorywidth = 150.0f;
constexpr float kContentwidth  = 450.0f;
constexpr float kContentHeight = 450.0f;

namespace RocProfVis
{
namespace View
{

SettingsPanel::SettingsPanel(SettingsManager& settings)
: m_should_open(false)
, m_settings_changed(false)
, m_category(Display)
, m_settings(settings)
, m_fonts(settings.GetFontManager())
, m_usersettings(settings.GetUserSettings())
, m_usersettings_initial(m_usersettings)
, m_font_settings({ m_usersettings.display_settings.dpi_based_scaling,
                    m_usersettings.display_settings.font_size_index })
{
    for(const ImFont* font : m_fonts.GetAvailableFonts())
    {
        m_font_sizes.emplace_back(std::to_string(static_cast<int>(font->FontSize)));
        m_font_sizes_ptr.emplace_back(m_font_sizes.back().c_str());
    }
}

SettingsPanel::~SettingsPanel() {}

void
SettingsPanel::Show()
{
    m_should_open               = true;
    m_category                  = Display;
    m_usersettings_initial      = m_usersettings;
    m_font_settings.dpi_scaling = m_usersettings.display_settings.dpi_based_scaling;
    m_font_settings.size_index  = m_usersettings.display_settings.font_size_index;
}

void
SettingsPanel::Render()
{
    if(m_should_open)
    {
        ImGui::OpenPopup("Settings");
        m_should_open = false;
    }

    if(ImGui::IsPopupOpen("Settings"))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                            m_settings.GetDefaultStyle().WindowPadding);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                            m_settings.GetDefaultStyle().ItemSpacing);
        if(ImGui::BeginPopupModal("Settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
        {
            ImGui::BeginChild("SettingsCategories",
                              ImVec2(kCategorywidth, kContentHeight),
                              ImGuiChildFlags_Borders);
            if(ImGui::Selectable("Display", m_category == Display))
            {
                m_category = Display;
            }
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("SettingsContent", ImVec2(kContentwidth, kContentHeight),
                              ImGuiChildFlags_Borders);
            switch(m_category)
            {
                case Display:
                {
                    RenderDisplayOptions();
                }
                break;
            }
            ImGui::EndChild();

            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x -
                                 ImGui::CalcTextSize("O").x -
                                 2 * ImGui::GetStyle().FramePadding.x);
            ImGui::SetCursorPosY(ImGui::GetCursorPos().y -
                                 ImGui::GetFrameHeightWithSpacing());
            if(ResetButton())
            {
                ResetCategory(m_category);
            }

            // Bottom bar for Ok/Cancel
            float button_width =
                ImGui::CalcTextSize("Cancel").x + 2 * ImGui::GetStyle().FramePadding.x;
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x -
                                 ImGui::GetStyle().ItemSpacing.x - 2 * button_width);
            if(ImGui::Button("Ok", ImVec2(button_width, 0)))
            {
                m_usersettings.display_settings.dpi_based_scaling =
                    m_font_settings.dpi_scaling;
                m_usersettings.display_settings.font_size_index =
                    m_font_settings.size_index;

                m_settings_changed = true;
                m_should_open      = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
            {
                m_usersettings     = m_usersettings_initial;
                m_settings_changed = true;
                m_should_open      = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(2);

        if(m_settings_changed)
        {
            m_settings.ApplyUserSettings();
            m_settings_changed = false;
        }
    }
}

void
SettingsPanel::RenderDisplayOptions()
{
    ImGuiStyle& style       = ImGui::GetStyle();
    int         theme_index = m_usersettings.display_settings.use_dark_mode ? 1 : 0;

    // Theme selection
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Theme");
    ImGui::SameLine();
    if(ImGui::Combo("##theme", &theme_index, "Light\0Dark\0\0"))
    {
        m_usersettings.display_settings.use_dark_mode = (theme_index == 0) ? false : true;
        m_settings_changed                            = true;
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Switch between dark and light UI themes.");
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Fonts");
    ImGui::Separator();

    // DPI-based scaling toggle
    ImGui::Checkbox("DPI-based Font Scaling", &m_font_settings.dpi_scaling);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically scale font size based on display DPI.");
    }

    // Font size section
    float button_width = ImGui::CalcTextSize("+").x + 2 * style.FramePadding.x;
    ImGui::BeginDisabled(m_font_settings.dpi_scaling);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Font Size");
    ImGui::SameLine();
    if(ImGui::Button("-", ImVec2(button_width, 0)) && m_font_settings.size_index > 0)
    {
        m_font_settings.size_index--;
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("000").x +
                            ImGui::GetFrameHeightWithSpacing());
    ImGui::Combo("##font_size", &m_font_settings.size_index, m_font_sizes_ptr.data(),
                 m_font_sizes_ptr.size());
    ImGui::SameLine();
    if(ImGui::Button("+", ImVec2(button_width, 0)) &&
       m_font_settings.size_index < m_fonts.GetAvailableFonts().size())
    {
        m_font_settings.size_index++;
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Increase or decrease the font size for the UI.");
    }

    // Font preview
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Preview");
    ImGui::SameLine();
    ImFont* preview_font = m_fonts.GetFontByIndex(m_font_settings.dpi_scaling
                                                      ? m_fonts.GetDPIScaledFontIndex()
                                                      : m_font_settings.size_index);
    if(preview_font)
    {
        ImGui::Spacing();
        ImGui::SameLine();
        ImGui::PushFont(preview_font);
        ImGui::GetWindowDrawList()->AddRectFilled(
            ImGui::GetCursorScreenPos() - ImVec2(style.FramePadding.x, 0),
            ImGui::GetCursorScreenPos() +
                ImVec2(ImGui::CalcTextSize("AMD ROCm Visualizer").x +
                           style.FramePadding.x,
                       ImGui::GetFrameHeightWithSpacing()),
            ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg)),
            style.FrameRounding);
        ImGui::Text("AMD ROCm Visualizer");
        ImGui::PopFont();
    }
}

void
SettingsPanel::ResetCategory(Category category)
{
    const UserSettings& usersettings_default = m_settings.GetDefaultUserSettings();
    switch(m_category)
    {
        case Display:
        {
            m_usersettings.display_settings = usersettings_default.display_settings;
            m_font_settings.dpi_scaling =
                usersettings_default.display_settings.dpi_based_scaling;
            m_font_settings.size_index =
                usersettings_default.display_settings.font_size_index;
        }
        break;
    }
    m_settings_changed = true;
}

bool
SettingsPanel::ResetButton()
{
    bool        clicked = false;
    ImGuiStyle& style   = ImGui::GetStyle();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::BeginChild("reset_button",
                      ImGui::CalcTextSize("O") + style.FramePadding + style.FramePadding);
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                          m_settings.GetColor(Colors::kTransparent));
    ImGui::PushFont(m_fonts.GetIconFont(FontType::kDefault));
    clicked = ImGui::Button(ICON_ARROWS_CYCLE);
    ImGui::PopFont();
    ImGui::PopStyleColor(3);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,
                        m_settings.GetDefaultStyle().WindowPadding);
    if(ImGui::BeginItemTooltip())
    {
        ImGui::TextUnformatted("Restore defaults");
        ImGui::EndTooltip();
    }
    ImGui::PopStyleVar();
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    return clicked;
}

}  // namespace View
}  // namespace RocProfVis
