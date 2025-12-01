// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "glfw_util.h"
#include "rocprofvis_view_module.h"
#include "spdlog/spdlog.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb-image/stb_image.h"

namespace RocProfVis
{
namespace View
{

void
init_fullscreen_state(GLFWwindow* window, FullscreenState& state)
{
    if(!window)
    {
        spdlog::warn("Cannot initialize fullscreen state: Window is not valid");
        return;
    }
    
    // Detect if window is already in fullscreen mode
    // glfwGetWindowMonitor returns nullptr if windowed, or monitor handle if fullscreen
    GLFWmonitor* monitor = glfwGetWindowMonitor(window);
    state.is_fullscreen = (monitor != nullptr);
    
    if(!state.is_fullscreen)
    {
        // Window is in windowed mode - save current position and size
        glfwGetWindowPos(window, &state.windowed_xpos, &state.windowed_ypos);
        glfwGetWindowSize(window, &state.windowed_width, &state.windowed_height);
    }
    else
    {
        // Window is already fullscreen - we can't retrieve windowed position/size
        // Set reasonable defaults (will be overwritten when user exits fullscreen)
        state.windowed_xpos = DEFAULT_WINDOWED_XPOS;
        state.windowed_ypos = DEFAULT_WINDOWED_YPOS;
        state.windowed_width = DEFAULT_WINDOWED_WIDTH;
        state.windowed_height = DEFAULT_WINDOWED_HEIGHT;
        spdlog::info("Window initialized in fullscreen mode, setting defaults for windowed mode");
    }
}

GLFWmonitor*
get_current_monitor(GLFWwindow* window)
{
    int wx, wy, ww, wh;
    glfwGetWindowPos(window, &wx, &wy);
    glfwGetWindowSize(window, &ww, &wh);

    // Calculate window center
    int window_center_x = wx + ww / 2;
    int window_center_y = wy + wh / 2;

    int           monitor_count;
    GLFWmonitor** monitors     = glfwGetMonitors(&monitor_count);
    GLFWmonitor*  best_monitor = nullptr;
    int           best_overlap = 0;

    for(int i = 0; i < monitor_count; i++)
    {
        int mx, my;
        glfwGetMonitorPos(monitors[i], &mx, &my);
        const GLFWvidmode* mode = glfwGetVideoMode(monitors[i]);
        if(!mode)
        {
            spdlog::warn("Failed to get video mode for monitor {}", i);
            continue;
        }

        // Check if window center is within this monitor's bounds
        if(window_center_x >= mx && window_center_x < mx + mode->width &&
           window_center_y >= my && window_center_y < my + mode->height)
        {
            return monitors[i];
        }

        // Calculate overlap area for fallback
        int overlap_x1 = (wx > mx) ? wx : mx;
        int overlap_y1 = (wy > my) ? wy : my;
        int overlap_x2 =
            ((wx + ww) < (mx + mode->width)) ? (wx + ww) : (mx + mode->width);
        int overlap_y2 =
            ((wy + wh) < (my + mode->height)) ? (wy + wh) : (my + mode->height);

        int overlap_area = 0;
        if(overlap_x2 > overlap_x1 && overlap_y2 > overlap_y1)
        {
            overlap_area = (overlap_x2 - overlap_x1) * (overlap_y2 - overlap_y1);
        }

        if(overlap_area > best_overlap)
        {
            best_overlap = overlap_area;
            best_monitor = monitors[i];
        }
    }

    // Fallback to primary monitor if no overlap found
    return best_monitor ? best_monitor : glfwGetPrimaryMonitor();
}

void
toggle_fullscreen(GLFWwindow* window, FullscreenState& state)
{
    if(!window) return;

    if(state.is_fullscreen)
    {
        // Switch to windowed mode
        glfwSetWindowMonitor(window, nullptr, state.windowed_xpos, state.windowed_ypos,
                             state.windowed_width, state.windowed_height, GLFW_DONT_CARE);
        state.is_fullscreen = false;
    }
    else
    {
        // Save current window position and size
        glfwGetWindowPos(window, &state.windowed_xpos, &state.windowed_ypos);
        glfwGetWindowSize(window, &state.windowed_width, &state.windowed_height);

        // Get the monitor where the window is currently located
        GLFWmonitor*       monitor = get_current_monitor(window);
        const GLFWvidmode* mode    = glfwGetVideoMode(monitor);
        if(!mode)
        {
            spdlog::warn(
                "Cannot switch to fullscreen mode: Failed to get video mode for monitor");
            return;
        }
        // Switch to fullscreen mode on the current monitor
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height,
                             mode->refreshRate);
        state.is_fullscreen = true;
    }

    // Update the view layer with the new fullscreen state
    rocprofvis_view_set_fullscreen_state(state.is_fullscreen);
}

std::pair<GLFWimage, unsigned char*>
create_icon(const unsigned char* icon_data, size_t icon_data_len)
{
    int            width, height, channels;
    unsigned char* pixels =
        stbi_load_from_memory(icon_data, static_cast<int>(icon_data_len), &width, &height,
                              &channels, STBI_rgb_alpha);

    GLFWimage image;
    if(!pixels)
    {
        spdlog::error("Failed to load icon image: {}", stbi_failure_reason());
        image = { 0, 0, nullptr };
        return { image, nullptr };
    }
    else
    {
        image.width  = width;
        image.height = height;
        image.pixels = pixels;
        return { image, pixels };
    }
}

void
free_icon(unsigned char* pixels)
{
    stbi_image_free(pixels);
}

}  // namespace View
}  // namespace RocProfVis
