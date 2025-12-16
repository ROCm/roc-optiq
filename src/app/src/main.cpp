// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "glfw_util.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "rocprofvis_core.h"
#include "rocprofvis_imgui_backend.h"
#define GLFW_INCLUDE_NONE
#include "AMD_LOGO.h"
#include "rocprofvis_version.h"
#include "rocprofvis_view_module.h"
#include <CLI/CLI.hpp>
#include <GLFW/glfw3.h>
#include <filesystem>
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string>

// Needed for CLI interactions that involve print.
#ifdef _WIN32
#    include <conio.h>
#    include <windows.h>
#endif

const char* APP_NAME = "ROCm(TM) Optiq Beta";

// globals shared with callbacks
static std::vector<std::string>         g_dropped_file_paths;
static bool                             g_file_was_dropped = false;
static rocprofvis_view_render_options_t g_render_options =
    rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_None;
static std::string g_user_file_path_cli;

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
parse_command_line_args(int argc, char** argv)
{
#ifdef _WIN32
    // Attach to parent console if present. If already attached, ERROR_ACCESS_DENIED is
    // expected, so still wire stdout/stderr to the console handles.
    if(AttachConsole(ATTACH_PARENT_PROCESS) || GetLastError() == ERROR_ACCESS_DENIED)
    {
        FILE* pCout;
        freopen_s(&pCout, "CONOUT$", "w", stdout);
        freopen_s(&pCout, "CONOUT$", "w", stderr);
    }
#endif
    CLI::App app(APP_NAME + std::string(": A visualizer for the ROCm Profiler Tools"));

    // Version option
    bool version_flag = false;
    app.add_flag("-v,--version", version_flag, "Print version information");

    // File option
    app.add_option("-f,--file", g_user_file_path_cli, "Path to the file to open")
        ->type_name("PATH");

    int  exit_code = 0;
    bool do_exit   = false;
    try
    {
        app.parse(argc, argv);
    } catch(const CLI::ParseError& e)
    {
        // app.exit() handles printing the error message
        exit_code = app.exit(e);

        // Help option --help is handled by throwing exception so check
        // error code before logging as an error
        if(exit_code != static_cast<int>(CLI::ExitCodes::Success))
        {
            spdlog::error("Failed to parse command line arguments.");
        }
        do_exit = true;
    }

    // Handle version
    if(!do_exit && version_flag)
    {
        print_version();
        // exit if version was the only argument
        if(argc == 2)
        {
            do_exit = true;
        }
    }

    if(do_exit)
    {
        std::cout.flush();
        std::cerr.flush();
        fflush(stdout);
        fflush(stderr);
        exit(exit_code);
    }
}

int
main(int argc, char** argv)
{
    int resultCode = 0;

    parse_command_line_args(argc, argv);

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

    glfwSetErrorCallback(glfw_error_callback);
    if(glfwInit())
    {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
#if defined(GLFW_SCALE_TO_MONITOR)  // GLFW 3.3+
        glfwWindowHint(GLFW_SCALE_TO_MONITOR, GLFW_TRUE);
#endif
        GLFWwindow* window = glfwCreateWindow(RocProfVis::View::DEFAULT_WINDOWED_WIDTH,
                                              RocProfVis::View::DEFAULT_WINDOWED_HEIGHT,
                                              APP_NAME, nullptr, nullptr);
        rocprofvis_imgui_backend_t backend;

        // Initialize fullscreen state with actual window position and size
        RocProfVis::View::init_fullscreen_state(window, g_fullscreen_state);

        // Drop file callback
        glfwSetDropCallback(window, drop_callback);
        // DPI scaling callback
        glfwSetWindowContentScaleCallback(window, content_scale_callback);
        // Initialize once (callback may not fire immediately on some platforms)
        {
            float xs, ys;
            glfwGetWindowContentScale(window, &xs, &ys);
            content_scale_callback(window, xs, ys);
        }
        // Window close callback
        glfwSetWindowCloseCallback(window, close_callback);
        // Window size change callback
        glfwSetWindowSizeCallback(window, window_size_change_callback);
        // Keyboard callback for fullscreen toggle
        glfwSetKeyCallback(window, key_callback);

        if(window && rocprofvis_imgui_backend_setup(&backend, window))
        {
            glfwShowWindow(window);

            if(backend.m_init(&backend, window))
            {
                IMGUI_CHECKVERSION();
                ImGui::CreateContext();
                ImGuiIO& io = ImGui::GetIO();
                io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
                io.ConfigWindowsMoveFromTitleBarOnly = true;

                ImGui::StyleColorsLight();

                rocprofvis_view_init([window](int notification) -> void {
                    app_notification_callback(window, notification);
                });

                backend.m_config(&backend, window);

                if(!g_user_file_path_cli.empty())
                {
                    // If the user inputted a filepath open it here.
                    rocprofvis_view_open_files({ g_user_file_path_cli });
                }

                ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

                auto [image, pixels] =
                    RocProfVis::View::create_icon(AMD_LOGO_png, AMD_LOGO_png_len);
                if(image.pixels != nullptr)
                {
                    // Set the window icon
                    GLFWimage images[1] = { image };
                    glfwSetWindowIcon(window, 1, images);

                    // Free the image pixels after setting the icon
                    RocProfVis::View::free_icon(pixels);
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
                        backend.m_present(&backend);
                    }
                }

                backend.m_shutdown(&backend);

                rocprofvis_view_destroy();
                ImGui_ImplGlfw_Shutdown();
                ImGui::DestroyContext();

                backend.m_destroy(&backend);
            }

            glfwDestroyWindow(window);
        }
        else
        {
            spdlog::error("GLFW: Failed to initialize window & graphics API backend");
            resultCode = 1;
        }

        glfwTerminate();
    }
    else
    {
        spdlog::error("GLFW: Failed to initialize GLFW library");
        resultCode = 1;
    }

    return resultCode;
}
