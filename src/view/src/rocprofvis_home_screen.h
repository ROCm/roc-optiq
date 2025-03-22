#pragma once
#include "imgui.h"
#include "rocprofvis_structs.h"
#include <map>
#include "rocprofvis_main_view.h"

#include <vector>
class HomeScreen
{
public:
    void Render(std::map<std::string, rocprofvis_trace_process_t>& trace_data);
    HomeScreen();
    ~HomeScreen();

private:
    ImVec2 m_sidepanel_size;
    ImVec2 m_main_screen_size;
    ImVec2 m_analysis_screen_size;
    RocProfVis::View::MainView* m_main_view; 
    bool   m_view_initialized;
};