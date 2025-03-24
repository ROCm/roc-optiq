#pragma once
#include "rocprofvis_home_screen.h"
#include "imgui.h"
#include "rocprofvis_main_view.h"
#include "rocprofvis_sidebar.h"
#include <iostream>

float
clamp(float input, float max, float min)
{
    if(input > max)
    {
        return max;
    }
    else if(input < min)
    {
        return min;
    }
    else
    {
        return input;
    }
}

HomeScreen::HomeScreen()
: m_sidepanel_size()
, m_main_screen_size()
, m_analysis_screen_size()
, m_view_initialized(false)
, m_main_view(new RocProfVis::View::MainView())
, m_window_size_previous(100.0f)
, m_graph_map(nullptr)
, m_sidebar(SideBar())
{}

HomeScreen::~HomeScreen() {}

void
HomeScreen::Render(std::map<std::string, rocprofvis_trace_process_t>& trace_data)
{
    ImVec2 window_size = ImGui::GetIO().DisplaySize;
    window_size.y      = window_size.y - 20.0f;

    if(!m_view_initialized || m_window_size_previous != window_size.x)
    {
        m_window_size_previous = window_size.x;
        ImVec2 window_size     = ImGui::GetIO().DisplaySize;
        m_sidepanel_size =
            ImVec2(clamp(window_size.x * 0.3f, 400.0f, 500.0f), window_size.y);
        m_main_screen_size =
            ImVec2(window_size.x - m_sidepanel_size.x, window_size.y * 0.8f);
        m_analysis_screen_size =
            ImVec2(window_size.x - m_sidepanel_size.x, window_size.y * 0.2f);
    }

    // Sidebar View
    ImGui::SetNextWindowPos(ImVec2(0, 20));
    ImGui::SetNextWindowSize(m_sidepanel_size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if(ImGui::Begin("Sidebar", nullptr))
    {
        float current_main_view_size = ImGui::GetWindowSize().x;
        if(m_sidepanel_size.x != current_main_view_size)
        {
            m_sidepanel_size = ImVec2(current_main_view_size, m_sidepanel_size.y);
            m_main_screen_size =
                ImVec2(window_size.x - m_sidepanel_size.x, window_size.y * 0.7f);
            m_analysis_screen_size =
                ImVec2(window_size.x - m_sidepanel_size.x, window_size.y * 0.7f);
        }

        m_sidebar.ConstructTree(m_graph_map);
    }
    ImGui::End();
    ImGui::PopStyleColor();

    // Main View
    ImGui::SetNextWindowPos(ImVec2(m_sidepanel_size.x, 0));
    ImGui::SetNextWindowSize(m_main_screen_size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.00, 1.0f, 1.00, 1.0f));
    if(ImGui::Begin("Main View", nullptr,
                    ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
    {
        float current_main_view_size = ImGui::GetWindowSize().x;
        if(m_main_screen_size.x != current_main_view_size)
        {
            m_sidepanel_size   = ImVec2(m_sidepanel_size.x -
                                            (current_main_view_size - m_main_screen_size.x),
                                        m_sidepanel_size.y);
            m_main_screen_size = ImVec2(current_main_view_size, m_main_screen_size.y);
            m_analysis_screen_size =
                ImVec2(current_main_view_size, m_analysis_screen_size.y);
        }
        float current_main_view_size_y = ImGui::GetWindowSize().y;
        if(m_main_screen_size.y != current_main_view_size_y)
        {
            m_main_screen_size = ImVec2(m_main_screen_size.x, current_main_view_size_y);
            m_analysis_screen_size = ImVec2(m_analysis_screen_size.x,
                                            window_size.y - current_main_view_size_y);
        }
        m_main_view->GenerateGraphPoints(trace_data);
        m_graph_map = m_main_view->GetGraphMap();
    }
    ImGui::End();
    ImGui::PopStyleColor();

    // Analysis View
    ImGui::SetNextWindowPos(ImVec2(m_sidepanel_size.x, m_main_screen_size.y));
    ImGui::SetNextWindowSize(m_analysis_screen_size);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0, 1.0, 1.0f, 1.0f));
    if(ImGui::Begin("Analysis View", nullptr))
    {
        float current_main_view_size = ImGui::GetWindowSize().x;
        if(m_analysis_screen_size.x != current_main_view_size)
        {
            m_sidepanel_size = ImVec2(
                m_sidepanel_size.x - (current_main_view_size - m_analysis_screen_size.x),
                m_sidepanel_size.y);
            m_main_screen_size = ImVec2(current_main_view_size, m_analysis_screen_size.y);
            m_analysis_screen_size =
                ImVec2(current_main_view_size, m_analysis_screen_size.y);
        }
        float current_main_view_size_y = ImGui::GetWindowSize().y;
        if(m_analysis_screen_size.y != current_main_view_size_y)
        {
            m_main_screen_size =
                ImVec2(m_main_screen_size.x, window_size.y - current_main_view_size_y);
            m_analysis_screen_size =
                ImVec2(m_analysis_screen_size.x, current_main_view_size_y);
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();
    m_view_initialized = true;
}