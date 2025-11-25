// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_track_topology.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_utils.h"
#include "rocprofvis_settings_manager.h"

namespace RocProfVis
{
namespace View
{

TrackTopology::TrackTopology(DataProvider& dp)
: m_data_provider(dp)
, m_topology_dirty(true)
, m_graphs_dirty(true)
, m_metadata_changed_event_token(EventManager::InvalidSubscriptionToken)
, m_format_changed_token(EventManager::InvalidSubscriptionToken)
{
    auto metadata_changed_event_handler = [this](std::shared_ptr<RocEvent> event) {
        if(event) {
            if(m_data_provider.GetTraceFilePath() == event->GetSourceId()) {
                m_topology_dirty = true;
                m_graphs_dirty = true;
            }
        }
    };
    m_metadata_changed_event_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        metadata_changed_event_handler);

    //subscribe to time format changed event
    auto format_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        // Reformat time columns
        FormatCells();
    };

    m_format_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), format_changed_handler);    
}

TrackTopology::~TrackTopology()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTrackMetadataChanged),
        m_metadata_changed_event_token);

    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kTimeFormatChanged), m_format_changed_token);
}

void
TrackTopology::Update()
{
    UpdateTopology();
    UpdateGraphs();
}

bool
TrackTopology::Dirty()
{
    return m_topology_dirty || m_graphs_dirty;
}

const TopologyModel&
TrackTopology::GetTopology() const
{
    return m_topology;
}

void
TrackTopology::UpdateTopology()
{
    if(m_topology_dirty && m_data_provider.GetState() == ProviderState::kReady)
    {
        m_topology.nodes.clear();
        m_topology.node_lut.clear();
        std::vector<const node_info_t*> node_infos = m_data_provider.GetNodeInfoList();
        m_topology.nodes.resize(node_infos.size());
        std::vector<std::vector<rocprofvis_graph_t*>> graph_categories(node_infos.size());
        m_topology.node_header = "Nodes (" + std::to_string(node_infos.size()) + ")";
        for(int i = 0; i < node_infos.size(); i++)
        {
            const node_info_t* node_info = node_infos[i];
            if(node_info)
            {
                m_topology.node_lut[node_info->id] = &m_topology.nodes[i];
                m_topology.nodes[i].info           = node_info;
                m_topology.nodes[i].info_table     = InfoTable{
                        { { InfoTable::Cell{ "OS", false },
                            InfoTable::Cell{ node_infos[i]->os_name, false } },
                          { InfoTable::Cell{ "OS Release", false },
                            InfoTable::Cell{ node_infos[i]->os_release, false } },
                          { InfoTable::Cell{ "OS Version", false },
                            InfoTable::Cell{ node_infos[i]->os_version, false } } }
                };
                for(const uint64_t& device_id : node_info->device_ids)
                {
                    const device_info_t* device_info =
                        m_data_provider.GetDeviceInfo(device_id);
                    if(device_info)
                    {
                        m_topology.nodes[i].info_table.cells.emplace_back(std::vector{
                            InfoTable::Cell{ device_info->type + " " +
                                                 std::to_string(device_info->type_index),
                                             false },
                            InfoTable::Cell{ device_info->product_name, false } });
                    }
                }
                const std::vector<uint64_t>& process_ids = node_infos[i]->process_ids;
                m_topology.nodes[i].processes.resize(process_ids.size());
                m_topology.nodes[i].process_header =
                    "Processes (" + std::to_string(process_ids.size()) + ")";
                for(int j = 0; j < process_ids.size(); j++)
                {
                    m_topology.nodes[i].process_lut[process_ids[j]] =
                        &m_topology.nodes[i].processes[j];
                    const process_info_t* process_info =
                        m_data_provider.GetProcessInfo(process_ids[j]);
                    if(process_info)
                    {
                        m_topology.nodes[i].processes[j].info       = process_info;
                        m_topology.nodes[i].processes[j].info_table = InfoTable{
                            { { InfoTable::Cell{ "Start Time", false },
                                InfoTable::Cell{
                                    std::to_string(process_info->start_time), false, true,
                                    [this](const std::string& raw,
                                           std::string&       formatted_out) {
                                        return FormatTimeCell(raw, formatted_out);
                                    } } },
                              { InfoTable::Cell{ "End Time", false },
                                InfoTable::Cell{
                                    std::to_string(process_info->end_time), false, true,
                                    [this](const std::string& raw,
                                           std::string&       formatted_out) {
                                        return FormatTimeCell(raw, formatted_out);
                                    } } },
                              { InfoTable::Cell{ "Command", false },
                                InfoTable::Cell{ process_info->command, false } },
                              { InfoTable::Cell{ "Environment", false },
                                InfoTable::Cell{ process_info->environment, false } } }
                        };
                        m_topology.nodes[i].processes[j].header =
                            "[" + std::to_string(process_info->id) + "]";
                        const std::vector<uint64_t>& queue_ids = process_info->queue_ids;
                        m_topology.nodes[i].processes[j].queues.resize(queue_ids.size());
                        m_topology.nodes[i].processes[j].queue_header =
                            "Queues (" + std::to_string(queue_ids.size()) + ")";
                        for(int k = 0; k < queue_ids.size(); k++)
                        {
                            m_topology.nodes[i].processes[j].queue_lut[queue_ids[k]] =
                                &m_topology.nodes[i].processes[j].queues[k];
                            const queue_info_t* queue_info =
                                m_data_provider.GetQueueInfo(queue_ids[k]);
                            if(queue_info)
                            {
                                const device_info_t* device_info =
                                    m_data_provider.GetDeviceInfo(queue_info->device_id);
                                m_topology.nodes[i].processes[j].queues[k].info =
                                    queue_info;
                                if(device_info)
                                {
                                    m_topology.nodes[i]
                                        .processes[j]
                                        .queues[k]
                                        .info_table = InfoTable{
                                        { { InfoTable::Cell{
                                                device_info->type + " " +
                                                    std::to_string(
                                                        device_info->type_index),
                                                false },
                                            InfoTable::Cell{ device_info->product_name,
                                                             false } } }
                                    };
                                }
                            }
                        }

                        const std::vector<uint64_t>& stream_ids = process_info->stream_ids;
                        m_topology.nodes[i].processes[j].streams.resize(stream_ids.size());
                        m_topology.nodes[i].processes[j].stream_header =
                            "Streams (" + std::to_string(stream_ids.size()) + ")";
                        for(int k = 0; k < stream_ids.size(); k++)
                        {
                            m_topology.nodes[i].processes[j].stream_lut[stream_ids[k]] =
                                &m_topology.nodes[i].processes[j].streams[k];
                            const stream_info_t* stream_info =
                                m_data_provider.GetStreamInfo(stream_ids[k]);
                            if(stream_info)
                            {
                                const device_info_t* device_info =
                                    m_data_provider.GetDeviceInfo(stream_info->device_id);
                                m_topology.nodes[i].processes[j].streams[k].info =
                                    stream_info;
                                if(device_info)
                                {
                                    m_topology.nodes[i]
                                        .processes[j]
                                        .streams[k]
                                        .info_table = InfoTable{
                                        { { InfoTable::Cell{
                                                device_info->type + " " +
                                                    std::to_string(
                                                        device_info->type_index),
                                                false },
                                            InfoTable::Cell{ device_info->product_name,
                                                             false } } }
                                    };
                                }
                            }
                        }

                        const std::vector<uint64_t>& instrumented_thread_ids =
                            process_info->instrumented_thread_ids;
                        m_topology.nodes[i].processes[j].instrumented_threads.resize(
                            instrumented_thread_ids.size());
                        m_topology.nodes[i].processes[j].instrumented_thread_header =
                            "Threads (" + std::to_string(instrumented_thread_ids.size()) +
                            ")";
                        for(int k = 0; k < instrumented_thread_ids.size(); k++)
                        {
                            m_topology.nodes[i]
                                .processes[j]
                                .instrumented_thread_lut[instrumented_thread_ids[k]] =
                                &m_topology.nodes[i].processes[j].instrumented_threads[k];
                            const thread_info_t* thread_info =
                                m_data_provider.GetInstrumentedThreadInfo(instrumented_thread_ids[k]);
                            if(thread_info)
                            {
                                m_topology.nodes[i]
                                    .processes[j]
                                    .instrumented_threads[k]
                                    .info = thread_info;
                                m_topology.nodes[i]
                                    .processes[j]
                                    .instrumented_threads[k]
                                    .info_table = InfoTable{ {
                                    { InfoTable::Cell{ "Start Time", false },
                                      InfoTable::Cell{
                                          std::to_string(thread_info->start_time), false,
                                          true,
                                          [this](const std::string& raw,
                                                 std::string&       formatted_out) {
                                              return FormatTimeCell(raw, formatted_out);
                                          } } },
                                    { InfoTable::Cell{ "End Time", false },
                                      InfoTable::Cell{
                                          std::to_string(thread_info->end_time), false,
                                          true,
                                          [this](const std::string& raw,
                                                 std::string&       formatted_out) {
                                              return FormatTimeCell(raw, formatted_out);
                                          } } },
                                } };
                            }
                        }

                        const std::vector<uint64_t>& sampled_thread_ids =
                            process_info->sampled_thread_ids;
                        m_topology.nodes[i].processes[j].sampled_threads.resize(
                            sampled_thread_ids.size());
                        m_topology.nodes[i].processes[j].sampled_thread_header =
                            "Sample Threads (" +
                            std::to_string(sampled_thread_ids.size()) + ")";
                        for(int k = 0; k < sampled_thread_ids.size(); k++)
                        {
                            m_topology.nodes[i]
                                .processes[j]
                                .sampled_thread_lut[sampled_thread_ids[k]] =
                                &m_topology.nodes[i].processes[j].sampled_threads[k];
                            const thread_info_t* thread_info =
                                m_data_provider.GetSampledThreadInfo(sampled_thread_ids[k]);
                            if(thread_info)
                            {
                                m_topology.nodes[i].processes[j].sampled_threads[k].info =
                                    thread_info;
                                m_topology.nodes[i]
                                    .processes[j]
                                    .sampled_threads[k]
                                    .info_table = InfoTable{ {
                                    { InfoTable::Cell{ "Start Time", false },
                                      InfoTable::Cell{
                                          std::to_string(thread_info->start_time), false,
                                          true,
                                          [this](const std::string& raw,
                                                 std::string&       formatted_out) {
                                              return FormatTimeCell(raw, formatted_out);
                                          } } },
                                    { InfoTable::Cell{ "End Time", false },
                                      InfoTable::Cell{
                                          std::to_string(thread_info->end_time), false,
                                          true,
                                          [this](const std::string& raw,
                                                 std::string&       formatted_out) {
                                              return FormatTimeCell(raw, formatted_out);
                                          } } },
                                } };
                            }
                        }
                        const std::vector<uint64_t>& counter_ids =
                            process_info->counter_ids;
                        m_topology.nodes[i].processes[j].counters.resize(
                            counter_ids.size());
                        m_topology.nodes[i].processes[j].counter_header =
                            "Counters (" + std::to_string(counter_ids.size()) + ")";
                        for(int k = 0; k < counter_ids.size(); k++)
                        {
                            m_topology.nodes[i].processes[j].counter_lut[counter_ids[k]] =
                                &m_topology.nodes[i].processes[j].counters[k];
                            const counter_info_t* counter_info =
                                m_data_provider.GetCounterInfo(counter_ids[k]);
                            if(counter_info)
                            {
                                const device_info_t* device_info =
                                    m_data_provider.GetDeviceInfo(
                                        counter_info->device_id);
                                m_topology.nodes[i].processes[j].counters[k].info =
                                    counter_info;
                                m_topology.nodes[i].processes[j].counters[k].info_table =
                                    InfoTable{
                                        { { InfoTable::Cell{ "Description", false },
                                            InfoTable::Cell{ counter_info->description,
                                                             false } },
                                          { InfoTable::Cell{ "Units", false },
                                            InfoTable::Cell{ counter_info->units,
                                                             false } },
                                          { InfoTable::Cell{ "Value Type", false },
                                            InfoTable::Cell{ counter_info->value_type,
                                                             false } } }
                                    };
                                if(device_info)
                                {
                                    m_topology.nodes[i]
                                        .processes[j]
                                        .counters[k]
                                        .info_table.cells.push_back(
                                            { InfoTable::Cell{
                                                  device_info->type + " " +
                                                      std::to_string(
                                                          device_info->type_index),
                                                  false },
                                              InfoTable::Cell{ device_info->product_name,
                                                               false } });
                                }
                            }
                        }
                    }
                }
            }
        }
        FormatCells();
        m_topology_dirty = false;
        EventManager::GetInstance()->AddEvent(
            std::make_shared<RocEvent>(static_cast<int>(RocEvents::kTopologyChanged),
                                       m_data_provider.GetTraceFilePath()));
    }
}

void
TrackTopology::FormatCells()
{
    for(auto& node : m_topology.nodes)
    {
        for(auto& process : node.processes)
        {
            // Format process table
            for(auto& row : process.info_table.cells)
            {
                for(auto& cell : row)
                {
                    if(cell.needs_format && cell.formatter)
                    {
                        cell.formatter(cell.data, cell.formatted);
                    }
                }
            }
            // Format child tables
            for(auto& t : process.instrumented_threads)
            {
                for(auto& row : t.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
            for(auto& t : process.sampled_threads)
            {
                for(auto& row : t.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
            for(auto& s : process.streams)
            {
                for(auto& row : s.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
            for(auto& q : process.queues)
            {
                for(auto& row : q.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
            for(auto& c : process.counters)
            {
                for(auto& row : c.info_table.cells)
                {
                    for(auto& cell : row)
                    {
                        if(cell.needs_format && cell.formatter)
                        {
                            cell.formatter(cell.data, cell.formatted);
                        }
                    }
                }
            }
        }
    }
}

bool
TrackTopology::FormatTimeCell(const std::string& raw, std::string& formatted_out)
{
    SettingsManager& settings    = SettingsManager::GetInstance();
    auto             time_format = settings.GetUserSettings().unit_settings.time_format;

    formatted_out = nanosecond_to_formatted_str(std::stod(raw), time_format, true);
    return true;
}

void
TrackTopology::UpdateGraphs()
{
    if(m_graphs_dirty && m_data_provider.GetState() == ProviderState::kReady)
    {
        m_topology.uncategorized_graph_indices.clear();
        for(const track_info_t* track : m_data_provider.GetTrackInfoList())
        {
            if(track)
            {
                const uint64_t& node_id    = track->topology.node_id;
                const uint64_t& process_id = track->topology.process_id;
                const uint64_t& id         = track->topology.id;
                const uint64_t& index      = track->index;
                switch(track->topology.type)
                {
                    case track_info_t::Topology::Queue:
                    {
                        m_topology.node_lut[node_id]
                            ->process_lut[process_id]
                            ->queue_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    case track_info_t::Topology::Stream:
                    {
                        m_topology.node_lut[node_id]
                            ->process_lut[process_id]
                            ->stream_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    case track_info_t::Topology::InstrumentedThread:
                    {
                        m_topology.node_lut[node_id]
                            ->process_lut[process_id]
                            ->instrumented_thread_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    case track_info_t::Topology::SampledThread:
                    {
                        m_topology.node_lut[node_id]
                            ->process_lut[process_id]
                            ->sampled_thread_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    case track_info_t::Topology::Counter:
                    {
                        m_topology.node_lut[node_id]
                            ->process_lut[process_id]
                            ->counter_lut[id]
                            ->graph_index = index;
                        break;
                    }
                    default:
                    {
                        m_topology.uncategorized_graph_indices.push_back(index);
                        break;
                    }
                }
            }
        }
        m_graphs_dirty = false;
    }
}

}  // namespace View
}  // namespace RocProfVis