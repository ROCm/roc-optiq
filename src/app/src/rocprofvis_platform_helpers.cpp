#ifdef __linux__

#    include "imgui.h"
#    include <cstdlib>
#    include <cstring>
#    include <unordered_map>

// Raw X11 access for repairing pointer-window association after a
// floating-window drag under Xwayland.  GLFW's glfwFocusWindow() routes
// through _NET_ACTIVE_WINDOW, which Mutter (GNOME) silently drops under
// focus-stealing-prevention when the request doesn't match its idea of
// "recent user action" -- so we bypass GLFW and call XSync() / XMap-
// related primitives directly.
#    define GLFW_EXPOSE_NATIVE_X11
#    include <GLFW/glfw3.h>
#    include <GLFW/glfw3native.h>
#    include <X11/Xatom.h>
#    include <X11/Xlib.h>

// All workarounds in this file address bugs that only manifest when an
// X11 client (us) runs against an Xwayland server hosted by a Wayland
// compositor -- specifically GNOME's Mutter, which (a) silently clamps
// XMoveWindow requests near screen edges without sending a corrective
// ConfigureNotify, and (b) does not refresh pointer-window association
// during programmatic XMoveWindow bursts.  Native X11 sessions do not
// exhibit either bug (X server handles both correctly), so on RHEL/
// Fedora/X11-on-Ubuntu/etc. the helpers below are a no-op.
//
// Detected via the standard freedesktop session env vars; result is
// cached because env vars do not change for the lifetime of a process.
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
    if(!is_wayland_session())
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
// association from scratch.  Causes a sub-frame flicker but reliably
// repairs the routing.
//
// Native X11 sessions (no compositor / X11-only WMs on RHEL, Fedora,
// etc.) do not exhibit this bug, so we early-return on those.
void
raise_dragged_viewport_after_release()
{
    if(!is_wayland_session())
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
            // Final XSync so the unmap/map/move round-trip completes
            // before the next pointer/click event is dispatched.
            Display* display = glfwGetX11Display();
            if(display != nullptr)
            {
                XSync(display, False);
            }
        }
        g_dragged_viewport_id = 0;
    }
    g_prev_mouse_left_down = curr_mouse_down;
}
#endif
