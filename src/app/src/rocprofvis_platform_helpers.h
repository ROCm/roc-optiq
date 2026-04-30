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
