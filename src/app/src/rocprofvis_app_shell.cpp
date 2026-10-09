// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_app_shell.h"
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
#include "imgui.h"
#include "rocprofvis_cli_parser.h"
#include "rocprofvis_core.h"
#include "rocprofvis_version.h"
#ifdef __APPLE__
#include "rocprofvis_platform_helpers.h"
#endif

namespace RocProfVis
{
namespace App
{

// GLFW callbacks take no user pointer here, so the state they hand to the frame
// loop lives at file scope.
static std::vector<std::string>         g_dropped_file_paths;
static bool                             g_file_was_dropped = false;
static rocprofvis_view_render_options_t g_render_options =
    rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_None;
static Platform::FullscreenState g_fullscreen_state = {};
#ifndef __APPLE__
static bool g_toggle_fullscreen_requested = false;
#endif

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
window_size_change_callback(GLFWwindow* window, int width, int height)
{
    if(Platform::sync_fullscreen_state(window, width, height, g_fullscreen_state))
    {
        rocprofvis_view_set_fullscreen_state(g_fullscreen_state.is_fullscreen);
    }
}

void
enable_application_log()
{
    std::string log_dir = rocprofvis_get_application_log_path();
#ifndef NDEBUG
    std::filesystem::path log_path =
        std::filesystem::path(log_dir) / "roc-optiq.debug.log";
    rocprofvis_core_enable_log(log_path.string().c_str(), spdlog::level::debug);
#else
    std::filesystem::path log_path = std::filesystem::path(log_dir) / "roc-optiq.log";
    rocprofvis_core_enable_log(log_path.string().c_str(), spdlog::level::info);
#endif
}

static void
print_version(bool include_commit)
{
    std::cout << APP_NAME << " version: " << ROCPROFVIS_VERSION_MAJOR << "."
              << ROCPROFVIS_VERSION_MINOR << "." << ROCPROFVIS_VERSION_PATCH << "."
              << ROCPROFVIS_VERSION_BUILD;
    if(include_commit)
    {
        std::cout << " commit: " << ROCPROFVIS_GIT_COMMIT;
        if(std::string_view(ROCPROFVIS_GIT_COMMIT) == "unknown")
        {
            std::cout << "\n" << ROCPROFVIS_GIT_COMMIT_UNKNOWN_NOTE;
        }
    }
    std::cout << std::endl;
}

bool
add_common_cli_options(CLIParser& cli_parser)
{
    cli_parser.SetAppDescription(APP_NAME, "A visualizer for profiling ROCm Data");
    bool result = true;
    result &= cli_parser.AddOption(
        "v", "version",
        "Print version and exit. Pass 'hash' to also print the git commit", false,
        "hash");
    result &= cli_parser.AddOption("f", "file", "Open a trace or project file", true);
    result &= cli_parser.AddOption(
        "b", "backend",
        "Set rendering backend: 'auto' (default), 'vulkan', or 'opengl'", true);
    result &= cli_parser.AddOption(
        "d", "file-dialog",
        "Set file dialog backend: 'auto' (default), 'native' (system file "
        "dialog), or 'imgui' (built-in). Use 'imgui' when running over SSH",
        true);
    result &= cli_parser.AddOption("h", "help", "Show this help message and exit",
                                   false);
    return result;
}

void
handle_help_and_version(const CLIParser& cli_parser, bool& exit_app)
{
    if(cli_parser.WasOptionFound("help"))
    {
        std::cout << cli_parser.GetHelp() << std::endl;
        exit_app = true;
    }

    if(!exit_app && cli_parser.WasOptionFound("version"))
    {
        print_version(!cli_parser.GetOptionValue("version").empty());

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

bool
parse_backend_preference(const CLIParser&                       cli_parser,
                         rocprofvis_imgui_backend_preference_t& backend_pref)
{
    bool valid   = true;
    backend_pref = kRPVBackendAuto;
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
            spdlog::error("Invalid backend '{}'. Valid options: auto, vulkan, opengl",
                          backend_str);
            valid = false;
        }
    }
    return valid;
}

bool
parse_file_dialog_preference(const CLIParser&                          cli_parser,
                             rocprofvis_view_file_dialog_preference_t& file_dialog_pref)
{
    bool valid       = true;
    file_dialog_pref = kRocProfVisViewFileDialog_Auto;
    if(cli_parser.WasOptionFound("file-dialog"))
    {
        std::string fd_str = cli_parser.GetOptionValue("file-dialog");
        if(fd_str == "auto")
        {
            file_dialog_pref = kRocProfVisViewFileDialog_Auto;
        }
        else if(fd_str == "native")
        {
            file_dialog_pref = kRocProfVisViewFileDialog_Native;
        }
        else if(fd_str == "imgui")
        {
            file_dialog_pref = kRocProfVisViewFileDialog_ImGui;
        }
        else
        {
            spdlog::error("Invalid --file-dialog '{}'. Valid options: auto, "
                          "native, imgui",
                          fd_str);
            valid = false;
        }
    }
    return valid;
}

void
glfw_error_callback(int error, const char* description)
{
    spdlog::error("GLFW Error {}: {}", error, description);
}

void
install_window_callbacks(GLFWwindow* window)
{
    glfwSetDropCallback(window, drop_callback);
    glfwSetWindowCloseCallback(window, close_callback);
    glfwSetWindowSizeCallback(window, window_size_change_callback);
}

void
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

void
open_dropped_files()
{
    if(g_file_was_dropped)
    {
        rocprofvis_view_open_files(g_dropped_file_paths);
        g_file_was_dropped = false;
    }
}

rocprofvis_view_render_options_t
take_render_options()
{
    const rocprofvis_view_render_options_t options = g_render_options;
    g_render_options = rocprofvis_view_render_options_t::kRocProfVisViewRenderOption_None;
    return options;
}

Platform::FullscreenState&
app_fullscreen_state()
{
    return g_fullscreen_state;
}

#ifndef __APPLE__
void
request_fullscreen_toggle()
{
    g_toggle_fullscreen_requested = true;
}

bool
apply_fullscreen_toggle_request(GLFWwindow* window)
{
    if(!g_toggle_fullscreen_requested)
    {
        return false;
    }
    g_toggle_fullscreen_requested = false;
    if(Platform::toggle_fullscreen(window, g_fullscreen_state))
    {
        rocprofvis_view_set_fullscreen_state(g_fullscreen_state.is_fullscreen);
    }
    return true;
}
#endif

#ifdef __APPLE__
void
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

void
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

}  // namespace App
}  // namespace RocProfVis
