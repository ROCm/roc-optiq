#pragma once
#include "imgui.h"
#include "rocprofvis_structs.h"
#include <map>
#include "rocprofvis_main_view.h"
#include "rocprofvis_sidebar.h"

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
    float m_window_size_previous; //Here to account for user resizing window. 
    bool   m_view_initialized;
    std::map<int, rocprofvis_graph_map_t>* m_graph_map;
    SideBar m_sidebar; 

};