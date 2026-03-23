// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_hotkey_manager.h"

#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

HotkeyManager* HotkeyManager::s_instance = nullptr;

HotkeyManager&
HotkeyManager::GetInstance()
{
    if(!s_instance)
    {
        s_instance = new HotkeyManager();
    }
    return *s_instance;
}

void
HotkeyManager::DestroyInstance()
{
    delete s_instance;
    s_instance = nullptr;
}

HotkeyManager::HotkeyManager()
: m_last_processed_frame(-1)
{
    RegisterDefaultActions();
}

HotkeyManager::~HotkeyManager() = default;

void
HotkeyManager::RegisterAction(const std::string& id,
                               const std::string& display_name,
                               const std::string& category,
                               HotkeyBinding      default_binding,
                               ActionType          type,
                               bool                active_during_text_input)
{
    if(m_action_index.count(id))
    {
        spdlog::warn("HotkeyManager: action '{}' already registered", id);
        return;
    }

    HotkeyAction action;
    action.id                       = id;
    action.display_name             = display_name;
    action.category                 = category;
    action.default_binding          = default_binding;
    action.current_binding          = default_binding;
    action.type                     = type;
    action.active_during_text_input = active_during_text_input;

    m_action_index[id] = m_actions.size();
    m_actions.push_back(std::move(action));
}

void
HotkeyManager::RegisterDefaultActions()
{
    RegisterAction("timeline.pan_left", "Pan Left", "Timeline",
                   {ImGuiKey_A, ImGuiKey_LeftArrow});
    RegisterAction("timeline.pan_right", "Pan Right", "Timeline",
                   {ImGuiKey_D, ImGuiKey_RightArrow});
    RegisterAction("timeline.zoom_in", "Zoom In", "Timeline",
                   {ImGuiKey_W, ImGuiKey_None});
    RegisterAction("timeline.zoom_out", "Zoom Out", "Timeline",
                   {ImGuiKey_S, ImGuiKey_None});
    RegisterAction("timeline.scroll_up", "Scroll Up", "Timeline",
                   {ImGuiKey_UpArrow, ImGuiKey_None});
    RegisterAction("timeline.scroll_down", "Scroll Down", "Timeline",
                   {ImGuiKey_DownArrow, ImGuiKey_None});
    RegisterAction("timeline.clear_selection", "Clear Selection", "Timeline",
                   {ImGuiKey_Escape, ImGuiKey_None});
    RegisterAction("timeline.toggle_mark", "Toggle Mark", "Timeline",
                   {ImGuiKey_M, ImGuiKey_None});

    for(int i = 0; i <= 9; ++i)
    {
        ImGuiKey    digit = static_cast<ImGuiKey>(ImGuiKey_0 + i);
        std::string idx   = std::to_string(i);

        RegisterAction("bookmark.save_" + idx,
                       "Save Bookmark " + idx, "Bookmarks",
                       {static_cast<ImGuiKeyChord>(digit | ImGuiMod_Ctrl), ImGuiKey_None});

        RegisterAction("bookmark.restore_" + idx,
                       "Restore Bookmark " + idx, "Bookmarks",
                       {digit, ImGuiKey_None});
    }

    RegisterAction("modifier.multi_select", "Multi-Select", "Modifiers",
                   {ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kHold);
    RegisterAction("modifier.region_select", "Region Select", "Modifiers",
                   {ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kHold);
    RegisterAction("modifier.speed_boost", "Speed Boost", "Modifiers",
                   {ImGuiMod_Shift, ImGuiKey_None}, ActionType::kHold);

    RegisterAction("edit.cancel", "Cancel Edit", "Editing",
                   {ImGuiKey_Escape, ImGuiKey_None},
                   ActionType::kPress, true);
}

bool
HotkeyManager::IsKeyChordPressed(ImGuiKeyChord chord) const
{
    if(chord == ImGuiKey_None)
        return false;

    ImGuiKey       key       = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
    ImGuiKeyChord  mod_flags = chord & ImGuiMod_Mask_;
    const ImGuiIO& io        = ImGui::GetIO();

    if(key != ImGuiKey_None)
    {
        if(!ImGui::IsKeyPressed(key, true))
            return false;
    }

    if((mod_flags & ImGuiMod_Ctrl) && !io.KeyCtrl)    return false;
    if((mod_flags & ImGuiMod_Shift) && !io.KeyShift)   return false;
    if((mod_flags & ImGuiMod_Alt) && !io.KeyAlt)       return false;
    if(!(mod_flags & ImGuiMod_Ctrl) && io.KeyCtrl)     return false;

    return true;
}

bool
HotkeyManager::IsKeyChordHeld(ImGuiKeyChord chord) const
{
    if(chord == ImGuiKey_None)
        return false;

    ImGuiKey       key       = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
    ImGuiKeyChord  mod_flags = chord & ImGuiMod_Mask_;
    const ImGuiIO& io        = ImGui::GetIO();

    bool held = false;

    if(key != ImGuiKey_None)
    {
        held = ImGui::IsKeyDown(key);
    }

    if(key == ImGuiKey_None)
    {
        held = true;
        if(mod_flags & ImGuiMod_Ctrl)  held = held && io.KeyCtrl;
        if(mod_flags & ImGuiMod_Shift) held = held && io.KeyShift;
        if(mod_flags & ImGuiMod_Alt)   held = held && io.KeyAlt;
    }
    else
    {
        if(mod_flags & ImGuiMod_Ctrl)  held = held || io.KeyCtrl;
        if(mod_flags & ImGuiMod_Shift) held = held || io.KeyShift;
        if(mod_flags & ImGuiMod_Alt)   held = held || io.KeyAlt;
    }

    return held;
}

void
HotkeyManager::ProcessInput()
{
    int frame = ImGui::GetFrameCount();
    if(frame == m_last_processed_frame)
        return;
    m_last_processed_frame = frame;

    m_triggered_this_frame.clear();
    m_held_this_frame.clear();

    const ImGuiIO& io = ImGui::GetIO();
    bool text_active = io.WantTextInput || ImGui::IsAnyItemActive();
    bool popup_open  = ImGui::IsPopupOpen(
        "", ImGuiPopupFlags_AnyPopup | ImGuiHoveredFlags_NoPopupHierarchy);

    for(const auto& action : m_actions)
    {
        if((popup_open || text_active) && !action.active_during_text_input)
            continue;

        if(action.type == ActionType::kPress)
        {
            if(IsKeyChordPressed(action.current_binding.primary) ||
               IsKeyChordPressed(action.current_binding.alternate))
            {
                m_triggered_this_frame.insert(action.id);
            }
        }
        else if(action.type == ActionType::kHold)
        {
            if(IsKeyChordHeld(action.current_binding.primary) ||
               IsKeyChordHeld(action.current_binding.alternate))
            {
                m_held_this_frame.insert(action.id);
            }
        }
    }
}

bool
HotkeyManager::WasActionTriggered(const std::string& action_id) const
{
    return m_triggered_this_frame.count(action_id) > 0;
}

bool
HotkeyManager::IsActionHeld(const std::string& action_id) const
{
    return m_held_this_frame.count(action_id) > 0;
}

void
HotkeyManager::SetBinding(const std::string& action_id, HotkeyBinding new_binding)
{
    auto* action = FindAction(action_id);
    if(action)
        action->current_binding = new_binding;
}

void
HotkeyManager::ResetBinding(const std::string& action_id)
{
    auto* action = FindAction(action_id);
    if(action)
        action->current_binding = action->default_binding;
}

void
HotkeyManager::ResetAllBindings()
{
    for(auto& action : m_actions)
        action.current_binding = action.default_binding;
}

const std::vector<HotkeyAction>&
HotkeyManager::GetActions() const
{
    return m_actions;
}

HotkeyAction*
HotkeyManager::FindAction(const std::string& action_id)
{
    auto it = m_action_index.find(action_id);
    if(it == m_action_index.end())
        return nullptr;
    return &m_actions[it->second];
}

std::string
HotkeyManager::KeyChordToString(ImGuiKeyChord chord)
{
    if(chord == ImGuiKey_None)
        return "None";

    std::string result;
    if(chord & ImGuiMod_Ctrl)  result += "Ctrl+";
    if(chord & ImGuiMod_Shift) result += "Shift+";
    if(chord & ImGuiMod_Alt)   result += "Alt+";

    ImGuiKey key = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
    if(key != ImGuiKey_None)
    {
        const char* name = ImGui::GetKeyName(key);
        result += name ? name : "???";
    }
    else if(!result.empty())
    {
        result.pop_back();
    }

    return result.empty() ? "None" : result;
}

ImGuiKeyChord
HotkeyManager::StringToKeyChord(const std::string& str)
{
    if(str.empty() || str == "None")
        return ImGuiKey_None;

    ImGuiKeyChord chord     = 0;
    std::string   remaining = str;

    if(remaining.substr(0, 5) == "Ctrl+")
    {
        chord |= ImGuiMod_Ctrl;
        remaining = remaining.substr(5);
    }
    if(remaining.substr(0, 6) == "Shift+")
    {
        chord |= ImGuiMod_Shift;
        remaining = remaining.substr(6);
    }
    if(remaining.substr(0, 4) == "Alt+")
    {
        chord |= ImGuiMod_Alt;
        remaining = remaining.substr(4);
    }

    if(!remaining.empty())
    {
        for(int k = ImGuiKey_NamedKey_BEGIN; k < ImGuiKey_NamedKey_END; ++k)
        {
            const char* name = ImGui::GetKeyName(static_cast<ImGuiKey>(k));
            if(name && remaining == name)
            {
                chord |= k;
                return chord;
            }
        }
    }

    return chord;
}

}  // namespace View
}  // namespace RocProfVis
