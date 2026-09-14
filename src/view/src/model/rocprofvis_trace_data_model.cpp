// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_trace_data_model.h"

namespace RocProfVis
{
namespace View
{

TraceDataModel::TraceDataModel()
: m_analysis(m_topology)
{}

void
TraceDataModel::SetCompareSources(const std::vector<CompareSourceInfo>& sources)
{
    m_compare_sources = sources;
}

const CompareSourceInfo*
TraceDataModel::GetCompareSource(size_t index) const
{
    return index < m_compare_sources.size() ? &m_compare_sources[index] : nullptr;
}

std::string
TraceDataModel::BuildTrackName(uint64_t track_id) const
{
    const TrackInfo* track_info = m_timeline.GetTrack(track_id);
    if(!track_info)
    {
        return "";
    }

    const TopologyTree& tdm = m_topology;

    std::string          name;
    std::string          processor_type_label;
    const ProcessorInfo* processor_info = tdm.GetProcessor(track_info->agent_or_pid);
    const ProcessInfo*   process_info = tdm.GetProcess(track_info->topology.process_id);

    if(processor_info)
    {
        tdm.GetProcessorTypeLabel(*processor_info, processor_type_label);
    }

    switch(track_info->topology.type)
    {
        case TrackInfo::TrackType::Queue:
        {
            // If the category is not "GPU Queue", use it as the name
            // For example, "Memory Copy", "Memory Allocation", etc
            if(track_info->category != "GPU Queue")
            {
                name = track_info->category;
            }
            else
            {
                name = track_info->sub_name;
            }
            if(process_info && tdm.ProcessCount() > 1)
            {
                name += " (PID:" + std::to_string(process_info->GetId()) + ")";
            }
            break;
        }
        case TrackInfo::TrackType::Stream:
        {
            name = track_info->main_name;
            if(processor_info)
            {
                name += " (" + processor_type_label + ": " +
                        processor_info->product_name + ")";
            }
            break;
        }
        case TrackInfo::TrackType::InstrumentedThread:
        {
            name = track_info->sub_name;
            break;
        }
        case TrackInfo::TrackType::SampledThread:
        {
            name = track_info->sub_name + " (S)";
            break;
        }
        case TrackInfo::TrackType::Counter:
        {
            // Get Processor (device) type label from using track's agent_or_pid, ex:
            // "GPU0".
            name = track_info->sub_name;
            if(process_info && tdm.ProcessCount() > 1)
            {
                name += " (PID:" + std::to_string(process_info->GetId()) + ")";
            }
            break;
        }
        default:
        {
            name = track_info->category + ":" + track_info->main_name + ":" +
                   track_info->sub_name;
            break;
        }
    }
    return name;
}

void
TraceDataModel::Clear()
{
    m_topology.Clear();
    m_timeline.Clear();
    m_tables.ClearAllTables();
    m_summary.Clear();
    m_events.ClearEvents();
    m_analysis.Clear();
    m_trace_file_path.clear();
    m_compare_sources.clear();
}

}  // namespace View
}  // namespace RocProfVis
