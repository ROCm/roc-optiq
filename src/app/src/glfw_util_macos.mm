// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "glfw_util.h"

#define GLFW_EXPOSE_NATIVE_COCOA
#include <GLFW/glfw3native.h>

#import <Cocoa/Cocoa.h>

namespace RocProfVis
{
namespace View
{

void
macos_request_toggle_fullscreen(GLFWwindow* window)
{
    if(!window)
    {
        return;
    }

    NSWindow* ns_window = glfwGetCocoaWindow(window);
    if(!ns_window)
    {
        return;
    }

    // Same path as the green (zoom) button; AppKit animates it asynchronously.
    [ns_window toggleFullScreen:nil];
}

bool
macos_is_native_fullscreen(GLFWwindow* window)
{
    if(!window)
    {
        return false;
    }

    NSWindow* ns_window = glfwGetCocoaWindow(window);
    if(!ns_window)
    {
        return false;
    }

    return ([ns_window styleMask] & NSWindowStyleMaskFullScreen) != 0;
}

}  // namespace View
}  // namespace RocProfVis
