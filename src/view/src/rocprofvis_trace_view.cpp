#include "rocprofvis_trace_view.h"
#include "imgui.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings.h"
#include "rocprofvis_sidebar.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_track_topology.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_dialog.h"

using namespace RocProfVis::View;

TraceView::TraceView()
: m_timeline_view(nullptr)
, m_sidebar(nullptr)
, m_container(nullptr)
, m_view_created(false)
, m_open_loading_popup(false)
, m_analysis(nullptr)
, m_timeline_selection(nullptr)
, m_track_topology(nullptr)
, m_popup_info({false, "", ""})
, m_message_dialog(std::make_unique<MessageDialog>())
, m_tabselected_event_token(-1)
, m_event_selection_changed_event_token(-1)
{
    m_data_provider.SetTrackDataReadyCallback(
        [](uint64_t track_id, const std::string& trace_path) {
            std::shared_ptr<TrackDataEvent> e = std::make_shared<TrackDataEvent>(
                static_cast<int>(RocEvents::kNewTrackData), track_id, trace_path);
            EventManager::GetInstance()->AddEvent(e);
        });

    auto new_tab_selected_handler = [this](std::shared_ptr<RocEvent> e) {
        auto ets = std::dynamic_pointer_cast<TabEvent>(e);
        if(ets)
        {
            //Only handle the event if the tab source is the main tab source
            if(ets->GetTabSource() == AppWindow::GetInstance()->GetMainTabSourceName())
            {
                m_data_provider.SetSelectedState(ets->GetTabId());
            }
        }
    };

    m_data_provider.SetTraceLoadedCallback([this](const std::string& trace_path,
                                                  uint64_t response_code) {
        if(response_code != kRocProfVisResultSuccess)
        {
            spdlog::error("Failed to load trace: {}", response_code);
            if(m_message_dialog)
            {
                m_popup_info.show_popup = true;
                m_popup_info.title = "Error";
                m_popup_info.message = "Failed to load trace: " + trace_path;
            }
        }
    });

    m_data_provider.SetSaveTraceCallback(
        [this](bool success) {        
            m_popup_info.show_popup = true;
            m_popup_info.title = "Save Trimmed Trace";
            if(success)
            {
                m_popup_info.message = "Trimmed trace has been saved successfully.";
            }
            else
            {
                m_popup_info.message = "Failed to save the trimmed trace.";
            }
        });

    auto event_selection_handler = [this](std::shared_ptr<RocEvent> e) {
        std::shared_ptr<EventSelectionChangedEvent> event =
            std::dynamic_pointer_cast<EventSelectionChangedEvent>(e);
        if(event)
        {
            if(event->EventSelected())
            {
                m_data_provider.FetchEvent(event->GetEventTrackID(),
                                               event->GetEventID());
            }
            else
            {
                m_data_provider.FreeEvent(event->GetEventID());
            }
        }
    };

    m_tabselected_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabSelected), new_tab_selected_handler);

    m_event_selection_changed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        event_selection_handler);

    m_widget_name = GenUniqueName("TraceView");
}

TraceView::~TraceView()
{
    m_data_provider.SetTrackDataReadyCallback(nullptr);
    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabSelected),
                                             m_tabselected_event_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        m_event_selection_changed_event_token);
}

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
        }
    }

    if(m_timeline_view)
    {
        m_timeline_view->Update();
    }
    if(m_track_topology)
    {
        m_track_topology->Update();
    }
    if(m_analysis)
    {
        m_analysis->Update();
    }
    if(m_timeline_selection)
    {
        m_timeline_selection->Update();
    }
}

void
TraceView::CreateView()
{
    m_timeline_selection = std::make_shared<TimelineSelection>();
	m_track_topology = std::make_shared<TrackTopology>(m_data_provider);
    m_timeline_view  = std::make_shared<TimelineView>(m_data_provider, m_timeline_selection);
    m_sidebar = std::make_shared<SideBar>(m_track_topology, m_timeline_selection, m_timeline_view->GetGraphs()); 	
    m_analysis = std::make_shared<AnalysisView>(m_data_provider, m_track_topology,
                                                m_timeline_selection);

    LayoutItem left;
    left.m_item = m_sidebar;
    left.m_window_flags = ImGuiWindowFlags_HorizontalScrollbar;

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
    m_container->SetSplit(0.2f);
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
 
        m_container->Render();
    }

    if(m_popup_info.show_popup)
    {
        m_popup_info.show_popup = false;
        if(m_message_dialog)
        {
            m_message_dialog->Show(m_popup_info.title, m_popup_info.message);
        }
    }

    if(m_message_dialog)
    {
        m_message_dialog->Render();
    }

    if(m_data_provider.GetState() == ProviderState::kLoading)
    {
        if(m_open_loading_popup)
        {
            ImGui::OpenPopup("Loading");
            m_open_loading_popup = false;
        }

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
}

bool TraceView::HasTrimActiveTrimSelection() const
{
    if(m_timeline_selection)
    {
        return m_timeline_selection->HasValidTimeRangeSelection();
    }
    return false;
}

bool TraceView::IsTrimSaveAllowed() const {
    bool save_allowed = false;
    if(m_timeline_selection)
    {
        save_allowed = m_timeline_selection->HasValidTimeRangeSelection();
        save_allowed &= !m_data_provider.IsRequestPending(DataProvider::SAVE_TRIMMED_TRACE_REQUEST_ID);
    }

    return save_allowed;
}

bool TraceView::SaveSelection(const std::string& file_path) {
    if(m_timeline_selection)
    {
        if(!m_timeline_selection->HasValidTimeRangeSelection()) {
            spdlog::warn("No valid time range selection to save.");
            return false;
        }

        double start_ns = 0.0;
        double end_ns = 0.0;
        m_timeline_selection->GetSelectedTimeRange(start_ns, end_ns);

        m_data_provider.SaveTrimmedTrace(file_path, start_ns, end_ns);
        return true;

    } else {
        spdlog::warn("Timeline selection is not initialized.");
    }
    return false;
}
