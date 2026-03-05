// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_types.h"
#include "rocprofvis_shared_types.h"

#include "json.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace Web
{

class JsonMiddleware
{
public:
    JsonMiddleware();
    ~JsonMiddleware();

    jt::Json LoadTrace(const std::string& file_path);
    jt::Json CloseTrace();

    jt::Json GetTopology();
    jt::Json GetTimelineInfo();
    jt::Json GetTracks();
    jt::Json GetTrackGraphData(uint64_t track_id, double start, double end,
                               uint32_t resolution);

    jt::Json GetEventTable(const std::vector<uint64_t>& track_ids, double start,
                           double end, uint64_t offset, uint64_t count,
                           uint64_t sort_col, bool sort_ascending,
                           const std::string& filter, const std::string& group,
                           const std::string& group_cols);

    jt::Json GetSampleTable(const std::vector<uint64_t>& track_ids, double start,
                            double end, uint64_t offset, uint64_t count,
                            uint64_t sort_col, bool sort_ascending,
                            const std::string& filter);

    jt::Json GetEventDetails(uint64_t event_id);
    jt::Json GetSummary(double start_time, double end_time);

    bool IsLoaded() const;

private:
    std::string ReadString(rocprofvis_handle_t* handle, rocprofvis_property_t property,
                           uint64_t index);

    rocprofvis_result_t WaitForFuture(rocprofvis_controller_future_t* future,
                                      float timeout = 60.0f);

    jt::Json MakeError(const std::string& message);
    jt::Json MakeSuccess(const std::string& message = "");

    jt::Json SerializeNode(rocprofvis_handle_t* node_handle);
    jt::Json SerializeDevice(rocprofvis_handle_t* processor_handle);
    jt::Json SerializeProcess(rocprofvis_handle_t* process_handle);
    jt::Json SerializeThread(rocprofvis_handle_t* thread_handle);
    jt::Json SerializeQueue(rocprofvis_handle_t* queue_handle);
    jt::Json SerializeStream(rocprofvis_handle_t* stream_handle);
    jt::Json SerializeCounter(rocprofvis_handle_t* counter_handle);

    jt::Json SerializeTrackMeta(rocprofvis_handle_t* track_handle, uint64_t index);
    jt::Json SerializeEvent(rocprofvis_handle_t* event_handle);
    jt::Json SerializeSample(rocprofvis_handle_t* sample_handle);

    jt::Json SerializeTableResults(rocprofvis_controller_table_t* table_handle,
                                   rocprofvis_controller_array_t* result_array);

    jt::Json SerializeSummaryLevel(rocprofvis_handle_t* metrics_handle);

    void LoadTopologyData();
    void LoadTimelineData();

    rocprofvis_controller_t*          m_controller = nullptr;
    rocprofvis_controller_timeline_t* m_timeline   = nullptr;

    std::unordered_map<uint64_t, rocprofvis_handle_t*> m_track_handles;
    std::unordered_map<uint64_t, rocprofvis_handle_t*> m_graph_handles;

    jt::Json m_cached_topology;
    jt::Json m_cached_tracks;
    jt::Json m_cached_timeline_info;

    mutable std::mutex m_mutex;
};

}  // namespace Web
}  // namespace RocProfVis
