// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "glfw_util.h"
#include "rocprofvis_view_module.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

// Upper bound on the corrective resizes issued after leaving fullscreen. The
// window manager normally settles within one or two corrections; the budget
// only exists so a geometry the window manager refuses outright cannot make us
// retry forever.
constexpr int WINDOWED_RESTORE_MAX_CORRECTIONS = 8;

// How long to keep watching the window after leaving fullscreen. This cannot
// stop at the first frame where the geometry looks right: the window manager's
// own reply to glfwSetWindowMonitor() can arrive tens of milliseconds later and
// move the window again, so the watch has to outlive it. Measured worst case on
// GNOME/Xwayland was under 50 ms.
constexpr double WINDOWED_RESTORE_WATCH_SECONDS = 0.5;

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
    state.is_fullscreen    = (monitor != nullptr);
    state.restore_attempts = 0;
    state.restore_deadline = 0.0;
    
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
        spdlog::debug("Window initialized in fullscreen mode, setting defaults for windowed mode");
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
    if(!window)
    {
        return;
    }

    if(state.is_fullscreen)
    {
        // Switch to windowed mode
        glfwSetWindowMonitor(window, nullptr, state.windowed_xpos, state.windowed_ypos,
                             state.windowed_width, state.windowed_height, GLFW_DONT_CARE);
        state.is_fullscreen = false;
        // The geometry handed to glfwSetWindowMonitor() is frequently not what
        // the window ends up with, so arrange for it to be checked and
        // corrected over the next few frames.
        state.restore_attempts = WINDOWED_RESTORE_MAX_CORRECTIONS;
        state.restore_deadline = glfwGetTime() + WINDOWED_RESTORE_WATCH_SECONDS;
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

// Leaving fullscreen with glfwSetWindowMonitor() asks for the saved windowed
// geometry, but the request is not reliably honoured: on GNOME/Xwayland the
// position and size are applied to the window frame rather than to the content
// area that glfwGetWindowPos()/glfwGetWindowSize() reported when the geometry
// was saved. The window therefore came back one title bar lower and one title
// bar shorter, and because the shrunken size was saved on the next toggle the
// loss accumulated on every F11.
//
// glfwSetWindowPos()/glfwSetWindowSize() are symmetric with the getters the
// geometry was saved from, so re-applying through them lands the window
// exactly. They cannot be called from toggle_fullscreen() directly, because at
// that point the window manager has not finished leaving fullscreen and simply
// overrides them; the correction has to happen on a later frame, which is why
// this is driven from the render loop.
//
// The watch also cannot stop at the first frame where the geometry matches. The
// window manager's own reply to glfwSetWindowMonitor() can land after the
// correction has already been applied and move the window a second time, so the
// window is watched until the deadline set when fullscreen was left.
void
settle_windowed_geometry(GLFWwindow* window, FullscreenState& state)
{
    if(!window || state.restore_attempts <= 0)
    {
        return;
    }

    if(state.is_fullscreen || glfwGetTime() > state.restore_deadline)
    {
        state.restore_attempts = 0;
        return;
    }

    int xpos   = 0;
    int ypos   = 0;
    int width  = 0;
    int height = 0;
    glfwGetWindowPos(window, &xpos, &ypos);
    glfwGetWindowSize(window, &width, &height);

    if(xpos == state.windowed_xpos && ypos == state.windowed_ypos &&
       width == state.windowed_width && height == state.windowed_height)
    {
        return;
    }

    spdlog::debug("Correcting windowed geometry: ({},{}) {}x{} -> ({},{}) {}x{}", xpos,
                  ypos, width, height, state.windowed_xpos, state.windowed_ypos,
                  state.windowed_width, state.windowed_height);

    glfwSetWindowPos(window, state.windowed_xpos, state.windowed_ypos);
    glfwSetWindowSize(window, state.windowed_width, state.windowed_height);
    state.restore_attempts--;
}

void
sync_fullscreen_state(GLFWwindow* window, int width, int height, FullscreenState& state)
{
    if(!window)
    {
        return;
    }

    // This runs from the GLFW window-size callback and must only reconcile
    // state, never drive the window. An earlier version called
    // glfwSetWindowMonitor() here to force the window back to windowed mode,
    // which re-entered GLFW from inside its own event handler and fought the
    // fullscreen transition that was still in progress.
    bool is_actually_fullscreen = is_fullscreen_active(window);

    if(state.is_fullscreen == is_actually_fullscreen)
    {
        return;
    }

    spdlog::debug("Detected OS-initiated fullscreen change: {} -> {}",
                  state.is_fullscreen ? "fullscreen" : "windowed",
                  is_actually_fullscreen ? "fullscreen" : "windowed");

    state.is_fullscreen = is_actually_fullscreen;

    // Remember the geometry so the next exit from fullscreen can restore it.
    if(!is_actually_fullscreen)
    {
        glfwGetWindowPos(window, &state.windowed_xpos, &state.windowed_ypos);
        state.windowed_width  = width;
        state.windowed_height = height;
    }

    rocprofvis_view_set_fullscreen_state(state.is_fullscreen);
}

bool
is_fullscreen_active(GLFWwindow* window)
{
    if(!window)
    {
        return false;
    }

    // glfwGetWindowMonitor() is non-null exactly while the window is fullscreen,
    // so it is the whole answer.
    //
    // Cross-checking the window geometry against the monitor's video mode was
    // tried here and removed. Under Xwayland with fractional scaling enabled
    // (mutter's scale-monitor-framebuffer) the reported window size and the
    // XRandR video mode legitimately disagree, so the comparison declared a
    // genuinely fullscreen window "windowed". sync_fullscreen_state() then
    // forced it back to windowed mode on the first resize event after every
    // F11, which is what made fullscreen appear to toggle at random.
    return glfwGetWindowMonitor(window) != nullptr;
}

}  // namespace View
}  // namespace RocProfVis
