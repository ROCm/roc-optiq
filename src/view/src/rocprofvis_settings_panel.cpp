// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_settings_panel.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"

// Layout constants
constexpr float kCategorywidth = 150.0f;
constexpr float kContentwidth  = 550.0f;
constexpr float kContentHeight = 450.0f;

namespace RocProfVis
{
namespace View
{

SettingsPanel::SettingsPanel(SettingsManager& settings)
: m_should_open(false)
, m_settings_changed(false)
, m_settings_confirmed(false)
, m_category(Display)
, m_settings(settings)
, m_fonts(settings.GetFontManager())
, m_usersettings_default(settings.GetDefaultUserSettings())
, m_usersettings_initial(m_usersettings_default)
, m_usersettings(settings.GetUserSettings())
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
    m_usersettings_previous     = m_usersettings;
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
        ImGuiStyle& style = ImGui::GetStyle();
        
        // Center and fix position - non-moveable
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        
        ImGui::SetNextWindowSizeConstraints(ImVec2(720, 500), ImVec2(FLT_MAX, FLT_MAX));

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(20, 16));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(12, 6));
        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 6.0f);

        if(ImGui::BeginPopupModal("Settings", nullptr, 
            ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoResize | 
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove))
        {
            ImGui::PushStyleColor(ImGuiCol_Text, m_settings.GetColor(Colors::kTextMain));

            ImGui::PushFont(m_settings.GetFontManager().GetFont(FontType::kLarge));
            ImGui::TextUnformatted("Settings");
            ImGui::PopStyleColor();
            ImGui::PopFont();
            
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::BeginChild("SettingsCategories",
                              ImVec2(kCategorywidth, kContentHeight),
                              ImGuiChildFlags_Borders);
            
            // Style category selectables
            ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.0f, 0.5f));
            ImGui::PushStyleColor(ImGuiCol_Header, m_settings.GetColor(Colors::kAccentRed));
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, m_settings.GetColor(Colors::kAccentRedHover));
            ImGui::PushStyleColor(ImGuiCol_HeaderActive, m_settings.GetColor(Colors::kAccentRedActive));
            
            if(ImGui::Selectable("Display", m_category == Display, 0, ImVec2(0, 0)))
            {
                m_category = Display;
            }
            if(ImGui::Selectable("Units", m_category == Units, 0, ImVec2(0, 0)))
            {
                m_category = Units;
            }
            if(ImGui::Selectable("Other", m_category == Other, 0, ImVec2(0, 0)))
            {
                m_category = Other;
            }
            
            ImGui::PopStyleColor(3);
            ImGui::PopStyleVar();
            ImGui::EndChild();

            ImGui::SameLine();

            ImGui::BeginChild("SettingsContent", ImVec2(kContentwidth, kContentHeight),
                              ImGuiChildFlags_Borders);
            switch(m_category)
            {
                case Display:
                {
                    RenderDisplayOptions();
                    break;
                }
                case Units:
                {
                    RenderUnitOptions();
                    break;
                }
                case Other:
                {
                    RenderOtherSettings();
                }
                break;
            }
            ImGui::EndChild();

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            // Bottom bar with Reset, Cancel, and OK buttons
            float button_width = 100.0f;
            float available_width = ImGui::GetContentRegionAvail().x;
            
            // Reset button (left side)
            if(ResetButton())
            {
                switch(m_category)
                {
                    case Display:
                    {
                        ResetDisplayOptions();
                        break;
                    }
                    case Units:
                    {
                        ResetUnitOptions();
                        break;
                    }
                    break;
                }
            }

            float button_start_x = available_width - (button_width * 2 + style.ItemSpacing.x);
            ImGui::SetCursorPosX(button_start_x);

            if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
            {
                auto temp_currentsettings = m_usersettings;
                m_usersettings            = m_usersettings_initial;
                m_settings.ApplyUserSettings(temp_currentsettings, false);
                m_settings_changed = true;
                m_should_open      = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::SameLine();

            ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kAccentRed));
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_settings.GetColor(Colors::kAccentRedHover));
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, m_settings.GetColor(Colors::kAccentRedActive));
            ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(255, 255, 255, 255));
            
            if(ImGui::Button("OK", ImVec2(button_width, 0)))
            {
                m_usersettings.display_settings.dpi_based_scaling =
                    m_font_settings.dpi_scaling;
                m_usersettings.display_settings.font_size_index =
                    m_font_settings.size_index;

                m_settings_changed   = true;
                m_settings_confirmed = true;
                m_should_open        = false;
                ImGui::CloseCurrentPopup();
            }
            
            ImGui::PopStyleColor(4);
            ImGui::SetItemDefaultFocus();

            ImGui::EndPopup();
        }
        ImGui::PopStyleVar(6);

        if(m_settings_changed)
        {
            m_settings.ApplyUserSettings(m_usersettings_previous, m_settings_confirmed);
            m_usersettings_previous = m_settings.GetUserSettings();
            m_settings_changed   = false;
            m_settings_confirmed = false;
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
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Light").x + 2 * style.FramePadding.x +
                            ImGui::GetFrameHeightWithSpacing());
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
    ImGui::Spacing();
    ImGui::TextUnformatted("Fonts");
    ImGui::Separator();
    ImGui::Spacing();

    // DPI-based scaling toggle
    ImGui::Checkbox("DPI-based Font Scaling", &m_font_settings.dpi_scaling);
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Automatically scale font size based on display DPI.");
    }

    ImGui::Spacing();

    // Font size section
    float button_width = ImGui::CalcTextSize("+").x + 2 * style.FramePadding.x;
    ImGui::BeginDisabled(m_font_settings.dpi_scaling);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Font Size");
    ImGui::SameLine();
    ImGui::BeginDisabled(m_font_settings.size_index < 1);
    if(ImGui::Button("-", ImVec2(button_width, 0)))
    {
        m_font_settings.size_index--;
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("00").x + 2 * style.FramePadding.x +
                            ImGui::GetFrameHeightWithSpacing());
    ImGui::Combo("##font_size", &m_font_settings.size_index, m_font_sizes_ptr.data(),
                 static_cast<int>(m_font_sizes_ptr.size()));
    ImGui::SameLine();
    ImGui::BeginDisabled(m_font_settings.size_index > m_fonts.GetAvailableFonts().size() - 2);
    if(ImGui::Button("+", ImVec2(button_width, 0)))
    {
        m_font_settings.size_index++;
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered())
    {
        ImGui::SetTooltip("Increase or decrease the font size for the UI.");
    }
    ImGui::EndDisabled();
    
    ImGui::Spacing();
    ImGui::Spacing();
    
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
            ImGui::GetCursorScreenPos() + ImGui::CalcTextSize("AMD ROCm Visualizer") +
                ImVec2(style.FramePadding.x, 2 * style.FramePadding.y),
            ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg)),
            style.FrameRounding);
        ImGui::Text("AMD ROCm Visualizer");
        ImGui::PopFont();
    }
}

void
SettingsPanel::RenderUnitOptions()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::TextUnformatted("Time");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Time Format");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Condensed Timecode").x +
                            2 * style.FramePadding.x +
                            ImGui::GetFrameHeightWithSpacing());
    int time_format_index = static_cast<int>(m_usersettings.unit_settings.time_format);
    //Options must match TimeFormat enum
    if(ImGui::Combo("##time_format", &time_format_index, 
        "Timecode\0"
        "Condensed Timecode\0"
        "Seconds\0"
        "Milliseconds\0"
        "Microseconds\0"
        "Nanoseconds\0\0"))
    {
        m_usersettings.unit_settings.time_format =
            static_cast<TimeFormat>(time_format_index);
        m_settings_changed = true;
    }
}

void
SettingsPanel::RenderOtherSettings()
{
    ImGui::TextUnformatted("Closing options");
    ImGui::Separator();
    ImGui::Spacing();
    
    ImGui::AlignTextToFramePadding();
    ImGui::Checkbox("Don't ask before exiting application", &m_usersettings.dont_ask_before_exit);
    ImGui::Checkbox("Don't ask before closing tabs", &m_usersettings.dont_ask_before_tab_closing);
    m_settings_changed = true;
}

void
SettingsPanel::ResetDisplayOptions()
{
    m_usersettings.display_settings = m_usersettings_default.display_settings;
    m_font_settings.dpi_scaling =
        m_usersettings_default.display_settings.dpi_based_scaling;
    m_font_settings.size_index = m_usersettings_default.display_settings.font_size_index;
    m_settings_changed         = true;
}

void
SettingsPanel::ResetUnitOptions()
{
    m_usersettings.unit_settings.time_format =
        m_usersettings_default.unit_settings.time_format;
    m_settings_changed = true;
}

bool
SettingsPanel::ResetButton()
{
    bool clicked = false;
    ImGuiStyle& style = ImGui::GetStyle();
    
    ImGui::PushStyleColor(ImGuiCol_Button, m_settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, m_settings.GetColor(Colors::kButtonHovered));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, m_settings.GetColor(Colors::kButtonActive));
    ImGui::PushStyleColor(ImGuiCol_Text, m_settings.GetColor(Colors::kTextDim));
    
    ImGui::PushFont(m_fonts.GetIconFont(FontType::kDefault));
    clicked = ImGui::Button(ICON_ARROWS_CYCLE, ImVec2(0, 0));
    ImGui::PopFont();
    
    ImGui::PopStyleColor(4);
    
    if(ImGui::IsItemHovered())
    {
        if(ImGui::BeginItemTooltip())
        {
            ImGui::TextUnformatted("Restore defaults");
            ImGui::EndTooltip();
        }
    }
    
    return clicked;
}

}  // namespace View
}  // namespace RocProfVis
