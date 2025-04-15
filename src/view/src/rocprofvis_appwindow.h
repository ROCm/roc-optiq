// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_home_screen.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class AppWindow : public RocWidget
{
public:
    static AppWindow* GetInstance();
    static void DestroyInstance();

    bool         Init();
    void Render() override;

    void Update();

private:
    AppWindow();
    ~AppWindow();

    void RenderDebugOuput();

    static AppWindow* s_instance;

    std::shared_ptr<HomeScreen> m_home_screen;
    std::shared_ptr<RocWidget>  m_main_view;

    bool         m_show_debug_widow;
    DataProvider m_data_provider;
    bool         m_show_provider_test_widow;
};

}  // namespace View
}  // namespace RocProfVis
