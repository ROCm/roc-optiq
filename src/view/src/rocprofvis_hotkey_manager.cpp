// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_hotkey_manager.h"

namespace RocProfVis
{
namespace View
{

// clang-format off
static const HotkeyActionInfo kActionTable[kHotkeyActionCount] = {
    {"Pan Left",          "Timeline",  "pan_left",           {ImGuiKey_A,         ImGuiKey_LeftArrow},  ActionType::kPress, true,  false},
    {"Pan Right",         "Timeline",  "pan_right",          {ImGuiKey_D,         ImGuiKey_RightArrow}, ActionType::kPress, true,  false},
    {"Zoom In",           "Timeline",  "zoom_in",            {ImGuiKey_W,         ImGuiKey_None},       ActionType::kPress, true,  false},
    {"Zoom Out",          "Timeline",  "zoom_out",           {ImGuiKey_S,         ImGuiKey_None},       ActionType::kPress, true,  false},
    {"Scroll Up",         "Timeline",  "scroll_up",          {ImGuiKey_UpArrow,   ImGuiKey_None},       ActionType::kPress, true,  false},
    {"Scroll Down",       "Timeline",  "scroll_down",        {ImGuiKey_DownArrow, ImGuiKey_None},       ActionType::kPress, true,  false},
    {"Clear Selection",   "Timeline",  "clear_selection",    {ImGuiKey_Escape,    ImGuiKey_None},       ActionType::kPress, true,  false},
    {"Toggle Mark",       "Timeline",  "toggle_mark",        {ImGuiKey_M,         ImGuiKey_None},       ActionType::kPress, true,  false},

    {"Save Bookmark 0",  "Bookmarks", "bookmark_save_0",    {ImGuiKey_0 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Save Bookmark 1",  "Bookmarks", "bookmark_save_1",    {ImGuiKey_1 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Save Bookmark 2",  "Bookmarks", "bookmark_save_2",    {ImGuiKey_2 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Save Bookmark 3",  "Bookmarks", "bookmark_save_3",    {ImGuiKey_3 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Save Bookmark 4",  "Bookmarks", "bookmark_save_4",    {ImGuiKey_4 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Save Bookmark 5",  "Bookmarks", "bookmark_save_5",    {ImGuiKey_5 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Save Bookmark 6",  "Bookmarks", "bookmark_save_6",    {ImGuiKey_6 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Save Bookmark 7",  "Bookmarks", "bookmark_save_7",    {ImGuiKey_7 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Save Bookmark 8",  "Bookmarks", "bookmark_save_8",    {ImGuiKey_8 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Save Bookmark 9",  "Bookmarks", "bookmark_save_9",    {ImGuiKey_9 | ImGuiMod_Ctrl, ImGuiKey_None}, ActionType::kPress, false, false},

    {"Restore Bookmark 0", "Bookmarks", "bookmark_restore_0", {ImGuiKey_0, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Restore Bookmark 1", "Bookmarks", "bookmark_restore_1", {ImGuiKey_1, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Restore Bookmark 2", "Bookmarks", "bookmark_restore_2", {ImGuiKey_2, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Restore Bookmark 3", "Bookmarks", "bookmark_restore_3", {ImGuiKey_3, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Restore Bookmark 4", "Bookmarks", "bookmark_restore_4", {ImGuiKey_4, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Restore Bookmark 5", "Bookmarks", "bookmark_restore_5", {ImGuiKey_5, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Restore Bookmark 6", "Bookmarks", "bookmark_restore_6", {ImGuiKey_6, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Restore Bookmark 7", "Bookmarks", "bookmark_restore_7", {ImGuiKey_7, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Restore Bookmark 8", "Bookmarks", "bookmark_restore_8", {ImGuiKey_8, ImGuiKey_None}, ActionType::kPress, false, false},
    {"Restore Bookmark 9", "Bookmarks", "bookmark_restore_9", {ImGuiKey_9, ImGuiKey_None}, ActionType::kPress, false, false},

    {"Multi-Select",   "Modifiers", "multi_select",  {ImGuiMod_Ctrl,  ImGuiKey_None}, ActionType::kHold, true, true},
    {"Region Select",  "Modifiers", "region_select", {ImGuiMod_Ctrl,  ImGuiKey_None}, ActionType::kHold, true, true},
    {"Speed Boost",    "Modifiers", "speed_boost",   {ImGuiMod_Shift, ImGuiKey_None}, ActionType::kHold, true, true},
};
// clang-format on

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
    ResetAllBindings();
    m_triggered.fill(false);
    m_held.fill(false);
}

HotkeyManager::~HotkeyManager() = default;

const HotkeyActionInfo&
HotkeyManager::GetActionInfo(HotkeyActionId action)
{
    return kActionTable[static_cast<size_t>(action)];
}

HotkeyActionId
HotkeyManager::BookmarkSaveAction(int index)
{
    return static_cast<HotkeyActionId>(
        static_cast<int>(HotkeyActionId::kBookmarkSave0) + index);
}

HotkeyActionId
HotkeyManager::BookmarkRestoreAction(int index)
{
    return static_cast<HotkeyActionId>(
        static_cast<int>(HotkeyActionId::kBookmarkRestore0) + index);
}

bool
HotkeyManager::IsKeyChordPressed(ImGuiKeyChord chord, bool repeat) const
{
    if(chord == ImGuiKey_None)
        return false;

    ImGuiKey       key       = static_cast<ImGuiKey>(chord & ~ImGuiMod_Mask_);
    ImGuiKeyChord  mod_flags = chord & ImGuiMod_Mask_;
    const ImGuiIO& io        = ImGui::GetIO();

    if(key != ImGuiKey_None)
    {
        if(!ImGui::IsKeyPressed(key, repeat))
            return false;
    }

    if((mod_flags & ImGuiMod_Ctrl) && !io.KeyCtrl)   return false;
    if((mod_flags & ImGuiMod_Shift) && !io.KeyShift) return false;
    if((mod_flags & ImGuiMod_Alt) && !io.KeyAlt)     return false;
    if(!(mod_flags & ImGuiMod_Ctrl) && io.KeyCtrl)   return false;

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

    m_triggered.fill(false);
    m_held.fill(false);

    const ImGuiIO& io = ImGui::GetIO();
    bool text_active = io.WantTextInput || ImGui::IsAnyItemActive();
    bool popup_open  = ImGui::IsPopupOpen(
        "", ImGuiPopupFlags_AnyPopup | ImGuiHoveredFlags_NoPopupHierarchy);

    for(size_t i = 0; i < kHotkeyActionCount; ++i)
    {
        const auto& info    = kActionTable[i];
        const auto& binding = m_bindings[i];

        if((popup_open || text_active) && !info.active_during_text_input)
            continue;

        if(info.type == ActionType::kPress)
        {
            if(IsKeyChordPressed(binding.primary, info.allow_repeat) ||
               IsKeyChordPressed(binding.alternate, info.allow_repeat))
            {
                m_triggered[i] = true;
            }
        }
        else if(info.type == ActionType::kHold)
        {
            if(IsKeyChordHeld(binding.primary) ||
               IsKeyChordHeld(binding.alternate))
            {
                m_held[i] = true;
            }
        }
    }
}

bool
HotkeyManager::WasActionTriggered(HotkeyActionId action) const
{
    return m_triggered[static_cast<size_t>(action)];
}

bool
HotkeyManager::IsActionHeld(HotkeyActionId action) const
{
    return m_held[static_cast<size_t>(action)];
}

void
HotkeyManager::SetBinding(HotkeyActionId action, HotkeyBinding binding)
{
    m_bindings[static_cast<size_t>(action)] = binding;
}

void
HotkeyManager::ResetBinding(HotkeyActionId action)
{
    m_bindings[static_cast<size_t>(action)] =
        kActionTable[static_cast<size_t>(action)].default_binding;
}

void
HotkeyManager::ResetAllBindings()
{
    for(size_t i = 0; i < kHotkeyActionCount; ++i)
        m_bindings[i] = kActionTable[i].default_binding;
}

HotkeyBinding
HotkeyManager::GetBinding(HotkeyActionId action) const
{
    return m_bindings[static_cast<size_t>(action)];
}

HotkeyActionId
HotkeyManager::FindConflictingAction(ImGuiKeyChord  chord,
                                     HotkeyActionId except_action) const
{
    if(chord == ImGuiKey_None)
        return HotkeyActionId::kCount;

    const ActionType new_type =
        kActionTable[static_cast<size_t>(except_action)].type;

    for(size_t i = 0; i < kHotkeyActionCount; ++i)
    {
        if(static_cast<HotkeyActionId>(i) == except_action)
            continue;

        const auto& info    = kActionTable[i];
        const auto& binding = m_bindings[i];

        if(new_type == ActionType::kHold && info.type == ActionType::kHold)
            continue;

        if(binding.primary == chord || binding.alternate == chord)
            return static_cast<HotkeyActionId>(i);
    }

    return HotkeyActionId::kCount;
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
