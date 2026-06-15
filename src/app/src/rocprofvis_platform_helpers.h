// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

// Declarations for small, platform-specific helpers that work around OS quirks.
// Each function is implemented only on the platform(s) it applies to; callers
// must guard usage with the matching platform macro (e.g. #ifdef __APPLE__).

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

}  // namespace Platform
}  // namespace RocProfVis
