// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_model_types.h"
#include "rocprofvis_tables_model.h"

namespace RocProfVis
{
namespace View
{

enum class TimeFormat;
/**
 * @brief Manages aggregated trace statistics.
 *
 * This model holds summary metrics computed from the trace data,
 * including kernel execution statistics and utilization metrics.
 */
class SummaryModel
{
public:
    SummaryModel();
    ~SummaryModel() = default;

    // Summary data access
    const SummaryInfo::AggregateMetrics& GetSummaryData() const;
    SummaryInfo::AggregateMetrics&       GetSummaryData();

    const TablesModel& GetTables() const;
    TablesModel&       GetTables();

    const std::vector<SummaryInfo::AggregateMetrics>& GetSummaryNodes() const;

    const SummaryInfo::TopOpEvents* TopOpEvents(
        const SummaryInfo::AggregateMetrics&   metrics,
        rocprofvis_analysis_summary_category_t type) const;

    void RegisterEventValue(SummaryInfo::Value& value);
    void RegisterCounterValue(SummaryInfo::Value& value);
    void RegisterTimeunitValue(SummaryInfo::Value& value);
    void UpdateValueStrings();

    void     SetRevision(uint64_t revision);
    uint64_t GetRevision() const;
    void     Clear();

private:
    SummaryInfo::AggregateMetrics m_summary;
    TablesModel                   m_tables;

    std::vector<SummaryInfo::Value*> m_event_values;
    std::vector<SummaryInfo::Value*> m_counter_values;
    std::vector<SummaryInfo::Value*> m_timeunit_values;

    uint64_t m_revision;
};

}  // namespace View
}  // namespace RocProfVis
