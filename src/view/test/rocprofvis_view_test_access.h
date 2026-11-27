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
#include "rocprofvis_project.h"
#include "rocprofvis_sidebar.h"
#include "rocprofvis_summary_view.h"
#include "rocprofvis_timeline_track_options.h"
#include "rocprofvis_timeline_view.h"
#include "rocprofvis_track_details.h"
#include "rocprofvis_trace_view.h"
#include "compute/isa/rocprofvis_compute_isa_view.h"
#include "compute/rocprofvis_compute_kernel_details.h"
#include "compute/rocprofvis_compute_kernel_metric_table.h"
#include "compute/rocprofvis_compute_memory_chart.h"
#include "compute/rocprofvis_compute_roofline.h"
#include "compute/rocprofvis_compute_view.h"
#include "compute/rocprofvis_compute_workload_view.h"
#include "compute/rocprofvis_compute_table_view.h"
#include "compute/rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_model_types.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"
#include "widgets/rocprofvis_tab_container.h"
#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
#    include "agenticprofiling/rocprofvis_ai_assistant.h"
#    include "agenticprofiling/rocprofvis_ai_trace_links.h"
// Dropping a conversation destroys its TextSelects, which needs the full type.
#    include "textselect.hpp"
#endif

#include <memory>
#include <string>
#include <unordered_map>

#include <cstdio>
#include <map>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace View
{

struct ProjectTestPeer
{
    const Project& v;
    std::string OpenErrorMessage() const { return v.m_open_error_message; }
};

struct EventsViewTestPeer
{
    const EventsView& v;
    size_t EventItemCount() const { return v.m_event_items.size(); }

    // Args live on each cached event's EventInfo (populated async by the
    // controller); info is null until it arrives, so every reach is guarded.
    size_t ArgCount(size_t item_idx) const
    {
        size_t i = 0;
        for(const auto& item : v.m_event_items)
        {
            if(i++ == item_idx)
                return item.info ? item.info->args.size() : 0;
        }
        return 0;
    }
    std::string ArgName(size_t item_idx, size_t arg_idx) const
    {
        size_t i = 0;
        for(const auto& item : v.m_event_items)
        {
            if(i++ == item_idx)
            {
                if(!item.info || arg_idx >= item.info->args.size()) return std::string();
                return item.info->args[arg_idx].name;
            }
        }
        return std::string();
    }
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
    // A leaf is not necessarily childless: a stream row carries its inline
    // processor subtree, so both walks recurse through leaves as well.
    static size_t CountLeaves(const TreeNode* node)
    {
        if(node == nullptr) return 0;
        size_t count = node->IsLeaf() ? 1 : 0;
        for(const auto& child : node->children) count += CountLeaves(child.get());
        return count;
    }
    static void CollectLeaves(const TreeNode* node, std::vector<uint64_t>& ids)
    {
        if(node == nullptr) return;
        if(node->IsLeaf())
        {
            ids.push_back(static_cast<const LeafNode*>(node)->track_id);
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
    ComputeKernelDetailsView* KernelDetailsPtr() const { return v.m_kernel_details.get(); }
    ComputeSelection* ComputeSelectionPtr() const { return v.m_compute_selection.get(); }
    bool PopupPending() const { return v.m_error_dialog_state == ComputeView::ErrorDialogState::kPending; }
    const std::string& PopupTitle() const { return v.m_popup_info.title; }
    const std::string& PopupMessage() const { return v.m_popup_info.message; }
};

struct ComputeKernelDetailsViewTestPeer
{
    using LayoutSettings = ComputeKernelDetailsView::LayoutSettings;

    ComputeKernelDetailsView& v;
    LayoutSettings&    Layout() const { return v.m_layout; }
    PresetComponent&   LayoutPreset() const { return *v.m_preset; }
    KernelMetricTable* KernelMetricTablePtr() const { return v.m_kernel_metric_table.get(); }
    ComputeTableView*  TableViewPtr() const
    {
        const ComputeMetricTablesPane* pane =
            dynamic_cast<const ComputeMetricTablesPane*>(
                PanePtr(ComputePane::kMetricTables));
        return pane ? pane->m_table_view.get() : nullptr;
    }
    // Same effect as picking B in the compare bar (turns comparing on).
    void     CompareWith(uint32_t workload_id, uint32_t kernel_id)
    {
        v.SetCompareTarget(workload_id, kernel_id);
    }
    void     StopCompare() { v.DisableCompare(); }
    bool     IsComparing() const { return v.m_compare.enabled; }
    uint32_t CompareWorkloadId() const { return v.m_compare.workload_id; }
    uint32_t CompareKernelId() const { return v.m_compare.kernel_id; }
    bool     DeltaAvailable() const { return v.DeltaAvailable(); }
    bool     MemoryChartShowsDelta() const
    {
        return PaneShows(ComputePane::kMemoryChart, CompareSide::kDelta);
    }
    Roofline* RooflinePtr() const
    {
        const ComputeRooflinePane* pane =
            dynamic_cast<const ComputeRooflinePane*>(PanePtr(ComputePane::kRoofline));
        return pane ? pane->m_roofline.get() : nullptr;
    }
    ComputeIsaView* IsaViewPtr() const
    {
        const ComputeIsaPane* pane =
            dynamic_cast<const ComputeIsaPane*>(PanePtr(ComputePane::kIsa));
        return pane ? pane->m_isa_view.get() : nullptr;
    }
    bool IsaShowsB() const { return PaneShows(ComputePane::kIsa, CompareSide::kB); }
    // Same effect as the ISA pane's A / B header buttons.
    void ShowIsaSide(CompareSide side) { ShowPaneSide(ComputePane::kIsa, side); }
    ComputeWorkloadView* WorkloadViewPtr() const
    {
        const ComputeWorkloadPane* pane = dynamic_cast<const ComputeWorkloadPane*>(
            PanePtr(ComputePane::kWorkloadDetails));
        return pane ? pane->m_workload_view.get() : nullptr;
    }
    bool WorkloadShowsB() const
    {
        return PaneShows(ComputePane::kWorkloadDetails, CompareSide::kB);
    }
    // Same effect as the Workload Details pane's A / B header buttons.
    void ShowWorkloadSide(CompareSide side)
    {
        ShowPaneSide(ComputePane::kWorkloadDetails, side);
    }
    // The rail List's kernels (workload, kernel), in order, before its search.
    std::vector<std::pair<uint32_t, uint32_t>> ListedKernels() const
    {
        std::vector<std::pair<uint32_t, uint32_t>> listed;
        for(const ComputeKernelDetailsView::ListedKernel& item : v.ListedKernels())
        {
            listed.emplace_back(item.workload->id, item.kernel->id);
        }
        return listed;
    }
    // Same effects as the rail's List / Table and the memory chart's A / B / delta
    // header buttons.
    void SetKernelRailTable(bool table) { v.SetKernelRailTableMode(table); }
    void ShowMemoryChartSide(CompareSide side)
    {
        ShowPaneSide(ComputePane::kMemoryChart, side);
    }

private:
    ComputeKernelDetailsPane* PanePtr(ComputePane pane) const
    {
        return v.m_panes[static_cast<size_t>(pane)].get();
    }
    ComputeSidedPane* SidedPane(ComputePane pane) const
    {
        return dynamic_cast<ComputeSidedPane*>(PanePtr(pane));
    }
    bool PaneShows(ComputePane pane, CompareSide side) const
    {
        const ComputeSidedPane* sided = SidedPane(pane);
        return sided && sided->m_side == side;
    }
    void ShowPaneSide(ComputePane pane, CompareSide side)
    {
        if(ComputeSidedPane* sided = SidedPane(pane))
        {
            sided->ShowSide(side);
        }
    }
};

struct ComputeIsaViewTestPeer
{
    const ComputeIsaView& v;
    uint32_t CurrentWorkloadId() const { return v.m_current_workload_id; }
    uint32_t CurrentKernelId() const { return v.m_current_kernel_id; }
    bool     FollowsSelection() const { return v.m_follow_selection; }
    // The shown kernel's ISA lines have arrived.
    bool     IsaLoaded() const { return v.m_isa.fetch.status == CodeLoadState::kLoaded; }
};

struct RooflineTestPeer
{
    const Roofline& v;
    // Seed of the ImPlot plot's id ("plot" hashed with it).
    ImGuiID PlotIdSeed() const { return v.m_plot_id; }
    bool IsCompareMode() const { return v.m_mode == Roofline::Compare; }
    bool HasSecondaryKernel() const { return v.m_kernel_secondary != nullptr; }
    // Intensity points plotted for the primary (A) or the secondary (B) kernel.
    size_t PlottedIntensities(bool secondary) const
    {
        const KernelInfo* kernel = secondary ? v.m_kernel_secondary : v.m_kernel_primary;
        size_t count = 0;
        for(const auto& item : v.m_items)
        {
            if(kernel && item.type == Roofline::ItemModel::Intensity &&
               item.parent_info.kernel == kernel &&
               item.visible[Roofline::ItemModel::Visible::Plot])
            {
                count++;
            }
        }
        return count;
    }
};

struct ComputeWorkloadViewTestPeer
{
    const ComputeWorkloadView& v;
    const WorkloadInfo* WorkloadInfoPtr() const { return v.m_workload_info; }
    bool                FollowsSelection() const { return v.m_follow_selection; }
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

struct MetricTableTestPeer
{
    const MetricTableBase& t;
    // Filled cells of the columns whose header ends with `suffix` (e.g. the
    // comparison's "\xCE\x94%"), i.e. values that were actually computed.
    size_t FilledCells(const std::string& suffix) const
    {
        size_t count = 0;
        for(const auto& column : t.m_columns)
        {
            const std::string& name = column.second;
            if(name.size() < suffix.size() ||
               name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
            {
                continue;
            }
            for(const auto& row : t.m_rows)
            {
                auto cell = row.second.values.find(column.first);
                if(cell != row.second.values.end() && !cell->second.value.empty()) count++;
            }
        }
        return count;
    }
    // Comparison columns ("Avg A", "Avg \xCE\x94%", ...) whose "Send metric to
    // kernel details" value is the header itself rather than its value ("Avg").
    // `checked` counts the value columns looked at.
    size_t ColumnsSendingHeader(size_t& checked) const
    {
        size_t wrong = 0;
        for(const auto& column : t.m_columns)
        {
            const std::string* value = t.ColumnValueName(column.first);
            if(value == nullptr) continue;
            checked++;
            const std::string& header = column.second;
            if(header == *value || header.compare(0, value->size() + 1, *value + " ") != 0)
                wrong++;
        }
        return wrong;
    }
};

struct ComputeTableViewTestPeer
{
    ComputeTableView& v;
    bool   HasAvailableMetrics() const { return v.m_has_available_metrics; }
    bool   FetchPending() const { return v.m_fetch_pending; }
    size_t TableWidgetCount() const { return v.m_table_widgets.size(); }
    bool   IsComparing() const { return v.m_compare_active; }
    bool   CompareLoading() const { return v.m_compare_loading || v.m_compare_fetch_pending; }
    // Filled B - A percentage cells across every built table.
    size_t DeltaPctCellCount() const
    {
        size_t count = 0;
        for(const auto& table : v.m_table_widgets)
        {
            count += MetricTableTestPeer{ table.second }.FilledCells("\xCE\x94%");
        }
        return count;
    }
    // Comparison columns across every built table that send their header as the
    // value name; `checked` counts the value columns looked at.
    size_t CompareColumnsSendingHeader(size_t& checked) const
    {
        size_t wrong = 0;
        for(const auto& table : v.m_table_widgets)
        {
            wrong += MetricTableTestPeer{ table.second }.ColumnsSendingHeader(checked);
        }
        return wrong;
    }
    // Each table's widget; the ImGui table ids (and so scroll positions) follow it.
    std::map<uint64_t, const MetricTable*> TableWidgets() const
    {
        std::map<uint64_t, const MetricTable*> widgets;
        for(const auto& table : v.m_table_widgets)
        {
            widgets[table.first] = &table.second;
        }
        return widgets;
    }
    // What a compare option change or a fetched kernel does to the tables.
    void RebuildTables() { v.RebuildTableDataCache(); }
    bool   IsTableShown(uint64_t table_key) const
    {
        return v.m_enabled_tables.count(table_key) > 0;
    }
    // Same effect as ticking/unticking the table in the Tables picker.
    void ShowTable(uint64_t table_key, bool shown)
    {
        if(shown)
        {
            v.m_enabled_tables.insert(table_key);
        }
        else
        {
            v.m_enabled_tables.erase(table_key);
        }
    }
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
    KernelMetricTable& v;
    // Same effect as typing `text` into a column's filter and Apply Filters.
    void ApplyFilter(int column, const char* text)
    {
        v.m_pending_column_filters.resize(v.m_permanent_column_names.size() +
                                          v.m_metrics_params.size());
        KernelMetricTable::ColumnFilter& filter = v.m_pending_column_filters[column];
        std::snprintf(filter.filter_text, sizeof(filter.filter_text), "%s", text);
        filter.is_active = filter.filter_text[0] != '\0';
        v.ApplyFilters();
    }
    int SortColumnIndex() const { return v.m_sort_column_index; }
    int SortOrder() const { return v.m_sort_order; }
    // The shown rows (every workload's) and each row's workload.
    const std::vector<std::vector<std::string>>& Rows() const { return v.m_rows; }
    const std::vector<uint32_t>& RowWorkloads() const { return v.m_row_workloads; }
    size_t MetricColumnCount() const { return v.m_metrics_params.size(); }
    // True until every workload's rows of the latest query have arrived.
    bool Fetching() const { return v.m_cycle_active || v.m_fetch_requested; }
    // Whether the latest query reported an error (e.g. an invalid filter).
    bool ShowedFetchError() const { return v.m_fetch_error_shown; }
    // The query the table would send `workload` were its picks `picks` (each an
    // entry and value name) with a filter on pick `filter_pick` (-1 for none);
    // `present` gets which picks it queries. `stored` replaces the picks' stored
    // queries (by default each entry's own). The table is left as it was.
    bool WorkloadQuery(const WorkloadInfo&                                                 workload,
                       const std::vector<std::pair<AvailableMetrics::Entry, std::string>>& picks,
                       int filter_pick, std::vector<std::string>& metrics,
                       std::vector<bool>& present,
                       const std::vector<std::string>* stored = nullptr)
    {
        const std::vector<KernelMetricTable::MetricInfo>   saved_info    = v.m_metrics_info;
        const std::vector<std::string>                     saved_params  = v.m_metrics_params;
        const std::vector<KernelMetricTable::ColumnFilter> saved_filters = v.m_column_filters;
        const std::vector<bool> saved_present = v.m_fetch_metric_present;

        const size_t permanent = v.m_permanent_column_names.size();
        SetPicks(picks);
        if(stored)
        {
            v.m_metrics_params = *stored;
        }
        v.m_column_filters.assign(permanent + picks.size(), KernelMetricTable::ColumnFilter());
        if(filter_pick >= 0)
        {
            KernelMetricTable::ColumnFilter& filter =
                v.m_column_filters[permanent + static_cast<size_t>(filter_pick)];
            std::snprintf(filter.filter_text, sizeof(filter.filter_text), "%s", "> 0");
            filter.is_active = true;
        }
        std::unordered_map<uint64_t, std::string> filters;
        int                                       sort_column = 0;
        const bool built = v.BuildWorkloadQuery(workload, metrics, filters, sort_column);
        present          = v.m_fetch_metric_present;

        v.m_metrics_info         = saved_info;
        v.m_metrics_params       = saved_params;
        v.m_column_filters       = saved_filters;
        v.m_fetch_metric_present = saved_present;
        return built;
    }
    // Whether the table would take `entry`'s `value_name` for a duplicate were
    // its picks `picks`. The table is left as it was.
    bool HasMetric(const std::vector<std::pair<AvailableMetrics::Entry, std::string>>& picks,
                   const AvailableMetrics::Entry& entry, const std::string& value_name)
    {
        const std::vector<KernelMetricTable::MetricInfo> saved_info   = v.m_metrics_info;
        const std::vector<std::string>                   saved_params = v.m_metrics_params;
        SetPicks(picks);
        const std::string query =
            MetricId{ entry.category_id, entry.table_id, entry.id }.ToString() + ":" +
            value_name;
        const bool found   = v.HasMetric(query, entry, value_name);
        v.m_metrics_info   = saved_info;
        v.m_metrics_params = saved_params;
        return found;
    }

private:
    void SetPicks(const std::vector<std::pair<AvailableMetrics::Entry, std::string>>& picks)
    {
        v.m_metrics_info.clear();
        v.m_metrics_params.clear();
        for(const std::pair<AvailableMetrics::Entry, std::string>& pick : picks)
        {
            v.m_metrics_info.push_back({ pick.first, pick.second });
            v.m_metrics_params.push_back(
                MetricId{ pick.first.category_id, pick.first.table_id, pick.first.id }
                    .ToString() +
                ":" + pick.second);
        }
    }
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

// Chunk size used when a track splits a fetch into requests. Shrinking it makes a
// short trace span several chunks.
struct TrackItemTestPeer
{
    TrackItem& v;
    uint64_t ChunkDuration() const { return v.m_chunk_duration_ns; }
    void     SetChunkDuration(uint64_t duration_ns) { v.m_chunk_duration_ns = duration_ns; }
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

    // All displayed tracks of any type, in track order.
    std::vector<TrackItem*> DisplayedTracks() const
    {
        std::vector<TrackItem*> tracks;
        if(!v.m_tracks) return tracks;
        for(TrackItem* track : *v.m_tracks)
        {
            if(track != nullptr && track->IsDisplayed()) tracks.push_back(track);
        }
        return tracks;
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
    SummaryTopEvents* TopEventsPtr(rocprofvis_analysis_summary_category_t category) const
    {
        return v.m_top_events[category].get();
    }
    TabContainer* TabContainerPtr() const { return v.m_tab_container.get(); }
};

// SummaryTopEvents drives the pie/bar/table top-event selection for one category.
// The pie is ImPlot-canvas drawn (no ImGui widget ID), so tests drive the model path
// (ToggleSelectEvent) rather than clicking a wedge, and assert on m_selected_idx.
struct SummaryTopEventsTestPeer
{
    SummaryTopEvents& v;
    // EventCount() is 0 when m_op_events is null (pre-load) or empty; one check covers both.
    size_t      EventCount() const { return v.m_op_events ? v.m_op_events->top.size() : 0; }
    // Return name of event at given index
    std::string EventName(size_t idx) const
    {
        return (v.m_op_events && idx < v.m_op_events->top.size())
                   ? v.m_op_events->top[idx].name
                   : std::string{};
    }
    uint64_t DurationSum(size_t idx) const
    {
        return (v.m_op_events && idx < v.m_op_events->top.size())
                   ? v.m_op_events->top[idx]
                         .metrics[kRPVAnalysisSummaryTopEventMetricDurationSum]
                         .numeric.uint
                   : 0;
    }
    std::optional<size_t> SelectedIdx() const { return v.m_selected_idx; }
    // Current chart/table display mode, exposed as booleans so the private
    // SummaryTopEvents::DisplayMode enum stays encapsulated.
    bool IsDisplayPie() const { return v.m_display_mode == SummaryTopEvents::Pie; }
    bool IsDisplayBar() const { return v.m_display_mode == SummaryTopEvents::Bar; }
    bool IsDisplayTable() const { return v.m_display_mode == SummaryTopEvents::Table; }
    // The synthetic "Others" bucket, if present. ToggleSelectEvent treats it as a
    // deselect, so tests must avoid selecting it.
    std::optional<size_t> PaddedIdx() const { return v.m_padded_idx; }
    // Toggles idx; on a cleared baseline (SelectedIdx()==nullopt) that is a select.
    // Caller must ClearSelection() first and avoid the padded index.
    void                  Select(size_t idx) { v.ToggleSelectEvent(idx); }
    void                  ClearSelection()
    {
        if(v.m_selected_idx.has_value()) v.ToggleSelectEvent(v.m_selected_idx.value());
    }
};

#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING
struct AssistantPanelTestPeer
{
    AssistantPanel& v;
    bool            HasConversation(const std::string& project_id) const
    {
        return v.m_conversations.count(project_id) > 0;
    }
    size_t LineCount(const std::string& project_id) const
    {
        const std::unordered_map<
            std::string, std::unique_ptr<AssistantPanel::Conversation>>::const_iterator
            it = v.m_conversations.find(project_id);
        return it == v.m_conversations.end() ? 0 : it->second->lines.size();
    }
    // Puts a user line in a trace's conversation without sending anything.
    void AddUserLine(const std::string& project_id, const std::string& text)
    {
        AssistantPanel::ChatLine line;
        line.speaker = AssistantPanel::Speaker::kUser;
        line.text    = text;
        v.ConversationFor(project_id).lines.push_back(line);
    }
    void DropConversation(const std::string& project_id)
    {
        v.m_conversations.erase(project_id);
    }
    const std::string& ViewProjectId() const { return v.m_view_project_id; }
};

struct TraceLinkStoreTestPeer
{
    TraceLinkStore&    v;
    const std::string& Path() const { return v.m_file_path; }
    // Points the store at another file and reloads from it.
    void UsePath(const std::string& path)
    {
        v.m_file_path = path;
        v.Load();
    }
};
#endif

}  // namespace View
}  // namespace RocProfVis
