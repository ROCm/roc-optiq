// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_analysis_model.h"
#include "rocprofvis_event_model.h"
#include "rocprofvis_model_types.h"
#include "rocprofvis_summary_model.h"
#include "rocprofvis_tables_model.h"
#include "rocprofvis_timeline_model.h"
#include "rocprofvis_topology_model.h"

#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{

/**
 * @brief Aggregates all trace data models.
 *
 * This is the root data model that provides a unified interface to access
 * topology, timeline, table, and summary data. It acts as a facade for
 * the underlying specialized models.
 */
class TraceDataModel
{
public:
    TraceDataModel();
    ~TraceDataModel() = default;

    // Sub-model access
    const TopologyDataModel& GetTopology() const { return m_topology; }
    TopologyDataModel&       GetTopology() { return m_topology; }

    const TimelineModel& GetTimeline() const { return m_timeline; }
    TimelineModel&       GetTimeline() { return m_timeline; }

    const TablesModel& GetTables() const { return m_tables; }
    TablesModel&       GetTables() { return m_tables; }

    const SummaryModel& GetSummary() const { return m_summary; }
    SummaryModel&       GetSummary() { return m_summary; }

    const EventModel& GetEvents() const { return m_events; }
    EventModel&       GetEvents() { return m_events; }

    const AnalysisModel& GetAnalysis() const { return m_analysis; }
    AnalysisModel&       GetAnalysis() { return m_analysis; }

    // Trace file metadata
    const std::string& GetTraceFilePath() const { return m_trace_file_path; }
    void SetTraceFilePath(const std::string& path) { m_trace_file_path = path; }

    // Source tags map a track's source file index (file_id) back to its .db file. They are
    // set for both compare projects (A, B, ...) and merged/combined projects (so the source
    // badge can tell otherwise-identical tracks from same-hardware files apart).
    // GetCompareSource maps a track's source instance index back to its file.
    void SetCompareSources(const std::vector<CompareSourceInfo>& sources);
    const CompareSourceInfo* GetCompareSource(size_t index) const;
    bool HasCompareSources() const { return !m_compare_sources.empty(); }

    // Whether counterpart tracks from each source should be reordered to sit adjacent on the
    // timeline. Only compare projects want this; a plain merge keeps its natural (file-
    // grouped) load order even though it, too, carries source tags for the badge.
    void SetReorderCounterparts(bool reorder) { m_reorder_counterparts = reorder; }
    bool ShouldReorderCounterparts() const { return m_reorder_counterparts; }

    // Build display name for a track from topology/timeline data
    std::string BuildTrackName(uint64_t track_id) const;

    // Clear all data
    void Clear();

private:
    TopologyDataModel m_topology;
    TimelineModel     m_timeline;
    TablesModel       m_tables;
    SummaryModel      m_summary;
    EventModel        m_events;
    AnalysisModel     m_analysis;

    std::string m_trace_file_path;
    std::vector<CompareSourceInfo> m_compare_sources;
    bool m_reorder_counterparts = false;
};

}  // namespace View
}  // namespace RocProfVis
