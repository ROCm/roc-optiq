// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_settings_panel.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_font_manager.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_widget.h"

// Layout constants
constexpr float kCategorywidth            = 150.0f;
constexpr float kContentwidth             = 550.0f;
constexpr float kContentHeight            = 450.0f;
constexpr float kHotkeyCategoryColWidth   = 90.0f;
constexpr float kHotkeyBindingColWidth    = 120.0f;
constexpr float kHotkeyTableBottomMargin  = 60.0f;

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
        m_font_sizes.emplace_back(std::to_string(static_cast<int>(font->LegacySize)));
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
        PopUpStyle popup_style;
        popup_style.PushPopupStyles();
        popup_style.PushTitlebarColors();
        popup_style.CenterPopup();

        if(ImGui::BeginPopupModal("Settings", nullptr,
                                  ImGuiWindowFlags_AlwaysAutoResize |
                                      ImGuiWindowFlags_NoSavedSettings))
        {
            ImGui::BeginChild("SettingsCategories",
                              ImVec2(kCategorywidth, kContentHeight),
                              ImGuiChildFlags_Borders);
            if(ImGui::Selectable("Display", m_category == Display))
            {
                m_category = Display;
            }
            if(ImGui::Selectable("Units", m_category == Units))
            {
                m_category = Units;
            }
            if(ImGui::Selectable("Other", m_category == Other))
            {
                m_category = Other;
            }
            if(ImGui::Selectable("Hotkeys", m_category == Hotkeys))
            {
                m_category = Hotkeys;
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
                    break;
                }
                case Hotkeys:
                {
                    RenderHotkeySettings();
                    break;
                }
            }
            ImGui::EndChild();

            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x -
                                 ImGui::CalcTextSize("O").x -
                                 2 * ImGui::GetStyle().FramePadding.x);
            if(m_category != Hotkeys)
            {
                ImGui::SetCursorPosY(ImGui::GetCursorPos().y -
                                     ImGui::GetFrameHeightWithSpacing());
            }
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
                    case Hotkeys:
                    {
                        ResetHotkeySettings();
                        break;
                    }
                }
            }

            // Bottom bar for Ok/Cancel
            float button_width =
                ImGui::CalcTextSize("Cancel").x + 2 * ImGui::GetStyle().FramePadding.x;
            ImGui::SetCursorPosX(ImGui::GetContentRegionMax().x -
                                 ImGui::GetStyle().ItemSpacing.x - 2 * button_width);
            if(ImGui::Button("OK", ImVec2(button_width, 0)))
            {
                m_usersettings.display_settings.dpi_based_scaling =
                    m_font_settings.dpi_scaling;
                m_usersettings.display_settings.font_size_index =
                    m_font_settings.size_index;

                m_settings_changed   = true;
                m_settings_confirmed = true;
                m_should_open        = false;
                if(m_hotkeys_changed)
                {
                    m_settings.SaveHotkeySettings();
                    m_hotkeys_changed = false;
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
            {
                auto temp_currentsettings = m_usersettings;
                m_usersettings            = m_usersettings_initial;
                m_settings.ApplyUserSettings(temp_currentsettings, false);
                if(m_hotkeys_changed)
                {
                    HotkeyManager::GetInstance().ResetAllBindings();
                    m_hotkeys_changed = false;
                }
                m_settings_changed = true;
                m_should_open      = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        if(m_settings_changed)
        {
            m_settings.ApplyUserSettings(m_usersettings_previous, m_settings_confirmed);
            m_usersettings_previous = m_settings.GetUserSettings();
            m_settings_changed      = false;
            m_settings_confirmed    = false;
        }
        popup_style.PopStyles();
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
        SetTooltipStyled("Switch between dark and light UI themes.");
    }

    ImGui::Spacing();
    ImGui::TextUnformatted("Fonts");
    ImGui::Separator();

    // DPI-based scaling toggle
    ImGui::Checkbox("DPI-based Font Scaling", &m_font_settings.dpi_scaling);
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Automatically scale font size based on display DPI.");
    }

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
    ImGui::BeginDisabled(m_font_settings.size_index >
                         m_fonts.GetAvailableFonts().size() - 2);
    if(ImGui::Button("+", ImVec2(button_width, 0)))
    {
        m_font_settings.size_index++;
    }
    ImGui::EndDisabled();
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Increase or decrease the font size for the UI.");
    }
    ImGui::EndDisabled();
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
            ImGui::GetCursorScreenPos() + ImGui::CalcTextSize("AMD ROCm(TM) Optiq") +
                ImVec2(style.FramePadding.x, 2 * style.FramePadding.y),
            ImGui::GetColorU32(ImGui::GetStyleColorVec4(ImGuiCol_FrameBg)),
            style.FrameRounding);
        ImGui::Text("AMD ROCm(TM) Optiq");
        ImGui::PopFont();
    }
}

void
SettingsPanel::RenderUnitOptions()
{
    ImGuiStyle& style = ImGui::GetStyle();

    ImGui::TextUnformatted("Time");
    ImGui::Separator();
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Time Format");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("Condensed Timecode").x +
                            2 * style.FramePadding.x +
                            ImGui::GetFrameHeightWithSpacing());
    int time_format_index = static_cast<int>(m_usersettings.unit_settings.time_format);
    // Options must match TimeFormat enum
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
    ImGui::AlignTextToFramePadding();
    ImGui::Checkbox("Don't ask before exiting application",
                    &m_usersettings.dont_ask_before_exit);
    ImGui::Checkbox("Don't ask before closing tabs",
                    &m_usersettings.dont_ask_before_tab_closing);
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
    if(ImGui::IsItemHovered())
        SetTooltipStyled("Restore defaults");
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    return clicked;
}

void
SettingsPanel::RenderHotkeySettings()
{
    auto& hk = HotkeyManager::GetInstance();

    ImGui::TextUnformatted("Keyboard Shortcuts");
    ImGui::Separator();
    ImGui::Spacing();

    if(ImGui::BeginTable("##hotkeys_table", 4,
                         ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                             ImGuiTableFlags_ScrollY,
                         ImVec2(0, kContentHeight - kHotkeyTableBottomMargin)))
    {
        ImGui::TableSetupColumn("Category", ImGuiTableColumnFlags_WidthFixed, kHotkeyCategoryColWidth);
        ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Primary", ImGuiTableColumnFlags_WidthFixed, kHotkeyBindingColWidth);
        ImGui::TableSetupColumn("Alternate", ImGuiTableColumnFlags_WidthFixed, kHotkeyBindingColWidth);
        ImGui::TableHeadersRow();

        for(int i = 0; i < static_cast<int>(HotkeyActionId::kCount); ++i)
        {
            HotkeyActionId action_id = static_cast<HotkeyActionId>(i);
            const auto&    info      = HotkeyManager::GetActionInfo(action_id);
            HotkeyBinding  binding   = hk.GetBinding(action_id);

            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            ImGui::TextUnformatted(info.category);

            ImGui::TableSetColumnIndex(1);
            ImGui::TextUnformatted(info.display_name);

            ImGui::TableSetColumnIndex(2);
            {
                std::string label = HotkeyManager::KeyChordToString(binding.primary);
                if(m_rebinding_action == action_id && m_rebinding_primary)
                    label = "Press a key...";

                ImGui::PushID(i * 2);
                if(ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0)))
                {
                    m_rebinding_action  = action_id;
                    m_rebinding_primary = true;
                }
                ImGui::PopID();
            }

            ImGui::TableSetColumnIndex(3);
            {
                std::string label = HotkeyManager::KeyChordToString(binding.alternate);
                if(m_rebinding_action == action_id && !m_rebinding_primary)
                    label = "Press a key...";

                ImGui::PushID(i * 2 + 1);
                if(ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0)))
                {
                    m_rebinding_action  = action_id;
                    m_rebinding_primary = false;
                }
                ImGui::PopID();
            }
        }
        ImGui::EndTable();
    }

    if(m_rebinding_action != HotkeyActionId::kCount)
    {
        if(ImGui::IsKeyPressed(ImGuiKey_Escape))
        {
            m_rebinding_action = HotkeyActionId::kCount;
        }
        else
        {
            ImGuiKeyChord captured = ImGuiKey_None;
            for(int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
            {
                ImGuiKey key = static_cast<ImGuiKey>(k);
                if(key == ImGuiKey_LeftCtrl || key == ImGuiKey_RightCtrl ||
                   key == ImGuiKey_LeftShift || key == ImGuiKey_RightShift ||
                   key == ImGuiKey_LeftAlt || key == ImGuiKey_RightAlt)
                    continue;

                if(ImGui::IsKeyPressed(key, false))
                {
                    captured = key;
                    const ImGuiIO& io = ImGui::GetIO();
                    if(io.KeyCtrl)  captured |= ImGuiMod_Ctrl;
                    if(io.KeyShift) captured |= ImGuiMod_Shift;
                    if(io.KeyAlt)   captured |= ImGuiMod_Alt;
                    break;
                }
            }

            if(captured == ImGuiKey_None)
            {
                if(ImGui::IsKeyPressed(ImGuiKey_LeftCtrl) || ImGui::IsKeyPressed(ImGuiKey_RightCtrl))
                    captured = ImGuiMod_Ctrl;
                else if(ImGui::IsKeyPressed(ImGuiKey_LeftShift) || ImGui::IsKeyPressed(ImGuiKey_RightShift))
                    captured = ImGuiMod_Shift;
                else if(ImGui::IsKeyPressed(ImGuiKey_LeftAlt) || ImGui::IsKeyPressed(ImGuiKey_RightAlt))
                    captured = ImGuiMod_Alt;
            }

            if(captured != ImGuiKey_None)
            {
                HotkeyBinding binding = hk.GetBinding(m_rebinding_action);
                if(m_rebinding_primary)
                    binding.primary = captured;
                else
                    binding.alternate = captured;
                hk.SetBinding(m_rebinding_action, binding);
                m_hotkeys_changed  = true;
                m_rebinding_action = HotkeyActionId::kCount;
            }
        }
    }
}

void
SettingsPanel::ResetHotkeySettings()
{
    HotkeyManager::GetInstance().ResetAllBindings();
    m_hotkeys_changed = true;
}

}  // namespace View
}  // namespace RocProfVis