// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

// Platform-specific helper functions for implementing workarounds for platform-specific
// issues.

#ifdef __linux__

#    include "imgui.h"
#    include <unordered_map>

void
snap_secondary_viewports_to_os_pos(
    std::unordered_map<ImGuiID, ImVec2>& viewport_intended_pos);

void
restore_secondary_viewport_intended_pos(
    std::unordered_map<ImGuiID, ImVec2>& viewport_intended_pos);

void
raise_dragged_viewport_after_release();

// Workaround for the "black box left on the desktop" artifact seen on
// Wayland (RHEL 10 confirmed) when a secondary (floating) viewport is
// dragged so that part of it leaves the screen.  Wayland's compositor
// keeps compositing the previous buffer at the previous surface
// position for an extra frame, leaving a stale rectangle on the
// desktop where the window used to overlap.
//
// Call once per frame, AFTER ImGui::RenderPlatformWindowsDefault() but
// BEFORE the main backend present.  No-op on non-Wayland sessions and
// on frames where no secondary viewport moved.  See implementation for
// the exact mechanism (re-issue Platform_SetWindowPos + post a wakeup
// event so the compositor processes the move on the same frame the
// new buffer is presented).
void
nudge_wayland_viewports_after_render();

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
void
clear_drag_repair_override();

#endif
