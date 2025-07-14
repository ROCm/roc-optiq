#pragma once
#include "rocprofvis_trace_view.h"
#include "imgui.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_sidebar.h"
#include "rocprofvis_timeline_view.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"

using namespace RocProfVis::View;

TraceView::TraceView()
: m_timeline_view(nullptr)
, m_sidebar(nullptr)
, m_container(nullptr)
, m_view_created(false)
, m_open_loading_popup(false)
, m_analysis(nullptr)
{
    m_data_provider.SetTrackDataReadyCallback(
        [](uint64_t track_id, const std::string& trace_path) {
            std::shared_ptr<TrackDataEvent> e = std::make_shared<TrackDataEvent>(
                static_cast<int>(RocEvents::kNewTrackData), track_id, trace_path);
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
        if(m_timeline_view)
        {
            m_timeline_view->MakeGraphView();
            if(m_sidebar)
            {
                m_sidebar->SetGraphs(m_timeline_view->GetGraphs());
            }
        }
    }

    if(m_timeline_view)
    {
        m_timeline_view->Update();
    }
    if(m_analysis)
    {
        m_analysis->Update();
    }
    if(m_sidebar)
    {
        m_sidebar->Update();
    }
}

void
TraceView::CreateView()
{
    m_sidebar       = std::make_shared<SideBar>(m_data_provider);
    m_timeline_view = std::make_shared<TimelineView>(m_data_provider);
    m_analysis      = std::make_shared<AnalysisView>(m_data_provider);

    LayoutItem left;
    left.m_item = m_sidebar;

    LayoutItem top;
    top.m_item = m_timeline_view;

    LayoutItem bottom;
    bottom.m_item = m_analysis;

    LayoutItem traceArea;
    auto       split_container = std::make_shared<VSplitContainer>(top, bottom);
    split_container->SetSplit(0.75);
    traceArea.m_item     = split_container;
    traceArea.m_bg_color = IM_COL32(255, 255, 255, 255);

    m_container = std::make_shared<HSplitContainer>(left, traceArea);
    m_container->SetSplit(0.2);
    m_container->SetMinRightWidth(400);
}

void
TraceView::DestroyView()
{
    m_timeline_view = nullptr;
    m_sidebar       = nullptr;
    m_container     = nullptr;
    m_analysis      = nullptr;
    m_view_created  = false;
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
            m_timeline_view->ResetView();
            m_timeline_view->DestroyGraphs();
        }
    }
    return result;
}

void
TraceView::Render()
{
    if(m_container && m_data_provider.GetState() == ProviderState::kReady)
    {
        // Use global DPI to adjust font. Reactivate later.
        ImGui::GetIO().FontGlobalScale = Settings::GetInstance().GetDPI() -
                                         0.20;  // Scale font by DPI. -0.20 should be
                                                // removed once font lib is in place.
        m_container->Render();
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
            const char* label      = "Please wait...";
            ImVec2      label_size = ImGui::CalcTextSize(label);

            int item_spacing = 10.0f;

            float dot_radius  = 5.0f;
            int   num_dots    = 3;
            float dot_spacing = 5.0f;
            float anim_speed  = 5.0f;

            ImVec2 dot_size =
                MeasureLoadingIndicatorDots(dot_radius, num_dots, dot_spacing);

            ImVec2 available_space = ImGui::GetContentRegionAvail();
            ImVec2 pos             = ImGui::GetCursorScreenPos();
            ImVec2 center_pos      = ImVec2(
                pos.x + (available_space.x - label_size.x) * 0.5f,
                pos.y + (available_space.y - (label_size.y + dot_size.y + item_spacing)) *
                            0.5f);
            ImGui::SetCursorScreenPos(center_pos);

            ImGui::Text(label);

            pos            = ImGui::GetCursorScreenPos();
            ImVec2 dot_pos = ImVec2(pos.x + (available_space.x - dot_size.x) * 0.5f,
                                    pos.y + item_spacing);
            ImGui::SetCursorScreenPos(dot_pos);

            RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing,
                                       IM_COL32(85, 85, 85, 255), anim_speed);
            ImGui::EndPopup();
        }
    }
    else
    {
        ImGui::CloseCurrentPopup();
    }
}
