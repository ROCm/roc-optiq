// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

enum class ActionType
{
    kPress,
    kHold
};

struct HotkeyBinding
{
    ImGuiKeyChord primary   = ImGuiKey_None;
    ImGuiKeyChord alternate = ImGuiKey_None;
};

struct HotkeyAction
{
    std::string   id;
    std::string   display_name;
    std::string   category;
    HotkeyBinding default_binding;
    HotkeyBinding current_binding;
    ActionType    type                     = ActionType::kPress;
    bool          active_during_text_input = false;
};

class HotkeyManager
{
public:
    HotkeyManager(const HotkeyManager&)            = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    static HotkeyManager& GetInstance();
    static void            DestroyInstance();

    void ProcessInput();

    bool WasActionTriggered(const std::string& action_id) const;
    bool IsActionHeld(const std::string& action_id) const;

    void SetBinding(const std::string& action_id, HotkeyBinding new_binding);
    void ResetBinding(const std::string& action_id);
    void ResetAllBindings();

    const std::vector<HotkeyAction>& GetActions() const;
    HotkeyAction*                    FindAction(const std::string& action_id);

    static std::string   KeyChordToString(ImGuiKeyChord chord);
    static ImGuiKeyChord StringToKeyChord(const std::string& str);

private:
    HotkeyManager();
    ~HotkeyManager();

    void RegisterDefaultActions();
    void RegisterAction(const std::string& id,
                        const std::string& display_name,
                        const std::string& category,
                        HotkeyBinding      default_binding,
                        ActionType          type                     = ActionType::kPress,
                        bool                active_during_text_input = false);

    bool IsKeyChordPressed(ImGuiKeyChord chord) const;
    bool IsKeyChordHeld(ImGuiKeyChord chord) const;

    static HotkeyManager* s_instance;

    std::vector<HotkeyAction>        m_actions;
    std::map<std::string, size_t>    m_action_index;
    std::set<std::string>            m_triggered_this_frame;
    std::set<std::string>            m_held_this_frame;
    int                              m_last_processed_frame;
};

}  // namespace View
}  // namespace RocProfVis
