// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_compare_files_dialog.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_panel.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_image_helpers.h"
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_split_containers.h"
#include "widgets/rocprofvis_tab_container.h"
// TEMPORARY (remote/SSH): the SSH test dialog is a remote-only dev aid.
// Remove this guard when the remote feature graduates.
#ifdef ROCPROFVIS_ENABLE_REMOTE
#include "remote/rocprofvis_ssh_test_dialog.h"
#endif

#include <atomic>
#include <chrono>
#include <future>
#include <memory>
#include <thread>
#include <vector>

namespace RocProfVis
{
namespace View
{

class ConfirmationDialog;
class MessageDialog;
#ifdef ROCPROFVIS_ENABLE_PROFILER
class ProfilerLauncherDialog;  // TEMPORARY (profiler launch)
#endif
class Project;
class WelcomePage;

struct FileFilter
{
    std::string m_name;
    std::vector<std::string> m_extensions;
};

class AppWindow : public RocWidget
{
public:
    static AppWindow* GetInstance();
    static void       DestroyInstance();

    bool Init();
    void SetNotificationCallback(std::function<void(int)> callback);

    void Render() override;
    void Update() override;

    // True when async work, queued events, or animations require redraws even
    // without OS input. Drives the lazy render loop in the app shell.
    bool WantsContinuousRender();

    const std::string& GetMainTabSourceName() const;
    void               SetTabLabel(const std::string& label, const std::string& id);

    void ShowConfirmationDialog(const std::string& title, const std::string& message,
                                std::function<void()> on_confirm_callback) const;
    void ShowMessageDialog(const std::string& title, const std::string& message) const;

    void ShowSaveFileDialog(const std::string&               title,
                            const std::vector<FileFilter>&   file_filters,
                            const std::string&               initial_path,
                            std::function<void(std::string)> callback);

    void ShowOpenFileDialog(const std::string&               title,
                            const std::vector<FileFilter>&   file_filters,
                            const std::string&               initial_path,
                            std::function<void(std::string)> callback);

    // Like ShowOpenFileDialog but lets the user select multiple files at once. The
    // callback receives all chosen paths (empty vector if the dialog was cancelled).
    void ShowOpenFilesDialog(
        const std::string&                                   title,
        const std::vector<FileFilter>&                       file_filters,
        const std::string&                                   initial_path,
        std::function<void(const std::vector<std::string>&)> callback);

    void ShowPathPickerDialog(const std::string&               title,
                            const std::string&               initial_path,
                            std::function<void(std::string)> callback);
                            
    Project* GetProject(const std::string& id);
    Project* GetCurrentProject();
    // Returns the open project that has `file_path` among its source files (canonical match),
    // or nullptr. Finds merged constituents that GetProject (primary-id only) would miss.
    Project* FindProjectContainingSource(const std::string& file_path);

    void OpenFile(std::string file_path);

    // Opens two trace files as a single compare project (combined timeline, A/B tags).
    void OpenCompare(const std::string& first_file, const std::string& second_file);

    // Opens N (>= 2) trace files as a single combined project (one timeline, A/B/C...
    // tags). Reuses the multinode/compare engine. Switches to the tab if already open.
    void OpenCompare(const std::vector<std::string>& files);

    // Opens N (>= 1) trace files merged into a single unified view, like a yaml manifest
    // (parts of one program's run). No compare A/B tagging. One file opens normally.
    void OpenCombined(const std::vector<std::string>& files);

    // Stable, file-derived project id/key for a compare of the given source files.
    // Used as the tab id and the m_projects key for both fresh and reopened compares.
    static std::string MakeCompareId(const std::vector<std::string>& files);

    // Stable, file-derived id/key for a merged (multi-file) trace view.
    static std::string MakeCombinedId(const std::vector<std::string>& files);

    // "<scheme><file0>|<file1>|..." - the shared id form for compare/combined views.
    static std::string JoinFileListId(const char*                     scheme,
                                      const std::vector<std::string>& files);
    // Add a tab for the project, make it active, and take ownership in m_projects.
    void RegisterAndActivateProject(std::unique_ptr<Project> project);

    void ShowCloseConfirm();
#ifdef ROCPROFVIS_ENABLE_PROFILER
    void ShowProfilerLauncher();  // TEMPORARY (profiler launch)
#endif

    void SetFullscreenState(bool is_fullscreen);
    bool GetFullscreenState() const;

    void SetFileDialogPreference(rocprofvis_view_file_dialog_preference_t pref);

private:
    enum class ProviderCleanupReason
    {
        kTabClose,
        kAppShutdown
    };

    struct ProviderCleanupJob
    {
        std::string                            label;
        std::string                            notification_id;
        ProviderCleanupReason                  reason;
        std::future<DataProviderCleanupResult> future;
    };

    AppWindow();
    ~AppWindow();

    void RenderDisableScreen();
    void RenderShutdownState();
    void RenderFileMenu(Project* project);
    void RenderEditMenu(Project* project);
    void RenderViewMenu(Project* project);
    void RenderHelpMenu();

    void RenderFileDialog();
    void RenderAboutDialog();
    void RenderStatusBar();
    void UpdateStatusBar();

    void HandleTabClosed(std::shared_ptr<RocEvent> e);
    void HandleTabSelectionChanged(std::shared_ptr<RocEvent> e);
    void HandleFontChanged();
    void HandleOpenFile();
    // Picks another trace file and opens a combined view of the current system view's
    // source file(s) plus the newly picked one (adds a trace into the same kind of view).
    void AddTraceToCurrentView();
    // Removes one source file from the current merged view: reopens the remaining subset
    // (or the single remaining trace) and closes the previous merged tab.
    void RemoveTraceFromView(const std::string& file_to_remove);
    // Wire a provider's add/remove-trace-source completion to commit the project's source list
    // + tab label on success, or surface an error dialog on failure - only once the async op
    // has finished (so a failed add/remove never leaves a phantom source or wrong tab name).
    void WireSourceMutationCallback(DataProvider* provider, const std::string& project_id);
    void HandleCompareFiles();
    void HandleCompareFileBrowse(CompareFilesDialog::FileSlot slot);
    void HandleSaveAsFile();
    void ConfigureFileDialogBackend();
    void BeginAppShutdown();
    void DetachProjectProviderCleanup(Project& project, ProviderCleanupReason reason);
    void StartProviderCleanup(DataProviderCleanupWork cleanup_work,
                              const std::string&    label,
                              ProviderCleanupReason reason);
    void UpdateProviderCleanups();
    void RequestExitIfProviderCleanupsComplete();


#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    void UpdateNativeFileDialog();

    void ShowNativeFileDialog(const std::vector<FileFilter>&   file_filters,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback,
                              bool                             save_dialog,
                              bool                             path_picker = false);

    void ShowNativeFilesDialog(
        const std::vector<FileFilter>&                       file_filters,
        const std::string&                                   initial_path,
        std::function<void(const std::vector<std::string>&)> callback);
#endif
    void ShowImGuiFileDialog(const std::string&             title,
                        const std::vector<FileFilter>& file_filters,
                        const std::string& initial_path, const bool& confirm_overwrite,
                        std::function<void(std::string)> callback,
                        bool                             folder_mode  = false,
                        bool                             multi_select = false);
    static AppWindow* s_instance;

    std::shared_ptr<VFixedContainer> m_main_view;
    std::shared_ptr<TabContainer>    m_tab_container;

    ImVec2 m_default_padding;
    ImVec2 m_default_spacing;

    std::unordered_map<std::string, std::unique_ptr<Project>> m_projects;

    EventManager::SubscriptionToken m_tabclosed_event_token;
    EventManager::SubscriptionToken m_tabselected_event_token;
    EventManager::SubscriptionToken m_font_changed_token;

#ifdef ROCPROFVIS_DEVELOPER_MODE
    void RenderDebugOuput();
    void RenderDeveloperMenu();
#ifdef ROCPROFVIS_ENABLE_REMOTE
    void HandleTestRemoteSSH();
#endif
    bool         m_show_metrics;
    bool         m_show_debug_window;
    DataProvider m_test_data_provider;
    bool         m_show_provider_test_widow;
#endif
    bool m_open_about_dialog;
    bool m_disable_app_interaction;
    bool m_shutdown_requested;
    bool m_exit_notification_sent;
    // Set when BeginAppShutdown() is first called. Bounds how long the exit gate
    // waits for AppMonitor operations to drain so a stuck future cannot pin the
    // app on the shutdown screen forever.
    std::chrono::steady_clock::time_point m_shutdown_start;

    rocprofvis_view_file_dialog_preference_t m_file_dialog_preference;

    // Decided at Init() time; can be downgraded to false if NFD_Init fails at
    // runtime. Atomic because the async native-dialog lambda can flip it.
    std::atomic<bool>                m_use_native_file_dialog;

    bool                             m_init_file_dialog;
    // Only used by the ImGuiFileDialog backend (not the native dialog): its directory
    // mode returns the result via GetCurrentPath() instead of GetFilePathName(), so the
    // shared ImGui callback site needs to know which to read.
    bool                             m_imgui_file_dialog_folder_mode = false;
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    std::atomic<bool>                m_is_native_file_dialog_open;
    std::future<std::string>         m_file_dialog_future;
    std::future<std::vector<std::string>> m_files_dialog_future;
#endif

    // True while the active open dialog is in multi-select mode; routes the result to
    // m_files_dialog_callback instead of m_file_dialog_callback.
    bool                                                 m_file_dialog_is_multi = false;
    std::function<void(const std::vector<std::string>&)> m_files_dialog_callback;
    std::function<void(std::string)>    m_file_dialog_callback;
    std::unique_ptr<ConfirmationDialog> m_confirmation_dialog;
    std::unique_ptr<MessageDialog>      m_message_dialog;
    std::unique_ptr<CompareFilesDialog> m_compare_files_dialog;
    std::unique_ptr<SettingsPanel>      m_settings_panel;
    std::unique_ptr<WelcomePage>        m_welcome_page;

#ifdef ROCPROFVIS_ENABLE_PROFILER
    std::unique_ptr<ProfilerLauncherDialog> m_profiler_launcher_dialog;  // TEMPORARY (profiler launch)
#endif

    int                              m_tool_bar_index;
    std::function<void(int)>         m_notification_callback;
    bool                             m_is_fullscreen;
    bool                             m_restore_fullscreen_later;
    std::vector<ProviderCleanupJob>  m_provider_cleanup_jobs;
    uint64_t                         m_next_provider_cleanup_id;
#ifdef ROCPROFVIS_ENABLE_REMOTE
    std::unique_ptr<SshTestDialog>   m_ssh_test_dialog;
#endif

    std::string m_status_message;
    bool        m_status_show_busy_indicator;
};

}  // namespace View
}  // namespace RocProfVis
