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
#include "widgets/rocprofvis_gui_helpers.h"
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#ifdef __linux__
#    include <unordered_map>
#endif

const char* APP_NAME = "ROCm(TM) Optiq Beta";

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
static void
snap_secondary_viewports_to_os_pos()
{
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    if(pio.Platform_GetWindowPos == nullptr)
    {
        return;
    }
    g_viewport_intended_pos.clear();
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
        g_viewport_intended_pos[vp->ID] = vp->Pos;
        vp->Pos                         = os_pos;
        // Make the subsequent WindowSyncOwnedViewport() propagate our
        // corrected Pos into the owning ImGuiWindow (window->Pos =
        // viewport->Pos), which is what hit-testing actually uses.
        vp->PlatformRequestMove = true;
    }
}

// Restore the drag-target positions we saved in
// snap_secondary_viewports_to_os_pos(), so UpdatePlatformWindows() still
// transmits the requested move to the OS.  PlatformRequestMove is cleared
// because UpdatePlatformWindows() skips Platform_SetWindowPos() whenever
// that flag is set.
static void
restore_secondary_viewport_intended_pos()
{
    if(g_viewport_intended_pos.empty())
    {
        return;
    }
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    for(int i = 1; i < pio.Viewports.Size; ++i)
    {
        ImGuiViewport* vp = pio.Viewports[i];
        auto           it = g_viewport_intended_pos.find(vp->ID);
        if(it == g_viewport_intended_pos.end())
        {
            continue;
        }
        vp->Pos                 = it->second;
        vp->PlatformRequestMove = false;
    }
    g_viewport_intended_pos.clear();
}
#endif

// Fullscreen state (initialized after window creation)
static RocProfVis::View::FullscreenState g_fullscreen_state = {};

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
content_scale_callback(GLFWwindow* window, float xscale, float yscale)
{
    // Unused parameters
    (void) window;
    (void) yscale;
    rocprofvis_view_set_dpi(xscale);
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
    else if(notification ==
            static_cast<int>(rocprofvis_view_notification_t::
                                 kRocProfVisViewNotification_Toggle_Fullscreen))
    {
        RocProfVis::View::toggle_fullscreen(window, g_fullscreen_state);
    }
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

static void
key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // Unused parameters
    (void) scancode;
    (void) mods;

    // Toggle fullscreen with F11
    if(key == GLFW_KEY_F11 && action == GLFW_PRESS)
    {
        RocProfVis::View::toggle_fullscreen(window, g_fullscreen_state);
    }
}

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
    result &= cli_parser.AddOption("v", "version", "Print application version", false);
    result &= cli_parser.AddOption("f", "file", "Open file", true);
    result &= cli_parser.AddOption("b", "backend", "Force rendering backend: 'vulkan' or 'opengl' (default: auto with fallback)", true);
    result &= cli_parser.AddOption("h", "help", "Help the user with commands", false);
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

    RocProfVis::View::CLIParser::AttachToConsole();
    RocProfVis::View::CLIParser cli_parser;
    bool                        exit_app = false;
    parse_command_line_args(argc, argv, cli_parser, exit_app);
    if(exit_app)
    {
        return app_result_code;
    }

    std::string config_path = rocprofvis_get_application_config_path();
#ifndef NDEBUG
    std::filesystem::path log_path =
        std::filesystem::path(config_path) / "visualizer.debug.log";
    rocprofvis_core_enable_log(log_path.string().c_str(), spdlog::level::debug);
#else
    std::filesystem::path log_path =
        std::filesystem::path(config_path) / "visualizer.log";
    rocprofvis_core_enable_log(log_path.string().c_str(), spdlog::level::info);
#endif

    // Parse backend preference from command line
    rocprofvis_imgui_backend_preference_t backend_pref = kRPVBackendAuto;
    if(cli_parser.WasOptionFound("backend"))
    {
        std::string backend_str = cli_parser.GetOptionValue("backend");
        if(backend_str == "vulkan")
        {
            backend_pref = kRPVBackendForceVulkan;
        }
        else if(backend_str == "opengl")
        {
            backend_pref = kRPVBackendForceOpenGL;
        }
        else
        {
            spdlog::error("Invalid backend '{}'. Valid options: vulkan, opengl", backend_str);
            return 1;
        }
    }

    glfwSetErrorCallback(glfw_error_callback);
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
                glfwSetWindowContentScaleCallback(window, content_scale_callback);
                {
                    float xs, ys;
                    glfwGetWindowContentScale(window, &xs, &ys);
                    content_scale_callback(window, xs, ys);
                }
                glfwSetWindowCloseCallback(window, close_callback);
                glfwSetWindowSizeCallback(window, window_size_change_callback);
                glfwSetKeyCallback(window, key_callback);

                RocProfVis::View::init_fullscreen_state(window, g_fullscreen_state);
                glfwShowWindow(window);

                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
                io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
                io.ConfigWindowsMoveFromTitleBarOnly = true;
                // Keep undocked windows out of the OS taskbar.
                io.ConfigViewportsNoTaskBarIcon = false;

                ImGui::StyleColorsLight();

                rocprofvis_view_init([window](int notification) -> void {
                    app_notification_callback(window, notification);
                });

                backend.m_config(&backend, window);

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

                while(!glfwWindowShouldClose(window))
                {
                    // handle dropped file signal flag from callback
                    if(g_file_was_dropped)
                    {
                        rocprofvis_view_open_files(g_dropped_file_paths);
                        g_file_was_dropped = false;
                    }

                    glfwPollEvents();

                    // Handle changes in the frame buffer size
                    int fb_width, fb_height;
                    glfwGetFramebufferSize(window, &fb_width, &fb_height);
                    backend.m_update_framebuffer(&backend, fb_width, fb_height);

                    if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
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
                    // See snap_secondary_viewports_to_os_pos().
                    if(io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
                    {
                        snap_secondary_viewports_to_os_pos();
                    }
#endif

                    rocprofvis_view_render(g_render_options);
                    g_render_options = rocprofvis_view_render_options_t::
                        kRocProfVisViewRenderOption_None;

                    ImGui::Render();
                    ImDrawData* draw_data    = ImGui::GetDrawData();
                    const bool  is_minimized = (draw_data->DisplaySize.x <= 0.0f ||
                                               draw_data->DisplaySize.y <= 0.0f);
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
                        // requested move.  See
                        // restore_secondary_viewport_intended_pos().
                        restore_secondary_viewport_intended_pos();
#endif
                        ImGui::UpdatePlatformWindows();
                        ImGui::RenderPlatformWindowsDefault();
                        glfwMakeContextCurrent(backup_current_context);
                    }

                    if(!is_minimized)
                    {
                        backend.m_present(&backend);
                    }
                }

                backend.m_shutdown(&backend);

                rocprofvis_view_destroy();
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
