// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_model_types.h"

namespace RocProfVis
{
namespace View
{
namespace Model
{

/**
 * @brief Manages aggregated trace statistics.
 * 
 * This model holds summary metrics computed from the trace data,
 * including kernel execution statistics and utilization metrics.
 */
class SummaryModel
{
public:
    SummaryModel() = default;
    ~SummaryModel() = default;

    // Summary data access
    const SummaryInfo::AggregateMetrics& GetSummary() const { return m_summary; }
    SummaryInfo::AggregateMetrics& GetSummary() { return m_summary; }
    
    void SetSummary(SummaryInfo::AggregateMetrics&& summary);
    void Clear();

private:
    SummaryInfo::AggregateMetrics m_summary;
};

}  // namespace Model
}  // namespace View
}  // namespace RocProfVis
