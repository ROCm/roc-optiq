// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "glfw_util.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "rocprofvis_app_shell.h"
#include "rocprofvis_core.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_imgui_backend.h"
#define GLFW_INCLUDE_NONE
#include "AMD_LOGO.h"
#include "rocprofvis_cli_parser.h"
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_image_helpers.h"
#if defined(__APPLE__) || defined(__linux__)
#include "rocprofvis_platform_helpers.h"
#endif
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>
#if defined(__linux__) && defined(ROCPROFVIS_MULTI_WINDOW)
#    include <unordered_map>
#endif

#if defined(__linux__) && defined(ROCPROFVIS_MULTI_WINDOW)
// Per-frame snapshot of the "intended" (drag-target) position that
// UpdateMouseMovingWindowNewFrame() wrote into viewport->Pos before we
// snap it to the actual OS position.  Populated in the post-NewFrame
// hook, consumed in the pre-UpdatePlatformWindows hook so the requested
// move is still transmitted to the OS.  Key: viewport ID.
static std::unordered_map<ImGuiID, ImVec2> g_viewport_intended_pos;
#endif

// Frames left to render before the loop may sleep; see RENDER_FRAMES_AFTER_INPUT.
static int g_frames_to_render = 1;

#if defined(__linux__) && defined(ROCPROFVIS_MULTI_WINDOW)
// Resolve the post-drag click-through workaround from the stored preference,
// updating it first when --drag-repair was passed. Must run after the view is
// initialized, because that is what loads the settings file.
static void
configure_drag_repair(RocProfVis::App::CLIParser& cli_parser)
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
parse_command_line_args(int argc, char** argv, RocProfVis::App::CLIParser& cli_parser,
                        bool& exit_app)
{
    bool result = RocProfVis::App::add_common_cli_options(cli_parser);
// The workaround only has anything to repair when panels can be dragged into
// their own OS window, so the option is absent from a build without it.
#if defined(__linux__) && defined(ROCPROFVIS_MULTI_WINDOW)
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
    RocProfVis::App::handle_help_and_version(cli_parser, exit_app);
}

int
main(int argc, char** argv)
{
    int app_result_code = 0;

    // Enable logging before parsing arguments so diagnostics emitted while
    // handling CLI options reach the log file.
    RocProfVis::App::enable_application_log();

    RocProfVis::App::CLIParser::AttachToConsole();
    RocProfVis::App::CLIParser cli_parser;
    bool                       exit_app = false;
    parse_command_line_args(argc, argv, cli_parser, exit_app);
    if(exit_app)
    {
        return app_result_code;
    }

    rocprofvis_imgui_backend_preference_t    backend_pref = kRPVBackendAuto;
    rocprofvis_view_file_dialog_preference_t fd_pref      = kRocProfVisViewFileDialog_Auto;
    if(!RocProfVis::App::parse_backend_preference(cli_parser, backend_pref) ||
       !RocProfVis::App::parse_file_dialog_preference(cli_parser, fd_pref))
    {
        return 1;
    }

#ifdef __APPLE__
    RocProfVis::Platform::configure_bundled_vulkan_icd();
#endif

    glfwSetErrorCallback(RocProfVis::App::glfw_error_callback);
#ifdef __linux__
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
        GLFWwindow* window =
            glfwCreateWindow(RocProfVis::App::DEFAULT_WINDOWED_WIDTH,
                             RocProfVis::App::DEFAULT_WINDOWED_HEIGHT,
                             RocProfVis::App::APP_NAME, nullptr, nullptr);
        rocprofvis_imgui_backend_t backend;

        if(window && rocprofvis_imgui_backend_setup_with_fallback(
                         &backend, &window, RocProfVis::App::DEFAULT_WINDOWED_WIDTH,
                         RocProfVis::App::DEFAULT_WINDOWED_HEIGHT,
                         RocProfVis::App::APP_NAME, backend_pref))
        {
            RocProfVis::App::CLIParser::DetachFromConsole();

            if(rocprofvis_imgui_backend_complete_init_with_opengl_fallback(
                   &backend, &window, RocProfVis::App::DEFAULT_WINDOWED_WIDTH,
                   RocProfVis::App::DEFAULT_WINDOWED_HEIGHT, RocProfVis::App::APP_NAME,
                   backend_pref))
            {
                // After init: window may be recreated (e.g. Vulkan -> OpenGL fallback)
                RocProfVis::App::install_window_callbacks(window);

#ifdef ROCPROFVIS_MULTI_WINDOW
                // A fullscreen GLFW window iconifies itself whenever it loses
                // input focus while GLFW_AUTO_ICONIFY is set, which is the
                // default. Multi-viewport puts panels that have been dragged out
                // into sibling OS windows, so clicking one of them takes focus
                // away from the main window and would minimize the entire app,
                // leaving only the floating panel on screen.
                glfwSetWindowAttrib(window, GLFW_AUTO_ICONIFY, GLFW_FALSE);
#endif

#ifdef __linux__
                // GLFW_SCALE_TO_MONITOR sized the window using the scale GLFW
                // knew about, so redo it with the corrected one.
                const float initial_scale =
                    RocProfVis::Platform::get_content_scale(window);
                glfwSetWindowSize(
                    window,
                    static_cast<int>(RocProfVis::App::DEFAULT_WINDOWED_WIDTH *
                                     initial_scale),
                    static_cast<int>(RocProfVis::App::DEFAULT_WINDOWED_HEIGHT *
                                     initial_scale));
#endif

                RocProfVis::App::init_fullscreen_state(
                    window, RocProfVis::App::app_fullscreen_state());
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
#ifdef __linux__
                // Set from get_content_scale() each frame instead.
                io.ConfigDpiScaleFonts = false;
#endif

                ImGui::StyleColorsLight();

                rocprofvis_view_init(
                    [window](int notification) -> void {
                        RocProfVis::App::app_notification_callback(window, notification);
                    },
                    fd_pref);

#if defined(__linux__) && defined(ROCPROFVIS_MULTI_WINDOW)
                configure_drag_repair(cli_parser);
#endif

                backend.m_config(&backend, window);
#ifdef __APPLE__
                // Install after m_config so this overrides the ImGui GLFW
                // backend's own mouse-button callback (set during m_config).
                glfwSetMouseButtonCallback(window,
                                           RocProfVis::App::mouse_button_callback);
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
                    RocProfVis::App::open_dropped_files();

                    // Async work/animation in flight: refill the budget so the
                    // settle tail also covers the final frames after it finishes.
                    if(rocprofvis_view_wants_continuous_render())
                    {
                        g_frames_to_render = RocProfVis::App::RENDER_FRAMES_AFTER_INPUT;
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
                        glfwWaitEventsTimeout(RocProfVis::App::IDLE_WAIT_TIMEOUT_SECONDS);
                        g_frames_to_render = RocProfVis::App::RENDER_FRAMES_AFTER_INPUT;
                    }

                    // Correct the windowed geometry if the window manager did
                    // not honour the one requested when fullscreen was left.
                    RocProfVis::App::settle_windowed_geometry(
                        window, RocProfVis::App::app_fullscreen_state());

#ifdef __APPLE__
                    // Clear any phantom-stuck modifier (e.g. Control left down
                    // after a Mission Control gesture) before the frame renders.
                    RocProfVis::App::sync_imgui_modifiers_with_os();
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

#ifdef __linux__
                    // Re-read every frame so moving the window between monitors
                    // of different density rescales the UI.
                    ImGui::GetStyle().FontScaleDpi =
                        RocProfVis::Platform::get_content_scale(window);
#endif

                    backend.m_new_frame(&backend);
                    ImGui::NewFrame();

#if defined(__linux__) && defined(ROCPROFVIS_MULTI_WINDOW)
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
                        RocProfVis::App::request_fullscreen_toggle();
                    }
#endif

                    rocprofvis_view_render(RocProfVis::App::take_render_options());

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
#if defined(__linux__) && defined(ROCPROFVIS_MULTI_WINDOW)
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
                    if(RocProfVis::App::take_fullscreen_toggle_request())
                    {
                        RocProfVis::App::toggle_fullscreen(
                            window, RocProfVis::App::app_fullscreen_state());
                        g_frames_to_render = RocProfVis::App::RENDER_FRAMES_AFTER_INPUT;
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
