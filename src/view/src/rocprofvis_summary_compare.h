// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "model/rocprofvis_model_types.h"
#include "rocprofvis_compare_panes.h"
#include "rocprofvis_controller_analysis.h"
#include "rocprofvis_requests.h"
#include "widgets/rocprofvis_widget.h"
#include <array>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class HSplitContainer;
class SettingsManager;
class SummaryInstanceTable;
class SummaryModel;
class SummaryTopology;
class TabContainer;
class TimelineSelection;
class VSplitContainer;

// The top events of one name, across the compare sources.
struct SummaryEventDiff
{
    std::string name;
    // Null where a source has no event of that name in its top list.
    std::array<const SummaryInfo::TopOpEvents::Event*, COMPARE_SOURCE_COUNT> events;
    // Whether a source's top list was cut short into "Others", so an event it
    // lacks may still be in it.
    std::array<bool, COMPARE_SOURCE_COUNT> truncated;

    // A source's figures for the event are unknown: it is not listed, but may
    // be hidden in the "Others" remainder.
    bool Unknown(size_t source) const;
    // Every source's figures are known, so the change between them is too.
    bool   Known() const;
    double TotalNs(size_t source) const;
    double DeltaNs() const;
    // B relative to A; infinite for an event A does not have.
    double RelativeChange() const;
};

/* The compare form of the summary overview, built from the single trace
 * overview's tiles: each source's figures with the change between them and
 * the event composition of each.
 */
class SummaryCompareOverview : public RocWidget
{
public:
    // topology holds each source's own, which draws its composition bar.
    SummaryCompareOverview(DataProvider&                           dp,
                           const std::array<std::shared_ptr<SummaryTopology>,
                                            COMPARE_SOURCE_COUNT>& topology,
                           TabContainer&                           tab_container);
    void Update() override;
    void Render() override;

    void Fetched();

private:
    struct SourceFacts
    {
        double      span_ns = 0.0;
        uint64_t    events  = 0;
        std::string system;  // Host and GPU of the run
        std::array<double, __kRPVAnalysisSummaryCategoryLast>   category_ns    = {};
        std::array<uint64_t, __kRPVAnalysisSummaryCategoryLast> category_calls = {};
    };

    void RenderSources(ImVec2 tile_size);
    void RenderFigure(const char* label, double a, double b, bool is_time, ImVec2 size);
    void RenderComposition(float width);
    void RenderCategory(rocprofvis_analysis_summary_category_t category, ImVec2 size);

    std::array<SourceFacts, COMPARE_SOURCE_COUNT>                      m_facts;
    std::array<std::shared_ptr<SummaryTopology>, COMPARE_SOURCE_COUNT> m_topology;
    bool                                                               m_data_changed;

    DataProvider&    m_data_provider;
    TabContainer&    m_tab_container;
    SettingsManager& m_settings;
};

/* One category of events compared across the sources: a table of every event
 * with each source's figures and the change, narrowed by the same node, device,
 * process and thread filters as the single trace view, and each source's
 * instances of the selected event below it.
 */
class SummaryCompareTopEvents : public RocWidget
{
    friend struct SummaryCompareTopEventsTestPeer;

public:
    SummaryCompareTopEvents(DataProvider&                          dp,
                            std::shared_ptr<TimelineSelection>     timeline_selection,
                            rocprofvis_analysis_summary_category_t category,
                            TableType                              table_type,
                            rocprofvis_controller_table_type_t     request_table_type,
                            RequestType                            request_type);
    ~SummaryCompareTopEvents();
    void Update() override;
    void Render() override;

    void Fetched();
    // True when neither source has events of this category.
    bool Empty() const;

private:
    enum Column
    {
        kName,
        kCallsA,
        kCallsB,
        kTotalA,
        kTotalB,
        kDifference,
        kChange,
        kAvgA,
        kAvgB,
        kColumnCount
    };
    struct FilterOption
    {
        std::string label;
        // Null where a source has no events under this option.
        std::array<const SummaryInfo::AggregateMetrics*, COMPARE_SOURCE_COUNT> metrics;
        std::optional<rocprofvis_dm_query_criteria_t>                          criteria;
    };
    struct FilterLevel
    {
        const char*               name;
        size_t                    selected;
        std::vector<FilterOption> options;  // The first is "All"
    };

    void RebuildFilterLevel(size_t level);
    void RebuildRows();
    void SortRows();
    void SelectEvent(const std::string& name);
    void ClearSelection();

    void RenderEvents();
    void RenderFilters();
    void RenderTable(float height);
    void RenderTotalsRow();
    void RenderRow(const SummaryEventDiff& row, size_t index);
    void RenderValueCell(const SummaryEventDiff& row, size_t source,
                         rocprofvis_analysis_summary_top_event_metric_t metric);
    void RenderInstances(size_t source);

    const SummaryModel* SourceSummary(size_t source) const;

    const rocprofvis_analysis_summary_category_t m_category;

    std::vector<FilterLevel> m_filters;
    // What the filters narrow the events to, per source, and the criteria that
    // scope an instance search the same way.
    std::array<const SummaryInfo::TopOpEvents*, COMPARE_SOURCE_COUNT> m_op_events;
    std::optional<rocprofvis_dm_query_criteria_t>                     m_criteria;
    std::vector<SummaryEventDiff>                                     m_rows;
    std::optional<std::string>                                        m_selected;
    Column                                                            m_sort_column;
    bool                                                              m_sort_ascending;

    std::array<std::unique_ptr<SummaryInstanceTable>, COMPARE_SOURCE_COUNT> m_instances;
    std::shared_ptr<HSplitContainer> m_instance_split;
    std::unique_ptr<VSplitContainer> m_v_container;

    bool m_data_changed;
    bool m_filter_changed;

    DataProvider&    m_data_provider;
    SettingsManager& m_settings;
};

}  // namespace View
}  // namespace RocProfVis
