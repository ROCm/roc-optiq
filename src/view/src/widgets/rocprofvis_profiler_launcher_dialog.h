// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_launch_config.h"
#include "rocprofvis_profiler_backend.h"
#include "rocprofvis_launch_preset_manager.h"
#include "imgui.h"
#include <string>
#include <memory>
#include <vector>
#include <functional>

namespace RocProfVis
{
namespace View
{

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
    void PollProfilerState();
    void UpdateOutput();
    void RebuildComposedOutput();

    void RenderLeftPane();
    void RenderRightPane();
    void RenderButtonRow();

    void SwitchBackend(int index);
    void LoadFromSettings();
    void SaveToSettings();
    void AddRecentTarget(std::string const& exe);
    std::string GetProfilerPath() const;

    AppWindow* m_app_window;
    DataProvider m_data_provider;

    bool m_should_open;
    bool m_show_window;
    bool m_is_running;

    // Backend system
    std::vector<std::unique_ptr<IProfilerBackend>> m_backends;
    int m_backend_index;
    int m_tool_index;

    // Config
    LaunchConfig m_config;
    std::string m_profiler_path_override;

    // Presets
    LaunchPresetManager m_preset_manager;
    std::string m_current_preset_name;

    // Profiler state
    rocprofvis_profiler_state_t m_profiler_state;
    std::string m_output_text;
    std::string m_output_preamble;
    std::string m_output_epilogue;
    std::string m_process_output_raw;
    std::string m_process_output_stripped;
    std::string m_error_message;

    // UI state
    bool m_auto_scroll_output;
};

}  // namespace View
}  // namespace RocProfVis
