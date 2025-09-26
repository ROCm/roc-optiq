// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_panel.h"
#include "widgets/rocprofvis_widget.h"


namespace RocProfVis
{
namespace View
{

class ConfirmationDialog;
class MessageDialog;
class Project;

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
    void SetTabLabel(const std::string& label, const std::string& id);

    void ShowConfirmationDialog(const std::string& title, const std::string& message,
                                std::function<void()> on_confirm_callback) const;
    void ShowMessageDialog(const std::string& title, const std::string& message) const;

    Project* GetProject(const std::string& id);
    Project* GetCurrentProject();

    void OpenFile(std::string file_path);
    
    void ShowCloseConfirm();

private:
    AppWindow();
    ~AppWindow();

    void RenderFileMenu(Project* project);
    void RenderEditMenu(Project* project);
    void RenderViewMenu(Project* project);
    void RenderHelpMenu();

    void RenderFileDialogs();
    void RenderAboutDialog();

    void HandleTabClosed(std::shared_ptr<RocEvent> e);
    void HandleTabSelectionChanged(std::shared_ptr<RocEvent> e);
    
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
    bool m_analysis_bar_visible = true;
    bool m_sidebar_visible = true;

    std::unique_ptr<ConfirmationDialog> m_confirmation_dialog;
    std::unique_ptr<MessageDialog>      m_message_dialog;
    std::unique_ptr<SettingsPanel>      m_settings_panel;

    size_t m_tool_bar_index;
    std::function<void(int)> m_notification_callback;
};

}  // namespace View
}  // namespace RocProfVis
