// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_platform_helpers.h"

#import <AppKit/AppKit.h>

namespace RocProfVis
{
namespace Platform
{

ModifierState
get_os_modifier_state()
{
    // +[NSEvent modifierFlags] reflects the actual hardware modifier state and
    // is not affected by events that were delivered to another responder (e.g.
    // Mission Control swallowing the Control key-up during a Ctrl+Up gesture).
    const NSUInteger flags = [NSEvent modifierFlags];

    ModifierState state;
    state.ctrl  = (flags & NSEventModifierFlagControl) != 0;
    state.shift = (flags & NSEventModifierFlagShift) != 0;
    state.alt   = (flags & NSEventModifierFlagOption) != 0;
    state.super = (flags & NSEventModifierFlagCommand) != 0;
    return state;
}

}  // namespace Platform
}  // namespace RocProfVis
