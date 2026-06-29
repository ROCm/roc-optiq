// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller_enums.h"
#include "rocprofvis_events.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_profiler_launch_orchestrator.h"
#include "rocprofvis_launch_config.h"
#include "rocprofvis_profiler_backend.h"
#include "rocprofvis_launch_preset_manager.h"
#include "rocprofvis_launch_shared_tabs.h"
#include "remote/rocprofvis_ssh_uri.h"
#include "remote/rocprofvis_ssh_connection_store.h"
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
    void OnCancelClicked();
    void OnCloseClicked();
    // Reacts to run-state edges reported by the orchestrator: appends the
    // completion/failure/cancel epilogue lines and sets m_error_message.
    void HandleStateTransition(rocprofvis_profiler_state_t new_state);
    void RebuildComposedOutput();
    void RefreshExecutionCache();

    bool IsSshMode() const { return m_config.connection == ConnectionType::kSsh; }
    rocprofvis_profiler_type_t ResolveProfilerType() const;

    void RenderToolbar();
    void RenderMainContent();
    void RenderRemoteSection();
    void RenderButtonRow();
    void RenderRemotePopups();

    // Collapses the local profiler state and the remote workflow phase into a
    // single console status badge (label + semantic level) plus an optional
    // detail line (e.g. the remote download path).
    void ComputeConsoleStatus(std::string& out_label, ConsoleStatusLevel& out_level,
                              std::string& out_detail) const;

    void SwitchBackend(int index);
    void LoadFromSettings();
    void SaveToSettings();
    void ApplySelectedConnection();
    void AddRecentTarget(std::string const& exe);
    std::string GetProfilerPath() const;

    AppWindow* m_app_window;

    // Run engine: owns the local / remote sessions and the normalized run state.
    // The dialog drives it (Launch/Cancel/Close/Update) and reads state back via
    // its getters; it never touches the underlying sessions directly.
    ProfilerLaunchOrchestrator m_orchestrator;

    // Remote (SSH) connection authoring. The connection config is owned here as
    // a shared RemoteUri (edited via the on-demand SshSettingsDialog) and handed
    // to the orchestrator at launch, mirroring the SshTestDialog pattern.
    std::shared_ptr<RemoteUri>             m_remote_uri;
    SshConnectionStore                     m_connection_store;
    std::string                            m_selected_connection_id;
    std::unique_ptr<SshSettingsDialog>     m_ssh_settings_dialog;
    bool                                   m_remote_show_progress_popup;
    FileStat::Snapshot                     m_remote_last_progress;

    bool m_should_open;
    bool m_show_window;
    // Last run state the dialog has reacted to, so Update() can detect edges and
    // append epilogue text once per transition.
    rocprofvis_profiler_state_t m_last_seen_state;

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

    // Composed console output. The run state itself lives in the orchestrator;
    // these are the view-owned text pieces assembled for display.
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
