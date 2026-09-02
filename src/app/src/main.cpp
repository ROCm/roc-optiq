// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "glfw_util.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_imgui_backend.h"
#define GLFW_INCLUDE_NONE
#include "AMD_LOGO.h"
#include "rocprofvis_cli_parser.h"
#include "rocprofvis_version.h"
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_image_helpers.h"
#if defined(__APPLE__) || defined(__linux__)
#include "rocprofvis_platform_helpers.h"
#endif
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#    include <unordered_map>
#endif

const char* APP_NAME = "ROCm(TM) Optiq";

// globals shared with callbacks
static std::vector<std::string>         g_dropped_file_paths;
static bool                             g_file_was_dropped = false;
static rocprofvis_view_render_options_t g_render_options =
    rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_None;

#ifdef __linux__
// Per-frame snapshot of the "intended" (drag-target) position that
// UpdateMouseMovingWindowNewFrame() wrote into viewport->Pos before we
// snap it to the actual OS position.  Populated in the post-NewFrame
// hook, consumed in the pre-UpdatePlatformWindows hook so the requested
// move is still transmitted to the OS.  Key: viewport ID.
static std::unordered_map<ImGuiID, ImVec2> g_viewport_intended_pos;
#endif

// Fullscreen state (initialized after window creation)
static RocProfVis::View::FullscreenState g_fullscreen_state = {};

#ifndef __APPLE__
// Set by F11 or by the View's fullscreen menu item, and applied once at the end
// of the frame. Resizing the window part-way through a frame would leave the
// already-built draw data describing the previous size.
static bool g_toggle_fullscreen_requested = false;
#endif

// Lazy rendering: after each OS event render a few frames so animations and the
// deferred event dispatch settle, then sleep until the next event when idle.
static int       g_frames_to_render        = 1;
constexpr int    RENDER_FRAMES_AFTER_INPUT = 4;
constexpr double IDLE_WAIT_TIMEOUT_SECONDS = 1.0;

static void
drop_callback(GLFWwindow* window, int count, const char* paths[])
{
    (void) window;  // Unused parameter
    g_dropped_file_paths.clear();
    for(int i = 0; i < count; i++)
    {
        g_dropped_file_paths.push_back(paths[i]);
    }
    g_file_was_dropped = true;
}

static void
close_callback(GLFWwindow* window)
{
    g_render_options =
        rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_RequestExit;
    glfwSetWindowShouldClose(window, GLFW_FALSE);
}

static void
app_notification_callback(GLFWwindow* window, int notification)
{
    if(notification ==
       static_cast<int>(
           rocprofvis_view_notification_t::kRocProfVisViewNotification_Exit_App))
    {
        glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
#ifndef __APPLE__
    else if(notification ==
            static_cast<int>(rocprofvis_view_notification_t::
                                 kRocProfVisViewNotification_Toggle_Fullscreen))
    {
        g_toggle_fullscreen_requested = true;
    }
#endif
}

static void
window_size_change_callback(GLFWwindow* window, int width, int height)
{
    RocProfVis::View::sync_fullscreen_state(window, width, height, g_fullscreen_state);
}

static void
glfw_error_callback(int error, const char* description)
{
    spdlog::error("GLFW Error {}: {}", error, description);
}

#ifdef __APPLE__
// Reconcile ImGui's modifier state with the live OS modifier state.
//
// macOS system gestures (e.g. Mission Control via Ctrl+Up while dragging the
// window to a new Space) can consume the modifier key-up before GLFW sees it,
// leaving GLFW's cached key state stuck "down". Because ImGui enables
// ConfigMacOSXBehaviors on macOS, a stuck Control key makes ImGui translate
// every left-click into a right-click, so buttons and menus stop responding.
// Feeding the true OS state back into ImGui clears the phantom modifier.
static void
sync_imgui_modifiers_with_os()
{
    ImGuiIO&                            io = ImGui::GetIO();
    RocProfVis::Platform::ModifierState m  = RocProfVis::Platform::get_os_modifier_state();

    io.AddKeyEvent(ImGuiMod_Ctrl, m.ctrl);
    io.AddKeyEvent(ImGuiMod_Shift, m.shift);
    io.AddKeyEvent(ImGuiMod_Alt, m.alt);
    io.AddKeyEvent(ImGuiMod_Super, m.super);

    if(!m.ctrl)
    {
        io.AddKeyEvent(ImGuiKey_LeftCtrl, false);
        io.AddKeyEvent(ImGuiKey_RightCtrl, false);
    }
    if(!m.shift)
    {
        io.AddKeyEvent(ImGuiKey_LeftShift, false);
        io.AddKeyEvent(ImGuiKey_RightShift, false);
    }
    if(!m.alt)
    {
        io.AddKeyEvent(ImGuiKey_LeftAlt, false);
        io.AddKeyEvent(ImGuiKey_RightAlt, false);
    }
    if(!m.super)
    {
        io.AddKeyEvent(ImGuiKey_LeftSuper, false);
        io.AddKeyEvent(ImGuiKey_RightSuper, false);
    }
}

// Replaces the ImGui GLFW backend's mouse-button callback on macOS so the
// modifier state is corrected from the OS *before* the click is queued. This
// guarantees a phantom-stuck Control key cannot turn a left-click into a
// right-click for the very click that exposes the problem.
static void
mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    (void) window;
    (void) mods;

    sync_imgui_modifiers_with_os();

    ImGuiIO& io = ImGui::GetIO();
    if(button >= 0 && button < ImGuiMouseButton_COUNT)
    {
        io.AddMouseButtonEvent(button, action == GLFW_PRESS);
    }
}
#endif

#ifdef __linux__
// Resolve the post-drag click-through workaround from the stored preference,
// updating it first when --drag-repair was passed. Must run after the view is
// initialized, because that is what loads the settings file.
static void
configure_drag_repair(RocProfVis::View::CLIParser& cli_parser)
{
    if(cli_parser.WasOptionFound("drag-repair"))
    {
        const std::string value = cli_parser.GetOptionValue("drag-repair");
        if(value == "on" || value == "1" || value == "true" || value == "yes")
        {
            rocprofvis_view_set_drag_repair_enabled(true);
        }
        else if(value == "off" || value == "0" || value == "false" || value == "no")
        {
            rocprofvis_view_set_drag_repair_enabled(false);
        }
        else
        {
            spdlog::warn("Ignoring unrecognized --drag-repair value '{}'", value);
        }
    }

    RocProfVis::Platform::set_drag_repair_enabled(
        rocprofvis_view_get_drag_repair_enabled());
}
#endif

static void
print_version()
{
    std::cout << APP_NAME << " version: " << ROCPROFVIS_VERSION_MAJOR << "."
              << ROCPROFVIS_VERSION_MINOR << "." << ROCPROFVIS_VERSION_PATCH << "."
              << ROCPROFVIS_VERSION_BUILD << std::endl;
}

static void
parse_command_line_args(int argc, char** argv, RocProfVis::View::CLIParser& cli_parser,
                        bool& exit_app)
{
    cli_parser.SetAppDescription(APP_NAME, "A visualizer for profiling ROCm Data");
    bool result = true;
    result &= cli_parser.AddOption("v", "version", "Print version and exit", false);
    result &= cli_parser.AddOption("f", "file", "Open a trace or project file", true);
    result &= cli_parser.AddOption(
        "b", "backend",
        "Set rendering backend: 'auto' (default), 'vulkan', or 'opengl'", true);
    result &= cli_parser.AddOption(
        "d", "file-dialog",
        "Set file dialog backend: 'auto' (default), 'native' (system file "
        "dialog), or 'imgui' (built-in). Use 'imgui' when running over SSH",
        true);
#ifdef __linux__
    result &= cli_parser.AddOption(
        "r", "drag-repair",
        "Linux post-drag click-through fix for floating windows "
        "(Ubuntu Wayland bug; trade-off: brief flicker per drag-release): "
        "'on'|'off'. Saved to the application settings, so it only needs to be "
        "passed when changing it (default: off)",
        true);
#endif
    result &= cli_parser.AddOption("h", "help",
        "Show this help message and exit", false);
    ROCPROFVIS_ASSERT(result);

    cli_parser.Parse(argc, argv);

    if(cli_parser.WasOptionFound("help"))
    {
        std::cout << cli_parser.GetHelp() << std::endl;
        exit_app = true;
    }

    if(!exit_app && cli_parser.WasOptionFound("version"))
    {
        print_version();

        if(cli_parser.GetOptionCount() == 1)
        {
            exit_app = true;
        }
    }

    if(exit_app)
    {
        std::cout.flush();
        std::cerr.flush();
        fflush(stdout);
        fflush(stderr);
    }
 
}

int
main(int argc, char** argv)
{
    int app_result_code = 0;

    // Enable logging before parsing arguments so diagnostics emitted while
    // handling CLI options reach the log file.
    std::string log_dir = rocprofvis_get_application_log_path();
#ifndef NDEBUG
    std::filesystem::path log_path =
        std::filesystem::path(log_dir) / "roc-optiq.debug.log";
    rocprofvis_core_enable_log(log_path.string().c_str(), spdlog::level::debug);
#else
    std::filesystem::path log_path =
        std::filesystem::path(log_dir) / "roc-optiq.log";
    rocprofvis_core_enable_log(log_path.string().c_str(), spdlog::level::info);
#endif

    RocProfVis::View::CLIParser::AttachToConsole();
    RocProfVis::View::CLIParser cli_parser;
    bool                        exit_app = false;
    parse_command_line_args(argc, argv, cli_parser, exit_app);
    if(exit_app)
    {
        return app_result_code;
    }

    // Parse backend preference from command line
    rocprofvis_imgui_backend_preference_t backend_pref = kRPVBackendAuto;
    if(cli_parser.WasOptionFound("backend"))
    {
        std::string backend_str = cli_parser.GetOptionValue("backend");
        if(backend_str == "auto")
        {
            backend_pref = kRPVBackendAuto;
        }
        else if(backend_str == "vulkan")
        {
            backend_pref = kRPVBackendForceVulkan;
        }
        else if(backend_str == "opengl")
        {
            backend_pref = kRPVBackendForceOpenGL;
        }
        else
        {
            spdlog::error("Invalid backend '{}'. Valid options: auto, vulkan, opengl", backend_str);
            return 1;
        }
    }

    rocprofvis_view_file_dialog_preference_t fd_pref = kRocProfVisViewFileDialog_Auto;
    if(cli_parser.WasOptionFound("file-dialog"))
    {
        std::string fd_str = cli_parser.GetOptionValue("file-dialog");
        if(fd_str == "auto")
        {
            fd_pref = kRocProfVisViewFileDialog_Auto;
        }
        else if(fd_str == "native")
        {
            fd_pref = kRocProfVisViewFileDialog_Native;
        }
        else if(fd_str == "imgui")
        {
            fd_pref = kRocProfVisViewFileDialog_ImGui;
        }
        else
        {
            spdlog::error("Invalid --file-dialog '{}'. Valid options: auto, "
                          "native, imgui",
                          fd_str);
            return 1;
        }
    }

#ifdef __APPLE__
    RocProfVis::Platform::configure_bundled_vulkan_icd();
#endif

    glfwSetErrorCallback(glfw_error_callback);
#if defined(__linux__) && defined(ROCPROFVIS_MULTI_WINDOW)
    // Force X11 on Linux for multi-viewport and window positioning support
    // Wayland does not support window positioning which is required for ImGui viewports
    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif    
    if(glfwInit())
    {
        // Create initial window with Vulkan hint (GLFW_NO_API) by default
        // The backend setup will recreate the window if OpenGL is needed
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#if defined(GLFW_SCALE_TO_MONITOR)  // GLFW 3.3+
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif
        GLFWwindow* window = glfwCreateWindow(RocProfVis::View::DEFAULT_WINDOWED_WIDTH,
                                              RocProfVis::View::DEFAULT_WINDOWED_HEIGHT,
                                              APP_NAME, nullptr, nullptr);
        rocprofvis_imgui_backend_t backend;

        if(window && rocprofvis_imgui_backend_setup_with_fallback(&backend, &window,
                                                                  RocProfVis::View::DEFAULT_WINDOWED_WIDTH,
                                                                  RocProfVis::View::DEFAULT_WINDOWED_HEIGHT,
                                                                  APP_NAME,
                                                                  backend_pref))
        {
            RocProfVis::View::CLIParser::DetachFromConsole();

            if(rocprofvis_imgui_backend_complete_init_with_opengl_fallback(
                   &backend, &window, RocProfVis::View::DEFAULT_WINDOWED_WIDTH,
                   RocProfVis::View::DEFAULT_WINDOWED_HEIGHT, APP_NAME, backend_pref))
            {
                // After init: window may be recreated (e.g. Vulkan -> OpenGL fallback)
                glfwSetDropCallback(window, drop_callback);
                glfwSetWindowCloseCallback(window, close_callback);
                glfwSetWindowSizeCallback(window, window_size_change_callback);

#ifdef ROCPROFVIS_MULTI_WINDOW
                // A fullscreen GLFW window iconifies itself whenever it loses
                // input focus while GLFW_AUTO_ICONIFY is set, which is the
                // default. Multi-viewport puts panels that have been dragged out
                // into sibling OS windows, so clicking one of them takes focus
                // away from the main window and would minimize the entire app,
                // leaving only the floating panel on screen.
                glfwSetWindowAttrib(window, GLFW_AUTO_ICONIFY, GLFW_FALSE);
#endif

                RocProfVis::View::init_fullscreen_state(window, g_fullscreen_state);
                glfwShowWindow(window);

                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
#ifdef ROCPROFVIS_MULTI_WINDOW
                // Multi-viewport lets panels be dragged out into their own OS
                // window. Docking is intentionally left disabled.
                io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
#endif
                // ConfigDpiScaleViewports is deliberately left off. It rescales a
                // window every time its viewport reports a new DPI, which upstream
                // documents as a resizing feedback loop for a window straddling a
                // DPI boundary, and the runaway size then persists to imgui.ini.
                // Fonts still scale per monitor through ConfigDpiScaleFonts.
                io.ConfigDpiScaleFonts               = true;
                io.ConfigWindowsMoveFromTitleBarOnly = true;

                ImGui::StyleColorsLight();

                rocprofvis_view_init([window](int notification) -> void {
                    app_notification_callback(window, notification);
                }, fd_pref);

#ifdef __linux__
                configure_drag_repair(cli_parser);
#endif

                backend.m_config(&backend, window);
#ifdef __APPLE__
                // Install after m_config so this overrides the ImGui GLFW
                // backend's own mouse-button callback (set during m_config).
                glfwSetMouseButtonCallback(window, mouse_button_callback);
#endif
                rocprofvis_view_set_texture_backend(
                    rocprofvis_imgui_backend_create_gui_texture_rgba32,
                    rocprofvis_imgui_backend_destroy_gui_texture, &backend);

                if(cli_parser.WasOptionFound("file") &&
                   !cli_parser.GetOptionValue("file").empty())
                {
                    // If the user inputted a filepath open it here.
                    rocprofvis_view_open_files({ cli_parser.GetOptionValue("file") });
                }

                ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

                RocProfVis::View::EmbeddedImage icon(AMD_LOGO_png,
                                                     static_cast<int>(AMD_LOGO_png_len));
                if(icon.Valid())
                {
                    GLFWimage glfw_icon = { icon.GetWidth(), icon.GetHeight(),
                                            icon.GetPixels() };
                    glfwSetWindowIcon(window, 1, &glfw_icon);
                }
                // EmbeddedImage already logs a decode failure, so no warning here.

                while(!glfwWindowShouldClose(window))
                {
                    // handle dropped file signal flag from callback
                    if(g_file_was_dropped)
                    {
                        rocprofvis_view_open_files(g_dropped_file_paths);
                        g_file_was_dropped = false;
                    }

                    // Async work/animation in flight: refill the budget so the
                    // settle tail also covers the final frames after it finishes.
                    if(rocprofvis_view_wants_continuous_render())
                    {
                        g_frames_to_render = RENDER_FRAMES_AFTER_INPUT;
                    }

                    if(g_frames_to_render > 0)
                    {
                        // Busy: poll so per-frame controller/event work keeps
                        // running. vsync in present() caps the frame rate.
                        glfwPollEvents();
                    }
                    else
                    {
                        // Idle: sleep until an OS event or a short timeout, then
                        // render a few frames. The timeout lets pending
                        // multi-frame layout settle without user input.
                        glfwWaitEventsTimeout(IDLE_WAIT_TIMEOUT_SECONDS);
                        g_frames_to_render = RENDER_FRAMES_AFTER_INPUT;
                    }

                    // Correct the windowed geometry if the window manager did
                    // not honour the one requested when fullscreen was left.
                    RocProfVis::View::settle_windowed_geometry(window,
                                                               g_fullscreen_state);

#ifdef __APPLE__
                    // Clear any phantom-stuck modifier (e.g. Control left down
                    // after a Mission Control gesture) before the frame renders.
                    sync_imgui_modifiers_with_os();
#endif

                    // Handle changes in the frame buffer size
                    int fb_width, fb_height;
                    glfwGetFramebufferSize(window, &fb_width, &fb_height);
                    backend.m_update_framebuffer(&backend, fb_width, fb_height);

                    // Panels dragged into their own OS window remain on screen
                    // while the main window is minimized, so the frame can only
                    // be skipped outright when there is nothing else to draw.
                    const bool main_window_iconified =
                        glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0;
                    if(main_window_iconified &&
                       ImGui::GetPlatformIO().Viewports.Size <= 1)
                    {
                        ImGui_ImplGlfw_Sleep(10);
                        continue;
                    }

                    backend.m_new_frame(&backend);
                    ImGui::NewFrame();

#ifdef __linux__
                    // Hook A: snap secondary viewport Pos to the actual
                    // OS window position before user code runs, so
                    // hit-testing and rendering agree with reality when
                    // the window manager clamps our requested drag pos.
                    // Call raise_dragged_viewport_after_release(), this is a fix
                    // for the post-drag click-fall-through bug under
                    // Xwayland/Mutter.
                    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                    {
                        RocProfVis::Platform::snap_secondary_viewports_to_os_pos(
                            g_viewport_intended_pos);
                        RocProfVis::Platform::raise_dragged_viewport_after_release();
                    }
#endif

#ifndef __APPLE__
                    // Read F11 from ImGui's key state instead of a GLFW key
                    // callback: the ImGui GLFW backend only chains user callbacks
                    // for the main window, so a callback is never reached while a
                    // panel in its own OS window holds focus.
                    if(ImGui::IsKeyPressed(ImGuiKey_F11, false))
                    {
                        g_toggle_fullscreen_requested = true;
                    }
#endif

                    rocprofvis_view_render(g_render_options);
                    g_render_options = rocprofvis_view_render_options_t::
                        kRocProfVisViewRenderOption_None;

                    ImGui::Render();
                    ImDrawData* draw_data    = ImGui::GetDrawData();
                    const bool  is_minimized = (draw_data->DisplaySize.x <= 0.0f ||
                                               draw_data->DisplaySize.y <= 0.0f ||
                                               main_window_iconified);
                    if(!is_minimized)
                    {
                        backend.m_render(&backend, draw_data, &clear_color);
                    }

                    // Render windows that have been dragged out of the main viewport.
                    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                    {
                        GLFWwindow* backup_current_context = glfwGetCurrentContext();
#ifdef __linux__
                        // Hook B: restore the drag-target Pos we
                        // temporarily replaced with the OS pos in Hook A
                        // so UpdatePlatformWindows() still transmits the
                        // requested move.
                        RocProfVis::Platform::restore_secondary_viewport_intended_pos(
                            g_viewport_intended_pos);
#endif
                        ImGui::UpdatePlatformWindows();
                        ImGui::RenderPlatformWindowsDefault();
                        glfwMakeContextCurrent(backup_current_context);
                    }

                    if(!is_minimized)
                    {
                        backend.m_present(&backend);
                    }

#ifndef __APPLE__
                    // Applied here so the window is never resized part-way
                    // through a frame, whether the request came from F11 or from
                    // the View's fullscreen menu item.
                    if(g_toggle_fullscreen_requested)
                    {
                        g_toggle_fullscreen_requested = false;
                        RocProfVis::View::toggle_fullscreen(window, g_fullscreen_state);
                        g_frames_to_render = RENDER_FRAMES_AFTER_INPUT;
                    }
#endif

                    if(g_frames_to_render > 0)
                    {
                        --g_frames_to_render;
                    }
                }

                rocprofvis_view_destroy();
                rocprofvis_view_set_texture_backend(nullptr, nullptr, nullptr);
                backend.m_shutdown(&backend);

                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();

                backend.m_destroy(&backend);
            }
            else
            {
                spdlog::error(
                    "GLFW: Failed to initialize graphics device (Vulkan and/or OpenGL)");
                app_result_code = 1;
            }

            if(window)
            {
                glfwDestroyWindow(window);
            }
        }
        else
        {
            spdlog::error("GLFW: Failed to initialize window & graphics API backend");
            app_result_code = 1;
        }

        glfwTerminate();
    }
    else
    {
        spdlog::error("GLFW: Failed to initialize GLFW library");
        app_result_code = 1;
    }

    return app_result_code;
}
