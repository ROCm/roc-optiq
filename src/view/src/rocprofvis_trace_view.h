// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_annotations.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_project.h"
#include "rocprofvis_root_view.h"
#include "rocprofvis_timeline_view.h"
#include "widgets/rocprofvis_widget.h"
#include <unordered_map>

namespace RocProfVis
{
namespace View
{

class TimelineView;
class SideBar;
class AnalysisView;
class TimelineSelection;
class TrackTopology;
class MessageDialog;
class TraceView;
class SettingsManager;
class EventSearch;
class SummaryView;
class Minimap;

class SystemTraceProjectSettings : public ProjectSetting
{
public:
    SystemTraceProjectSettings(const std::string& project_id, TraceView& view);
    ~SystemTraceProjectSettings() override;
    void ToJson() override;
    bool Valid() const override;

    std::unordered_map<int, ViewCoords> Bookmarks();

private:
    TraceView& m_view;
};

class TraceView : public RootView
{
    friend SystemTraceProjectSettings;

public:
    TraceView();
    ~TraceView();

    void Update() override;
    void Render() override;

    bool OpenFile(const std::string& file_path);

    void CreateView();
    void DestroyView();

    bool HasTrimActiveTrimSelection() const;
    bool IsTrimSaveAllowed() const;

    bool                               SaveSelection(const std::string& file_path);
    void                               RenderBookmarkControls();
    std::shared_ptr<TimelineSelection> GetTimelineSelection() const;
    std::shared_ptr<RocWidget>         GetToolbar() override;
    void                               RenderEditMenuOptions() override;
    void                               SetAnalysisViewVisibility(bool visibility); 
    void                               SetSidebarViewVisibility(bool visibility);
    void                               SetHistogramVisibility(bool visibility);
    void                               SetMinimapVisibility(bool visibility);

private:
    void HandleHotKeys();
    void RenderToolbar();
    void RenderFlowControls();
    void RenderAnnotationControls();
    void RenderSeparator();
    void RenderEventSearch();

    std::shared_ptr<TimelineView>      m_timeline_view;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    std::shared_ptr<TrackTopology>     m_track_topology;
    std::shared_ptr<RocCustomWidget>   m_tool_bar;
    std::shared_ptr<HSplitContainer>   m_horizontal_split_container;
    std::shared_ptr<VSplitContainer>   m_vertical_split_container;
    std::shared_ptr<VFixedContainer>   m_timeline_container;
    std::shared_ptr<EventSearch>       m_event_search;
    std::shared_ptr<SummaryView>       m_summary_view;
    std::shared_ptr<Minimap>           m_minimap;

    LayoutItem::Ptr m_sidebar_item;
    LayoutItem::Ptr m_analysis_item;
    LayoutItem::Ptr m_minimap_item;

    DataProvider m_data_provider;
    bool         m_view_created;
    bool         m_open_loading_popup;

    SettingsManager& m_settings_manager;

    typedef struct popup_info_t
    {
        bool        show_popup;
        std::string title;
        std::string message;
    } popup_info_t;

    popup_info_t                        m_popup_info;
    std::unordered_map<int, ViewCoords> m_bookmarks;

    std::shared_ptr<AnnotationsManager> m_annotations;

    EventManager::SubscriptionToken m_tabselected_event_token;
    EventManager::SubscriptionToken m_event_selection_changed_event_token;

    std::string m_save_notification_id;

    std::unique_ptr<SystemTraceProjectSettings> m_project_settings;
};

}  // namespace View
}  // namespace RocProfVis
