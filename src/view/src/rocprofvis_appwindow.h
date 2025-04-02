// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "widgets/rocprofvis_widget.h"
#include "rocprofvis_controller.h"
#include "rocprofvis_home_screen.h"

namespace RocProfVis
{
namespace View
{

class AppWindow : public RocWidget
{
public:
    static AppWindow* getInstance();
    ~AppWindow();

    bool         Init();
    virtual void Render();

private:
    AppWindow();
    
    void handleOpenFile(std::string& file_path);

    static AppWindow* m_instance;

    std::shared_ptr<HomeScreen> m_home_screen;
    std::shared_ptr<RocWidget>  m_main_view;
    bool                        m_is_loading_trace;
    bool                        m_data_changed;
    bool                        m_is_trace_loaded;

    rocprofvis_controller_future_t*   m_trace_future;
    rocprofvis_controller_t*          m_trace_controller;
    rocprofvis_controller_timeline_t* m_trace_timeline;
    rocprofvis_controller_array_t*    m_graph_data_array;
    rocprofvis_controller_array_t*    m_graph_futures;
};

}  // namespace View
}  // namespace RocProfVis
