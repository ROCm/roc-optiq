// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_trace_data_model.h"

namespace RocProfVis
{
namespace View
{

TraceDataModel::TraceDataModel() {}

void
TraceDataModel::Clear()
{
    m_topology.Clear();
    m_timeline.Clear();
    m_tables.ClearAllTables();
    m_summary.Clear();
    m_events.ClearEvents();
    m_trace_file_path.clear();
}

}  // namespace View
}  // namespace RocProfVis
