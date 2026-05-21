// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_settings_manager.h"
#include <list>

namespace RocProfVis
{
namespace View
{

class SettingsPanel
{
public:
    SettingsPanel(SettingsManager& settings);
    ~SettingsPanel();

    void Show();
    void Render();

private:
    struct FontSettings
    {
        bool dpi_scaling;
        int  size_index;
    };
    enum Category
    {
        Display,
        Units,
        Other,
        Hotkeys
    };

    void RenderDisplayOptions();
    void RenderUnitOptions();
    void RenderOtherSettings();
    void RenderHotkeySettings();

    void ResetDisplayOptions();
    void ResetUnitOptions();
    void ResetHotkeySettings();
    void StealChord(HotkeyActionId from, ImGuiKeyChord chord);

    bool ResetButton();

    bool                     m_should_open;
    bool                     m_settings_changed;
    bool                     m_settings_confirmed;
    Category                 m_category;
    std::list<std::string>   m_font_sizes;
    std::vector<const char*> m_font_sizes_ptr;

    SettingsManager& m_settings;
    FontManager&     m_fonts;
    UserSettings&    m_usersettings;

    const UserSettings& m_usersettings_default;
    UserSettings m_usersettings_initial;
    UserSettings m_usersettings_previous;
    FontSettings m_font_settings;

    HotkeyActionId m_rebinding_action  = HotkeyActionId::kCount;
    bool           m_rebinding_primary = true;
    bool           m_hotkeys_changed   = false;
};

}  // namespace View
}  // namespace RocProfVis
