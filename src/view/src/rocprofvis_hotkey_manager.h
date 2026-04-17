// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"

#include <array>
#include <string>

namespace RocProfVis
{
namespace View
{

enum class HotkeyActionId
{
    kPanLeft,
    kPanRight,
    kZoomIn,
    kZoomOut,
    kScrollUp,
    kScrollDown,
    kClearSelection,
    kToggleMark,
    kBookmarkSave0,
    kBookmarkSave1,
    kBookmarkSave2,
    kBookmarkSave3,
    kBookmarkSave4,
    kBookmarkSave5,
    kBookmarkSave6,
    kBookmarkSave7,
    kBookmarkSave8,
    kBookmarkSave9,
    kBookmarkRestore0,
    kBookmarkRestore1,
    kBookmarkRestore2,
    kBookmarkRestore3,
    kBookmarkRestore4,
    kBookmarkRestore5,
    kBookmarkRestore6,
    kBookmarkRestore7,
    kBookmarkRestore8,
    kBookmarkRestore9,
    kMultiSelect,
    kRegionSelect,
    kSpeedBoost,
    kCount
};

constexpr size_t kHotkeyActionCount = static_cast<size_t>(HotkeyActionId::kCount);

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

struct HotkeyActionInfo
{
    const char*   display_name;
    const char*   category;
    const char*   key;
    HotkeyBinding default_binding;
    ActionType    type;
    bool          allow_repeat;
    bool          active_during_text_input;
};

class HotkeyManager
{
public:
    HotkeyManager(const HotkeyManager&)            = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    static HotkeyManager& GetInstance();
    static void            DestroyInstance();

    void ProcessInput();

    bool WasActionTriggered(HotkeyActionId action) const;
    bool IsActionHeld(HotkeyActionId action) const;

    void           SetBinding(HotkeyActionId action, HotkeyBinding binding);
    void           ResetBinding(HotkeyActionId action);
    void           ResetAllBindings();
    HotkeyBinding  GetBinding(HotkeyActionId action) const;
    HotkeyActionId FindConflictingAction(ImGuiKeyChord  chord,
                                         HotkeyActionId except_action) const;

    static const HotkeyActionInfo& GetActionInfo(HotkeyActionId action);
    static HotkeyActionId          BookmarkSaveAction(int index);
    static HotkeyActionId          BookmarkRestoreAction(int index);

    static std::string   KeyChordToString(ImGuiKeyChord chord);
    static ImGuiKeyChord StringToKeyChord(const std::string& str);

private:
    HotkeyManager();
    ~HotkeyManager();

    bool IsKeyChordPressed(ImGuiKeyChord chord, bool repeat) const;
    bool IsKeyChordHeld(ImGuiKeyChord chord) const;

    static HotkeyManager* s_instance;

    std::array<HotkeyBinding, kHotkeyActionCount> m_bindings;
    std::array<bool, kHotkeyActionCount>           m_triggered;
    std::array<bool, kHotkeyActionCount>           m_held;
    int                                            m_last_processed_frame;
};

}  // namespace View
}  // namespace RocProfVis
