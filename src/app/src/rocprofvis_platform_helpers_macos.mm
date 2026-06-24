// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_platform_helpers.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <cstdlib>

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

void
configure_bundled_vulkan_icd()
{
    @autoreleasepool
    {
        if(getenv("VK_ICD_FILENAMES") != nullptr || getenv("VK_DRIVER_FILES") != nullptr)
        {
            return;
        }

        NSURL* icd = [[NSBundle mainBundle] URLForResource:@"MoltenVK_icd"
                                             withExtension:@"json"
                                              subdirectory:@"vulkan/icd.d"];
        if(icd == nil)
        {
            return;
        }

        const char* path = [[icd path] fileSystemRepresentation];
        if(path == nullptr)
        {
            return;
        }

        setenv("VK_ICD_FILENAMES", path, 1);
        setenv("VK_DRIVER_FILES", path, 1);
    }
}

}  // namespace Platform
}  // namespace RocProfVis
