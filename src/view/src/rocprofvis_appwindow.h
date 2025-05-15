// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_compute_root.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_trace_view.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class AppWindow : public RocWidget
{
public:
    static AppWindow* GetInstance();
    static void       DestroyInstance();

    bool Init();
    void Render() override;
    void Update() override;

private:
    AppWindow();
    ~AppWindow();

    void RenderDebugOuput();
    void RenderSettingsMenu();    
    void HandleTabClosed(std::shared_ptr<RocEvent> e);

    static AppWindow* s_instance;

    std::shared_ptr<RocWidget>    m_main_view;
    std::shared_ptr<TabContainer> m_tab_container;

    bool         m_show_debug_widow;
    DataProvider m_data_provider;
    bool         m_show_provider_test_widow;

    ImVec2 m_default_padding;
    ImVec2 m_default_spacing;

    std::map<std::string, TabItem> m_open_views;

    EventManager::SubscriptionToken m_tabclosed_event_token;
};

}  // namespace View
}  // namespace RocProfVis
