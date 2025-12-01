// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <utility>

namespace RocProfVis
{
namespace View
{

// Structure to track fullscreen state
struct FullscreenState
{
    bool is_fullscreen;
    int  windowed_xpos;
    int  windowed_ypos;
    int  windowed_width;
    int  windowed_height;
};

// Get the monitor where the window is currently located
GLFWmonitor* get_current_monitor(GLFWwindow* window);

// Toggle between fullscreen and windowed mode
void toggle_fullscreen(GLFWwindow* window, FullscreenState& state);

// Create a GLFW image from an icon image
std::pair<GLFWimage, unsigned char*> create_icon(const unsigned char* icon_data, size_t icon_data_len);

// Free icon image data
void free_icon(unsigned char* pixels);

}  // namespace View
}  // namespace RocProfVis

