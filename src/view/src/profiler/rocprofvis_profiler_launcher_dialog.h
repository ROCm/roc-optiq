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
// TEMPORARY (remote/SSH): the SSH connection authoring UI is only available
// when remote support is built. Remove this guard when the remote feature
// graduates.
#ifdef ROCPROFVIS_ENABLE_REMOTE
#include "remote/rocprofvis_ssh_uri.h"
#include "remote/rocprofvis_ssh_connection_store.h"
#include "remote/rocprofvis_ssh_settings_dialog.h"
#include "remote/rocprofvis_ssh_fetch.h"
#include "remote/rocprofvis_remote_file_browser.h"
#endif
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
        // Complete argument list for the profiler process, as emitted by the
        // backend. Both the launch and the command preview read this, so what
        // is shown is what runs.
        std::vector<std::string>                         argv;
        rocprofvis_profiler_tool_t                       tool = kRPVProfilerToolNone;
        // argv[0] as the run will see it: for a local run the absolute path
        // resolved on this machine, for a remote one what the remote will resolve
        // (<tool_directory>/<name>, or the bare name for its $PATH). Display only
        // - the launch passes the tool enum and the controller resolves it again.
        std::string                                      argv0;
        // Why local resolution failed, if it did. Always empty for a remote run:
        // the tool lives on a filesystem this machine cannot search, so this
        // machine's answer would be about the wrong host.
        std::string                                      resolve_error;
        std::string                                      command_preview;
    };

    // Memoized argv[0]. Resolving it is a controller call that searches the
    // filesystem, so it is not redone for edits that cannot affect it.
    struct ToolPathCache
    {
        rocprofvis_profiler_tool_t tool = kRPVProfilerToolNone;
        std::string                directory;
        bool                       ssh       = false;
        bool                       populated = false;

        std::string argv0;
        std::string error;
    };

    void OnLaunchClicked();
    void OnCancelClicked();
    void OnCloseClicked();
    // Reacts to run-state edges reported by the orchestrator: appends the
    // completion/failure/cancel epilogue lines and sets m_error_message.
    void HandleStateTransition(rocprofvis_profiler_state_t new_state);
    void RebuildComposedOutput();
    void RefreshExecutionCache();
    void RefreshToolPath(bool force);

    bool IsSshMode() const
    {
#ifdef ROCPROFVIS_ENABLE_REMOTE
        return m_config.connection == ConnectionType::kSsh;
#else
        // Remote support not built: always local, regardless of loaded config.
        return false;
#endif
    }
    // Forces m_config.tool to be one the current backend actually offers, falling
    // back to its first tool. Call after anything that can change either side of
    // that pairing - a backend switch, or loading a profile that may name a tool
    // this backend does not have (or none at all). Without it the combo could
    // display one tool while the launch ran another.
    void SyncToolWithBackend();
    // Display name of the currently selected tool, for the combo and the title.
    std::string CurrentToolDisplayName() const;

    // Top-level view routing: the dialog is either in "configure" mode (author
    // the launch) or "run" mode (a focused output console shown once a run has
    // been launched). m_show_run_view selects which is rendered.
    void RenderConfigureView();
    void RenderRunView();

    void RenderToolbar();
    void RenderMainContent();
    // One line above the command preview: why the tool could not be resolved, or
    // that a configured tool directory is deciding which build runs.
    void RenderToolResolutionNotice();
    // Deeper, less-common backend settings, shown in a separate floating window
    // opened from the "Advanced Options..." button.
    void RenderAdvancedWindow();
    // Combined "Arguments & Environment" panel: command-line args (one edit box,
    // added as pills) lead, environment variables (name/value, added as pills)
    // grow below. Clicking a pill pulls it back into the editor to edit/remove.
    void RenderArgsEnvPanel();
    void RenderButtonRow();
    // Buttons for the run view: Cancel while running; Run Again / Back to
    // Configuration / Open Trace / Close once the run has finished.
    void RenderRunButtonRow();
    // One-line "what is being run" summary shown atop the run view.
    std::string BuildRunSummary() const;
#ifdef ROCPROFVIS_ENABLE_REMOTE
    // TEMPORARY (remote/SSH): SSH connection selector + popups.
    void RenderRemoteSection();
    void RenderRemotePopups();
#endif

    // Collapses the local profiler state and the remote workflow phase into a
    // single console status badge (label + semantic level) plus an optional
    // detail line (e.g. the remote download path).
    void ComputeConsoleStatus(std::string& out_label, ConsoleStatusLevel& out_level,
                              std::string& out_detail) const;

    void SwitchBackend(int index);
    void LoadFromSettings();
    void SaveToSettings();
#ifdef ROCPROFVIS_ENABLE_REMOTE
    void ApplySelectedConnection();  // TEMPORARY (remote/SSH)
    // Lazily constructs m_remote_file_browser (bound to the shared RemoteUri).
    void EnsureRemoteFileBrowser();
#endif
    void AddRecentTarget(std::string const& exe);

    AppWindow* m_app_window;

    // Run engine: owns the local / remote sessions and the normalized run state.
    // The dialog drives it (Launch/Cancel/Close/Update) and reads state back via
    // its getters; it never touches the underlying sessions directly.
    ProfilerLaunchOrchestrator m_orchestrator;

#ifdef ROCPROFVIS_ENABLE_REMOTE
    // TEMPORARY (remote/SSH): SSH connection authoring. The connection config is
    // owned here as a shared RemoteUri (edited via the on-demand
    // SshSettingsDialog) and handed to the orchestrator at launch, mirroring the
    // SshTestDialog pattern.
    std::shared_ptr<RemoteUri>             m_remote_uri;
    SshConnectionStore                     m_connection_store;
    std::string                            m_selected_connection_id;
    std::unique_ptr<SshSettingsDialog>     m_ssh_settings_dialog;
    // Shared remote file/directory picker for the Target section's Browse
    // buttons; created lazily on first remote browse (see EnsureRemoteFileBrowser).
    std::unique_ptr<RemoteFileBrowser>     m_remote_file_browser;
    bool                                   m_remote_show_progress_popup;
    FileStat::Snapshot                     m_remote_last_progress;
#endif

    bool m_should_open;
    bool m_show_window;
    // Once a run is launched the dialog swaps to a focused output view; the
    // user returns to configuration via "Back to Configuration". Reset on open.
    bool m_show_run_view;
    // Whether the separate "Advanced Options" window is open.
    bool m_show_advanced_window;
    // Width (px) of the command-preview panel; user-adjustable via the splitter.
    // Seeded to ~1/3 of the form width the first time the content is laid out.
    float m_preview_width;
    bool  m_preview_width_initialized;
    // "Add" edit-box buffers for the Arguments & Environment panel.
    std::string m_arg_input;
    std::string m_env_name_input;
    std::string m_env_value_input;
    // Wall-clock timing for the run view's elapsed readout. Start is set on
    // launch; end is frozen when the run finishes (0 while still running).
    double m_run_start_time;
    double m_run_end_time;
    // Last run state the dialog has reacted to, so Update() can detect edges and
    // append epilogue text once per transition.
    rocprofvis_profiler_state_t m_last_seen_state;

    // Backend system
    std::vector<std::unique_ptr<IProfilerBackend>> m_backends;
    int m_backend_index;

    // Config
    LaunchConfig m_config;
    ExecutionCache m_execution_cache;
    ToolPathCache  m_tool_path;
    // Rebuild m_execution_cache (SaveSettings/flatten/preview - all allocating)
    // only when a control reported an actual change, not every frame. Also set
    // on open / backend switch / preset load.
    bool m_execution_cache_dirty = true;

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
