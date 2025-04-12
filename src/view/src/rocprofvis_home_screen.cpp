#pragma once
#include "rocprofvis_home_screen.h"
#include "imgui.h"
#include "rocprofvis_main_view.h"
#include "rocprofvis_sidebar.h"

using namespace RocProfVis::View;

HomeScreen::HomeScreen()
: m_main_view(nullptr)
, m_sidebar(nullptr)
, m_container(nullptr)
, m_view_created(false)
{}

HomeScreen::~HomeScreen() {}

void
HomeScreen::Update()
{
    m_data_provider.Update();
    if(!m_view_created && m_data_provider.GetState() == ProviderState::kReady)
    {
        CreateView();
        if(m_main_view)
        {
            m_main_view->MakeGraphView();
            if(m_sidebar)
            {
                m_sidebar->SetGraphMap(m_main_view->GetGraphMap());
            }
        }
        m_view_created = true;
    }
}

void
HomeScreen::CreateView()
{
    m_sidebar   = std::make_shared<SideBar>(m_data_provider);
    m_main_view = std::make_shared<MainView>(m_data_provider);

    LayoutItem left;
    left.m_item     = m_sidebar;
    left.m_bg_color = IM_COL32(255, 255, 255, 255);

    LayoutItem top;
    top.m_item     = m_main_view;
    top.m_bg_color = IM_COL32(255, 255, 255, 255);

    LayoutItem bottom;
    bottom.m_item     = std::make_shared<RocWidget>();  // Analysis view, empty for now
    bottom.m_bg_color = IM_COL32(255, 255, 255, 255);

    LayoutItem traceArea;
    auto       split_container = std::make_shared<VSplitContainer>(top, bottom);
    split_container->SetSplit(0.75);
    traceArea.m_item = split_container;
    // traceArea.m_bg_color = IM_COL32(255, 255, 255, 255);

    m_container = std::make_shared<HSplitContainer>(left, traceArea);
    m_container->SetSplit(0.2);
    m_container->SetMinRightWidth(400);
}

void
HomeScreen::DestroyView()
{
    m_main_view    = nullptr;
    m_sidebar      = nullptr;
    m_container    = nullptr;
    m_view_created = false;
}

bool
HomeScreen::OpenFile(const std::string& file_path)
{
    return m_data_provider.FetchTrace(file_path);
}

/*
void
HomeScreen::SetData(rocprofvis_controller_timeline_t* trace_timeline,
                    rocprofvis_controller_array_t*    graph_data_array)
{
    if(m_main_view)
    {
        m_main_view->MakeGraphView(trace_timeline, graph_data_array, 1);
        if(m_sidebar)
        {
            m_sidebar->SetGraphMap(m_main_view->GetGraphMap());
        }
    }
}
*/

void
HomeScreen::Render()
{
    if(m_container)
    {
        m_container->Render();
        return;
    }

    // if(!m_is_trace_loaded)
    // {
    //     if(ImGui::BeginPopupModal("Loading"))
    //     {
    //         ImGui::Text("Please wait...");
    //         ImGui::EndPopup();
    //     }

    //     if(m_is_loading_trace)
    //     {
    //         ImGui::SetNextWindowSize(ImVec2(300, 200));
    //         ImGui::OpenPopup("Loading");
    //     }
    // }
}
