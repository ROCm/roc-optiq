#include "rocprofvis_trace_view.h"
#include "icons/rocprovfis_icon_defines.h"
#include "imgui.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_appwindow.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_settings_manager.h"
#include "rocprofvis_sidebar.h"
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
, m_sidebar(nullptr)
, m_container(nullptr)
, m_view_created(false)
, m_open_loading_popup(false)
, m_analysis(nullptr)
, m_timeline_selection(nullptr)
, m_track_topology(nullptr)
, m_popup_info({ false, "", "" })
, m_tabselected_event_token(-1)
, m_event_selection_changed_event_token(-1)
, m_save_notification_id("")
, m_project_settings(nullptr)
{
    m_data_provider.SetTrackDataReadyCallback(
        [](uint64_t track_id, const std::string& trace_path, const data_req_info_t& req) {
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
            static_cast<int>(RocEvents::kTrackMetadataChanged)));
    });

    m_data_provider.SetTableDataReadyCallback(
        [](const std::string& trace_path, uint64_t request_id) {
            EventManager::GetInstance()->AddEvent(
                std::make_shared<TableDataEvent>(trace_path, request_id));
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
                m_data_provider.FreeAllEvents();
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

    m_tool_bar = std::make_shared<RocCustomWidget>([this]() { this->RenderToolbar(); });
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
    m_timeline_selection = std::make_shared<TimelineSelection>(m_data_provider);
    m_track_topology     = std::make_shared<TrackTopology>(m_data_provider);
    m_timeline_view =
        std::make_shared<TimelineView>(m_data_provider, m_timeline_selection);
    m_sidebar  = std::make_shared<SideBar>(m_track_topology, m_timeline_selection,
                                           m_timeline_view->GetGraphs(), m_data_provider);
    m_analysis = std::make_shared<AnalysisView>(m_data_provider, m_track_topology,
                                                m_timeline_selection);

    LayoutItem left;
    left.m_item         = m_sidebar;
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

        HandleHotKeys();
    }

    if(m_popup_info.show_popup)
    {
        m_popup_info.show_popup = false;
        AppWindow::GetInstance()->ShowMessageDialog(m_popup_info.title,
                                                    m_popup_info.message);
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

            const char* progress_label      = m_data_provider.GetProgressMessage();
            ImVec2      progress_label_size = ImGui::CalcTextSize(progress_label);

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
                pos.y + (available_space.y - (label_size.y + dot_size.y +
                                              progress_label_size.y + item_spacing)) *
                            0.5f);
            ImGui::SetCursorScreenPos(center_pos);

            ImGui::Text(label);

            pos            = ImGui::GetCursorScreenPos();
            ImVec2 dot_pos = ImVec2(pos.x + (available_space.x - dot_size.x) * 0.5f,
                                    pos.y + item_spacing);
            ImGui::SetCursorScreenPos(dot_pos);

            RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing,
                                       IM_COL32(85, 85, 85, 255), anim_speed);

            pos        = ImGui::GetCursorScreenPos();
            center_pos = ImVec2(
                pos.x + (available_space.x - progress_label_size.x) * 0.5f,
                pos.y + (available_space.y - (label_size.y + dot_size.y +
                                              progress_label_size.y + item_spacing)) *
                            0.5f);
            ImGui::SetCursorScreenPos(center_pos);
            ImGui::Text(progress_label);

            ImGui::EndPopup();
        }
    }
}

void
TraceView::HandleHotKeys()
{
    // TODO: handling hot keys here for now.. this should be reworked to use a hotkey
    // manager in the future
    const ImGuiIO& io = ImGui::GetIO();

    // Don’t process global hotkeys if ImGui wants the keyboard (e.g., typing in
    // InputText)
    if(io.WantTextInput || ImGui::IsAnyItemActive())
    {
        return;
    }

    // handle numeric hotkeys 0-9
    // Press Ctrl + [0-9] to save a bookmark, press [0-9] to recall it
    for(int i = 0; i <= 9; ++i)
    {
        ImGuiKey key = static_cast<ImGuiKey>(ImGuiKey_0 + i);
        if(ImGui::IsKeyPressed(key, false))
        {
            if(io.KeyCtrl)
            {
                if(m_timeline_view)
                {
                    auto coords    = m_timeline_view->GetViewCoords();
                    m_bookmarks[i] = coords;
                    spdlog::info("Bookmark {} saved at time offset: {}, scroll position: "
                                 "{}, zoom: {}",
                                 i, coords.time_offset_ns, coords.y_scroll_position,
                                 coords.zoom);
                    NotificationManager::GetInstance().Show(
                        "Bookmark " + std::to_string(i) + " saved.",
                        NotificationLevel::Info);
                }
            }
            else
            {
                auto it = m_bookmarks.find(i);
                if(it != m_bookmarks.end())
                {
                    if(m_timeline_view)
                    {
                        m_timeline_view->SetViewCoords(it->second);
                        NotificationManager::GetInstance().Show(
                            "Bookmark " + std::to_string(i) + " restored.",
                            NotificationLevel::Info);
                    }
                }
                else
                {
                    NotificationManager::GetInstance().Show(
                        "Bookmark slot " + std::to_string(i) + " not assigned",
                        NotificationLevel::Warning);
                }
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

std::shared_ptr<TimelineSelection>
TraceView::GetTimelineSelection() const
{
    return m_timeline_selection;
}

std::shared_ptr<RocWidget>
TraceView::GetToolbar()
{
    return m_tool_bar;
};

void
TraceView::RenderEditMenuOptions()
{
    if(ImGui::MenuItem("Unselect All Tracks", nullptr, false,
                       m_timeline_selection && m_timeline_selection->HasSelectedTracks()))
    {
        if(m_timeline_selection)
        {
            std::vector<rocprofvis_graph_t>* graphs = m_timeline_view->GetGraphs();
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
    ImGui::Separator();
}

void
TraceView::RenderToolbar()
{
    ImGuiStyle& style          = ImGui::GetStyle();
    ImVec2      frame_padding  = style.FramePadding;
    float       frame_rounding = style.FrameRounding;

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::BeginChild("Toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, frame_padding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, frame_rounding);
    ImGui::AlignTextToFramePadding();

    // Toolbar Controls
    RenderFlowControls();
    ImGui::SameLine();
    ImGui::Dummy(ImVec2(10, 0));
    ImGui::SameLine();
    RenderAnnotationControls();
    // pop content style
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    // pop child window style
    ImGui::PopStyleVar(2);
}

void
TraceView::RenderAnnotationControls()
{
    ImGuiStyle& style = ImGui::GetStyle();
    ImFont*     icon_font =
        SettingsManager::GetInstance().GetFontManager().GetIconFont(FontType::kDefault);
    ImGui::PushFont(icon_font);
    ImGui::BeginGroup();

    AnnotationsView& view              = m_timeline_view->GetAnnotationsView();
    bool             is_sticky_visible = view.IsVisibile();

    // Show All Stickies
    ImGui::PushID("show_all_stickies");
    if(is_sticky_visible)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              style.Colors[ImGuiCol_ButtonActive]);
    }
    if(ImGui::Button(ICON_EYE))
    {
        view.SetVisible(true);
    }
    if(is_sticky_visible)
    {
        ImGui::PopStyleColor(2);
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::PopFont();
        ImGui::SetTooltip("Show All Stickies");
        ImGui::PushFont(icon_font);
    }
    ImGui::PopID();
    ImGui::SameLine();

    // Hide All Stickies
    ImGui::PushID("hide_all_stickies");
    if(!is_sticky_visible)
    {
        ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              style.Colors[ImGuiCol_ButtonActive]);
    }
    if(ImGui::Button(ICON_EYE_THIN))
    {
        view.SetVisible(false);
    }
    if(!is_sticky_visible)
    {
        ImGui::PopStyleColor(2);
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::PopFont();
        ImGui::SetTooltip("Hide All Stickies");
        ImGui::PushFont(icon_font);
    }
    ImGui::PopID();
    ImGui::SameLine();

    // Add New Sticky
    ImGui::PushID("add_new_sticky");
    if(ImGui::Button(ICON_ADD_NOTE))
    {
        view.OpenStickyNotePopup(-1.0f, -1.0f);
        view.ShowStickyNotePopup();
    }
    if(ImGui::IsItemHovered())
    {
        ImGui::PopFont();
        ImGui::SetTooltip("Add New Sticky");
        ImGui::PushFont(icon_font);
    }
    ImGui::PopID();

    ImGui::EndGroup();
    ImGui::PopFont();

    ImGui::SameLine();
    ImGui::TextUnformatted("Annotations");
}

void
TraceView::RenderFlowControls()
{
    ImGuiStyle& style = ImGui::GetStyle();

    static const char* flow_labels[]    = { ICON_EYE, ICON_EYE_THIN, ICON_EYE_SLASH };
    static const char* flow_tool_tips[] = { "Show All", "Show First & Last", "Hide All" };

    TimelineArrow&  arrow_layer  = m_timeline_view->GetArrowLayer();
    FlowDisplayMode current_mode = arrow_layer.GetFlowDisplayMode();

    FlowDisplayMode mode = current_mode;

    ImFont* icon_font =
        SettingsManager::GetInstance().GetFontManager().GetIconFont(FontType::kDefault);
    ImGui::PushFont(icon_font);

    ImGui::BeginGroup();
    for(int i = 0; i <= static_cast<int>(FlowDisplayMode::__kLastMode); ++i)
    {
        bool selected = static_cast<int>(current_mode) == i;
        // Use active colors when selected
        if(selected)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_ButtonActive]);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                                  style.Colors[ImGuiCol_ButtonActive]);
        }

        ImGui::PushID(i);
        if(ImGui::Button(flow_labels[i]))
        {
            mode = static_cast<FlowDisplayMode>(i);
        }

        if(ImGui::IsItemHovered())
        {
            ImGui::PopFont();
            ImGui::SetTooltip(flow_tool_tips[i]);
            ImGui::PushFont(icon_font);
        }

        if(selected)
        {
            ImGui::PopStyleColor(2);
        }
        ImGui::PopID();
        ImGui::SameLine();
    }
    ImGui::EndGroup();
    ImGui::PopFont();

    ImGui::SameLine();
    ImGui::TextUnformatted("Flow");

    // Update the mode if changed
    if(mode != current_mode) arrow_layer.SetFlowDisplayMode(mode);
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
        bookmark[JSON_KEY_TIMELINE_BOOKMARK_KEY] = it.first;
        bookmark[JSON_KEY_TIMELINE_BOOKMARK_X]   = it.second.time_offset_ns;
        bookmark[JSON_KEY_TIMELINE_BOOKMARK_Y]   = it.second.y_scroll_position;
        bookmark[JSON_KEY_TIMELINE_BOOKMARK_Z]   = it.second.zoom;
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
               bookmark[JSON_KEY_TIMELINE_BOOKMARK_X].isNumber() &&
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
        bookmarks[bookmark[JSON_KEY_TIMELINE_BOOKMARK_KEY].getNumber()] = ViewCoords{
            static_cast<double>(bookmark[JSON_KEY_TIMELINE_BOOKMARK_X].getNumber()),
            static_cast<double>(bookmark[JSON_KEY_TIMELINE_BOOKMARK_Y].getNumber()),
            static_cast<float>(bookmark[JSON_KEY_TIMELINE_BOOKMARK_Z].getNumber())
        };
    }
    return bookmarks;
}

}  // namespace View
}  // namespace RocProfVis