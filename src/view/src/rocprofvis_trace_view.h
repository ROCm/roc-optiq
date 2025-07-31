#pragma once
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class TimelineView;
class SideBar;
class AnalysisView;
class TimelineSelection;
class TrackTopology;

class TraceView : public RocWidget
{
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
        bool success;
    } popup_info_t;
    
    popup_info_t m_save_trace_popup_info;

    EventManager::SubscriptionToken       m_tabselected_event_token;
};

}  // namespace View
}  // namespace RocProfVis
