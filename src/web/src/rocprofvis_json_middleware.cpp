// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_json_middleware.h"

#include "spdlog/spdlog.h"

#include <cfloat>

namespace RocProfVis
{
namespace Web
{

JsonMiddleware::JsonMiddleware() = default;

JsonMiddleware::~JsonMiddleware()
{
    if(m_controller)
    {
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
    }
}

bool
JsonMiddleware::IsLoaded() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_controller != nullptr;
}

std::string
JsonMiddleware::ReadString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                           uint64_t index)
{
    uint32_t        length = 0;
    rocprofvis_result_t res =
        rocprofvis_controller_get_string(handle, property, index, nullptr, &length);
    if(res != kRocProfVisResultSuccess || length == 0)
        return "";

    std::string buf(length, '\0');
    res = rocprofvis_controller_get_string(handle, property, index, buf.data(), &length);
    if(res != kRocProfVisResultSuccess)
        return "";

    if(!buf.empty() && buf.back() == '\0')
        buf.pop_back();
    return buf;
}

rocprofvis_result_t
JsonMiddleware::WaitForFuture(rocprofvis_controller_future_t* future, float timeout)
{
    return rocprofvis_controller_future_wait(future, timeout);
}

jt::Json
JsonMiddleware::MakeError(const std::string& message)
{
    jt::Json root;
    root.setObject();
    root["status"]  = "error";
    root["message"] = message;
    return root;
}

jt::Json
JsonMiddleware::MakeSuccess(const std::string& message)
{
    jt::Json root;
    root.setObject();
    root["status"] = "ok";
    if(!message.empty())
        root["message"] = message;
    return root;
}

// ---------- Trace Lifecycle ----------

jt::Json
JsonMiddleware::LoadTrace(const std::string& file_path)
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_controller)
    {
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
        m_timeline   = nullptr;
        m_track_handles.clear();
        m_graph_handles.clear();
        m_cached_topology      = jt::Json();
        m_cached_tracks        = jt::Json();
        m_cached_timeline_info = jt::Json();
    }

    m_controller = rocprofvis_controller_alloc(file_path.c_str());
    if(!m_controller)
        return MakeError("Failed to allocate controller for: " + file_path);

    rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
    if(!future)
    {
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
        return MakeError("Failed to allocate future");
    }

    rocprofvis_result_t res = rocprofvis_controller_load_async(m_controller, future);
    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
        return MakeError("Failed to start async load");
    }

    res = WaitForFuture(future, 120.0f);
    rocprofvis_controller_future_free(future);

    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
        return MakeError("Trace load timed out or failed");
    }

    res = rocprofvis_controller_get_object(m_controller, kRPVControllerSystemTimeline, 0,
                                           &m_timeline);
    if(res != kRocProfVisResultSuccess || !m_timeline)
    {
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
        return MakeError("Failed to get timeline from controller");
    }

    LoadTopologyData();
    LoadTimelineData();

    jt::Json result = MakeSuccess("Trace loaded");
    result["file_path"] = file_path;
    result["timeline"]  = m_cached_timeline_info;
    return result;
}

jt::Json
JsonMiddleware::CloseTrace()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if(m_controller)
    {
        rocprofvis_controller_free(m_controller);
        m_controller = nullptr;
        m_timeline   = nullptr;
        m_track_handles.clear();
        m_graph_handles.clear();
        m_cached_topology      = jt::Json();
        m_cached_tracks        = jt::Json();
        m_cached_timeline_info = jt::Json();
    }
    return MakeSuccess("Trace closed");
}

// ---------- Internal Loading ----------

void
JsonMiddleware::LoadTopologyData()
{
    m_cached_topology.setObject();

    jt::Json nodes_arr;
    nodes_arr.setArray();

    uint64_t num_nodes = 0;
    rocprofvis_controller_get_uint64(m_controller, kRPVControllerSystemNumNodes, 0,
                                     &num_nodes);

    for(uint64_t i = 0; i < num_nodes; i++)
    {
        rocprofvis_handle_t* node_handle = nullptr;
        rocprofvis_controller_get_object(m_controller, kRPVControllerSystemNodeIndexed, i,
                                         &node_handle);
        if(node_handle)
            nodes_arr.getArray().push_back(SerializeNode(node_handle));
    }

    m_cached_topology["nodes"] = std::move(nodes_arr);
}

void
JsonMiddleware::LoadTimelineData()
{
    m_cached_timeline_info.setObject();
    m_cached_tracks.setArray();

    if(!m_timeline)
        return;

    double min_ts = 0, max_ts = 0;
    rocprofvis_controller_get_double(m_timeline, kRPVControllerTimelineMinTimestamp, 0,
                                     &min_ts);
    rocprofvis_controller_get_double(m_timeline, kRPVControllerTimelineMaxTimestamp, 0,
                                     &max_ts);

    uint64_t num_graphs = 0;
    rocprofvis_controller_get_uint64(m_timeline, kRPVControllerTimelineNumGraphs, 0,
                                     &num_graphs);

    m_cached_timeline_info["min_timestamp"] = min_ts;
    m_cached_timeline_info["max_timestamp"] = max_ts;
    m_cached_timeline_info["num_tracks"]    = (long long)num_graphs;

    for(uint64_t i = 0; i < num_graphs; i++)
    {
        rocprofvis_handle_t* track_handle = nullptr;
        rocprofvis_controller_get_object(m_controller, kRPVControllerSystemTrackIndexed, i,
                                         &track_handle);
        if(!track_handle)
            continue;

        uint64_t track_id = 0;
        rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackId, 0,
                                         &track_id);

        m_track_handles[track_id] = track_handle;

        rocprofvis_handle_t* graph_handle = nullptr;
        rocprofvis_controller_get_object(m_timeline, kRPVControllerTimelineGraphIndexed, i,
                                         &graph_handle);
        if(graph_handle)
            m_graph_handles[track_id] = graph_handle;

        m_cached_tracks.getArray().push_back(SerializeTrackMeta(track_handle, i));
    }
}

// ---------- Public API ----------

jt::Json
JsonMiddleware::GetTopology()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_controller)
        return MakeError("No trace loaded");

    jt::Json result = MakeSuccess();
    result["topology"] = m_cached_topology;
    return result;
}

jt::Json
JsonMiddleware::GetTimelineInfo()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_controller)
        return MakeError("No trace loaded");

    jt::Json result = MakeSuccess();
    result["timeline"] = m_cached_timeline_info;
    return result;
}

jt::Json
JsonMiddleware::GetTracks()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_controller)
        return MakeError("No trace loaded");

    jt::Json result = MakeSuccess();
    result["tracks"]   = m_cached_tracks;
    return result;
}

jt::Json
JsonMiddleware::GetTrackGraphData(uint64_t track_id, double start, double end,
                                  uint32_t resolution)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_controller)
        return MakeError("No trace loaded");

    auto graph_it = m_graph_handles.find(track_id);
    if(graph_it == m_graph_handles.end())
        return MakeError("Track not found: " + std::to_string(track_id));

    rocprofvis_controller_future_t* future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_array_t*  arr    = rocprofvis_controller_array_alloc(256);
    if(!future || !arr)
    {
        if(future) rocprofvis_controller_future_free(future);
        if(arr) rocprofvis_controller_array_free(arr);
        return MakeError("Failed to allocate future/array");
    }

    rocprofvis_result_t res = rocprofvis_controller_graph_fetch_async(
        m_controller, reinterpret_cast<rocprofvis_controller_graph_t*>(graph_it->second),
        start, end, resolution, future, arr);

    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_array_free(arr);
        return MakeError("Failed to start graph fetch");
    }

    res = WaitForFuture(future);
    rocprofvis_controller_future_free(future);

    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_array_free(arr);
        return MakeError("Graph fetch timed out or failed");
    }

    uint64_t num_entries = 0;
    rocprofvis_controller_get_uint64(arr, kRPVControllerArrayNumEntries, 0, &num_entries);

    auto     track_it   = m_track_handles.find(track_id);
    uint64_t track_type = kRPVControllerTrackTypeEvents;
    if(track_it != m_track_handles.end())
    {
        rocprofvis_controller_get_uint64(track_it->second, kRPVControllerTrackType, 0,
                                         &track_type);
    }

    jt::Json entries;
    entries.setArray();
    for(uint64_t i = 0; i < num_entries; i++)
    {
        rocprofvis_handle_t* entry = nullptr;
        rocprofvis_controller_get_object(arr, kRPVControllerArrayEntryIndexed, i, &entry);
        if(!entry)
            continue;

        if(track_type == kRPVControllerTrackTypeEvents)
            entries.getArray().push_back(SerializeEvent(entry));
        else
            entries.getArray().push_back(SerializeSample(entry));
    }

    rocprofvis_controller_array_free(arr);

    jt::Json result = MakeSuccess();
    result["track_id"]   = (long long)track_id;
    result["track_type"] = (track_type == kRPVControllerTrackTypeEvents) ? "events" : "samples";
    result["num_entries"] = (long long)num_entries;
    result["entries"]     = std::move(entries);
    return result;
}

jt::Json
JsonMiddleware::GetEventTable(const std::vector<uint64_t>& track_ids, double start,
                              double end, uint64_t offset, uint64_t count,
                              uint64_t sort_col, bool sort_ascending,
                              const std::string& filter, const std::string& group,
                              const std::string& group_cols)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_controller)
        return MakeError("No trace loaded");

    // Get the event table handle from the controller (singleton, not allocated)
    rocprofvis_handle_t* table_handle = nullptr;
    rocprofvis_result_t  res          = rocprofvis_controller_get_object(
        m_controller, kRPVControllerSystemEventTable, 0, &table_handle);
    if(res != kRocProfVisResultSuccess || !table_handle)
        return MakeError("Event table not available");

    rocprofvis_controller_arguments_t* args   = rocprofvis_controller_arguments_alloc();
    rocprofvis_controller_future_t*    future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_array_t*     arr    = rocprofvis_controller_array_alloc(512);

    if(!args || !future || !arr)
    {
        if(args) rocprofvis_controller_arguments_free(args);
        if(future) rocprofvis_controller_future_free(future);
        if(arr) rocprofvis_controller_array_free(arr);
        return MakeError("Failed to allocate table resources");
    }

    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsType, 0,
                                     kRPVControllerTableTypeEvents);

    // Resolve track handles to their graph->track handles for the table API
    std::vector<rocprofvis_handle_t*> resolved_tracks;
    for(size_t i = 0; i < track_ids.size(); i++)
    {
        auto it = m_track_handles.find(track_ids[i]);
        if(it != m_track_handles.end())
        {
            rocprofvis_handle_t* graph = nullptr;
            auto git = m_graph_handles.find(track_ids[i]);
            if(git != m_graph_handles.end())
            {
                rocprofvis_handle_t* track_from_graph = nullptr;
                rocprofvis_controller_get_object(git->second, kRPVControllerGraphTrack, 0,
                                                 &track_from_graph);
                if(track_from_graph)
                    resolved_tracks.push_back(track_from_graph);
            }
        }
    }

    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumTracks, 0,
                                     resolved_tracks.size());
    for(size_t i = 0; i < resolved_tracks.size(); i++)
    {
        rocprofvis_controller_set_object(args, kRPVControllerTableArgsTracksIndexed, i,
                                         resolved_tracks[i]);
    }

    rocprofvis_controller_set_double(args, kRPVControllerTableArgsStartTime, 0, start);
    rocprofvis_controller_set_double(args, kRPVControllerTableArgsEndTime, 0, end);
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortColumn, 0, sort_col);
    rocprofvis_controller_set_uint64(
        args, kRPVControllerTableArgsSortOrder, 0,
        sort_ascending ? kRPVControllerSortOrderAscending
                       : kRPVControllerSortOrderDescending);
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartIndex, 0, offset);
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartCount, 0, count);

    if(!filter.empty())
        rocprofvis_controller_set_string(args, kRPVControllerTableArgsFilter, 0,
                                         filter.c_str());
    if(!group.empty())
        rocprofvis_controller_set_string(args, kRPVControllerTableArgsGroup, 0,
                                         group.c_str());
    if(!group_cols.empty())
        rocprofvis_controller_set_string(args, kRPVControllerTableArgsGroupColumns, 0,
                                         group_cols.c_str());

    auto* table = reinterpret_cast<rocprofvis_controller_table_t*>(table_handle);
    res = rocprofvis_controller_table_fetch_async(m_controller, table, args, future, arr);

    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_arguments_free(args);
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_array_free(arr);
        return MakeError("Failed to start table fetch");
    }

    res = WaitForFuture(future);
    rocprofvis_controller_future_free(future);

    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_arguments_free(args);
        rocprofvis_controller_array_free(arr);
        return MakeError("Table fetch timed out or failed");
    }

    jt::Json result = SerializeTableResults(table, arr);

    rocprofvis_controller_arguments_free(args);
    rocprofvis_controller_array_free(arr);

    return result;
}

jt::Json
JsonMiddleware::GetSampleTable(const std::vector<uint64_t>& track_ids, double start,
                               double end, uint64_t offset, uint64_t count,
                               uint64_t sort_col, bool sort_ascending,
                               const std::string& filter)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_controller)
        return MakeError("No trace loaded");

    // Get the sample table handle from the controller (singleton, not allocated)
    rocprofvis_handle_t* table_handle = nullptr;
    rocprofvis_result_t  res          = rocprofvis_controller_get_object(
        m_controller, kRPVControllerSystemSampleTable, 0, &table_handle);
    if(res != kRocProfVisResultSuccess || !table_handle)
        return MakeError("Sample table not available");

    rocprofvis_controller_arguments_t* args   = rocprofvis_controller_arguments_alloc();
    rocprofvis_controller_future_t*    future = rocprofvis_controller_future_alloc();
    rocprofvis_controller_array_t*     arr    = rocprofvis_controller_array_alloc(512);

    if(!args || !future || !arr)
    {
        if(args) rocprofvis_controller_arguments_free(args);
        if(future) rocprofvis_controller_future_free(future);
        if(arr) rocprofvis_controller_array_free(arr);
        return MakeError("Failed to allocate table resources");
    }

    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsType, 0,
                                     kRPVControllerTableTypeSamples);

    // Resolve track handles to their graph->track handles for the table API
    std::vector<rocprofvis_handle_t*> resolved_tracks;
    for(size_t i = 0; i < track_ids.size(); i++)
    {
        auto git = m_graph_handles.find(track_ids[i]);
        if(git != m_graph_handles.end())
        {
            rocprofvis_handle_t* track_from_graph = nullptr;
            rocprofvis_controller_get_object(git->second, kRPVControllerGraphTrack, 0,
                                             &track_from_graph);
            if(track_from_graph)
                resolved_tracks.push_back(track_from_graph);
        }
    }

    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsNumTracks, 0,
                                     resolved_tracks.size());
    for(size_t i = 0; i < resolved_tracks.size(); i++)
    {
        rocprofvis_controller_set_object(args, kRPVControllerTableArgsTracksIndexed, i,
                                         resolved_tracks[i]);
    }

    rocprofvis_controller_set_double(args, kRPVControllerTableArgsStartTime, 0, start);
    rocprofvis_controller_set_double(args, kRPVControllerTableArgsEndTime, 0, end);
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsSortColumn, 0, sort_col);
    rocprofvis_controller_set_uint64(
        args, kRPVControllerTableArgsSortOrder, 0,
        sort_ascending ? kRPVControllerSortOrderAscending
                       : kRPVControllerSortOrderDescending);
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartIndex, 0, offset);
    rocprofvis_controller_set_uint64(args, kRPVControllerTableArgsStartCount, 0, count);

    if(!filter.empty())
        rocprofvis_controller_set_string(args, kRPVControllerTableArgsFilter, 0,
                                         filter.c_str());

    auto* table = reinterpret_cast<rocprofvis_controller_table_t*>(table_handle);
    res = rocprofvis_controller_table_fetch_async(m_controller, table, args, future, arr);

    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_arguments_free(args);
        rocprofvis_controller_future_free(future);
        rocprofvis_controller_array_free(arr);
        return MakeError("Failed to start sample table fetch");
    }

    res = WaitForFuture(future);
    rocprofvis_controller_future_free(future);

    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_arguments_free(args);
        rocprofvis_controller_array_free(arr);
        return MakeError("Sample table fetch timed out or failed");
    }

    jt::Json result = SerializeTableResults(table, arr);

    rocprofvis_controller_arguments_free(args);
    rocprofvis_controller_array_free(arr);

    return result;
}

jt::Json
JsonMiddleware::GetEventDetails(uint64_t event_id)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_controller)
        return MakeError("No trace loaded");

    rocprofvis_handle_t* event_handle = nullptr;
    rocprofvis_result_t  res          = rocprofvis_controller_get_object(
        m_controller, kRPVControllerSystemEventIndexed, event_id, &event_handle);

    if(res != kRocProfVisResultSuccess || !event_handle)
        return MakeError("Event not found: " + std::to_string(event_id));

    jt::Json event_json = SerializeEvent(event_handle);

    auto fetch_indexed = [&](rocprofvis_property_t property, const char* key) {
        rocprofvis_controller_future_t* fut = rocprofvis_controller_future_alloc();
        rocprofvis_controller_array_t*  a   = rocprofvis_controller_array_alloc(64);
        if(!fut || !a)
        {
            if(fut) rocprofvis_controller_future_free(fut);
            if(a) rocprofvis_controller_array_free(a);
            return;
        }

        res = rocprofvis_controller_get_indexed_property_async(
            m_controller, event_handle, property, event_id, 1, fut, a);
        if(res != kRocProfVisResultSuccess)
        {
            rocprofvis_controller_future_free(fut);
            rocprofvis_controller_array_free(a);
            return;
        }

        res = WaitForFuture(fut);
        rocprofvis_controller_future_free(fut);

        if(res == kRocProfVisResultSuccess)
        {
            uint64_t cnt = 0;
            rocprofvis_controller_get_uint64(a, kRPVControllerArrayNumEntries, 0, &cnt);

            jt::Json items;
            items.setArray();
            for(uint64_t i = 0; i < cnt; i++)
            {
                rocprofvis_handle_t* item = nullptr;
                rocprofvis_controller_get_object(a, kRPVControllerArrayEntryIndexed, i,
                                                 &item);
                if(!item) continue;

                rocprofvis_controller_object_type_t obj_type;
                rocprofvis_controller_get_object_type(item, &obj_type);

                jt::Json j;
                j.setObject();

                if(obj_type == kRPVControllerObjectTypeFlowControl)
                {
                    uint64_t fc_id = 0, fc_ts = 0, fc_track = 0, fc_dir = 0;
                    rocprofvis_controller_get_uint64(item, kRPVControllerFlowControltId, 0,
                                                     &fc_id);
                    rocprofvis_controller_get_uint64(
                        item, kRPVControllerFlowControlTimestamp, 0, &fc_ts);
                    rocprofvis_controller_get_uint64(
                        item, kRPVControllerFlowControlTrackId, 0, &fc_track);
                    rocprofvis_controller_get_uint64(
                        item, kRPVControllerFlowControlDirection, 0, &fc_dir);

                    j["id"]        = (long long)fc_id;
                    j["timestamp"] = (long long)fc_ts;
                    j["track_id"]  = (long long)fc_track;
                    j["direction"] = (fc_dir == 0) ? "outgoing" : "incoming";
                    j["name"]      = ReadString(item, kRPVControllerFlowControlName, 0);
                }
                else if(obj_type == kRPVControllerObjectTypeCallstack)
                {
                    j["file"]    = ReadString(item, kRPVControllerCallstackFile, 0);
                    j["pc"]      = ReadString(item, kRPVControllerCallstackPc, 0);
                    j["name"]    = ReadString(item, kRPVControllerCallstackName, 0);
                    j["address"] = ReadString(item, kRPVControllerCallstackLineAddress, 0);
                }
                else if(obj_type == kRPVControllerObjectTypeExtData)
                {
                    j["category"] = ReadString(item, kRPVControllerExtDataCategory, 0);
                    j["name"]     = ReadString(item, kRPVControllerExtDataName, 0);
                    j["value"]    = ReadString(item, kRPVControllerExtDataValue, 0);
                }
                items.getArray().push_back(std::move(j));
            }
            event_json[key] = std::move(items);
        }

        rocprofvis_controller_array_free(a);
    };

    fetch_indexed(kRPVControllerEventDataFlowControl, "flow_control");
    fetch_indexed(kRPVControllerEventDataCallStack, "call_stack");
    fetch_indexed(kRPVControllerEventDataExtData, "ext_data");

    jt::Json result = MakeSuccess();
    result["event"]    = std::move(event_json);
    return result;
}

jt::Json
JsonMiddleware::GetSummary(double start_time, double end_time)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if(!m_controller)
        return MakeError("No trace loaded");

    rocprofvis_handle_t* summary_handle = nullptr;
    rocprofvis_result_t  res            = rocprofvis_controller_get_object(
        m_controller, kRPVControllerSystemSummary, 0, &summary_handle);
    if(res != kRocProfVisResultSuccess || !summary_handle)
        return MakeError("Summary not available");

    rocprofvis_controller_arguments_t*      args    = rocprofvis_controller_arguments_alloc();
    rocprofvis_controller_summary_metrics_t* metrics = rocprofvis_controller_summary_metrics_alloc();
    rocprofvis_controller_future_t*         future  = rocprofvis_controller_future_alloc();

    if(!args || !metrics || !future)
    {
        if(args) rocprofvis_controller_arguments_free(args);
        if(metrics) rocprofvis_controller_summary_metric_free(metrics);
        if(future) rocprofvis_controller_future_free(future);
        return MakeError("Failed to allocate summary resources");
    }

    if(start_time >= 0 && end_time > start_time)
    {
        rocprofvis_controller_set_double(args, kRPVControllerSummaryArgsStartTimestamp, 0,
                                         start_time);
        rocprofvis_controller_set_double(args, kRPVControllerSummaryArgsEndTimestamp, 0,
                                         end_time);
    }

    res = rocprofvis_controller_summary_fetch_async(
        m_controller,
        reinterpret_cast<rocprofvis_controller_summary_t*>(summary_handle), args, future,
        metrics);

    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_arguments_free(args);
        rocprofvis_controller_summary_metric_free(metrics);
        rocprofvis_controller_future_free(future);
        return MakeError("Failed to start summary fetch");
    }

    res = WaitForFuture(future);
    rocprofvis_controller_future_free(future);
    rocprofvis_controller_arguments_free(args);

    if(res != kRocProfVisResultSuccess)
    {
        rocprofvis_controller_summary_metric_free(metrics);
        return MakeError("Summary fetch timed out or failed");
    }

    jt::Json summary_json = SerializeSummaryLevel(
        reinterpret_cast<rocprofvis_handle_t*>(metrics));

    rocprofvis_controller_summary_metric_free(metrics);

    jt::Json result = MakeSuccess();
    result["summary"]  = std::move(summary_json);
    return result;
}

// ---------- Serialization ----------

jt::Json
JsonMiddleware::SerializeNode(rocprofvis_handle_t* node_handle)
{
    jt::Json j;
    j.setObject();

    uint64_t id = 0;
    rocprofvis_controller_get_uint64(node_handle, kRPVControllerNodeId, 0, &id);

    j["id"]         = (long long)id;
    j["host_name"]  = ReadString(node_handle, kRPVControllerNodeHostName, 0);
    j["os_name"]    = ReadString(node_handle, kRPVControllerNodeOSName, 0);
    j["os_release"] = ReadString(node_handle, kRPVControllerNodeOSRelease, 0);
    j["os_version"] = ReadString(node_handle, kRPVControllerNodeOSVersion, 0);

    uint64_t num_processors = 0, num_processes = 0;
    rocprofvis_controller_get_uint64(node_handle, kRPVControllerNodeNumProcessors, 0,
                                     &num_processors);
    rocprofvis_controller_get_uint64(node_handle, kRPVControllerNodeNumProcesses, 0,
                                     &num_processes);

    jt::Json devices;
    devices.setArray();
    for(uint64_t i = 0; i < num_processors; i++)
    {
        rocprofvis_handle_t* proc = nullptr;
        rocprofvis_controller_get_object(node_handle, kRPVControllerNodeProcessorIndexed, i,
                                         &proc);
        if(proc) devices.getArray().push_back(SerializeDevice(proc));
    }
    j["devices"] = std::move(devices);

    jt::Json processes;
    processes.setArray();
    for(uint64_t i = 0; i < num_processes; i++)
    {
        rocprofvis_handle_t* proc = nullptr;
        rocprofvis_controller_get_object(node_handle, kRPVControllerNodeProcessIndexed, i,
                                         &proc);
        if(proc) processes.getArray().push_back(SerializeProcess(proc));
    }
    j["processes"] = std::move(processes);

    return j;
}

jt::Json
JsonMiddleware::SerializeDevice(rocprofvis_handle_t* processor_handle)
{
    jt::Json j;
    j.setObject();

    uint64_t id = 0;
    rocprofvis_controller_get_uint64(processor_handle, kRPVControllerProcessorId, 0, &id);

    j["id"]           = (long long)id;
    j["product_name"] = ReadString(processor_handle, kRPVControllerProcessorProductName, 0);

    uint64_t proc_type = 0, type_index = 0;
    rocprofvis_controller_get_uint64(processor_handle, kRPVControllerProcessorType, 0,
                                     &proc_type);
    rocprofvis_controller_get_uint64(processor_handle, kRPVControllerProcessorTypeIndex, 0,
                                     &type_index);

    j["type"]       = (proc_type == kRPVControllerProcessorTypeGPU) ? "GPU" : "CPU";
    j["type_index"] = (long long)type_index;

    uint64_t num_queues = 0, num_counters = 0;
    rocprofvis_controller_get_uint64(processor_handle, kRPVControllerProcessorNumQueues, 0,
                                     &num_queues);
    rocprofvis_controller_get_uint64(processor_handle, kRPVControllerProcessorNumCounters,
                                     0, &num_counters);

    jt::Json queues;
    queues.setArray();
    for(uint64_t i = 0; i < num_queues; i++)
    {
        rocprofvis_handle_t* q = nullptr;
        rocprofvis_controller_get_object(processor_handle,
                                         kRPVControllerProcessorQueueIndexed, i, &q);
        if(q) queues.getArray().push_back(SerializeQueue(q));
    }
    j["queues"] = std::move(queues);

    jt::Json counters;
    counters.setArray();
    for(uint64_t i = 0; i < num_counters; i++)
    {
        rocprofvis_handle_t* c = nullptr;
        rocprofvis_controller_get_object(processor_handle,
                                         kRPVControllerProcessorCounterIndexed, i, &c);
        if(c) counters.getArray().push_back(SerializeCounter(c));
    }
    j["counters"] = std::move(counters);

    return j;
}

jt::Json
JsonMiddleware::SerializeProcess(rocprofvis_handle_t* process_handle)
{
    jt::Json j;
    j.setObject();

    uint64_t id = 0;
    rocprofvis_controller_get_uint64(process_handle, kRPVControllerProcessId, 0, &id);

    j["id"]      = (long long)id;
    j["command"] = ReadString(process_handle, kRPVControllerProcessCommand, 0);

    double start_time = 0, end_time = 0;
    rocprofvis_controller_get_double(process_handle, kRPVControllerProcessStartTime, 0,
                                     &start_time);
    rocprofvis_controller_get_double(process_handle, kRPVControllerProcessEndTime, 0,
                                     &end_time);
    j["start_time"] = start_time;
    j["end_time"]   = end_time;

    uint64_t num_threads = 0, num_streams = 0;
    rocprofvis_controller_get_uint64(process_handle, kRPVControllerProcessNumThreads, 0,
                                     &num_threads);
    rocprofvis_controller_get_uint64(process_handle, kRPVControllerProcessNumStreams, 0,
                                     &num_streams);

    jt::Json threads;
    threads.setArray();
    for(uint64_t i = 0; i < num_threads; i++)
    {
        rocprofvis_handle_t* t = nullptr;
        rocprofvis_controller_get_object(process_handle,
                                         kRPVControllerProcessThreadIndexed, i, &t);
        if(t) threads.getArray().push_back(SerializeThread(t));
    }
    j["threads"] = std::move(threads);

    jt::Json streams;
    streams.setArray();
    for(uint64_t i = 0; i < num_streams; i++)
    {
        rocprofvis_handle_t* s = nullptr;
        rocprofvis_controller_get_object(process_handle,
                                         kRPVControllerProcessStreamIndexed, i, &s);
        if(s) streams.getArray().push_back(SerializeStream(s));
    }
    j["streams"] = std::move(streams);

    return j;
}

jt::Json
JsonMiddleware::SerializeThread(rocprofvis_handle_t* thread_handle)
{
    jt::Json j;
    j.setObject();

    uint64_t id = 0, tid = 0, type = 0;
    rocprofvis_controller_get_uint64(thread_handle, kRPVControllerThreadId, 0, &id);
    rocprofvis_controller_get_uint64(thread_handle, kRPVControllerThreadTid, 0, &tid);
    rocprofvis_controller_get_uint64(thread_handle, kRPVControllerThreadType, 0, &type);

    j["id"]   = (long long)id;
    j["tid"]  = (long long)tid;
    j["name"] = ReadString(thread_handle, kRPVControllerThreadName, 0);
    j["type"] = (type == kRPVControllerThreadTypeInstrumented)  ? "instrumented"
                : (type == kRPVControllerThreadTypeSampled)     ? "sampled"
                                                                : "undefined";

    double start = 0, end = 0;
    rocprofvis_controller_get_double(thread_handle, kRPVControllerThreadStartTime, 0,
                                     &start);
    rocprofvis_controller_get_double(thread_handle, kRPVControllerThreadEndTime, 0, &end);
    j["start_time"] = start;
    j["end_time"]   = end;

    return j;
}

jt::Json
JsonMiddleware::SerializeQueue(rocprofvis_handle_t* queue_handle)
{
    jt::Json j;
    j.setObject();
    uint64_t id = 0;
    rocprofvis_controller_get_uint64(queue_handle, kRPVControllerQueueId, 0, &id);
    j["id"]   = (long long)id;
    j["name"] = ReadString(queue_handle, kRPVControllerQueueName, 0);
    return j;
}

jt::Json
JsonMiddleware::SerializeStream(rocprofvis_handle_t* stream_handle)
{
    jt::Json j;
    j.setObject();
    uint64_t id = 0;
    rocprofvis_controller_get_uint64(stream_handle, kRPVControllerStreamId, 0, &id);
    j["id"]   = (long long)id;
    j["name"] = ReadString(stream_handle, kRPVControllerStreamName, 0);
    return j;
}

jt::Json
JsonMiddleware::SerializeCounter(rocprofvis_handle_t* counter_handle)
{
    jt::Json j;
    j.setObject();
    uint64_t id = 0;
    rocprofvis_controller_get_uint64(counter_handle, kRPVControllerCounterId, 0, &id);
    j["id"]          = (long long)id;
    j["name"]        = ReadString(counter_handle, kRPVControllerCounterName, 0);
    j["description"] = ReadString(counter_handle, kRPVControllerCounterDescription, 0);
    j["units"]       = ReadString(counter_handle, kRPVControllerCounterUnits, 0);
    j["value_type"]  = ReadString(counter_handle, kRPVControllerCounterValueType, 0);
    return j;
}

jt::Json
JsonMiddleware::SerializeTrackMeta(rocprofvis_handle_t* track_handle, uint64_t index)
{
    jt::Json j;
    j.setObject();

    uint64_t id = 0, track_type = 0, num_entries = 0;
    double   min_ts = 0, max_ts = 0, min_val = 0, max_val = 0;
    uint64_t agent_or_pid = 0, queue_id_or_tid = 0;

    rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackId, 0, &id);
    rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackType, 0, &track_type);
    rocprofvis_controller_get_double(track_handle, kRPVControllerTrackMinTimestamp, 0,
                                     &min_ts);
    rocprofvis_controller_get_double(track_handle, kRPVControllerTrackMaxTimestamp, 0,
                                     &max_ts);
    rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackNumberOfEntries, 0,
                                     &num_entries);
    rocprofvis_controller_get_double(track_handle, kRPVControllerTrackMinValue, 0,
                                     &min_val);
    rocprofvis_controller_get_double(track_handle, kRPVControllerTrackMaxValue, 0,
                                     &max_val);
    rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackAgentIdOrPid, 0,
                                     &agent_or_pid);
    rocprofvis_controller_get_uint64(track_handle, kRPVControllerTrackQueueIdOrTid, 0,
                                     &queue_id_or_tid);

    j["id"]              = (long long)id;
    j["index"]           = (long long)index;
    j["track_type"]      = (track_type == kRPVControllerTrackTypeEvents) ? "events" : "samples";
    j["min_timestamp"]   = min_ts;
    j["max_timestamp"]   = max_ts;
    j["num_entries"]     = (long long)num_entries;
    j["min_value"]       = min_val;
    j["max_value"]       = max_val;
    j["agent_or_pid"]    = (long long)agent_or_pid;
    j["queue_id_or_tid"] = (long long)queue_id_or_tid;
    j["category"]        = ReadString(track_handle, kRPVControllerCategory, 0);
    j["main_name"]       = ReadString(track_handle, kRPVControllerMainName, 0);
    j["sub_name"]        = ReadString(track_handle, kRPVControllerSubName, 0);

    return j;
}

jt::Json
JsonMiddleware::SerializeEvent(rocprofvis_handle_t* event_handle)
{
    jt::Json j;
    j.setObject();

    uint64_t id = 0, level = 0, num_children = 0;
    double   start_ts = 0, end_ts = 0, duration = 0;

    rocprofvis_controller_get_uint64(event_handle, kRPVControllerEventId, 0, &id);
    rocprofvis_controller_get_double(event_handle, kRPVControllerEventStartTimestamp, 0,
                                     &start_ts);
    rocprofvis_controller_get_double(event_handle, kRPVControllerEventEndTimestamp, 0,
                                     &end_ts);
    rocprofvis_controller_get_uint64(event_handle, kRPVControllerEventLevel, 0, &level);
    rocprofvis_controller_get_uint64(event_handle, kRPVControllerEventNumChildren, 0,
                                     &num_children);
    rocprofvis_controller_get_double(event_handle, kRPVControllerEventDuration, 0,
                                     &duration);

    j["id"]              = (long long)id;
    j["start_timestamp"] = start_ts;
    j["end_timestamp"]   = end_ts;
    j["duration"]        = duration;
    j["level"]           = (long long)level;
    j["num_children"]    = (long long)num_children;
    j["name"]            = ReadString(event_handle, kRPVControllerEventName, 0);
    j["category"]        = ReadString(event_handle, kRPVControllerEventCategory, 0);

    return j;
}

jt::Json
JsonMiddleware::SerializeSample(rocprofvis_handle_t* sample_handle)
{
    jt::Json j;
    j.setObject();

    uint64_t id = 0, sample_type = 0, num_children = 0;
    double   timestamp = 0, end_ts = 0;

    rocprofvis_controller_get_uint64(sample_handle, kRPVControllerSampleId, 0, &id);
    rocprofvis_controller_get_uint64(sample_handle, kRPVControllerSampleType, 0,
                                     &sample_type);
    rocprofvis_controller_get_double(sample_handle, kRPVControllerSampleTimestamp, 0,
                                     &timestamp);
    rocprofvis_controller_get_double(sample_handle, kRPVControllerSampleEndTimestamp, 0,
                                     &end_ts);
    rocprofvis_controller_get_uint64(sample_handle, kRPVControllerSampleNumChildren, 0,
                                     &num_children);

    if(sample_type == kRPVControllerPrimitiveTypeDouble)
    {
        double value = 0;
        rocprofvis_controller_get_double(sample_handle, kRPVControllerSampleValue, 0,
                                         &value);
        j["value"] = value;
    }
    else
    {
        uint64_t uval = 0;
        rocprofvis_controller_get_uint64(sample_handle, kRPVControllerSampleValue, 0,
                                         &uval);
        j["value"] = (long long)uval;
    }

    j["id"]            = (long long)id;
    j["timestamp"]     = timestamp;
    j["end_timestamp"] = end_ts;
    j["num_children"]  = (long long)num_children;

    if(num_children > 0)
    {
        double child_min = 0, child_mean = 0, child_max = 0;
        rocprofvis_controller_get_double(sample_handle, kRPVControllerSampleChildMin, 0,
                                         &child_min);
        rocprofvis_controller_get_double(sample_handle, kRPVControllerSampleChildMean, 0,
                                         &child_mean);
        rocprofvis_controller_get_double(sample_handle, kRPVControllerSampleChildMax, 0,
                                         &child_max);
        j["child_min"]  = child_min;
        j["child_mean"] = child_mean;
        j["child_max"]  = child_max;
    }

    return j;
}

jt::Json
JsonMiddleware::SerializeTableResults(rocprofvis_controller_table_t* table_handle,
                                      rocprofvis_controller_array_t* result_array)
{
    jt::Json result = MakeSuccess();

    uint64_t num_cols = 0, num_rows = 0;
    rocprofvis_controller_get_uint64(
        reinterpret_cast<rocprofvis_handle_t*>(table_handle),
        kRPVControllerTableNumColumns, 0, &num_cols);
    rocprofvis_controller_get_uint64(
        reinterpret_cast<rocprofvis_handle_t*>(table_handle),
        kRPVControllerTableNumRows, 0, &num_rows);

    result["title"] = ReadString(
        reinterpret_cast<rocprofvis_handle_t*>(table_handle), kRPVControllerTableTitle, 0);

    jt::Json headers;
    headers.setArray();
    jt::Json col_types;
    col_types.setArray();

    for(uint64_t i = 0; i < num_cols; i++)
    {
        headers.getArray().push_back(
            ReadString(reinterpret_cast<rocprofvis_handle_t*>(table_handle),
                       kRPVControllerTableColumnHeaderIndexed, i));
        uint64_t col_type = 0;
        rocprofvis_controller_get_uint64(
            reinterpret_cast<rocprofvis_handle_t*>(table_handle),
            kRPVControllerTableColumnTypeIndexed, i, &col_type);
        const char* type_str = "object";
        switch(col_type)
        {
            case kRPVControllerPrimitiveTypeUInt64: type_str = "uint64"; break;
            case kRPVControllerPrimitiveTypeDouble: type_str = "double"; break;
            case kRPVControllerPrimitiveTypeString: type_str = "string"; break;
        }
        col_types.getArray().push_back(jt::Json(type_str));
    }

    uint64_t arr_count = 0;
    rocprofvis_controller_get_uint64(result_array, kRPVControllerArrayNumEntries, 0,
                                     &arr_count);

    jt::Json rows;
    rows.setArray();
    for(uint64_t i = 0; i < arr_count; i++)
    {
        rocprofvis_handle_t* row_handle = nullptr;
        rocprofvis_controller_get_object(result_array, kRPVControllerArrayEntryIndexed, i,
                                         &row_handle);
        if(!row_handle) continue;

        uint64_t row_size = 0;
        rocprofvis_controller_get_uint64(row_handle, kRPVControllerArrayNumEntries, 0,
                                         &row_size);

        jt::Json row;
        row.setArray();
        for(uint64_t c = 0; c < row_size && c < num_cols; c++)
        {
            row.getArray().push_back(
                ReadString(row_handle, kRPVControllerArrayEntryIndexed, c));
        }
        rows.getArray().push_back(std::move(row));
    }

    result["num_columns"]  = (long long)num_cols;
    result["total_rows"]   = (long long)num_rows;
    result["headers"]      = std::move(headers);
    result["column_types"] = std::move(col_types);
    result["rows"]         = std::move(rows);
    return result;
}

jt::Json
JsonMiddleware::SerializeSummaryLevel(rocprofvis_handle_t* metrics_handle)
{
    jt::Json j;
    j.setObject();
    if(!metrics_handle) return j;

    uint64_t agg_level = 0;
    rocprofvis_controller_get_uint64(
        metrics_handle, kRPVControllerSummaryMetricPropertyAggregationLevel, 0, &agg_level);

    switch(agg_level)
    {
        case kRPVControllerSummaryAggregationLevelTrace: j["level"] = "trace"; break;
        case kRPVControllerSummaryAggregationLevelNode: j["level"] = "node"; break;
        case kRPVControllerSummaryAggregationLevelProcessor: j["level"] = "processor"; break;
        default: j["level"] = "unknown"; break;
    }

    j["name"] = ReadString(metrics_handle, kRPVControllerSummaryMetricPropertyName, 0);

    uint64_t id = 0;
    if(rocprofvis_controller_get_uint64(metrics_handle,
                                        kRPVControllerSummaryMetricPropertyId, 0,
                                        &id) == kRocProfVisResultSuccess)
    {
        j["id"] = (long long)id;
    }

    uint64_t proc_type = 0;
    if(rocprofvis_controller_get_uint64(
           metrics_handle, kRPVControllerSummaryMetricPropertyProcessorType, 0,
           &proc_type) == kRocProfVisResultSuccess)
    {
        j["processor_type"] =
            (proc_type == kRPVControllerProcessorTypeGPU) ? "GPU" : "CPU";
    }

    double gfx_util = 0;
    if(rocprofvis_controller_get_double(
           metrics_handle, kRPVControllerSummaryMetricPropertyGpuGfxUtil, 0,
           &gfx_util) == kRocProfVisResultSuccess)
    {
        j["gfx_utilization"] = gfx_util;
    }

    double mem_util = 0;
    if(rocprofvis_controller_get_double(
           metrics_handle, kRPVControllerSummaryMetricPropertyGpuMemUtil, 0,
           &mem_util) == kRocProfVisResultSuccess)
    {
        j["mem_utilization"] = mem_util;
    }

    uint64_t num_kernels = 0;
    rocprofvis_controller_get_uint64(
        metrics_handle, kRPVControllerSummaryMetricPropertyNumKernels, 0, &num_kernels);
    if(num_kernels > 0)
    {
        double total_exec = 0;
        rocprofvis_controller_get_double(
            metrics_handle, kRPVControllerSummaryMetricPropertyKernelsExecTimeTotal, 0,
            &total_exec);
        j["kernel_exec_time_total"] = total_exec;

        jt::Json kernels;
        kernels.setArray();
        for(uint64_t i = 0; i < num_kernels; i++)
        {
            jt::Json k;
            k.setObject();
            k["name"] = ReadString(
                metrics_handle, kRPVControllerSummaryMetricPropertyKernelNameIndexed, i);

            uint64_t invocations = 0;
            rocprofvis_controller_get_uint64(
                metrics_handle,
                kRPVControllerSummaryMetricPropertyKernelInvocationsIndexed, i,
                &invocations);
            k["invocations"] = (long long)invocations;

            double sum = 0, min_t = 0, max_t = 0, pct = 0;
            rocprofvis_controller_get_double(
                metrics_handle,
                kRPVControllerSummaryMetricPropertyKernelExecTimeSumIndexed, i, &sum);
            rocprofvis_controller_get_double(
                metrics_handle,
                kRPVControllerSummaryMetricPropertyKernelExecTimeMinIndexed, i, &min_t);
            rocprofvis_controller_get_double(
                metrics_handle,
                kRPVControllerSummaryMetricPropertyKernelExecTimeMaxIndexed, i, &max_t);
            rocprofvis_controller_get_double(
                metrics_handle,
                kRPVControllerSummaryMetricPropertyKernelExecTimePctIndexed, i, &pct);

            k["exec_time_sum"] = sum;
            k["exec_time_min"] = min_t;
            k["exec_time_max"] = max_t;
            k["exec_time_pct"] = pct;
            kernels.getArray().push_back(std::move(k));
        }
        j["top_kernels"] = std::move(kernels);
    }

    uint64_t num_sub = 0;
    rocprofvis_controller_get_uint64(
        metrics_handle, kRPVControllerSummaryMetricPropertyNumSubMetrics, 0, &num_sub);
    if(num_sub > 0)
    {
        jt::Json children;
        children.setArray();
        for(uint64_t i = 0; i < num_sub; i++)
        {
            rocprofvis_handle_t* child = nullptr;
            rocprofvis_controller_get_object(
                metrics_handle, kRPVControllerSummaryMetricPropertySubMetricsIndexed, i,
                &child);
            if(child)
                children.getArray().push_back(SerializeSummaryLevel(child));
        }
        j["sub_metrics"] = std::move(children);
    }

    return j;
}

}  // namespace Web
}  // namespace RocProfVis
