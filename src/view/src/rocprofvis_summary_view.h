// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "model/rocprofvis_model_types.h"
#include "rocprofvis_compare_panes.h"
#include "rocprofvis_event_manager.h"
#include "widgets/rocprofvis_infinite_scroll_table.h"
#include "widgets/rocprofvis_tab_container.h"
#include "widgets/rocprofvis_widget.h"
#include <functional>
#include <optional>
#include <utility>

struct ImPlotStyle;

namespace RocProfVis
{
namespace View
{

class DataProvider;
class HSplitContainer;
class SettingsManager;
class SummaryCompareOverview;
class SummaryCompareTopEvents;
class SummaryModel;
class SummaryOverview;
class SummaryTopEvents;
class SummaryTopology;
class TimelineSelection;
class VSplitContainer;
enum class TimeFormat;

/* Tab label and color wheel index of each summary category, in
 * rocprofvis_analysis_summary_category_t order. The label doubles as the
 * category's tab id.
 */
inline constexpr std::pair<const char*, size_t>
    SUMMARY_CATEGORY_DISPLAY[__kRPVAnalysisSummaryCategoryLast] = {
        { "Kernel Executions", 0 },
        { "Memory Operations", 2 },
        { "Thread Operations", 3 }
    };

class SummaryView : public RocWidget
{
    friend struct SummaryViewTestPeer;

public:
    SummaryView(DataProvider& dp, std::shared_ptr<TimelineSelection> timeline_selection);
    ~SummaryView();
    void Update() override;
    void Render() override;

private:
    ImVec2 MinSize() const;
    ImVec2 PreferredSize() const;
    // Issues the next fetch the summary is missing: the trace's, or each compare
    // source's in turn so the two never compete for the database.
    void  UpdateFetch();
    bool  Loading() const;
    float Progress() const;
    void  RebuildTabs();
    void  RenderSourceTopology(size_t source);

    std::unique_ptr<TabContainer>    m_tab_container;
    std::shared_ptr<SummaryOverview> m_overview;
    std::shared_ptr<SummaryTopology> m_topology;
    std::array<std::shared_ptr<SummaryTopEvents>, __kRPVAnalysisSummaryCategoryLast>
        m_top_events;

    // Compare traces show these in place of the single trace widgets above.
    bool                                                               m_compare_mode;
    std::shared_ptr<SummaryCompareOverview>                            m_compare_overview;
    std::array<std::shared_ptr<SummaryTopology>, COMPARE_SOURCE_COUNT> m_source_topology;
    std::shared_ptr<VSplitContainer>                                   m_compare_topology;
    std::array<std::shared_ptr<SummaryCompareTopEvents>,
               __kRPVAnalysisSummaryCategoryLast>
                                               m_compare_top_events;
    std::array<uint64_t, COMPARE_SOURCE_COUNT> m_source_fetched_revision;
    std::array<float, COMPARE_SOURCE_COUNT>    m_source_progress;

    uint64_t m_fetched_revision;
    bool     m_data_changed;
    bool     m_init_window;
    float    m_progress;

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;

    EventManager::SubscriptionToken m_time_format_changed_token;
    EventManager::SubscriptionToken m_progress_token;
};

class SummaryOverview : public RocWidget
{
public:
    SummaryOverview(DataProvider& dp, std::shared_ptr<SummaryTopology> topology,
                    const std::array<std::shared_ptr<SummaryTopEvents>,
                                     __kRPVAnalysisSummaryCategoryLast>& top_events,
                    TabContainer&                                        tab_container);
    void Render() override;

private:
    void Tile(ImGuiID id, ImVec2 size, const char* header, const char* body,
              std::function<void()> footer) const;
    void Tile(ImGuiID id, ImVec2 size, std::function<void()> content) const;

    TabContainer&                    m_tab_container;
    std::shared_ptr<SummaryTopology> m_topology;
    std::array<std::shared_ptr<SummaryTopEvents>, __kRPVAnalysisSummaryCategoryLast>
        m_top_events;

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;
};

class SummaryTopology : public RocWidget
{
public:
    // Shows the summary held in summary: the trace's, or one compare source's.
    SummaryTopology(DataProvider& dp, const SummaryModel& summary);
    void Update() override;
    void Render() override;

    void TimeFormatChanged();
    void Fetched();

    bool RenderPreview() const;
    // The time of each category in metrics as one bar; with labels it leaves a
    // band above and below for those that do not fit inside.
    void CompositionBar(ImVec2 size, const SummaryInfo::AggregateMetrics& metrics,
                        bool labels) const;

private:
    void RenderTopology();
    void RenderCounters() const;

    void CompositionBarSegment(ImVec2 bar_pos, ImVec2 bar_size, float segment_pct,
                               rocprofvis_analysis_summary_category_t category,
                               bool labels, float& segment_x) const;
    void CompositionTable(const SummaryInfo::AggregateMetrics& metrics,
                          float                                max_name_width);
    void Counters() const;
    void Counter(const SummaryInfo::Counters::Counter& counter, ImU32 color) const;

    std::unique_ptr<HSplitContainer> m_h_container;

    std::array<std::string, __kRPVAnalysisSummaryCategoryLast + 3>
                                         m_composition_table_headers;
    const SummaryInfo::AggregateMetrics* m_counters;

    bool m_data_changed;
    bool m_timeformat_changed;

    DataProvider&       m_data_provider;
    const SummaryModel& m_summary;
    SettingsManager&    m_settings;
};

// Lists the invocations of one summary event through its category's instance
// table. Given a compare source, it reads that source alone.
class SummaryInstanceTable : public InfiniteScrollTable
{
public:
    SummaryInstanceTable(DataProvider&                          dp,
                         std::shared_ptr<TimelineSelection>     timeline_selection,
                         rocprofvis_analysis_summary_category_t type,
                         TableType                              table_type,
                         rocprofvis_controller_table_type_t     request_table_type,
                         uint64_t                               request_id,
                         std::optional<uint64_t>                source = std::nullopt);
    // Status line and table, or a prompt to pick an event.
    void Render() override;
    // The table alone, for a parent that draws its own status.
    void RenderTable();
    void SearchInstances(const std::string&                            event_name,
                         std::optional<rocprofvis_dm_query_criteria_t> source_filter);
    void Clear();

    bool Searched() const;
    // A search is out with the controller or waiting for its table to free up.
    bool     Loading() const;
    uint64_t InstanceCount() const;

private:
    void UpdateFetchParams(std::shared_ptr<TableRequestParams>& params) const override;

    const rocprofvis_analysis_summary_category_t  m_type;
    const std::optional<uint64_t>                 m_source;
    std::string                                   m_event_name;
    std::optional<rocprofvis_dm_query_criteria_t> m_source_filter;
    bool                                          m_fetched;
};

class SummaryTopEvents : public RocWidget
{
    friend struct SummaryTopEventsTestPeer;

public:
    SummaryTopEvents(DataProvider&                          dp,
                     std::shared_ptr<TimelineSelection>     timeline_selection,
                     rocprofvis_analysis_summary_category_t type, TableType table_type,
                     rocprofvis_controller_table_type_t request_table_type,
                     uint64_t                           request_id);
    void Update() override;
    void Render() override;

    void TimeFormatChanged();
    void Fetched();

    bool Empty() const;
    bool RenderPreview() const;

private:
    enum DisplayMode
    {
        Pie,
        Bar,
        Table,
    };
    struct FilterModel
    {
        const char*                                                name;
        int                                                        selected_idx;
        const SummaryInfo::AggregateMetrics*                       parent_info;
        std::vector<const SummaryInfo::AggregateMetrics*>          info;
        std::vector<std::string>                                   labels;
        std::vector<std::optional<rocprofvis_dm_query_criteria_t>> query;
    };
    struct PieModel
    {
        struct Slice
        {
            float angle;
            float size_angle;
        };
        std::vector<const char*> labels;
        std::vector<float>       duration_pct_of_total;
        std::vector<Slice>       slices;
    };

    void RenderTopEvents();
    void RenderEventInstances();
    void RenderPieChart(const ImVec2 region, const ImGuiStyle& style,
                        const ImPlotStyle& plot_style, int& hovered_idx);
    void RenderBarChart(const ImVec2 region, const ImGuiStyle& style,
                        const ImPlotStyle& plot_style, TimeFormat time_format,
                        int& hovered_idx);
    void RenderTable(const ImPlotStyle& plot_style);
    void RenderLegend(const ImVec2 region, const ImGuiStyle& style,
                      const ImPlotStyle& plot_style, int& hovered_idx);

    // Input handling...
    int  PlotHoverIdx() const;
    void PlotInputHandler();
    void ToggleSelectEvent(const size_t& idx);

    std::unique_ptr<VSplitContainer>      m_v_container;
    std::unique_ptr<SummaryInstanceTable> m_instance_table;

    // Widget models...
    const SummaryInfo::TopOpEvents*                                      m_op_events;
    std::vector<FilterModel>                                             m_filters;
    PieModel                                                             m_pie;
    std::array<std::string, __kRPVAnalysisSummaryTopEventMetricLast + 1> m_table_headers;

    // Widget state/properties...
    const rocprofvis_analysis_summary_category_t m_type;
    DisplayMode                                  m_display_mode;
    bool                                         m_show_legend;
    std::optional<size_t>                        m_hovered_idx;
    std::optional<size_t>                        m_selected_idx;
    std::optional<size_t> m_padded_idx;  // Position of "Others" slice

    // Update flags...
    bool m_data_changed;
    bool m_filter_changed;
    bool m_timeformat_changed;

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;
};

}  // namespace View
}  // namespace RocProfVis
