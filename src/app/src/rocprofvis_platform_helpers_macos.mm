// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_platform_helpers.h"

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>

#include <cstdlib>
#include <sys/sysctl.h>

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

bool
check_supported_mac_hardware()
{
    @autoreleasepool
    {
        // "hw.optional.arm64" is 1 on Apple Silicon and absent/0 on Intel.
        // This reports the real CPU even when the process runs under Rosetta.
        int    is_arm64 = 0;
        size_t size     = sizeof(is_arm64);
        if(sysctlbyname("hw.optional.arm64", &is_arm64, &size, nullptr, 0) == 0 &&
           is_arm64 == 1)
        {
            return true;
        }

        // This runs before GLFW/NSApplication init, so bootstrap the shared
        // application and bring it to the foreground; otherwise the alert can
        // appear behind other windows or without focus.
        [NSApplication sharedApplication];
        [NSApp setActivationPolicy:NSApplicationActivationPolicyRegular];
        [NSApp activateIgnoringOtherApps:YES];

        NSAlert* alert = [[NSAlert alloc] init];
        [alert setAlertStyle:NSAlertStyleCritical];
        [alert setMessageText:@"Unsupported Mac"];
        [alert setInformativeText:
                   @"ROCm\u2122 Optiq runs only on Apple Silicon (M-series) Macs. "
                   @"It is not compatible with, nor tested on, Intel-based Macs and "
                   @"cannot be opened on this machine."];
        [alert addButtonWithTitle:@"Quit"];

        [[alert window] setLevel:NSModalPanelWindowLevel];
        [alert runModal];

        return false;
    }
}

}  // namespace Platform
}  // namespace RocProfVis
