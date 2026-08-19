// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_settings_manager.h"

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
    enum Category
    {
        Display,
        Units,
        Other,
        Assistant,
        Hotkeys
    };

    void RenderDisplayOptions();
    void RenderUnitOptions();
    void RenderOtherSettings();
    void RenderAssistantSettings();
    void RenderHotkeySettings();

    void ResetDisplayOptions();
    void ResetUnitOptions();
    void ResetHotkeySettings();
    void ResetAssistantOptions();
    // Name of the route the dialog is editing, used to key its stored API key.
    std::string ActiveAssistantProviderName() const;
    void StealChord(HotkeyActionId from, ImGuiKeyChord chord);

    bool ResetButton();

    bool             m_should_open;
    bool             m_settings_changed;
    bool             m_settings_confirmed;
    Category         m_category;
    SettingsManager& m_settings;
    FontManager&     m_fonts;
    UserSettings&    m_usersettings;

    const UserSettings& m_usersettings_default;
    UserSettings        m_usersettings_initial;
    UserSettings        m_usersettings_previous;

    // Pending font size index, applied to user settings on OK.
    int m_pending_font_size_index;

    HotkeyActionId m_rebinding_action  = HotkeyActionId::kCount;
    bool           m_rebinding_primary = true;
    bool           m_hotkeys_changed   = false;

    std::string m_assistant_token_draft;
    bool        m_assistant_show_token     = false;
    bool        m_assistant_clear_token    = false;
};

}  // namespace View
}  // namespace RocProfVis
