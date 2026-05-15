// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_panel.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_image_helpers.h"
#include "rocprofvis_view_module.h"
#include "widgets/rocprofvis_split_containers.h"
#include "widgets/rocprofvis_tab_container.h"

#include <atomic>
#include <chrono>
#include <future>
#include <thread>
#include <vector>

namespace RocProfVis
{
namespace View
{

class ConfirmationDialog;
class MessageDialog;
class Project;

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

    Project* GetProject(const std::string& id);
    Project* GetCurrentProject();

    void OpenFile(std::string file_path);

    void ShowCloseConfirm();
    
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
    void RenderEmptyState();
    void RenderStatusBar();
    void UpdateStatusBar();

    void HandleTabClosed(std::shared_ptr<RocEvent> e);
    void HandleTabSelectionChanged(std::shared_ptr<RocEvent> e);
    void HandleOpenFile();
    void HandleOpenRecentFile(const std::string& file_path);
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
                              bool                             save_dialog);
#endif
    void ShowImGuiFileDialog(const std::string&             title,
                        const std::vector<FileFilter>& file_filters,
                        const std::string& initial_path, const bool& confirm_overwrite,
                        std::function<void(std::string)> callback);
    static AppWindow* s_instance;

    std::shared_ptr<VFixedContainer> m_main_view;
    std::shared_ptr<TabContainer>    m_tab_container;
    EmbeddedImage                    m_amd_logo_light;
    EmbeddedImage                    m_amd_logo_dark;

    ImVec2 m_default_padding;
    ImVec2 m_default_spacing;

    std::unordered_map<std::string, std::unique_ptr<Project>> m_projects;

    EventManager::SubscriptionToken m_tabclosed_event_token;
    EventManager::SubscriptionToken m_tabselected_event_token;

#ifdef ROCPROFVIS_DEVELOPER_MODE
    void RenderDebugOuput();
    void RenderDeveloperMenu();

    bool         m_show_metrics;
    bool         m_show_debug_window;
    DataProvider m_test_data_provider;
    bool         m_show_provider_test_widow;
#endif
    bool m_open_about_dialog;
    bool m_disable_app_interaction;
    bool m_shutdown_requested;
    bool m_exit_notification_sent;

    rocprofvis_view_file_dialog_preference_t m_file_dialog_preference;

    // Decided at Init() time; can be downgraded to false if NFD_Init fails at
    // runtime. Atomic because the async native-dialog lambda can flip it.
    std::atomic<bool>                m_use_native_file_dialog;

    bool                             m_init_file_dialog;
#ifdef ROCPROFVIS_HAVE_NATIVE_FILE_DIALOG
    std::atomic<bool>                m_is_native_file_dialog_open;
    std::future<std::string>         m_file_dialog_future;
#endif

    std::function<void(std::string)>    m_file_dialog_callback;
    std::unique_ptr<ConfirmationDialog> m_confirmation_dialog;
    std::unique_ptr<MessageDialog>      m_message_dialog;
    std::unique_ptr<SettingsPanel>      m_settings_panel;

    int                              m_tool_bar_index;
    std::function<void(int)>         m_notification_callback;
    bool                             m_is_fullscreen;
    bool                             m_restore_fullscreen_later;
    std::vector<ProviderCleanupJob>  m_provider_cleanup_jobs;
    uint64_t                         m_next_provider_cleanup_id;

    std::string m_status_message;
    bool        m_status_show_busy_indicator;
};

}  // namespace View
}  // namespace RocProfVis
