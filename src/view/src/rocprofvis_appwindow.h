// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_panel.h"
#include "widgets/rocprofvis_widget.h"

#ifdef USE_NATIVE_FILE_DIALOG
#include <atomic>
#include <future>
#include <thread>
#include <chrono>
#endif

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

private:
    AppWindow();
    ~AppWindow();

    void RenderDisableScreen();
    void RenderFileMenu(Project* project);
    void RenderEditMenu(Project* project);
    void RenderViewMenu(Project* project);
    void RenderHelpMenu();

    void RenderFileDialog();
    void RenderAboutDialog();

    void HandleTabClosed(std::shared_ptr<RocEvent> e);
    void HandleTabSelectionChanged(std::shared_ptr<RocEvent> e);
    void HandleOpenFile();
    void HandleSaveAsFile();

#ifdef USE_NATIVE_FILE_DIALOG
    void UpdateNativeFileDialog();

    void ShowNativeFileDialog(const std::vector<FileFilter>&   file_filters,
                              const std::string&               initial_path,
                              std::function<void(std::string)> callback,
                              bool                             save_dialog);
#else
    void ShowImGuiFileDialog(const std::string&             title,
                        const std::vector<FileFilter>& file_filters,
                        const std::string& initial_path, const bool& confirm_overwrite,
                        std::function<void(std::string)> callback);
#endif
    static AppWindow* s_instance;

    std::shared_ptr<VFixedContainer> m_main_view;
    std::shared_ptr<TabContainer>    m_tab_container;

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
    bool m_analysis_bar_visible;
    bool m_sidebar_visible;
    bool m_histogram_visible;
    bool m_disable_app_interaction;

#ifndef USE_NATIVE_FILE_DIALOG
    bool                             m_init_file_dialog;
#else
    std::atomic<bool>                m_is_native_file_dialog_open;
    std::future<std::string>         m_file_dialog_future;
#endif

    std::function<void(std::string)>    m_file_dialog_callback;
    std::unique_ptr<ConfirmationDialog> m_confirmation_dialog;
    std::unique_ptr<MessageDialog>      m_message_dialog;
    std::unique_ptr<SettingsPanel>      m_settings_panel;

    int                              m_tool_bar_index;
    std::function<void(int)>         m_notification_callback;
};

}  // namespace View
}  // namespace RocProfVis
