#ifdef __linux__

#    include "imgui.h"
#    include <cstdlib>
#    include <cstring>
#    include <fstream>
#    include <string>
#    include <unordered_map>

#    include <GLFW/glfw3.h>

#    include "rocprofvis_platform_helpers.h"

// All workarounds in this file address bugs that only manifest under a
// specific subset of Linux configurations, where an X11 client (us)
// runs through Xwayland against a Mutter compositor that exhibits two
// regressions: (a) silently clamping XMoveWindow requests near screen
// edges without sending a corrective ConfigureNotify, and (b) failing
// to refresh pointer-window association during programmatic
// XMoveWindow bursts.  Empirically:
//
//   * Ubuntu 24 (Wayland)        : both bugs present -> workaround needed
//   * RHEL 9 (Wayland)           : neither bug present -> workaround off
//   * RHEL 10 (Wayland)          : neither bug present -> workaround off
//   * Native X11 sessions        : bugs by definition absent
//
// So the gate is finer than "is Wayland session" -- both RHEL versions
// run Wayland but neither trips the bug.  Bug presumably lives in an
// Ubuntu-specific Mutter patch; mainline Mutter (used by RHEL) seems
// unaffected.  We default to enabling the workaround only on Ubuntu's
// Wayland session, with explicit user override available via either
// the ROCPROFVIS_DRAG_REPAIR env var or the --drag-repair CLI flag.

// Returns true when running under a Wayland session (X11 client over
// Xwayland) detected via the standard freedesktop session env vars.
// Cached because env vars do not change for the lifetime of a process.
static bool
is_wayland_session()
{
    static const bool cached = []() {
        const char* wd = std::getenv("WAYLAND_DISPLAY");
        if(wd != nullptr && wd[0] != '\0')
        {
            return true;
        }
        const char* st = std::getenv("XDG_SESSION_TYPE");
        return st != nullptr && std::strcmp(st, "wayland") == 0;
    }();
    return cached;
}

// Returns true when /etc/os-release identifies the host distro as
// Ubuntu (ID=ubuntu).  /etc/os-release values may be quoted per
// freedesktop.org's spec, so strip surrounding single/double quotes.
// Cached because /etc/os-release does not change at runtime.
static bool
is_ubuntu_distro()
{
    static const bool cached = []() {
        std::ifstream f("/etc/os-release");
        if(!f.is_open())
        {
            return false;
        }
        std::string line;
        while(std::getline(f, line))
        {
            if(line.rfind("ID=", 0) != 0)
            {
                continue;
            }
            std::string v = line.substr(3);
            if(v.size() >= 2 && (v.front() == '"' || v.front() == '\'') &&
               v.front() == v.back())
            {
                v = v.substr(1, v.size() - 2);
            }
            return v == "ubuntu";
        }
        return false;
    }();
    return cached;
}

// Parse a string in the truthiness convention used by both the env
// var and the --drag-repair CLI flag.  Returns 1 for explicitly-on
// values, 0 for explicitly-off values, -1 for unrecognised / "auto".
static int
parse_drag_repair_value(const char* s)
{
    if(s == nullptr || s[0] == '\0')
    {
        return -1;
    }
    if(std::strcmp(s, "0") == 0 || std::strcmp(s, "false") == 0 ||
       std::strcmp(s, "off") == 0 || std::strcmp(s, "no") == 0)
    {
        return 0;
    }
    if(std::strcmp(s, "1") == 0 || std::strcmp(s, "true") == 0 ||
       std::strcmp(s, "on") == 0 || std::strcmp(s, "yes") == 0)
    {
        return 1;
    }
    // "auto" (or anything unrecognised) leaves the lower-priority
    // tiers in charge.
    return -1;
}

// CLI-supplied override.  -1 = unset (defer to env / auto-detect);
// 0 = forced off; 1 = forced on.  Mutated only from the
// set/clear public API at startup, then read freely thereafter.
static int g_cli_drag_repair_override = -1;

void
set_drag_repair_override(bool on)
{
    g_cli_drag_repair_override = on ? 1 : 0;
}

void
clear_drag_repair_override()
{
    g_cli_drag_repair_override = -1;
}

// Layered policy: CLI override wins, otherwise env var, otherwise
// auto-detect (Ubuntu + Wayland).  The env-var and auto-detect tiers
// are individually cached; the CLI tier is just an int read.  All
// callers (snap_secondary_viewports_to_os_pos,
// raise_dragged_viewport_after_release) hit this once per frame, so
// the cost is negligible.
static bool
should_apply_drag_repair()
{
    if(g_cli_drag_repair_override == 1) return true;
    if(g_cli_drag_repair_override == 0) return false;

    static const int env_value = []() {
        return parse_drag_repair_value(std::getenv("ROCPROFVIS_DRAG_REPAIR"));
    }();
    if(env_value == 1) return true;
    if(env_value == 0) return false;

    return is_wayland_session() && is_ubuntu_distro();
}

// File-static workaround state.  Owned here (not by the caller) because it
// is purely a private implementation detail of the X11 stacking-order
// repair: see raise_dragged_viewport_after_release() for the rationale.
//
// g_dragged_viewport_id holds the ID of the most recently snapped secondary
// viewport observed while the left mouse button was down -- in practice the
// floating window the user is dragging.  g_prev_mouse_left_down lets us
// detect the release transition (down -> up) without leaning on any
// internal ImGui drag state.
static ImGuiID g_dragged_viewport_id  = 0;
static bool    g_prev_mouse_left_down = false;

// Snap every secondary viewport's Pos to its actual OS window position,
// so hit-testing and rendering agree with what the user sees on screen.
//
// Motivation:
//   When a floating (secondary-viewport) window is dragged toward a
//   screen edge, ImGui::UpdateMouseMovingWindowNewFrame() every frame
//   writes viewport->Pos = mouse - click_offset -- even when that pos is
//   off-screen.  The X11/Xwayland window manager (Mutter on GNOME in our
//   case) refuses to move the OS window past certain bounds and clamps
//   the actual window placement, but it does NOT send a corrective
//   ConfigureNotify to disagree with our request.  The result is a
//   permanent mismatch: ImGui thinks the window is at the off-screen
//   "intended" pos while the OS window sits at the WM-clamped pos, and
//   because ImGui hit-tests against window->Pos (synced from
//   viewport->Pos) but mouse input is in absolute screen coordinates
//   derived from the actual OS window position, hovering anywhere in
//   the OS window is reported as hovering somewhere else -- producing
//   the ~50 px offset / "corner lock" the user sees.
//
//   Fix, in two hooks per frame:
//
//     Hook A (post-NewFrame, here): save the intended Pos, overwrite
//       viewport->Pos with the OS pos reported by
//       Platform_GetWindowPos() (which reads GLFW's cached post-
//       ConfigureNotify value), and raise PlatformRequestMove.  The
//       subsequent Begin() calls WindowSyncOwnedViewport(), which when
//       PlatformRequestMove is set does window->Pos = viewport->Pos and
//       marks the ini dirty -- so hit-testing, rendering, and the
//       persisted layout all agree with reality.
//
//     Hook B (post-Render, pre-UpdatePlatformWindows): restore
//       viewport->Pos to the saved intended value and clear
//       PlatformRequestMove so UpdatePlatformWindows() still issues the
//       drag-target SetWindowPos() to the OS (drag progresses normally
//       as long as the WM honors the move; when it refuses, the next
//       frame's Hook A reconciles ImGui to the OS again).
void
snap_secondary_viewports_to_os_pos(
    std::unordered_map<ImGuiID, ImVec2>& viewport_intended_pos)
{
    if(!should_apply_drag_repair())
    {
        return;
    }
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    if(pio.Platform_GetWindowPos == nullptr)
    {
        return;
    }
    viewport_intended_pos.clear();
    const bool mouse_left_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    for(int i = 1; i < pio.Viewports.Size; ++i)
    {
        ImGuiViewport* vp = pio.Viewports[i];
        if(!vp->PlatformWindowCreated || vp->PlatformHandle == nullptr)
        {
            continue;
        }
        const ImVec2 os_pos = pio.Platform_GetWindowPos(vp);
        if(vp->Pos.x == os_pos.x && vp->Pos.y == os_pos.y)
        {
            continue;
        }
        viewport_intended_pos[vp->ID] = vp->Pos;
        vp->Pos                       = os_pos;
        // Make the subsequent WindowSyncOwnedViewport() propagate our
        // corrected Pos into the owning ImGuiWindow (window->Pos =
        // viewport->Pos), which is what hit-testing actually uses.
        vp->PlatformRequestMove = true;
        // Snapping while the mouse is held strongly implies an active
        // drag of this viewport.  Remember it so the post-release hook
        // can raise its OS window back to the top of the X11 stack.
        if(mouse_left_down)
        {
            g_dragged_viewport_id = vp->ID;
        }
    }
}

// Restore the drag-target positions we saved in
// snap_secondary_viewports_to_os_pos(), so UpdatePlatformWindows() still
// transmits the requested move to the OS.  PlatformRequestMove is cleared
// because UpdatePlatformWindows() skips Platform_SetWindowPos() whenever
// that flag is set.
void
restore_secondary_viewport_intended_pos(
    std::unordered_map<ImGuiID, ImVec2>& viewport_intended_pos)
{
    // Cheap fast-path: native X11 never populates the map, so no
    // explicit is_wayland_session() check is required here -- this
    // early-return covers it.
    if(viewport_intended_pos.empty())
    {
        return;
    }
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    for(int i = 1; i < pio.Viewports.Size; ++i)
    {
        ImGuiViewport* vp = pio.Viewports[i];
        auto           it = viewport_intended_pos.find(vp->ID);
        if(it == viewport_intended_pos.end())
        {
            continue;
        }
        vp->Pos                 = it->second;
        vp->PlatformRequestMove = false;
    }
    viewport_intended_pos.clear();
}


// Repair X11 pointer routing after a floating-window drag.
//
// Symptom: dragging a secondary (external) viewport and then clicking
// on it again routes the click to whatever sits beneath it (the
// underlying window of our app or another app).  The give-away tell
// is that immediately after the drag the cursor glyph reflects the
// underlying window (e.g. a text caret from an editor below) and
// tooltips fire on the underlying window even though the cursor sits
// visually over ours.
//
// Cause: X11 only fires EnterNotify/LeaveNotify on **cursor** motion,
// not on **window** motion.  During the drag, ImGui issues
// glfwSetWindowPos every frame, so the floating window briefly slips
// out from under the stationary cursor at some point.  X11 emits a
// LeaveNotify for our window and EnterNotify for the underlying one;
// pointer focus latches onto the underlying.  When the drag ends the
// floating window is back on top, but the cursor has not moved, so
// X11 never re-evaluates containment -- pointer focus stays on the
// underlying and the next click is routed there.  This is independent
// of (visual) stacking, so even with _NET_WM_STATE_ABOVE applied to
// the floating window, pointer routing remains stale.
//
// Less invasive approaches we tried but found ineffective under
// Xwayland/Mutter:
//   * glfwFocusWindow() -- routes through _NET_ACTIVE_WINDOW which
//     Mutter drops under focus-stealing-prevention.
//   * XRaiseWindow() -- restacks correctly but stacking was already
//     fine; the bug is purely stale pointer-window association.
//   * XWarpPointer() -- silently dropped under Xwayland pointer-warp
//     restrictions when the destination surface isn't focused.
//   * XGrabPointer + XUngrabPointer -- grab is silently rejected /
//     does not refresh pointer-window association on ungrab.
//
// Working fix: unmap + remap the floating window (XUnmapWindow +
// XMapWindow, sequenced via glfwHide/ShowWindow).  Mutter treats the
// remap as the window appearing fresh and rebuilds pointer-window
// association from scratch.  Causes a sub-frame "magic-lamp" flicker
// (GNOME Shell's open + destroy effect) but reliably repairs the
// routing.
//
// Suppressing the GNOME Shell animation has been investigated and
// abandoned -- documenting here so we don't reattempt:
//
//   * Permanent retype to _NET_WM_WINDOW_TYPE_UTILITY/DOCK/POPUP_MENU
//     does suppress the animation (only NORMAL/DIALOG/MODAL_DIALOG are
//     animated by gnome-shell/js/ui/windowManager.js), but each type
//     trips a different unwanted Mutter code path: degraded raise on
//     alt-tab (UTILITY), screen-edge strut handling (DOCK), and focus-
//     loss auto-dismiss (POPUP_MENU).
//   * Transient retype (NORMAL -> UTILITY -> hide/show -> NORMAL)
//     does not work either: tested on Ubuntu 24 GNOME 46 and RHEL 10,
//     full magic-lamp animation runs in both cases.  Empirically
//     gnome-shell latches the "is animatable" decision onto the long-
//     lived Meta.Window object at first MapRequest and does not re-
//     evaluate it from _NET_WM_WINDOW_TYPE on subsequent maps.  So the
//     retype is too late, regardless of timing.
//   * Alternative pointer-routing repair operations (XRaiseWindow,
//     XWarpPointer, XGrabPointer/XUngrabPointer, glfwFocusWindow) all
//     fail under Xwayland for distinct reasons documented in the
//     "Less invasive approaches" list above.
//
// Net: hide/show is the only known-working repair, and the resulting
// flicker is an accepted trade-off versus leaving stale pointer-
// window association after every drag.
//
// Native X11 sessions and known-clean Wayland configurations (RHEL 9,
// RHEL 10) do not exhibit the bug, so should_apply_drag_repair()
// short-circuits the entire helper on those.
void
raise_dragged_viewport_after_release()
{
    if(!should_apply_drag_repair())
    {
        return;
    }
    const bool curr_mouse_down = ImGui::IsMouseDown(ImGuiMouseButton_Left);
    if(g_prev_mouse_left_down && !curr_mouse_down && g_dragged_viewport_id != 0)
    {
        ImGuiViewport* vp = ImGui::FindViewportByID(g_dragged_viewport_id);
        if(vp != nullptr && vp->PlatformWindowCreated &&
           vp->PlatformHandle != nullptr)
        {
            GLFWwindow* glfw_win = static_cast<GLFWwindow*>(vp->PlatformHandle);
            // Capture position in case Mutter re-places the window on
            // map; restore it explicitly to be safe.
            int pos_x = 0, pos_y = 0;
            glfwGetWindowPos(glfw_win, &pos_x, &pos_y);
            glfwHideWindow(glfw_win);
            glfwShowWindow(glfw_win);
            glfwSetWindowPos(glfw_win, pos_x, pos_y);
        }
        g_dragged_viewport_id = 0;
    }
    g_prev_mouse_left_down = curr_mouse_down;
}
#endif
