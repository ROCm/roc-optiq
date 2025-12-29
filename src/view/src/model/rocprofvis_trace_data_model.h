// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_model_types.h"
#include "rocprofvis_topology_model.h"
#include "rocprofvis_timeline_model.h"
#include "rocprofvis_tables_model.h"
#include "rocprofvis_summary_model.h"

#include <memory>
#include <string>

namespace RocProfVis
{
namespace View
{
namespace Model
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
    const TopologyModel& GetTopology() const { return m_topology; }
    TopologyModel& GetTopology() { return m_topology; }

    const TimelineModel& GetTimeline() const { return m_timeline; }
    TimelineModel& GetTimeline() { return m_timeline; }

    const TablesModel& GetTables() const { return m_tables; }
    TablesModel& GetTables() { return m_tables; }

    const SummaryModel& GetSummary() const { return m_summary; }
    SummaryModel& GetSummary() { return m_summary; }

    // Trace file metadata
    const std::string& GetTraceFilePath() const { return m_trace_file_path; }
    void SetTraceFilePath(const std::string& path) { m_trace_file_path = path; }

    // Clear all data
    void Clear();

private:
    TopologyModel m_topology;
    TimelineModel m_timeline;
    TablesModel   m_tables;
    SummaryModel  m_summary;

    std::string m_trace_file_path;
};

}  // namespace Model
}  // namespace View
}  // namespace RocProfVis
