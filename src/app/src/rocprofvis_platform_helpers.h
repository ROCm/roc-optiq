// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

// Declarations for small, platform-specific helpers that work around OS quirks.
// Each function is implemented only on the platform(s) it applies to; callers
// must guard usage with the matching platform macro (e.g. #ifdef __APPLE__).

#ifdef __linux__
#    include "imgui.h"
#    include <unordered_map>
#endif

namespace RocProfVis
{
namespace Platform
{

// Live keyboard modifier state read directly from the operating system.
struct ModifierState
{
    bool ctrl;
    bool shift;
    bool alt;
    bool super;
};

// Returns the current OS keyboard modifier state, independent of GLFW's cached
// per-window key state.
//
// macOS only (implemented via +[NSEvent modifierFlags]). Used to recover from
// system gestures (Mission Control via Ctrl+Up, screenshot chords, etc.) that
// consume a modifier key-up before GLFW observes it, which otherwise leaves a
// modifier "stuck" down.
ModifierState
get_os_modifier_state();

// macOS only. Points the Vulkan loader (VK_ICD_FILENAMES / VK_DRIVER_FILES) at
// the MoltenVK ICD manifest bundled in the .app. No-op if the manifest is
// missing or either variable is already set. Call before Vulkan/GLFW init.
void
configure_bundled_vulkan_icd();

#ifdef __linux__

// Linux only. Reconcile every secondary viewport's ImGui position with the
// actual OS window position, so hit-testing agrees with what is on screen.
// Call once per frame after NewFrame(); pair with
// restore_secondary_viewport_intended_pos() after Render().
void
snap_secondary_viewports_to_os_pos(
    std::unordered_map<ImGuiID, ImVec2>& viewport_intended_pos);

// Linux only. Restore the drag-target positions saved by
// snap_secondary_viewports_to_os_pos() so the requested move is still sent to
// the OS. Call after Render() and before UpdatePlatformWindows().
void
restore_secondary_viewport_intended_pos(
    std::unordered_map<ImGuiID, ImVec2>& viewport_intended_pos);

// Linux only. Repair stale X11 pointer routing after a floating-window drag
// ends. Gated by the drag-repair policy described below, so this is a no-op
// unless the user has opted in.
void
raise_dragged_viewport_after_release();

// Override the policy that decides whether the post-drag click-through
// fix runs (raise_dragged_viewport_after_release).  This does NOT
// affect the always-on corner-lock cursor-offset fix, which is
// engaged unconditionally on any Wayland session.
//
// Resolution order at call sites is (highest to lowest priority):
//   1. CLI-supplied override (set via these functions from main()).
//   2. Env var ROCPROFVIS_DRAG_REPAIR ("on"/"1"/"true"/"yes" => on,
//      "off"/"0"/"false"/"no" => off, anything else ignored).
//   3. Default OFF.  No auto-detect tier: the only known-buggy
//      configuration is Ubuntu Wayland, which users must opt into
//      explicitly because the fix incurs a magic-lamp flicker on
//      every drag-release.
//
// Callers in main.cpp typically call set_drag_repair_override() once
// after CLI parsing, when the user has passed --drag-repair on|off.
// If the user omits the flag (or passes "auto"), the override stays
// unset and policy falls through to the env-var / default tier.
void
set_drag_repair_override(bool on);

#endif  // __linux__

}  // namespace Platform
}  // namespace RocProfVis
