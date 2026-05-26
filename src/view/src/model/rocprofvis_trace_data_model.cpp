// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_trace_data_model.h"

#include <yaml-cpp/yaml.h>

namespace RocProfVis
{
namespace View
{

TraceDataModel::TraceDataModel()
: m_analysis(m_topology)
{}

void
TraceDataModel::LoadCompareMetadata(const std::string& path)
{
    m_compare_sources.clear();
    try
    {
        YAML::Node root  = YAML::LoadFile(path);
        YAML::Node optiq = root["optiq"];
        if(!optiq || !optiq["compare"].as<bool>(false))
        {
            return;
        }
        YAML::Node files = optiq["compare_files"];
        if(!files || !files.IsSequence())
        {
            return;
        }
        for(const YAML::Node& entry : files)
        {
            CompareSourceInfo info;
            info.id   = entry["id"].as<std::string>("");
            info.name = entry["name"].as<std::string>("");
            info.path = entry["path"].as<std::string>("");
            if(!info.id.empty())
            {
                m_compare_sources.push_back(std::move(info));
            }
        }
    }
    catch(const YAML::Exception&)
    {
        m_compare_sources.clear();
    }
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

    const TopologyDataModel& tdm = m_topology;

    std::string       name;
    std::string       device_type_label;
    const DeviceInfo* device_info   = tdm.GetDevice(track_info->agent_or_pid);
    const ProcessInfo* process_info = tdm.GetProcess(track_info->topology.process_id);

    if(device_info)
    {
        tdm.GetDeviceTypeLabel(*device_info, device_type_label);
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
                name += " (PID:" + std::to_string(process_info->id) + ")";
            }
            break;
        }
        case TrackInfo::TrackType::Stream:
        {
            name = track_info->main_name;
            if(device_info)
            {
                name +=
                    " (" + device_type_label + ": " + device_info->product_name + ")";
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
                name += " (PID:" + std::to_string(process_info->id) + ")";
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
