// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_summary_model.h"

namespace RocProfVis
{
namespace View
{

void
SummaryModel::SetSummaryData(SummaryInfo::AggregateMetrics&& summary)
{
    m_summary = std::move(summary);
}

void
SummaryModel::Clear()
{
    m_summary = SummaryInfo::AggregateMetrics{};
}

}  // namespace View
}  // namespace RocProfVis
