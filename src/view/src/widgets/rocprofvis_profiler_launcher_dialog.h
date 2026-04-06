// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_data_provider.h"
#include "imgui.h"
#include <string>
#include <functional>

namespace RocProfVis
{
namespace View
{

// Forward declarations
class AppWindow;
class DataProvider;

class ProfilerLauncherDialog
{
public:
    ProfilerLauncherDialog(AppWindow* app_window);
    ~ProfilerLauncherDialog();

    void Show();
    void Render();
    void Update();

private:
    void OnLaunchClicked();
    void OnCancelClicked();
    void OnCloseClicked();
    void OnBrowseProfilerPath();
    void OnBrowseTargetExecutable();
    void OnBrowseOutputDirectory();
    void PollProfilerState();
    void UpdateOutput();

    // File dialog callbacks
    void OnProfilerPathSelected(const std::string& path);
    void OnTargetExecutableSelected(const std::string& path);
    void OnOutputDirectorySelected(const std::string& path);

    AppWindow* m_app_window;
    DataProvider m_data_provider;  // Owns its own DataProvider

    bool m_should_open;
    bool m_is_running;

    // Configuration fields (ImGui input buffers)
    int m_profiler_type_index;
    char m_profiler_path[512];
    char m_target_executable[512];
    char m_target_args[512];
    char m_output_directory[512];
    char m_profiler_args[512];

    // Profiler state
    rocprofvis_profiler_state_t m_profiler_state;
    std::string m_output_text;
    std::string m_error_message;

    // UI state
    bool m_auto_scroll_output;
};

}  // namespace View
}  // namespace RocProfVis
