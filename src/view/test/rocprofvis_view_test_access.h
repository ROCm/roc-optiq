// Test-only access shims (Test Peer pattern). Compiled ONLY into the
// roc-optiq-tests executable; never referenced by production builds. Each view
// class grants a single `friend struct <Class>TestPeer;` so these peers can read
// private state without test accessors living in the production class body.
#pragma once

#include "imgui.h"

#include "rocprofvis_analysis_view.h"
#include "rocprofvis_event_search.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_minimap.h"
#include "rocprofvis_summary_view.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_trace_view.h"
#include "compute/rocprofvis_compute_view.h"
#include "compute/rocprofvis_compute_selection.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"
#include "widgets/rocprofvis_tab_container.h"

namespace RocProfVis
{
namespace View
{

struct EventsViewTestPeer
{
    const EventsView& v;
    size_t EventItemCount() const { return v.m_event_items.size(); }
};

struct AnalysisViewTestPeer
{
    const AnalysisView& v;
    EventsView* EventsViewPtr() const { return v.m_events_view.get(); }
};

struct MinimapTestPeer
{
    const Minimap& v;
    bool ShowEvents() const { return v.m_show_events; }
    bool ShowCounters() const { return v.m_show_counters; }
};

struct TabContainerTestPeer
{
    const TabContainer& v;
    int  ActiveTabIndex() const { return v.m_active_tab_index; }
    int  TabCount() const { return static_cast<int>(v.m_tabs.size()); }
};

struct ComputeViewTestPeer
{
    ComputeView& v;
    TabContainer*     TabContainerPtr() const { return v.m_tab_container.get(); }
    ComputeSelection* ComputeSelectionPtr() const { return v.m_compute_selection.get(); }
};

// EventSearch's state lives in its protected base InfiniteScrollTable, so this
// peer friends the base (friendship is not inherited).
struct EventSearchTestPeer
{
    const EventSearch& v;
    size_t ResultCount() const
    {
        const InfiniteScrollTable& t = v;
        return t.m_table_model().GetTableTotalRowCount(t.m_table_type);
    }
    bool RequestPending() const
    {
        const InfiniteScrollTable& t = v;
        return t.m_data_provider.IsRequestPending(t.m_request_id);
    }
};

// FlameTrackItem: only the non-capture accessors move here. The render-path
// rect capture and its reader accessors stay #ifdef'd in the class (geometry
// only exists during draw).
struct FlameTrackItemTestPeer
{
    FlameTrackItem& v;
    void           SetCompactMode(bool on)
    {
        v.m_compact_mode = on;
        v.ApplyCompactMode();
    }
    float          LevelHeight() const { return v.m_level_height; }
    EventColorMode GetEventColorMode() const { return v.m_event_color_mode; }
    void           SetEventColorMode(EventColorMode mode) { v.m_event_color_mode = mode; }
    // ImGui ID of the "FV" child window the bars register under; tests gather
    // bars by this parent. 0 until the track has rendered at least once.
    unsigned int   FlameWindowId() const { return v.m_test_flame_window_id; }
};

struct TimelineViewTestPeer
{
    const TimelineView& v;

    float MaxYScroll() const { return v.m_content_max_y_scroll; }

    FlameTrackItem* FirstFlameTrack() const
    {
        if(!v.m_tracks) return nullptr;
        for(TrackItem* track : *v.m_tracks)
        {
            if(track == nullptr || !track->IsDisplayed()) continue;
            FlameTrackItem* flame = dynamic_cast<FlameTrackItem*>(track);
            if(flame != nullptr) return flame;
        }
        return nullptr;
    }

    // ImGui ID of the first visible flame track's "FV" child window, the parent
    // tests pass to ctx->GatherItems() to enumerate that track's event bars.
    // Returns 0 if no flame track is present or it hasn't rendered yet.
    unsigned int FirstFlameWindowId() const
    {
        if(!v.m_tracks) return 0;
        for(TrackItem* track : *v.m_tracks)
        {
            if(track == nullptr || !track->IsDisplayed()) continue;
            FlameTrackItem* flame = dynamic_cast<FlameTrackItem*>(track);
            if(flame == nullptr) continue;
            unsigned int id = FlameTrackItemTestPeer{ *flame }.FlameWindowId();
            if(id != 0) return id;
        }
        return 0;
    }
};

struct TraceViewTestPeer
{
    TraceView& v;

    AnalysisView* AnalysisViewPtr() const
    {
        if(v.m_analysis_item == nullptr) return nullptr;
        return dynamic_cast<AnalysisView*>(v.m_analysis_item->m_item.get());
    }
    TimelineView* TimelineViewPtr() const { return v.m_timeline_view.get(); }
    Minimap*      MinimapPtr() const { return v.m_minimap.get(); }
    EventSearch*  EventSearchPtr() const { return v.m_event_search.get(); }
    SummaryView*  SummaryViewPtr() const { return v.m_summary_view.get(); }
    size_t        BookmarkCount() const { return v.m_bookmarks.size(); }
    void          ClearBookmarks() { v.m_bookmarks.clear(); }
    void          ClearEventSelection()
    {
        if(v.m_timeline_selection) v.m_timeline_selection->UnselectAllEvents();
    }
};

struct SummaryViewTestPeer
{
    const SummaryView& v;
    TopKernels* TopKernelsPtr() const { return v.m_top_kernels.get(); }
};

// TopKernels drives the pie/bar/table kernel selection. The pie is ImPlot-canvas
// drawn (no ImGui widget ID), so tests drive the model path (ToggleSelectKernel)
// rather than clicking a wedge, and assert on m_selected_idx.
struct TopKernelsTestPeer
{
    TopKernels& v;
    // KernelCount() is 0 when m_kernels is null (pre-load) or empty; one check covers both.
    size_t                KernelCount() const { return v.m_kernels ? v.m_kernels->size() : 0; }
    // Return name of kernel at given index
    std::string KernelName(size_t idx) const{ return (v.m_kernels && idx < v.m_kernels->size()) ? v.m_kernels->at(idx).name : std::string{};}
    double ExecTimeSum(size_t idx) const{ return (v.m_kernels && idx < v.m_kernels->size()) ? v.m_kernels->at(idx).exec_time_sum : 0.0;}
    std::optional<size_t> SelectedIdx() const { return v.m_selected_idx; }
    // The synthetic "Others" bucket, if present. ToggleSelectKernel treats it as a
    // deselect, so tests must avoid selecting it.
    std::optional<size_t> PaddedIdx() const { return v.m_padded_idx; }
    // Toggles idx; on a cleared baseline (SelectedIdx()==nullopt) that is a select.
    // Caller must ClearSelection() first and avoid the padded index.
    void                  Select(size_t idx) { v.ToggleSelectKernel(idx); }
    void                  ClearSelection()
    {
        if(v.m_selected_idx.has_value()) v.ToggleSelectKernel(v.m_selected_idx.value());
    }
};

}  // namespace View
}  // namespace RocProfVis
