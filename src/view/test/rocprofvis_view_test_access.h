// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
// Test-only access shims (Test Peer pattern). Compiled ONLY into the
// roc-optiq-tests executable; never referenced by production builds. Each view
// class grants a single `friend struct <Class>TestPeer;` so these peers can read
// private state without test accessors living in the production class body.
#pragma once

#include "imgui.h"

#include "rocprofvis_appwindow.h"
#include "rocprofvis_analysis_view.h"
#include "rocprofvis_event_search.h"
#include "rocprofvis_events_view.h"
#include "rocprofvis_flame_track_item.h"
#include "rocprofvis_measurement_controller.h"
#include "rocprofvis_minimap.h"
#include "rocprofvis_sidebar.h"
#include "rocprofvis_summary_view.h"
#include "rocprofvis_timeline_track_options.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_track_details.h"
#include "rocprofvis_trace_view.h"
#include "compute/rocprofvis_compute_kernel_details.h"
#include "compute/rocprofvis_compute_kernel_metric_table.h"
#include "compute/rocprofvis_compute_view.h"
#include "compute/rocprofvis_compute_workload_view.h"
#include "compute/rocprofvis_compute_comparison.h"
#include "compute/rocprofvis_compute_table_view.h"
#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_model_types.h"
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
    EventsView*   EventsViewPtr() const { return v.m_events_view.get(); }
    TrackDetails* TrackDetailsPtr() const { return v.m_track_details.get(); }
};

// TrackDetails holds one DetailItem per selected track (emplace_front on select,
// removed/cleared on deselect). Tests confirm the RIGHT track populated by id,
// not merely a non-empty pane.
struct TrackDetailsTestPeer
{
    const TrackDetails& v;
    size_t DetailCount() const { return v.m_track_details.size(); }
    bool   HasTrack(uint64_t track_id) const
    {
        for(const auto& item : v.m_track_details)
            if(item.track_id == track_id) return true;
        return false;
    }
};

// SideBar projects the model's TopologyTree into the rows it renders. The tree
// is rebuilt from Update(), so tests poll LeafCount() rather than assuming it is
// populated on the first frame.
struct SideBarTestPeer
{
    const SideBar& v;

    bool   HasTree() const { return v.m_sidebar_tree.root != nullptr; }
    size_t LeafCount() const { return CountLeaves(v.m_sidebar_tree.root.get()); }

    // Track ids of every leaf, including the repeats (a queue appears under its
    // processor and again under each stream that dispatched to it).
    std::vector<uint64_t> LeafTrackIds() const
    {
        std::vector<uint64_t> ids;
        CollectLeaves(v.m_sidebar_tree.root.get(), ids);
        return ids;
    }

private:
    static size_t CountLeaves(const TreeNode* node)
    {
        if(node == nullptr) return 0;
        if(node->IsLeaf()) return 1;
        size_t count = 0;
        for(const auto& child : node->children) count += CountLeaves(child.get());
        return count;
    }
    static void CollectLeaves(const TreeNode* node, std::vector<uint64_t>& ids)
    {
        if(node == nullptr) return;
        if(node->IsLeaf())
        {
            ids.push_back(static_cast<const LeafNode*>(node)->track_id);
            return;
        }
        for(const auto& child : node->children) CollectLeaves(child.get(), ids);
    }
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

struct AppWindowTestPeer
{
    AppWindow& v;
    TabContainer* TabContainerPtr() const { return v.m_tab_container.get(); }
};

struct ComputeViewTestPeer
{
    ComputeView& v;
    TabContainer*     TabContainerPtr() const { return v.m_tab_container.get(); }
    ComputeSelection* ComputeSelectionPtr() const { return v.m_compute_selection.get(); }
};

struct ComputeKernelDetailsViewTestPeer
{
    ComputeKernelDetailsView& v;
    KernelMetricTable* KernelMetricTablePtr() const { return v.m_kernel_metric_table.get(); }
};

struct ComputeWorkloadViewTestPeer
{
    const ComputeWorkloadView& v;
    const WorkloadInfo* WorkloadInfoPtr() const { return v.m_workload_info; }
    size_t SystemInfoCols() const
    {
        return v.m_workload_info ? v.m_workload_info->system_info.size() : 0;
    }
    size_t SystemInfoRows() const
    {
        return (v.m_workload_info && !v.m_workload_info->system_info.empty())
                   ? v.m_workload_info->system_info[0].size()
                   : 0;
    }
    size_t ProfilingConfigCols() const
    {
        return v.m_workload_info ? v.m_workload_info->profiling_config.size() : 0;
    }
    size_t ProfilingConfigRows() const
    {
        return (v.m_workload_info && !v.m_workload_info->profiling_config.empty())
                   ? v.m_workload_info->profiling_config[0].size()
                   : 0;
    }
};

struct ComputeComparisonViewTestPeer
{
    ComputeComparisonView& v;
    uint32_t TargetWorkloadId() const { return v.m_target_workload_id; }
    uint32_t TargetKernelId() const { return v.m_target_kernel_id; }
    size_t   CategoryCount() const { return v.m_categories.size(); }
    // True while either the baseline or target metrics fetch is still pending.
    bool RequestsPending() const
    {
        return v.m_data_provider.IsRequestPending(v.m_baseline_request_id) ||
               v.m_data_provider.IsRequestPending(v.m_target_request_id);
    }
    // True once a built table has a "Difference##" column, i.e. deltas were
    // actually computed (not just tables allocated).
    bool HasDifferenceColumn() const
    {
        for(const auto& category : v.m_categories)
        {
            for(const auto& table : category.tables)
            {
                if(!table) continue;
                for(const std::string& name : table->OrderedValueNames())
                {
                    if(name.rfind("Difference##", 0) == 0) return true;
                }
            }
        }
        return false;
    }
};

struct ComputeTableViewTestPeer
{
    ComputeTableView& v;
    bool   FetchPending() const { return v.m_fetch_pending; }
    size_t TableWidgetCount() const { return v.m_table_widgets.size(); }
    size_t PinnedCount() const { return v.m_pinned_metrics.size(); }
    bool   IsPinned(const MetricId& id) const { return v.m_pinned_metrics.count(id) > 0; }
    MetricId FirstPinned() const { return *v.m_pinned_metrics.begin(); }
    // Test-only unpin for state restore (no public unpin exists). Skips the pin
    // callback's source-table ChangePinState; safe only because callers refetch
    // after, rebuilding pin state from m_pinned_metrics.
    void Unpin(const MetricId& id)
    {
        v.m_pinned_metrics.erase(id);
        v.m_pinned_metric_table.RefillTable(v.m_pinned_metrics);
    }
};

// The kernel metric table's sort column/order are updated each frame from the
// ImGui table sort specs, so a TableClickHeader on a column drives these.
struct KernelMetricTableTestPeer
{
    const KernelMetricTable& v;
    int SortColumnIndex() const { return v.m_sort_column_index; }
    int SortOrder() const { return v.m_sort_order; }
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
    void SetCompactMode(bool on)
    {
        v.m_event_options->m_compact = on;
        v.m_event_options->m_updated = true;
        v.Update();
    }
    float LevelHeight() const { return v.m_level_height; }
    EventTrackOptions::EventColorMode GetEventColorMode() const
    {
        return v.m_event_options->m_color_mode;
    }
    void SetEventColorMode(EventTrackOptions::EventColorMode mode)
    {
        v.m_event_options->m_color_mode = mode;
    }
    // ImGui ID of the "FV" child window the bars register under; tests gather
    // bars by this parent. 0 until the track has rendered at least once.
    unsigned int   FlameWindowId() const { return v.m_test_flame_window_id; }

    bool IsExpanded() const { return v.m_event_options->m_expand; }
    int  MaxLevel() const { return static_cast<int>(v.m_max_level); }
    float DefaultTrackHeight() const { return v.DefaultTrackHeight(); }
    float ExpandedTrackHeight() const { return v.ExpandedTrackHeight(); }

    // Mirrors the two arrow-button branches in RenderMetaAreaExpand (the arrow
    // sits in a meta area with no stable widget ref): expanding grows the track
    // to fit all levels, collapsing snaps it back to the default height.
    void SetExpanded(bool expanded)
    {
        if(expanded)
        {
            v.RecalculateTrackHeight();
            v.m_event_options->m_expand = true;
        }
        else
        {
            v.m_event_options->m_height = v.DefaultTrackHeight();
            v.m_track_height_changed    = true;
            v.m_event_options->m_expand = false;
        }
    }

    // Restore an exact captured height (SetExpanded canonicalizes it) so the
    // track layout later tests see is unchanged.
    void SetTrackHeight(float height)
    {
        v.m_event_options->m_height = height;
        v.m_track_height_changed    = true;
    }

    size_t ChartItemCount() const { return v.m_chart_items.size(); }

    // Identity of the earliest event (smallest m_start_ts) in this track. Chart
    // item ordering is not guaranteed stable, so tests pick by timestamp rather
    // than index. Returns false when the track holds no events.
    bool EarliestEvent(uint64_t& uuid, std::string& name, double& start_ts) const
    {
        bool found = false;
        for(const auto& chart_item : v.m_chart_items)
        {
            if(!found || chart_item.event.m_start_ts < start_ts)
            {
                uuid     = chart_item.event.m_id.uuid;
                name     = chart_item.event.m_name;
                start_ts = chart_item.event.m_start_ts;
                found    = true;
            }
        }
        return found;
    }
};

struct TimelineViewTestPeer
{
    const TimelineView& v;

    float MaxYScroll() const { return v.m_content_max_y_scroll; }

    // Topology sort order, derived from the model's TopologyTree. Must be a full
    // permutation of the current tracks or ApplyTrackOrder rejects it.
    std::vector<uint64_t> TopologyOrder() const { return v.BuildTopologyOrder(); }
    size_t                TrackCount() const { return v.m_tracks ? v.m_tracks->size() : 0; }

    // Sidebar width, resized by dragging the "##MovePositionLineVert" splitter.
    float SidebarSize() const { return v.m_sidebar_size; }
    void  SetSidebarSize(float size) const { const_cast<TimelineView&>(v).m_sidebar_size = size; }

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

    // All displayed flame tracks, in track order. Tests scan this for a track
    // matching a criterion (has events, enough levels to expand) rather than
    // assuming the first flame track qualifies.
    std::vector<FlameTrackItem*> DisplayedFlameTracks() const
    {
        std::vector<FlameTrackItem*> flames;
        if(!v.m_tracks) return flames;
        for(TrackItem* track : *v.m_tracks)
        {
            if(track == nullptr || !track->IsDisplayed()) continue;
            if(FlameTrackItem* flame = dynamic_cast<FlameTrackItem*>(track))
                flames.push_back(flame);
        }
        return flames;
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
    SideBar*      SideBarPtr() const
    {
        if(v.m_sidebar_item == nullptr) return nullptr;
        return dynamic_cast<SideBar*>(v.m_sidebar_item->m_item.get());
    }
    MeasurementController* MeasurementControllerPtr() const { return v.m_measurement.get(); }
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
    // Current chart/table display mode, exposed as booleans so the private
    // TopKernels::DisplayMode enum stays encapsulated.
    bool IsDisplayPie() const { return v.m_display_mode == TopKernels::Pie; }
    bool IsDisplayBar() const { return v.m_display_mode == TopKernels::Bar; }
    bool IsDisplayTable() const { return v.m_display_mode == TopKernels::Table; }
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
