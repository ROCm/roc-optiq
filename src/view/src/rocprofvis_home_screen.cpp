#pragma once
#include "rocprofvis_home_screen.h"
#include "imgui.h"
#include "rocprofvis_main_view.h"
#include "rocprofvis_sidebar.h"

using namespace RocProfVis::View;

HomeScreen::HomeScreen()
: m_main_view(nullptr)
, m_sidebar(nullptr)
{
    m_sidebar   = std::make_shared<SideBar>();
    m_main_view = std::make_shared<RocProfVis::View::MainView>();

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
    traceArea.m_item = std::make_shared<VSplitContainer>(top, bottom);
    //traceArea.m_bg_color = IM_COL32(255, 255, 255, 255);

    m_container = std::make_shared<HSplitContainer>(left, traceArea);
    m_container->SetMinRightWidth(400);
}

HomeScreen::~HomeScreen() {}

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

void
HomeScreen::Render()
{
    if(m_container)
    {
        m_container->Render();
        return;
    }
}