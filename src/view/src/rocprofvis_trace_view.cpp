// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_trace_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_annotations.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_event_search.h"
#include "rocprofvis_hotkey_manager.h"
#include "rocprofvis_minimap.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_sidebar.h"
#include "rocprofvis_summary_view.h"
#include "rocprofvis_timeline_selection.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_track_topology.h"
#include "spdlog/spdlog.h"
#include "widgets/rocprofvis_dialog.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

TraceView::TraceView()
: m_timeline_view(nullptr)
, m_horizontal_split_container(nullptr)
, m_view_created(false)
, m_show_minimap_popup(false)
, m_timeline_selection(nullptr)
, m_track_topology(nullptr)
, m_popup_info({ false, "", "" })
, m_tabselected_event_token(static_cast<EventManager::SubscriptionToken>(-1))
, m_event_selection_changed_event_token(static_cast<EventManager::SubscriptionToken>(-1))
, m_progress_update_event_token(static_cast<EventManager::SubscriptionToken>(-1))
, m_save_notification_id("")
, m_project_settings(nullptr)
, m_annotations(nullptr)
, m_event_search(nullptr)
, m_summary_view(nullptr)
{
    m_data_provider.SetEventDataReadyCallback([this](uint64_t           event_id,
                                                     const std::string& trace_path,
                                                     bool               success) {
        (void)trace_path;                                    
        if(!success)
        {
            spdlog::debug("Failed to fetch event data for event ID: {}", event_id);
            return;
        }

        bool result = m_data_provider.FetchEventFlowDetails(event_id);
        if(!result)
        {
            spdlog::debug("Failed to fetch event flow details for event ID: {}",
                            event_id);
        }
        result = m_data_provider.FetchEventCallStackData(event_id);
        if(!result)
        {
            spdlog::debug("Failed to fetch event call stack data for event ID: {}",
                            event_id);
        }
    });

    m_data_provider.SetTrackDataReadyCallback(
        [](uint64_t track_id, const std::string& trace_path, const RequestInfo& req) {
            EventManager::GetInstance()->AddEvent(std::make_shared<TrackDataEvent>(
                static_cast<int>(RocEvents::kNewTrackData), track_id, trace_path,
                req.request_id, req.response_code));
        });

    auto new_tab_selected_handler = [this](std::shared_ptr<RocEvent> e) {
        auto ets = std::dynamic_pointer_cast<TabEvent>(e);
        if(ets)
        {
            // Only handle the event if the tab source is the main tab source.
            if(ets->GetSourceId() == AppWindow::GetInstance()->GetMainTabSourceName())
            {
                m_data_provider.SetSelectedState(ets->GetTabId());
            }
        }
    };

    m_data_provider.SetTrackMetadataChangedCallback([](const std::string& trace_path) {
        EventManager::GetInstance()->AddEvent(std::make_shared<RocEvent>(
            static_cast<int>(RocEvents::kTrackMetadataChanged), trace_path));
    });

    m_data_provider.SetTableDataReadyCallback(
        [](const std::string& trace_path, uint64_t request_id, uint64_t response_code) {
            EventManager::GetInstance()->AddEvent(
                std::make_shared<TableDataEvent>(trace_path, request_id, response_code));
        });

    m_data_provider.SetTraceLoadedCallback(
        [this](const std::string& trace_path, uint64_t response_code) {
            if(response_code != kRocProfVisResultSuccess)
            {
                spdlog::error("Failed to load trace: {}", response_code);
                m_popup_info.show_popup = true;
                m_popup_info.title      = "Error";
                m_popup_info.message    = "Failed to load trace: " + trace_path;
            }
        });

    m_data_provider.SetSaveTraceCallback([this](bool success) {
        m_popup_info.show_popup = true;
        m_popup_info.title      = "Save Trimmed Trace";
        if(success)
        {
            m_popup_info.message = "Trimmed trace has been saved successfully.";
        }
        else
        {
            m_popup_info.message = "Failed to save the trimmed trace.";
        }
        // clear the save notification
        NotificationManager::GetInstance().Hide(m_save_notification_id);
        m_save_notification_id = "";
    });

    m_data_provider.SetRequestProgressUpdateCallback(
        [this](const RequestInfo& request, uint64_t pct, const std::string& message) {
            EventManager::GetInstance()->AddEvent(
                std::make_shared<RequestProgressUpdateEvent>(
                    request.request_id, request.request_type, pct, message,
                    m_data_provider.GetTraceFilePath()));
        });

    auto event_selection_handler = [this](std::shared_ptr<RocEvent> e) {
        std::shared_ptr<EventSelectionChangedEvent> event =
            std::dynamic_pointer_cast<EventSelectionChangedEvent>(e);
        if(event && event->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            if(event->EventSelected())
            {
                m_data_provider.FetchEvent(event->GetEventTrackID(), event->GetEventID());
            }
            else if(event->IsBatch())
            {
                m_data_provider.DataModel().GetEvents().ClearEvents();
            }
            else
            {
                m_data_provider.DataModel().GetEvents().RemoveEvent(event->GetEventID());
            }
        }
    };

    m_tabselected_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTabSelected), new_tab_selected_handler);

    m_event_selection_changed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        event_selection_handler);

    auto request_progress_update_handler = [this](std::shared_ptr<RocEvent> e) {
        auto event = std::dynamic_pointer_cast<RequestProgressUpdateEvent>(e);
        if(event && event->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            if(event->GetRequestType() == RequestType::kSaveTrimmedTrace &&
               !m_save_notification_id.empty())
            {
                NotificationManager::GetInstance().UpdateProgress(
                    m_save_notification_id, event->GetProgressPercent(),
                    event->GetMessage());
            }
        }
    };
    m_progress_update_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate),
        request_progress_update_handler);

    m_tool_bar = std::make_shared<RocCustomWidget>([this]() { this->RenderToolbar(); });
    m_widget_name = GenUniqueName("TraceView");
}

TraceView::~TraceView()
{
    m_data_provider.SetTrackDataReadyCallback(nullptr);
    m_data_provider.SetTrackMetadataChangedCallback(nullptr);
    m_data_provider.SetTableDataReadyCallback(nullptr);
    m_data_provider.SetTraceLoadedCallback(nullptr);
    m_data_provider.SetSaveTraceCallback(nullptr);
    m_data_provider.SetCleanupDatabaseCallback(nullptr);

    EventManager::GetInstance()->Unsubscribe(static_cast<int>(RocEvents::kTabSelected),
                                             m_tabselected_event_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimelineEventSelectionChanged),
        m_event_selection_changed_event_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kRequestProgressUpdate),
        m_progress_update_event_token);
}

void
TraceView::Update()
{
    auto last_state = m_data_provider.GetState();
    m_data_provider.Update();

    if(m_timeline_selection)
    {
        m_timeline_selection->UpdateHighlightTimer();
    }

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
        m_project_settings = std::make_unique<SystemTraceProjectSettings>(
            m_data_provider.GetTraceFilePath(), *this);
        if(m_project_settings && m_project_settings->Valid())
        {
            m_bookmarks = std::move(m_project_settings->Bookmarks());
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
    if(m_analysis_item->m_item)
    {
        m_analysis_item->m_item->Update();
    }
    if(m_event_search)
    {
        m_event_search->Update();
    }
    if(m_summary_view)
    {
        m_summary_view->Update();
    }
    if(m_minimap && m_show_minimap_popup)
    {
        m_minimap->UpdateData();
    }
}

void
TraceView::CreateView()
{
    m_annotations =
        std::make_shared<AnnotationsManager>(m_data_provider.GetTraceFilePath());
    m_timeline_selection    = std::make_shared<TimelineSelection>(m_data_provider);
    m_track_topology        = std::make_shared<TrackTopology>(m_data_provider);
    m_timeline_view         = std::make_shared<TimelineView>(m_data_provider,
                                                             m_timeline_selection, m_annotations);
    m_event_search          = std::make_shared<EventSearch>(m_data_provider, m_timeline_selection);
    m_summary_view          = std::make_shared<SummaryView>(m_data_provider);
    m_minimap               = std::make_shared<Minimap>(m_data_provider, m_timeline_view.get());
    auto m_histogram_widget = std::make_shared<RocCustomWidget>(
        [this]() { m_timeline_view->RenderHistogram(); });

    auto sidebar =
        std::make_shared<SideBar>(m_track_topology, m_timeline_selection,
                                  m_timeline_view->GetGraphs(), m_data_provider);
    auto analysis = std::make_shared<AnalysisView>(m_data_provider, m_track_topology,
                                                   m_timeline_selection, m_annotations);

    m_sidebar_item            = LayoutItem::CreateFromWidget(sidebar);
    m_sidebar_item->m_visible = m_settings_manager.GetAppWindowSettings().show_sidebar;
    m_sidebar_item->m_window_flags = ImGuiWindowFlags_HorizontalScrollbar;

    m_analysis_item = LayoutItem::CreateFromWidget(analysis);
    m_analysis_item->m_visible =
        m_settings_manager.GetAppWindowSettings().show_details_panel;

    LayoutItem m_histogram_item(0, 80);
    m_histogram_item.m_item    = m_histogram_widget;
    m_histogram_item.m_visible = m_settings_manager.GetAppWindowSettings().show_histogram;
    LayoutItem timeline_item(0, 0);
    timeline_item.m_item = m_timeline_view;

    std::vector<LayoutItem> layout_items;
    layout_items.push_back(m_histogram_item);
    layout_items.push_back(timeline_item);
    m_timeline_container         = std::make_shared<VFixedContainer>(layout_items);
    auto timeline_container_item = LayoutItem::CreateFromWidget(m_timeline_container);

    m_vertical_split_container =
        std::make_shared<VSplitContainer>(timeline_container_item, m_analysis_item);
    m_vertical_split_container->SetSplit(0.75);

    auto trace_area    = std::make_shared<LayoutItem>();
    trace_area->m_item = m_vertical_split_container;

    m_horizontal_split_container =
        std::make_shared<HSplitContainer>(m_sidebar_item, trace_area);
    m_horizontal_split_container->SetSplit(0.2f);
    m_horizontal_split_container->SetMinRightWidth(400);
}

void
TraceView::DestroyView()
{
    m_minimap                    = nullptr;
    m_timeline_view              = nullptr;
    m_sidebar_item->m_item       = nullptr;
    m_horizontal_split_container = nullptr;
    m_analysis_item->m_item      = nullptr;
    m_view_created               = false;
}

bool
TraceView::LoadTrace(rocprofvis_controller_t* controller, const std::string& file_path)
{
    bool result = false;
    result      = m_data_provider.FetchTrace(controller, file_path);
    if(result)
    {
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

    if(m_data_provider.GetState() == ProviderState::kLoading)
    {
        RenderLoadingScreen(m_data_provider.GetProgressMessage());
    }
    else if(IsCleanupPending())
    {
        RenderLoadingScreen("Cleaning Database...");
    }
    else
    {
        if(m_horizontal_split_container &&
           m_data_provider.GetState() == ProviderState::kReady)
        {
            m_horizontal_split_container->Render();
            HandleHotKeys();
        }

        if(m_show_minimap_popup && m_minimap)
        {
            PopUpStyle popup_style;
            popup_style.PushPopupStyles();
            popup_style.PushTitlebarColors();

            float dpi = SettingsManager::GetInstance().GetDPI();
            ImGui::SetNextWindowSize(ImVec2(400.0f * dpi, 290.0f * dpi),
                                     ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSizeConstraints(ImVec2(200.0f, 150.0f),
                                                ImVec2(FLT_MAX, FLT_MAX));
            if(ImGui::Begin("Minimap", &m_show_minimap_popup,
                            ImGuiWindowFlags_NoCollapse))
            {
                const ImVec2 content = ImGui::GetContentRegionAvail();
                if(content.x >= 200.0f && content.y >= 150.0f)
                {
                    m_minimap->Render();
                }
            }
            ImGui::End();
            popup_style.PopStyles();
        }

        if(m_popup_info.show_popup)
        {
            m_popup_info.show_popup = false;
            AppWindow::GetInstance()->ShowMessageDialog(m_popup_info.title,
                                                        m_popup_info.message);
        }
    }

    if(m_summary_view)
    {
        m_summary_view->Render();
    }
}

void
TraceView::HandleHotKeys()
{
    if(!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
        return;

    auto& hk = HotkeyManager::GetInstance();

    // xDon’t process global hotkeys if ImGui wants the keyboard (e.g., typing in
    for(int i = 0; i <= 9; ++i)
    {
        std::string idx = std::to_string(i);

        if(hk.WasActionTriggered(HotkeyManager::BookmarkSaveAction(i)))
        {
            if(m_timeline_view)
            {
                auto coords    = m_timeline_view->GetViewCoords();
                m_bookmarks[i] = coords;
                spdlog::info("Bookmark {} saved at time offset: {}, scroll position: "
                             "{}, zoom: {}",
                             i, coords.v_min_x, coords.y, coords.z);
                NotificationManager::GetInstance().Show(
                    "Bookmark " + idx + " saved.",
                    NotificationLevel::Info);
            }
        }

        if(hk.WasActionTriggered(HotkeyManager::BookmarkRestoreAction(i)))
        {
            auto it = m_bookmarks.find(i);
            if(it != m_bookmarks.end())
            {
                if(m_timeline_view)
                {
                    m_timeline_view->MoveToPosition(
                        it->second.v_min_x, it->second.v_max_x, it->second.y, false);

                    NotificationManager::GetInstance().Show(
                        "Bookmark " + idx + " restored.",
                        NotificationLevel::Info);
                }
            }
            else
            {
                NotificationManager::GetInstance().Show(
                    "Bookmark slot " + idx + " not assigned",
                    NotificationLevel::Warning);
            }
        }
    }
}

bool
TraceView::HasTrimActiveTrimSelection() const
{
    if(m_timeline_selection)
    {
        return m_timeline_selection->HasValidTimeRangeSelection();
    }
    return false;
}

bool
TraceView::IsTrimSaveAllowed() const
{
    bool save_allowed = false;
    if(m_timeline_selection)
    {
        save_allowed = m_timeline_selection->HasValidTimeRangeSelection();
        save_allowed &= !m_data_provider.IsRequestPending(
            DataProvider::SAVE_TRIMMED_TRACE_REQUEST_ID);
    }

    return save_allowed;
}

bool
TraceView::SaveSelection(const std::string& file_path)
{
    if(m_timeline_selection)
    {
        if(!m_timeline_selection->HasValidTimeRangeSelection())
        {
            spdlog::warn("No valid time range selection to save.");
            return false;
        }

        double start_ns = 0.0;
        double end_ns   = 0.0;
        m_timeline_selection->GetSelectedTimeRange(start_ns, end_ns);

        m_data_provider.SaveTrimmedTrace(file_path, start_ns, end_ns);

        // create notification
        m_save_notification_id =
            "save_trace_" + std::to_string(std::hash<std::string>()(file_path));
        NotificationManager::GetInstance().ShowPersistent(m_save_notification_id,
                                                          "Saving Trace: " + file_path,
                                                          NotificationLevel::Info);

        return true;
    }
    else
    {
        spdlog::warn("Timeline selection is not initialized.");
    }
    return false;
}

bool
TraceView::CleanupDatabase(bool rebuild, std::function<void()> on_complete)
{
    if(m_data_provider.IsRequestPending(DataProvider::CLEANUP_DATABASE_REQUEST_ID))
    {
        spdlog::debug("Database cleanup already in progress.");
        return false;
    }

    m_data_provider.FreeRequests();

    m_data_provider.SetCleanupDatabaseCallback(
        [this, on_complete](bool success) {
            NotificationManager::GetInstance().Hide("cleanup_database");
            if(success)
            {
                NotificationManager::GetInstance().Show(
                    "Database cleanup completed successfully.",
                    NotificationLevel::Success);
            }
            else
            {
                NotificationManager::GetInstance().Show(
                    "Database cleanup failed.", NotificationLevel::Error);
            }
            if(on_complete)
            {
                on_complete();
            }
        });

    if(m_data_provider.CleanupDatabase(rebuild))
    {
        NotificationManager::GetInstance().ShowPersistent(
            "cleanup_database",
            rebuild ? "Cleaning and rebuilding database..." : "Cleaning database...",
            NotificationLevel::Info);
        return true;
    }

    return false;
}

bool
TraceView::IsCleanupPending() const
{
    return m_data_provider.IsRequestPending(DataProvider::CLEANUP_DATABASE_REQUEST_ID);
}

std::shared_ptr<TimelineSelection>
TraceView::GetTimelineSelection() const
{
    return m_timeline_selection;
}

std::shared_ptr<RocWidget>
TraceView::GetToolbar()
{
    return m_tool_bar;
}

void
TraceView::RenderEditMenuOptions()
{
    if(ImGui::MenuItem("Unselect All Tracks", nullptr, false,
                       m_timeline_selection && m_timeline_selection->HasSelectedTracks()))
    {
        if(m_timeline_selection)
        {
            std::shared_ptr<std::vector<TrackGraph>> graphs = m_timeline_view->GetGraphs();
            if(graphs)
            {
                m_timeline_selection->UnselectAllTracks(*graphs);
            }
        }
    }
    if(ImGui::MenuItem("Unselect All Events", nullptr, false,
                       m_timeline_selection && m_timeline_selection->HasSelectedEvents()))
    {
        if(m_timeline_selection)
        {
            m_timeline_selection->UnselectAllEvents();
        }
    }
    if(ImGui::MenuItem("Unhighlight All Events", nullptr, false,
                       m_timeline_selection && m_timeline_selection->HasHighlightedEvents()))
    {
        if(m_timeline_selection)
        {
            m_timeline_selection->UnhighlightAllEvents();
        }
    }
    ImGui::Separator();
    if(ImGui::MenuItem("Save Trace Selection", nullptr, false, IsTrimSaveAllowed()))
    {
        FileFilter trace_filter;
        trace_filter.m_name       = "Traces";
        trace_filter.m_extensions = { "db", "rpd" };

        std::vector<FileFilter> filters;
        filters.push_back(trace_filter);

        AppWindow::GetInstance()->ShowSaveFileDialog(
            "Save Trace Selection", filters, "",
            [this](std::string file_path) -> void { SaveSelection(file_path); });
    }
    ImGui::Separator();
}

void
TraceView::SetAnalysisViewVisibility(bool visibility)
{
    if(m_analysis_item)
    {
        m_analysis_item->m_visible = visibility;
    }
}

void
TraceView::SetSidebarViewVisibility(bool visibility)
{
    if(m_sidebar_item)
    {
        m_sidebar_item->m_visible = visibility;
    }
}

void
TraceView::SetHistogramVisibility(bool visibility)
{
    if(m_timeline_container)
    {
        auto histogram_item = m_timeline_container->GetMutableAt(0);
        if(histogram_item)
        {
            histogram_item->m_visible = visibility;
        }
    }
}

void
TraceView::RenderToolbar()
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    ImGui::PushStyleColor(
        ImGuiCol_ChildBg,
        ImGui::ColorConvertU32ToFloat4(m_settings_manager.GetColor(Colors::kBgPanel)));
    ImGui::PushStyleColor(ImGuiCol_Border,
                          ImGui::ColorConvertU32ToFloat4(
                              m_settings_manager.GetColor(Colors::kBorderColor)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

    ImGui::BeginChild("Toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.FrameRounding);
    ImGui::AlignTextToFramePadding();

    // Toolbar Controls
    ImGui::BeginGroup();
    RenderFlowControls();
    VerticalSeparator(&m_settings_manager);
    RenderAnnotationControls();
    VerticalSeparator(&m_settings_manager);
    RenderEventSearch();
    VerticalSeparator(&m_settings_manager);
    RenderBookmarkControls();
    VerticalSeparator(&m_settings_manager);
    
    ImFont* icon_font =
        m_settings_manager.GetFontManager().GetIconFont(FontType::kDefault);
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushFont(icon_font);
    if(ImGui::Button(ICON_COMPASS))
    {
        m_show_minimap_popup = !m_show_minimap_popup;
    }
    ImGui::PopFont();
    ImGui::PopStyleColor();

    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Show Minimap");
    }
    VerticalSeparator(&m_settings_manager);

    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kAccentRed)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kAccentRedHover)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kAccentRedActive)));
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kTextOnAccent)));
    if(ImGui::Button("Reset View"))
    {
        if(m_timeline_view)
        {
            const TimelineModel& timeline = m_data_provider.DataModel().GetTimeline();
            m_timeline_view->MoveToPosition(timeline.GetStartTime(),
                                            timeline.GetEndTime(), 0.0, false);
        }
    }
    ImGui::PopStyleColor(4);
    if(ImGui::IsItemHovered())
    {
        SetTooltipStyled("Reset view to default zoom and pan");
    }
    ImGui::EndGroup();
    float available_width = ImGui::GetContentRegionAvail().x - ImGui::GetItemRectSize().x;
    if(m_event_search && available_width != 0)
    {
        m_event_search->SetWidth(m_event_search->Width() + available_width);
    }

    ImGui::PopStyleVar(2);
    ImGui::EndChild();

    ImVec2      child_min = ImGui::GetItemRectMin();
    ImVec2      child_max = ImGui::GetItemRectMax();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    draw_list->AddLine(ImVec2(child_min.x, child_max.y - 1.0f),
                       ImVec2(child_max.x, child_max.y - 1.0f),
                       m_settings_manager.GetColor(Colors::kAccentRed), 2.0f);

    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);
}

void
TraceView::RenderAnnotationControls()
{
    if(m_annotations == nullptr) return;

    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kTextDim)));
    ImGui::TextUnformatted("Annotations");
    ImGui::PopStyleColor();
    auto default_style = m_settings_manager.GetDefaultStyle();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(default_style.ItemSpacing.x * 0.5f, 0));
    ImGui::SameLine();

    ImFont* icon_font =
        m_settings_manager.GetFontManager().GetIconFont(FontType::kDefault);
    ImGui::PushFont(icon_font);
    ImGui::BeginGroup();

    bool   is_sticky_visible = m_annotations->IsVisibile();
    ImVec4 transparent    = ImVec4(0, 0, 0, 0);
    ImVec4 accent         = ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kAccentRed));
    ImVec4 accent_hover   = ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kAccentRedHover));
    ImVec4 text_on_accent = ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kTextOnAccent));

    // Show All Stickies
    ImGui::PushID("show_all_stickies");
    if(is_sticky_visible)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent_hover);
        ImGui::PushStyleColor(ImGuiCol_Text, text_on_accent);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
    }
    if(ImGui::Button(ICON_EYE))
    {
        m_annotations->SetVisible(true);
    }
    ImGui::PopStyleColor(3);
    if(ImGui::IsItemHovered())
    {
        ImGui::PopFont();
        SetTooltipStyled("Show Annotation Layer");
        ImGui::PushFont(icon_font);
    }
    ImGui::PopID();
    ImGui::SameLine();

    // Hide All Stickies
    ImGui::PushID("hide_all_stickies");
    if(!is_sticky_visible)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent_hover);
        ImGui::PushStyleColor(ImGuiCol_Text, text_on_accent);
    }
    else
    {
        ImGui::PushStyleColor(ImGuiCol_Button, transparent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
        ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
    }
    if(ImGui::Button(ICON_EYE_THIN))
    {
        m_annotations->SetVisible(false);
    }
    ImGui::PopStyleColor(3);
    if(ImGui::IsItemHovered())
    {
        ImGui::PopFont();
        SetTooltipStyled("Hide Annotation Layer");
        ImGui::PushFont(icon_font);
    }
    ImGui::PopID();
    ImGui::SameLine();

    // Add New Sticky
    ImGui::PushID("add_new_sticky");
    ImGui::PushStyleColor(ImGuiCol_Button, transparent);
    if(ImGui::Button(ICON_ADD_NOTE))
    {
        auto tpt = m_timeline_view->GetTransform();
        if(tpt)
        {
            ViewCoords coords       = m_timeline_view->GetViewCoords();
            ImVec2     graph_size   = m_timeline_view->GetGraphSize();
            double     center_time  = tpt->PixelToTime(graph_size.x * 0.5f);
            float      center_y     = m_timeline_view->GetScrollPosition() + graph_size.y * 0.5f;
            m_annotations->OpenStickyNotePopup(
                center_time, center_y, coords.v_min_x,
                coords.v_max_x, graph_size);
            m_annotations->ShowStickyNotePopup();
        }
    }
    ImGui::PopStyleColor();
    if(ImGui::IsItemHovered())
    {
        ImGui::PopFont();
        SetTooltipStyled("Add New Annotation");
        ImGui::PushFont(icon_font);
    }
    ImGui::PopID();

    ImGui::EndGroup();
    ImGui::PopFont();
}

void
TraceView::RenderBookmarkControls()
{
    ImGui::BeginGroup();

    ImGui::PushID("bookmark_toggle_dropdown");

    static int selected_slot = -1;
    ImGui::SetNextItemWidth(ImGui::CalcTextSize("BookMarks").x +
                            2 * ImGui::GetStyle().FramePadding.x +
                            ImGui::GetFrameHeightWithSpacing());
    if(ImGui::BeginCombo("", "Bookmarks"))
    {
        if(ImGui::BeginTable("BookmarkTable", 2, ImGuiTableFlags_SizingStretchProp))
        {
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthStretch);
            float button_width = 15.0f;
            ImGui::TableSetupColumn(nullptr, ImGuiTableColumnFlags_WidthFixed,
                                    button_width);

            for(int i = 0; i <= 9; ++i)
            {
                bool        used  = m_bookmarks.count(i) > 0;
                std::string label = std::to_string(i);

                bool is_selected = (selected_slot == i);
                ImGui::PushID(i);

                ImU32 add_color  = m_settings_manager.GetColor(Colors::kTextDim);
                ImU32 used_color = m_settings_manager.GetColor(Colors::kTextMain);

                ImGui::TableNextRow();

                ImGui::TableSetColumnIndex(0);
                ImGui::PushStyleColor(ImGuiCol_Text, used ? used_color : add_color);
                if(ImGui::Selectable(label.c_str(), is_selected))
                {
                    if(used)
                    {
                        auto it = m_bookmarks.find(i);
                        if(it != m_bookmarks.end() && m_timeline_view)
                            m_timeline_view->MoveToPosition(it->second.v_min_x,
                                                            it->second.v_max_x,
                                                            it->second.y, false);
                    }

                    selected_slot = -1;
                }
                ImGui::PopStyleColor();

                ImGui::TableSetColumnIndex(1);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
                ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, IM_COL32(0, 0, 0, 0));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive, IM_COL32(0, 0, 0, 0));
                ImFont* icon_font =
                    m_settings_manager.GetFontManager().GetIconFont(FontType::kDefault);
                ImGui::PushFont(icon_font);
                if(used)
                {
                    if(ImGui::Button(ICON_DELETE))
                    {
                        m_bookmarks.erase(i);
                        NotificationManager::GetInstance().Show(
                            "Bookmark " + std::to_string(i) + " removed.",
                            NotificationLevel::Info);
                    }
                }
                else
                {
                    ImGui::PushFont(icon_font);
                    ImGui::PushFont(icon_font);
                    if(ImGui::Button(ICON_ADD_NOTE))
                    {
                        m_bookmarks[i] = m_timeline_view->GetViewCoords();
                        NotificationManager::GetInstance().Show(
                            "Bookmark " + std::to_string(i) + " created.",
                            NotificationLevel::Info);
                    }
                    ImGui::PopFont();
                    ImGui::PopFont();
                }
                ImGui::PopFont();
                ImGui::PopStyleColor(3);
                ImGui::PopStyleVar(2);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndCombo();
    }

    ImGui::PopID();
    ImGui::SameLine();

    ImGui::EndGroup();
}

void
TraceView::RenderFlowControls()
{
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kTextDim)));
    ImGui::TextUnformatted("Flow");
    ImGui::PopStyleColor();
    auto default_style = m_settings_manager.GetDefaultStyle();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(default_style.ItemSpacing.x * 0.5f, 0));
    ImGui::SameLine();

    ImVec4 transparent    = ImVec4(0, 0, 0, 0);
    ImVec4 accent         = ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kAccentRed));
    ImVec4 accent_hover   = ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kAccentRedHover));
    ImVec4 text_on_accent = ImGui::ColorConvertU32ToFloat4(
        m_settings_manager.GetColor(Colors::kTextOnAccent));

    static const char* flow_labels[]    = { ICON_EYE, ICON_EYE_SLASH };
    static const char* flow_tool_tips[] = { "Show All", "Hide All" };

    TimelineArrow&  arrow_layer  = m_timeline_view->GetArrowLayer();
    FlowDisplayMode current_mode = arrow_layer.GetFlowDisplayMode();

    FlowDisplayMode mode = current_mode;

    ImFont* icon_font =
        m_settings_manager.GetFontManager().GetIconFont(FontType::kDefault);
    ImGui::PushFont(icon_font);

    ImGui::BeginGroup();
    for(int i = 0; i <= static_cast<int>(FlowDisplayMode::__kLastMode); ++i)
    {
        bool selected = static_cast<int>(current_mode) == i;
        if(selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, accent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accent_hover);
            ImGui::PushStyleColor(ImGuiCol_Text, text_on_accent);
        }
        else
        {
            ImGui::PushStyleColor(ImGuiCol_Button, transparent);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered));
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text));
        }

        ImGui::PushID(i);
        if(ImGui::Button(flow_labels[i]))
        {
            mode = static_cast<FlowDisplayMode>(i);
        }

        ImGui::PopStyleColor(3);
        if(ImGui::IsItemHovered())
        {
            ImGui::PopFont();
            SetTooltipStyled("%s", flow_tool_tips[i]);
            ImGui::PushFont(icon_font);
        }

        ImGui::PopID();
        ImGui::SameLine();
    }

    const char* label = ICON_TREE;

    if(arrow_layer.GetRenderStyle() == TimelineArrow::RenderStyle::kChain)
    {
        label = ICON_CHAIN;
    }
    ImGui::PushStyleColor(ImGuiCol_Button, transparent);
    if(ImGui::Button(label))
    {
        arrow_layer.SetRenderStyle(arrow_layer.GetRenderStyle() ==
                                           TimelineArrow::RenderStyle::kChain
                                       ? TimelineArrow::RenderStyle::kFan
                                       : TimelineArrow::RenderStyle::kChain);
    }
    ImGui::PopStyleColor();

    if(ImGui::IsItemHovered())
    {
        ImGui::PopFont();
        SetTooltipStyled("Flow Render Style");
        ImGui::PushFont(icon_font);
    }
    ImGui::PopFont();

    ImGui::EndGroup();

    // Update the mode if changed
    if(mode != current_mode) arrow_layer.SetFlowDisplayMode(mode);
}

void
TraceView::RenderEventSearch()
{
    if(m_event_search && m_event_search->Width() > 0)
    {
        SettingsManager& settings = SettingsManager::GetInstance();
        if(m_event_search->FocusTextInput())
        {
            ImGui::SetKeyboardFocusHere();
        }
        std::pair<bool, bool> search_bar = InputTextWithClear(
            "search_bar", "Search: hipLaunchKernel or \"hip\"\"kernel\"",
            m_event_search->TextInput(), m_event_search->TextInputLimit(),
            settings.GetFontManager().GetIconFont(FontType::kDefault),
            settings.GetColor(Colors::kBgMain), settings.GetDefaultStyle(),
            m_event_search->Width());
        if(ImGui::IsItemClicked() && m_event_search->Searched())
        {
            m_event_search->Show();
        }
        if(ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_Enter))
        {
            m_event_search->Search();
        }
        if(search_bar.second)
        {
            m_event_search->Clear();
        }
        if(m_event_search->FocusTextInput())
        {
            ImGui::GetIO().MouseClicked[0] = false;
        }
        m_event_search->Render();
    }
}

SystemTraceProjectSettings::SystemTraceProjectSettings(const std::string& project_id,
                                                       TraceView&         view)
: ProjectSetting(project_id)
, m_view(view)
{}

SystemTraceProjectSettings::~SystemTraceProjectSettings() {}

void
SystemTraceProjectSettings::ToJson()
{
    int i = 0;
    for(const auto& it : m_view.m_bookmarks)
    {
        jt::Json& bookmark =
            m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_BOOKMARK][i];
        bookmark[JSON_KEY_TIMELINE_BOOKMARK_KEY]     = it.first;
        bookmark[JSON_KEY_TIMELINE_BOOKMARK_V_MIN_X] = it.second.v_min_x;
        bookmark[JSON_KEY_TIMELINE_BOOKMARK_V_MAX_X] = it.second.v_max_x;
        bookmark[JSON_KEY_TIMELINE_BOOKMARK_Y]       = it.second.y;
        bookmark[JSON_KEY_TIMELINE_BOOKMARK_Z]       = it.second.z;

        i++;
    }
}

bool
SystemTraceProjectSettings::Valid() const
{
    bool valid = false;
    if(m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_BOOKMARK].isArray())
    {
        int valid_count = 0;
        for(jt::Json& bookmark :
            m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_BOOKMARK]
                .getArray())
        {
            if(bookmark[JSON_KEY_TIMELINE_BOOKMARK_KEY].isLong() &&
               bookmark[JSON_KEY_TIMELINE_BOOKMARK_V_MIN_X].isNumber() &&
               bookmark[JSON_KEY_TIMELINE_BOOKMARK_V_MAX_X].isNumber() &&
               bookmark[JSON_KEY_TIMELINE_BOOKMARK_Y].isNumber() &&
               bookmark[JSON_KEY_TIMELINE_BOOKMARK_Z].isNumber())
            {
                valid_count++;
            }
            else
            {
                break;
            }
        }
        valid = (valid_count ==
                 m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_BOOKMARK]
                     .getArray()
                     .size());
    }
    return valid;
}

std::unordered_map<int, ViewCoords>
SystemTraceProjectSettings::Bookmarks()
{
    std::unordered_map<int, ViewCoords> bookmarks;
    for(jt::Json& bookmark :
        m_settings_json[JSON_KEY_GROUP_TIMELINE][JSON_KEY_TIMELINE_BOOKMARK].getArray())
    {
        bookmarks[static_cast<int>(
            bookmark[JSON_KEY_TIMELINE_BOOKMARK_KEY].getNumber())] = ViewCoords{
            static_cast<double>(bookmark[JSON_KEY_TIMELINE_BOOKMARK_Y].getNumber()),
            static_cast<float>(bookmark[JSON_KEY_TIMELINE_BOOKMARK_Z].getNumber()),
            static_cast<double>(bookmark[JSON_KEY_TIMELINE_BOOKMARK_V_MIN_X].getNumber()),
            static_cast<double>(bookmark[JSON_KEY_TIMELINE_BOOKMARK_V_MAX_X].getNumber())
        };
    }
    return bookmarks;
}

}  // namespace View
}  // namespace RocProfVis
