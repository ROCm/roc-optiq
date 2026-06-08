// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_events.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_profiler_session.h"
#include "rocprofvis_remote_profiler_session.h"
#include "rocprofvis_launch_config.h"
#include "rocprofvis_profiler_backend.h"
#include "rocprofvis_launch_preset_manager.h"
#include "remote/rocprofvis_ssh_uri.h"
#include "remote/rocprofvis_ssh_settings_dialog.h"
#include "remote/rocprofvis_ssh_fetch.h"
#include "imgui.h"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <utility>

namespace RocProfVis
{
namespace View
{

class AppWindow;

class ProfilerLauncherDialog
{
public:
    ProfilerLauncherDialog(AppWindow* app_window);
    ~ProfilerLauncherDialog();

    void Show();
    void Render();
    void Update();

private:
    struct ExecutionCache
    {
        std::vector<std::pair<std::string, std::string>> curated_env_vars;
        std::vector<std::pair<std::string, std::string>> env_vars;
        std::vector<std::string>                         argv;
        std::string                                      profiler_path;
        std::string                                      profiler_args;
        std::string                                      command_preview;
    };

    void OnLaunchClicked();
    void OnLaunchLocal();
    void OnLaunchRemote();
    void OnCancelClicked();
    void OnCloseClicked();
    void OnProfilerStateChanged(rocprofvis_profiler_state_t new_state);
    void UpdateOutput();
    void RebuildComposedOutput();
    void RefreshExecutionCache();

    bool IsSshMode() const { return m_config.connection == ConnectionType::kSsh; }
    rocprofvis_profiler_type_t ResolveProfilerType() const;

    void RenderToolbar();
    void RenderMainContent();
    void RenderRemoteSection();
    void RenderButtonRow();
    void RenderRemotePopups();

    void SwitchBackend(int index);
    void LoadFromSettings();
    void SaveToSettings();
    void AddRecentTarget(std::string const& exe);
    std::string GetProfilerPath() const;

    AppWindow* m_app_window;
    ProfilerSession m_profiler_session;
    EventManager::SubscriptionToken m_profiler_status_token;

    // Remote (SSH) profiling. The connection config is owned here as a shared
    // RemoteUri (edited via the on-demand SshSettingsDialog) and consumed by the
    // spawned RemoteProfilerSession, mirroring the SshTestDialog pattern.
    std::shared_ptr<RemoteUri>             m_remote_uri;
    std::unique_ptr<SshSettingsDialog>     m_ssh_settings_dialog;
    std::unique_ptr<RemoteProfilerSession> m_remote_session;
    bool                                   m_remote_show_progress_popup;
    FileStat::Snapshot                     m_remote_last_progress;

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
    ExecutionCache m_execution_cache;

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
