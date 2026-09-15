// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_settings_manager.h"

#include <string>
#include <vector>

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
#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
        Assistant,
#endif
        Hotkeys
    };

    void RenderDisplayOptions();
    void RenderUnitOptions();
    void RenderOtherSettings();
    void RenderHotkeySettings();

    void ResetDisplayOptions();
    void ResetUnitOptions();
    void ResetHotkeySettings();
    // The Assistant page. Defined in
    // agenticprofiling/rocprofvis_ai_settings.cpp.
#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
    void RenderAssistantSettings();
    void ResetAssistantOptions();
    // Writes the pending key edits to the credential store, or drops them.
    void ApplyAssistantTokenEdits();
    void DiscardAssistantTokenEdits();
    // Name of the saved endpoint, used to key its stored API key.
    std::string ActiveAssistantProviderName() const;
#endif
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

#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
    // Key edits are held here and applied on OK, like every other page. The
    // draft is the replacement key, and the orphan list names endpoints whose
    // configuration Reset threw away - their saved keys have to go with them,
    // or they linger in the credential store with nothing pointing at them.
    std::string              m_assistant_token_draft;
    std::vector<std::string> m_assistant_orphaned_providers;
    bool                     m_assistant_show_token  = false;
    bool                     m_assistant_clear_token = false;
#endif
};

}  // namespace View
}  // namespace RocProfVis
