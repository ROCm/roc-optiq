// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_summary_model.h"

namespace RocProfVis
{
namespace View
{
namespace Model
{

void SummaryModel::SetSummary(SummaryInfo::AggregateMetrics&& summary)
{
    m_summary = std::move(summary);
}

void SummaryModel::Clear()
{
    m_summary = SummaryInfo::AggregateMetrics{};
}

}  // namespace Model
}  // namespace View
}  // namespace RocProfVis
