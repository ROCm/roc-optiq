#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_project.h"
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

class TraceView : public RocWidget
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
    
    bool SaveSelection(const std::string& file_path);

private:
    void HandleHotKeys();

    std::shared_ptr<TimelineView>      m_timeline_view;
    std::shared_ptr<SideBar>           m_sidebar;
    std::shared_ptr<HSplitContainer>   m_container;
    std::shared_ptr<AnalysisView>      m_analysis;
    std::shared_ptr<TimelineSelection> m_timeline_selection;
    std::shared_ptr<TrackTopology>     m_track_topology;

    DataProvider m_data_provider;
    bool         m_view_created;
    bool         m_open_loading_popup;

    typedef struct popup_info_t{
        bool show_popup;
        std::string title;
        std::string message;
    } popup_info_t;
    
    popup_info_t m_popup_info;

    std::unique_ptr<MessageDialog> m_message_dialog;

    std::unordered_map<int,ViewCoords> m_bookmarks;

    EventManager::SubscriptionToken       m_tabselected_event_token;
    EventManager::SubscriptionToken       m_event_selection_changed_event_token;

    std::string m_save_notification_id;

    std::unique_ptr<SystemTraceProjectSettings> m_project_settings;
};

}  // namespace View
}  // namespace RocProfVis
