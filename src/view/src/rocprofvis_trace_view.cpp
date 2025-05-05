#pragma once
#include "rocprofvis_trace_view.h"
#include "imgui.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_sidebar.h"
#include "rocprofvis_timeline_view.h"
#include "spdlog/spdlog.h"
using namespace RocProfVis::View;

TraceView::TraceView()
: m_main_view(nullptr)
, m_sidebar(nullptr)
, m_container(nullptr)
, m_view_created(false)
, m_open_loading_popup(false)
, m_analysis(nullptr)
{
    m_data_provider.SetTrackDataReadyCallback([](uint64_t track_index) {
        std::shared_ptr<TrackDataEvent> e = std::make_shared<TrackDataEvent>(
            static_cast<int>(RocEvents::kNewTrackData), track_index);
        EventManager::GetInstance()->AddEvent(e);
    });
}

TraceView::~TraceView() { m_data_provider.SetTrackDataReadyCallback(nullptr); }

void
TraceView::Update()
{
    auto last_state = m_data_provider.GetState();
    m_data_provider.Update();

    if(!m_view_created)
    {
        CreateView();
        m_view_created = true;
    }

    auto new_state = m_data_provider.GetState();

    // new file loaded
    if(last_state != new_state && new_state == ProviderState::kReady)
    {
        if(m_main_view)
        {
            m_main_view->MakeGraphView();
            if(m_sidebar)
            {
                m_sidebar->SetGraphMap(m_main_view->GetGraphMap());
            }
        }
    }

    if(m_main_view)
    {
        m_main_view->Update();
    }
}

void
TraceView::CreateView()
{
    m_sidebar   = std::make_shared<SideBar>(m_data_provider);
    m_main_view = std::make_shared<TimelineView>(m_data_provider);
    m_analysis  = std::make_shared<AnalysisView>(m_data_provider);

    LayoutItem left;
    left.m_item = m_sidebar;

    LayoutItem top;
    top.m_item = m_main_view;

    LayoutItem bottom;
    bottom.m_item = std::make_shared<RocWidget>();  // Analysis view, empty for now

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
TraceView::DestroyView()
{
    m_main_view    = nullptr;
    m_sidebar      = nullptr;
    m_container    = nullptr;
    m_analysis     = nullptr;
    m_view_created = false;
}

bool
TraceView::OpenFile(const std::string& file_path)
{
    bool result = false;
    result      = m_data_provider.FetchTrace(file_path);
    if(result)
    {
        m_open_loading_popup = true;
        if(m_view_created)
        {
            m_main_view->ResetView();
            m_main_view->DestroyGraphs();
        }
    }
    return result;
}

void
TraceView::Render()
{
    if(m_container && m_data_provider.GetState() == ProviderState::kReady)
    {
        m_container->Render();
        //Use global DPI to adjust font. Reactivate later. 
        //ImGui::GetIO().FontGlobalScale = Settings::GetInstance().GetDPI() -
        //                                 0.20;  // Scale font by DPI. -0.20 should be
        //                                        // removed once font lib is in place.
        return;
    }

    if(m_open_loading_popup)
    {
        ImGui::OpenPopup("Loading");
        m_open_loading_popup = false;
    }

    if(m_data_provider.GetState() == ProviderState::kLoading)
    {
        ImGui::SetNextWindowSize(ImVec2(300, 200));
        if(ImGui::BeginPopupModal("Loading"))
        {
            ImGui::Text("Please wait...");
            ImGui::EndPopup();
        }
    }
    else
    {
        ImGui::CloseCurrentPopup();
    }
}
