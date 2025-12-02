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

// Default windowed mode settings
constexpr int DEFAULT_WINDOWED_XPOS = 100;
constexpr int DEFAULT_WINDOWED_YPOS = 100;
constexpr int DEFAULT_WINDOWED_WIDTH = 1280;
constexpr int DEFAULT_WINDOWED_HEIGHT = 720;

// Structure to track fullscreen state
struct FullscreenState
{
    bool is_fullscreen;
    int  windowed_xpos;
    int  windowed_ypos;
    int  windowed_width;
    int  windowed_height;
};

// Initialize fullscreen state with current window position and size
void init_fullscreen_state(GLFWwindow* window, FullscreenState& state);

// Get the monitor where the window is currently located
GLFWmonitor* get_current_monitor(GLFWwindow* window);

// Toggle between fullscreen and windowed mode
void toggle_fullscreen(GLFWwindow* window, FullscreenState& state);

// Sync fullscreen state with actual window state (in case of OS-initiated changes)
void sync_fullscreen_state(GLFWwindow* window, int width, int height, FullscreenState& state);

// Create a GLFW image from an icon image
std::pair<GLFWimage, unsigned char*> create_icon(const unsigned char* icon_data, size_t icon_data_len);

// Free icon image data
void free_icon(unsigned char* pixels);

}  // namespace View
}  // namespace RocProfVis

