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

#endif
