// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_glfw_util.h"
#include "rocprofvis_imgui_backend.h"
#include "rocprofvis_view_module.h"

namespace RocProfVis
{
namespace App
{

class CLIParser;

// Shell plumbing shared by the application (main.cpp) and the UI test harness
// (test/test_main.cpp), which drive the same window with different frame loops.

constexpr const char* APP_NAME = "ROCm(TM) Optiq";

// Lazy rendering: after each OS event render a few frames so animations and the
// deferred event dispatch settle, then sleep until the next event when idle.
constexpr int    RENDER_FRAMES_AFTER_INPUT = 4;
constexpr double IDLE_WAIT_TIMEOUT_SECONDS = 1.0;

// Writes the application log file, at debug level in debug builds.
void
enable_application_log();

// Sets the app description and registers --version [hash], --file, --backend,
// --file-dialog, and --help. Returns false if an option could not be registered.
bool
add_common_cli_options(CLIParser& cli_parser);

// Prints --help or --version after Parse(). Sets exit_app for --help, and for
// --version only when it is the sole option; its 'hash' value does not count as
// a second option.
void
handle_help_and_version(const CLIParser& cli_parser, bool& exit_app);

// Read --backend / --file-dialog. Return false, after logging, for an unknown value.
bool
parse_backend_preference(const CLIParser&                       cli_parser,
                         rocprofvis_imgui_backend_preference_t& backend_pref);

bool
parse_file_dialog_preference(const CLIParser&                          cli_parser,
                             rocprofvis_view_file_dialog_preference_t& file_dialog_pref);

void
glfw_error_callback(int error, const char* description);

// Installs the drop, close and resize callbacks. The backend can recreate the
// window during init, so call this once init has completed.
void
install_window_callbacks(GLFWwindow* window);

// Handles the notifications the view sends through rocprofvis_view_init.
void
app_notification_callback(GLFWwindow* window, int notification);

// Opens the files dropped on the window since the previous call, if any.
void
open_dropped_files();

// Options for this frame's rocprofvis_view_render; taking them resets them.
rocprofvis_view_render_options_t
take_render_options();

FullscreenState&
app_fullscreen_state();

#ifndef __APPLE__
// Fullscreen toggles requested by F11 or the View menu. The frame loop applies
// them between frames so the window is never resized part-way through one.
void
request_fullscreen_toggle();

bool
take_fullscreen_toggle_request();
#endif

#ifdef __APPLE__
// Reconcile ImGui's modifier state with the live OS modifier state.
//
// macOS system gestures (e.g. Mission Control via Ctrl+Up while dragging the
// window to a new Space) can consume the modifier key-up before GLFW sees it,
// leaving GLFW's cached key state stuck "down". Because ImGui enables
// ConfigMacOSXBehaviors on macOS, a stuck Control key makes ImGui translate
// every left-click into a right-click, so buttons and menus stop responding.
// Feeding the true OS state back into ImGui clears the phantom modifier.
void
sync_imgui_modifiers_with_os();

// Replaces the ImGui GLFW backend's mouse-button callback on macOS so the
// modifier state is corrected from the OS *before* the click is queued. This
// guarantees a phantom-stuck Control key cannot turn a left-click into a
// right-click for the very click that exposes the problem.
void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
#endif

}  // namespace App
}  // namespace RocProfVis
