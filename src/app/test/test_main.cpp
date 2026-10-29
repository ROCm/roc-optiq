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
#include "tutorial_chapters.h"
#include "tutorial_recorder.h"
#include "widgets/rocprofvis_image_helpers.h"
#ifdef __APPLE__
#include "rocprofvis_platform_helpers.h"
#endif
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>

// Frames left to render before the loop may sleep; see RENDER_FRAMES_AFTER_INPUT.
static int g_frames_to_render = 1;

static void
parse_command_line_args(int argc, char** argv, RocProfVis::App::CLIParser& cli_parser,
                        bool& exit_app)
{
    bool result = RocProfVis::App::add_common_cli_options(cli_parser);
    result &= cli_parser.AddOption(
        "t", "run-tests",
        "Run all registered UI tests headlessly, print results, and exit", false);
    result &= cli_parser.AddOption(
        "r", "record-tutorials",
        "Record the tutorial video series into the given folder, then exit. Traces "
        "are opened from the working directory",
        true);
    result &= cli_parser.AddOption(
        "c", "chapters",
        "With --record-tutorials, only record chapters matching this filter", true);
    result &= cli_parser.AddOption(
        "e", "ffmpeg",
        "With --record-tutorials, the ffmpeg executable (default: found on PATH)", true);
    result &= cli_parser.AddOption(
        "u", "ui-scale", "With --record-tutorials, the UI scale of the footage", true);
    result &= cli_parser.AddOption(
        "n", "narration",
        "With --record-tutorials, the folder of synthesized narration (lines.tsv) whose "
        "line lengths pace the chapters",
        true);
    ROCPROFVIS_ASSERT(result);

    cli_parser.Parse(argc, argv);
    RocProfVis::App::handle_help_and_version(cli_parser, exit_app);
}

int
main(int argc, char** argv)
{
    int app_result_code = 0;

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

    const bool record_tutorials = cli_parser.WasOptionFound("record-tutorials");
    if(record_tutorials)
    {
        RocProfVis::Tutorial::RecorderConfig config;
        config.output_dir =
            std::filesystem::absolute(cli_parser.GetOptionValue("record-tutorials")).string();
        config.samples_dir  = std::filesystem::current_path().string();
        config.filter       = cli_parser.GetOptionValue("chapters");
        if(cli_parser.WasOptionFound("narration"))
        {
            config.narration_dir =
                std::filesystem::absolute(cli_parser.GetOptionValue("narration")).string();
        }
        config.encoder_path =
            RocProfVis::Tutorial::ResolveEncoderPath(cli_parser.GetOptionValue("ffmpeg"));
        if(cli_parser.WasOptionFound("ui-scale"))
        {
            const float ui_scale =
                std::strtof(cli_parser.GetOptionValue("ui-scale").c_str(), nullptr);
            if(ui_scale > 0.0f)
            {
                config.ui_scale = ui_scale;
            }
        }
        if(config.encoder_path.empty())
        {
            spdlog::error("--record-tutorials needs ffmpeg: pass --ffmpeg <path> or add "
                          "it to PATH");
            return 1;
        }
        const std::string extras = RocProfVis::Tutorial::ExperimentalFeatures();
        if(!extras.empty())
        {
            spdlog::error("[tutorial] --record-tutorials needs a build with release features "
                          "(cmake --preset x64-tutorial), but this one also has {}",
                          extras);
            return 1;
        }
        RocProfVis::Tutorial::SetConfig(config);
        // The recording reads the OpenGL back buffer, and the built-in file dialog
        // draws inside the captured window where a native one would not.
        backend_pref = kRPVBackendForceOpenGL;
        fd_pref      = kRocProfVisViewFileDialog_ImGui;
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
        // A recording keeps its window at the video size when it lands on a
        // monitor with another scale, as when a laptop lid closes mid-run.
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, record_tutorials ? GLFW_FALSE : GLFW_TRUE);
#endif
        RocProfVis::Platform::set_window_class_hints(RocProfVis::App::APP_NAME_ASCII);
        GLFWwindow* window =
            glfwCreateWindow(RocProfVis::Platform::DEFAULT_WINDOWED_WIDTH,
                             RocProfVis::Platform::DEFAULT_WINDOWED_HEIGHT,
                             RocProfVis::App::APP_NAME, nullptr, nullptr);
        rocprofvis_imgui_backend_t backend;

        if(window && rocprofvis_imgui_backend_setup_with_fallback(
                         &backend, &window, RocProfVis::Platform::DEFAULT_WINDOWED_WIDTH,
                         RocProfVis::Platform::DEFAULT_WINDOWED_HEIGHT,
                         RocProfVis::App::APP_NAME, backend_pref))
        {
            RocProfVis::App::CLIParser::DetachFromConsole();

            if(rocprofvis_imgui_backend_complete_init_with_opengl_fallback(
                   &backend, &window, RocProfVis::Platform::DEFAULT_WINDOWED_WIDTH,
                   RocProfVis::Platform::DEFAULT_WINDOWED_HEIGHT,
                   RocProfVis::App::APP_NAME, backend_pref))
            {
                // After init: window may be recreated (e.g. Vulkan -> OpenGL fallback)
                RocProfVis::App::install_window_callbacks(window);

                RocProfVis::Platform::init_fullscreen_state(
                    window, RocProfVis::App::app_fullscreen_state());
                glfwShowWindow(window);
                if(record_tutorials)
                {
                    // Every captured frame must match the video size exactly.
                    glfwSetWindowAttrib(window, GLFW_RESIZABLE, GLFW_FALSE);
                    glfwSetWindowSize(window, RocProfVis::Tutorial::VIDEO_WIDTH,
                                      RocProfVis::Tutorial::VIDEO_HEIGHT);
                }

                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiTestEngine* engine = ImGuiTestEngine_CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                io.ConfigDpiScaleFonts               = true;
                io.ConfigWindowsMoveFromTitleBarOnly = true;
                if(record_tutorials)
                {
                    // The footage uses its own UI scale, set each frame below,
                    // rather than the recording machine's monitor DPI.
                    io.ConfigDpiScaleFonts = false;
                }

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
                if(record_tutorials)
                {
                    RocProfVis::Tutorial::InstallCapture(engine);
                    RocProfVis::Tutorial::RegisterChapters(engine);
                }
                else
                {
                    RegisterAppTests(engine);
                }

                const bool run_tests_headless =
                    cli_parser.WasOptionFound("run-tests") || record_tutorials;
                const std::string queue_filter =
                    record_tutorials ? RocProfVis::Tutorial::GetConfig().filter : std::string();
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
                                engine, ImGuiTestGroup_Tests,
                                queue_filter.empty() ? nullptr : queue_filter.c_str(),
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
                    RocProfVis::Platform::settle_windowed_geometry(
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

                    if(record_tutorials)
                    {
                        ImGui::GetStyle().FontScaleDpi =
                            RocProfVis::Tutorial::GetConfig().ui_scale;
                        // Display changes can still resize or maximize the
                        // window, and every captured frame must match the video.
                        if(fb_width != RocProfVis::Tutorial::VIDEO_WIDTH ||
                           fb_height != RocProfVis::Tutorial::VIDEO_HEIGHT)
                        {
                            if(glfwGetWindowAttrib(window, GLFW_MAXIMIZED) != 0)
                            {
                                glfwRestoreWindow(window);
                            }
                            glfwSetWindowSize(window, RocProfVis::Tutorial::VIDEO_WIDTH,
                                              RocProfVis::Tutorial::VIDEO_HEIGHT);
                        }
                    }

                    backend.m_new_frame(&backend);
                    ImGui::NewFrame();
                    // Hide the panel during a run so it can't cover the UI under test.
                    if(!ImGuiTestEngine_GetIO(engine).IsRunningTests && !record_tutorials)
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
                    if(record_tutorials)
                    {
                        RocProfVis::Tutorial::RenderOverlay();
                    }

                    ImGui::Render();
                    ImDrawData* draw_data    = ImGui::GetDrawData();
                    const bool  is_minimized = (draw_data->DisplaySize.x <= 0.0f ||
                                               draw_data->DisplaySize.y <= 0.0f);
                    ImGuiTestEngine_PreSwap(engine);
                    if(!is_minimized)
                    {
                        backend.m_render(&backend, draw_data, &clear_color);
                    }
                    // Screen captures read the back buffer, so they must run
                    // before present leaves its contents undefined.
                    ImGuiTestEngine_PostSwap(engine);
                    if(!is_minimized)
                    {
                        backend.m_present(&backend);
                    }

#ifndef __APPLE__
                    // Applied after the frame so the window is never resized
                    // part-way through one.
                    if(RocProfVis::App::apply_fullscreen_toggle_request(window))
                    {
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
