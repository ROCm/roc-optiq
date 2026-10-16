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
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <system_error>
#include <vector>

// Frames left to render before the loop may sleep; see RENDER_FRAMES_AFTER_INPUT.
static int g_frames_to_render = 1;

// Whatever the ImGui backend installed, so normal keyboard input still reaches it.
static GLFWkeyfun g_chained_key_callback = nullptr;

// Pause/resume the soak drivers on F9.
//
// Read from the GLFW callback rather than ImGui's key state because the Test
// Engine discards the backend's input events while a test is running, which is
// precisely when a soak needs pausing - so this is the only path a real key
// press has left. Chains to the backend's own callback so nothing else breaks.
static void
soak_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if(key == GLFW_KEY_F9 && action == GLFW_PRESS)
    {
        const bool paused = !g_soak_paused.load();
        g_soak_paused.store(paused);
        spdlog::info("Soak {} (F9)", paused ? "paused" : "resumed");
    }

    if(g_chained_key_callback != nullptr)
    {
        g_chained_key_callback(window, key, scancode, action, mods);
    }
}

// Directory expansion only picks up rocpd trace databases, by filename prefix.
// A trace directory typically also holds compute profiles and other formats,
// which open into views a timeline soak cannot drive.
static const char* const TRACE_FILE_PREFIX = "rocpd";

// Resolves the -f argument to the list of files to open. A plain file is passed
// through untouched, whatever its name; a directory expands to the rocpd trace
// databases directly inside it, sorted so repeated runs open the same set in the
// same tab order. Matching on the ".db" extension also excludes the "-shm" and
// "-wal" sidecars SQLite leaves next to an open database.
static std::vector<std::string>
collect_trace_paths(const std::string& path)
{
    std::error_code error;
    if(!std::filesystem::is_directory(path, error))
    {
        return { path };
    }

    std::vector<std::string> paths;
    for(const std::filesystem::directory_entry& entry :
        std::filesystem::directory_iterator(path, error))
    {
        if(!entry.is_regular_file(error) || entry.path().extension() != ".db")
        {
            continue;
        }
        if(entry.path().filename().string().rfind(TRACE_FILE_PREFIX, 0) != 0)
        {
            continue;
        }
        paths.push_back(entry.path().string());
    }
    std::sort(paths.begin(), paths.end());

    spdlog::info("Opening {} '{}*.db' trace(s) from directory {}", paths.size(),
                 TRACE_FILE_PREFIX, path);
    return paths;
}

static void
parse_command_line_args(int argc, char** argv, RocProfVis::App::CLIParser& cli_parser,
                        bool& exit_app)
{
    bool result = RocProfVis::App::add_common_cli_options(cli_parser);
    result &= cli_parser.AddOption(
        "t", "run-tests",
        "Run all registered UI tests headlessly, print results, and exit", false);
    result &= cli_parser.AddOption(
        "s", "soak",
        "Run the soak drivers instead of the test suite: continuously exercise "
        "the loaded view until interrupted. Use with -f to pick the trace, or a "
        "directory to open every rocpd*.db trace in it at once",
        false);
    result &= cli_parser.AddOption(
        "l", "log-level",
        "Override the log level: 'trace', 'debug', 'info', 'warn', 'error'. "
        "Defaults to debug in debug builds. A soak running for days at debug "
        "produces gigabytes an hour and rotates its own earlier output away, so "
        "prefer 'info' for long runs - the periodic memory samples are info",
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

    // Applied after parsing rather than through enable_application_log, which
    // runs before the command line is read and is guarded against a second call.
    // Only the level is overridden; the sinks and log path stay as that set them.
    if(cli_parser.WasOptionFound("log-level"))
    {
        const std::string requested = cli_parser.GetOptionValue("log-level");
        if(requested == "trace")
        {
            spdlog::set_level(spdlog::level::trace);
        }
        else if(requested == "debug")
        {
            spdlog::set_level(spdlog::level::debug);
        }
        else if(requested == "info")
        {
            spdlog::set_level(spdlog::level::info);
        }
        else if(requested == "warn")
        {
            spdlog::set_level(spdlog::level::warn);
        }
        else if(requested == "error")
        {
            spdlog::set_level(spdlog::level::err);
        }
        else
        {
            std::cout << "Unknown log level '" << requested
                      << "', keeping the build default" << std::endl;
        }
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

                // Installed after m_config so this sits in front of the ImGui
                // GLFW backend's key callback (set during m_config) and can chain
                // to it.
                g_chained_key_callback = glfwSetKeyCallback(window, soak_key_callback);
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
                    // If the user inputted a filepath open it here. A directory
                    // opens every trace inside it at once, one tab per file, so a
                    // soak run can be pointed at a whole corpus.
                    rocprofvis_view_open_files(
                        collect_trace_paths(cli_parser.GetOptionValue("file")));
                }

                ImGuiTestEngine_Start(engine, ImGui::GetCurrentContext());
                RegisterAppTests(engine);

                // The soak drivers live in their own "soak" category so the
                // normal sweep can exclude them - they never terminate. Running
                // with --soak inverts that: only the soak drivers are queued.
                const bool  run_soak    = cli_parser.WasOptionFound("soak");
                const char* test_filter = run_soak ? "soak" : "-soak";
                const bool  run_tests_headless =
                    cli_parser.WasOptionFound("run-tests") || run_soak;
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
                            // File load settled (or cap hit): queue the selected
                            // tests - everything but the soak drivers, or only
                            // them under --soak.
                            ImGuiTestEngine_QueueTests(
                                engine, ImGuiTestGroup_Tests, test_filter,
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

                // Release any soak driver before stopping the engine. A driver
                // still looping would sit inside ctx->Yield() waiting for a frame
                // that is never going to be drawn, deadlocking against the join
                // inside ImGuiTestEngine_Stop().
                g_soak_exit_requested.store(true);
                g_soak_paused.store(false);

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
