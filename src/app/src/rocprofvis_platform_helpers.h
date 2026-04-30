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

// Override the policy that decides whether the drag/click-through
// workaround runs.  Resolution order at call sites is (highest to
// lowest priority):
//   1. CLI-supplied override (set via these functions from main()).
//   2. Env var ROCPROFVIS_DRAG_REPAIR ("0"/"false" => off, otherwise on).
//   3. Auto-detect: Wayland session AND Ubuntu (via /etc/os-release).
//
// Callers in main.cpp typically call set_drag_repair_override() once
// after CLI parsing, when the user has passed --drag-repair=on|off.
// If the user passes --drag-repair=auto (or omits the flag), neither
// of these is called and the workaround falls through to the env-var
// or auto-detect tier.
void
set_drag_repair_override(bool on);
void
clear_drag_repair_override();

#endif
