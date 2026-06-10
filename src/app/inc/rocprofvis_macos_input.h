// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

namespace RocProfVis
{
namespace App
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
// per-window key state. macOS only: implemented via -[NSEvent modifierFlags].
// Used to recover from system gestures (Mission Control via Ctrl+Up, screenshot
// chords, etc.) that consume a modifier key-up before GLFW observes it, which
// otherwise leaves a modifier "stuck" down.
ModifierState
get_os_modifier_state();

}  // namespace App
}  // namespace RocProfVis
