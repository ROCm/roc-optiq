#pragma once
#include "imgui.h"

class HomeScreen
{
public:
    void Render();
    HomeScreen();
    ~HomeScreen();

private:
    ImVec2 m_sidepanel_size;
    ImVec2 m_main_screen_size;
    ImVec2 m_analysis_screen_size;
 

    bool m_view_initialized; 
};