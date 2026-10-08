// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_glfw_util.h"
#include "imgui.h"
#include "imgui_te_engine.h"
#include "app_tests.h"
#include "imgui_te_ui.h"
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
#ifdef __APPLE__
#include "rocprofvis_platform_helpers.h"
#endif
#include <GLFW/glfw3.h>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>

// Frames left to render before the loop may sleep; see RENDER_FRAMES_AFTER_INPUT.
static int g_frames_to_render = 1;

static void
parse_command_line_args(int argc, char** argv, RocProfVis::App::CLIParser& cli_parser,
                        bool& exit_app)
{
    bool result = RocProfVis::App::add_common_cli_options(cli_parser);
    result &= cli_parser.AddOption("h", "help",
        "Show this help message and exit", false);
    result &= cli_parser.AddOption(
        "t", "run-tests",
        "Run all registered UI tests headlessly, print results, and exit", false);
    ROCPROFVIS_ASSERT(result);

    cli_parser.Parse(argc, argv);
    RocProfVis::App::handle_help_and_version(cli_parser, exit_app);
}

int
main(int argc, char** argv)
{
    int app_result_code = 0;

    RocProfVis::App::CLIParser::AttachToConsole();
    RocProfVis::App::CLIParser cli_parser;
    bool                       exit_app = false;
    parse_command_line_args(argc, argv, cli_parser, exit_app);
    if(exit_app)
    {
        return app_result_code;
    }

    RocProfVis::App::enable_application_log();

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

                RocProfVis::App::init_fullscreen_state(
                    window, RocProfVis::App::app_fullscreen_state());
                glfwShowWindow(window);

                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                io.ConfigDpiScaleFonts               = true;
                io.ConfigWindowsMoveFromTitleBarOnly = true;

                ImGui::StyleColorsLight();

                rocprofvis_view_init(
                    [window](int notification) -> void {
                        RocProfVis::App::app_notification_callback(window, notification);
                    },
                    fd_pref);

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

                ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
                RegisterAppTests(engine);

                const bool run_tests_headless =
                    cli_parser.WasOptionFound("run-tests");
                bool headless_tests_queued = false;
                int headless_settle_frames = 0;
                // Upper bound (~50s @60fps), not a tuned value: a view that never
                // settles would otherwise hang the loop forever. Queue anyway at
                // the cap so the run terminates instead of spinning.
                constexpr int kHeadlessSettleFrameCap = 3000;
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
                    RocProfVis::App::open_dropped_files();

                    // Async work/animation in flight: refill the budget so the
                    // settle tail also covers the final frames after it finishes.
                    if(rocprofvis_view_wants_continuous_render())
                    {
                        g_frames_to_render = RocProfVis::App::RENDER_FRAMES_AFTER_INPUT;
                    }
                    bool tests_running = false;
                    // Poll the queue too: IsRunningTests flips false between
                    // queued tests, which would let the loop sleep mid-run.
                    tests_running = ImGuiTestEngine_GetIO(engine).IsRunningTests ||
                                    !ImGuiTestEngine_IsTestQueueEmpty(engine);

                    if(run_tests_headless)
                    {
                        g_frames_to_render = RocProfVis::App::RENDER_FRAMES_AFTER_INPUT;
                        if(!headless_tests_queued &&
                           (!rocprofvis_view_wants_continuous_render() ||
                            ++headless_settle_frames >= kHeadlessSettleFrameCap))
                        {
                            if(headless_settle_frames >= kHeadlessSettleFrameCap)
                            {
                                spdlog::warn("Headless: view never settled after "
                                             "{} frames; queueing tests anyway",
                                             kHeadlessSettleFrameCap);
                            }
                            // File load settled (or cap hit): queue every registered test.
                            ImGuiTestEngine_QueueTests(
                                engine, ImGuiTestGroup_Tests, nullptr,
                                ImGuiTestRunFlags_RunFromCommandLine);
                            headless_tests_queued = true;
                        }
                        else if(headless_tests_queued && !tests_running)
                        {
                            ImVector<ImGuiTest*> tests;
                            ImGuiTestEngine_GetTestList(engine, &tests);
                            int failed = 0;
                            int never_ran = 0;
                            std::cout << "\n=== headless UI test results ===\n";
                            for(ImGuiTest* test : tests)
                            {
                                const char* status = "UNKNOWN";
                                switch(test->Output.Status)
                                {
                                    case ImGuiTestStatus_Success: status = "PASS"; break;
                                    case ImGuiTestStatus_Error:   status = "FAIL"; ++failed; break;
                                    // By-design skips run + LogWarning + return -> Success/PASS.
                                    // A test still Queued here means the engine never ran it.
                                    case ImGuiTestStatus_Queued:  status = "NOT RUN"; ++never_ran; break;
                                    default: break;
                                }
                                std::cout << "  [" << status << "] " << test->Category
                                          << "/" << test->Name << "\n";
                            }
                            std::cout << "=== " << tests.Size << " tests, " << failed
                                      << " failed, " << never_ran << " not run ===\n";
                            std::cout.flush();
                            app_result_code = (failed > 0 || never_ran > 0) ? 1 : 0;
                            glfwSetWindowShouldClose(window, GLFW_TRUE);
                        }
                    }
                    if(g_frames_to_render > 0 || tests_running)
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

                    if(glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)
                    {
                        ImGui_ImplGlfw_Sleep(10);
                        continue;
                    }

                    backend.m_new_frame(&backend);
                    ImGui::NewFrame();
                    // Hide the panel during a run so it can't cover the UI under test.
                    if(!ImGuiTestEngine_GetIO(engine).IsRunningTests)
                    {
                        ImGuiTestEngine_ShowTestEngineWindows(engine, nullptr);
                    }

#ifndef __APPLE__
                    // Matches main.cpp: F11 is read from ImGui's key state rather
                    // than a GLFW key callback, which the ImGui backend only
                    // chains for the main window.
                    if(ImGui::IsKeyPressed(ImGuiKey_F11, false))
                    {
                        RocProfVis::App::request_fullscreen_toggle();
                    }
#endif

                    rocprofvis_view_render(RocProfVis::App::take_render_options());

                    ImGui::Render();
                    ImDrawData* draw_data    = ImGui::GetDrawData();
                    const bool  is_minimized = (draw_data->DisplaySize.x <= 0.0f ||
                                               draw_data->DisplaySize.y <= 0.0f);
                    ImGuiTestEngine_PreSwap(engine);
                    if(!is_minimized)
                    {
                        backend.m_render(&backend, draw_data, &clear_color);
                        backend.m_present(&backend);
                    }
                    ImGuiTestEngine_PostSwap(engine);

#ifndef __APPLE__
                    // Applied after the frame so the window is never resized
                    // part-way through one.
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
                ImGuiTestEngine_Stop(engine);
                rocprofvis_view_destroy();
                rocprofvis_view_set_texture_backend(nullptr, nullptr, nullptr);
                backend.m_shutdown(&backend);

                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();
                ImGuiTestEngine_DestroyContext(engine);
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
