// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#ifdef ROCPROFVIS_PROFILER_HUB_ENABLED

#    include "rocprofvis_db_pytorch_hub.h"
#    include "rocprofvis_dm_topology.h"
#    include "rocprofvis_error_handling.h"
#    include "trace_context.h"

#    include "spdlog/spdlog.h"

#    include <algorithm>
#    include <cfloat>
#    include <cstdint>
#    include <cstdlib>
#    include <cstring>

namespace RocProfVis
{
namespace DataModel
{
namespace
{
bool
PytorchRouteEnabled()
{
    const char* route = std::getenv("OPTIQ_PYTORCH_ROUTE");
    return route == nullptr || std::strcmp(route, "perfetto") != 0;
}
}  // namespace

rocprofvis_db_type_t
PytorchHubDatabase::Detect(rocprofvis_db_filename_t filename)
{
    if(PytorchRouteEnabled() &&
       optiq::ProbeTraceFormat(filename) == optiq::TraceFormat::kPytorchKineto)
        return kPytorchTrace;
    return kAutodetect;
}

PytorchHubDatabase::PytorchHubDatabase(rocprofvis_db_filename_t path)
: Database(path)
{}

PytorchHubDatabase::~PytorchHubDatabase() = default;

rocprofvis_dm_result_t
PytorchHubDatabase::Open()
{
    try
    {
        ph_ctx_ = std::make_unique<optiq::TraceContext>(Path());
    } catch(const optiq::TraceOpenError& e)
    {
        spdlog::error("[pytorch-hub] {}", e.what());
        return kRocProfVisDmResultDbAccessFailed;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
PytorchHubDatabase::Close()
{
    ph_ctx_.reset();
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
PytorchHubDatabase::ReadTraceMetadata(Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    while(true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties,
                                    ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;

        ShowProgress(5, "Reading track list from profiler-hub", kRPVDbBusy, future);

        DbInstances().push_back({ SingleNodeDbInstance(), "" });
        DbInstance* db_instance = &DbInstances()[0].first;

        TraceProperties()->num_db_instances = static_cast<uint32_t>(DbInstances().size());
        TraceProperties()->events_count[kRocProfVisDmOperationLaunch]         = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationDispatch]       = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryAllocate] = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationMemoryCopy]     = 0;
        TraceProperties()->events_count[kRocProfVisDmOperationLaunchSample]   = 0;
        TraceProperties()->tracks_info_restored                               = true;
        TraceProperties()->trace_duration                                     = 0;
        TraceProperties()->db_inst_start_time.push_back(UINT64_MAX);
        TraceProperties()->db_inst_end_time.push_back(0);

        track_id_map_.clear();
        const std::vector<ph_track_t> ph_tracks = ph_ctx_->GetTrackList();

        // Mirrors RocprofDatabase::ReadTraceMetadata's registration of profiler-hub
        // thread and stream tracks.
        for(const ph_track_t& t : ph_tracks)
        {
            if(t.category != PH_TRACK_CATEGORY_THREAD &&
               t.category != PH_TRACK_CATEGORY_STREAM)
                continue;
            const bool is_thread = t.category == PH_TRACK_CATEGORY_THREAD;

            rocprofvis_dm_track_params_t track_params = { 0 };
            track_params.track_indentifiers.track_id =
                (rocprofvis_dm_track_id_t) NumTracks();
            track_params.track_indentifiers.category =
                is_thread ? kRocProfVisDmRegionMainTrack : kRocProfVisDmStreamTrack;
            track_params.track_indentifiers.db_instance = db_instance;
            track_params.track_indentifiers.process_id = (rocprofvis_dm_track_id_t) t.pid;
            track_params.track_indentifiers.id[TRACK_ID_NODE]         = t.nid;
            track_params.track_indentifiers.is_numeric[TRACK_ID_NODE] = true;
            track_params.track_indentifiers.tag[TRACK_ID_NODE] =
                Builder::NODE_ID_SERVICE_NAME;
            track_params.track_indentifiers.id[TRACK_ID_PID]         = t.pid;
            track_params.track_indentifiers.is_numeric[TRACK_ID_PID] = true;
            track_params.track_indentifiers.tag[TRACK_ID_PID] =
                Builder::PROCESS_ID_SERVICE_NAME;
            if(is_thread)
            {
                track_params.track_indentifiers.id[TRACK_ID_TID]         = t.tid;
                track_params.track_indentifiers.is_numeric[TRACK_ID_TID] = true;
                track_params.track_indentifiers.tag[TRACK_ID_TID] =
                    Builder::THREAD_ID_SERVICE_NAME;
                track_params.track_indentifiers.name[TRACK_ID_TID] = t.track_name;
                track_params.op = kRocProfVisDmOperationLaunch;
            }
            else
            {
                track_params.track_indentifiers.id[TRACK_ID_STREAM]         = t.stream_id;
                track_params.track_indentifiers.is_numeric[TRACK_ID_STREAM] = true;
                track_params.track_indentifiers.tag[TRACK_ID_STREAM] =
                    Builder::STREAM_ID_SERVICE_NAME;
                track_params.track_indentifiers.name[TRACK_ID_STREAM] = t.track_name;
                // The view labels a stream from main_name (name[TRACK_ID_PID_OR_AGENT]),
                // not name[TRACK_ID_STREAM].
                track_params.track_indentifiers.name[TRACK_ID_PID_OR_AGENT] =
                    t.track_name;
                track_params.op = kRocProfVisDmMultipleOperations;
            }
            track_params.record_count = t.event_count;
            track_params.min_ts       = t.start_ts;
            track_params.max_ts       = t.end_ts;
            track_params.max_value    = 0;
            track_params.min_value    = DBL_MAX;

            track_id_map_[track_params.track_indentifiers.track_id] = t.id;

            if(kRocProfVisDmResultSuccess != AddTrackProperties(track_params))
            {
                result = kRocProfVisDmResultDbAccessFailed;
                break;
            }
            // Without the start/end update every MinTimestamp() underflows from the
            // UINT64_MAX sentinel.
            {
                uint32_t guid_index = db_instance->GuidIndex();
                TraceProperties()->db_inst_start_time[guid_index] = std::min(
                    TraceProperties()->db_inst_start_time[guid_index], t.start_ts);
                TraceProperties()->db_inst_end_time[guid_index] =
                    std::max(TraceProperties()->db_inst_end_time[guid_index], t.end_ts);
                TraceProperties()->trace_duration =
                    std::max(TraceProperties()->trace_duration,
                             TraceProperties()->db_inst_end_time[guid_index] -
                                 TraceProperties()->db_inst_start_time[guid_index]);
            }
            if(BindObject()->FuncAddTrack(BindObject()->trace_object,
                                          TrackPropertiesLast()) !=
               kRocProfVisDmResultSuccess)
            {
                result = kRocProfVisDmResultDbAccessFailed;
                break;
            }
            BindObject()->FuncAddTopologyNode(BindObject()->trace_object,
                                              &track_params.track_indentifiers);
        }
        if(result != kRocProfVisDmResultSuccess) break;

        spdlog::info("[pytorch-hub] ReadTraceMetadata: {} PH tracks registered",
                     NumTracks());

        CreateTracksOrderRanking();
        BindObject()->FuncAddString(BindObject()->trace_object, "");  // 0 index string

        TraceProperties()->metadata_loaded = true;
        BindObject()->FuncMetadataLoaded(BindObject()->trace_object);
        ShowProgress(100 - future->Progress(), "Trace metadata successfully loaded",
                     kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, future);
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
                                                    : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t
PytorchHubDatabase::ReadTraceSlice(rocprofvis_dm_timestamp_t            start,
                                   rocprofvis_dm_timestamp_t            end,
                                   rocprofvis_dm_hashed_timestamp_tag_t tag,
                                   rocprofvis_db_num_of_tracks_t        num,
                                   rocprofvis_db_track_selection_t tracks, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);

    auto it = (num == 1 && tracks) ? track_id_map_.find(*tracks) : track_id_map_.end();
    if(it == track_id_map_.end())
    {
        spdlog::error(
            "[pytorch-hub] ReadTraceSlice: no PH track for the request (num={})", num);
        return future->SetPromise(kRocProfVisDmResultInvalidParameter);
    }
    const rocprofvis_dm_track_id_t track_id    = *tracks;
    const uint32_t                 ph_track_id = it->second;

    const uint64_t offset   = TraceProperties()->db_inst_start_time[0];
    const uint64_t ph_start = start + offset;
    const uint64_t ph_end   = end + offset;

    std::vector<ph_event_t> events =
        ph_ctx_->GetTrackEvents(ph_track_id, ph_start, ph_end);

    spdlog::info("[pytorch-hub] ReadTraceSlice ph_track={} start={} end={} -> {} events",
                 ph_track_id, ph_start, ph_end, events.size());

    // PH returns start asc, end desc; stable_sort on start alone keeps that order for
    // ties.
    std::stable_sort(
        events.begin(), events.end(),
        [](const ph_event_t& a, const ph_event_t& b) { return a.start < b.start; });
    std::vector<uint64_t> open_ends;

    rocprofvis_dm_slice_t slice =
        BindObject()->FuncAddSlice(BindObject()->trace_object, track_id, start, end, tag);

    uint32_t sequence = 0;
    for(const ph_event_t& event : events)
    {
        while(!open_ends.empty() && open_ends.back() <= event.start)
            open_ends.pop_back();

        rocprofvis_db_record_data_t record{};
        record.event.id.bitfield.event_id =
            (static_cast<uint64_t>(ph_track_id) << 32) | sequence++;
        record.event.id.bitfield.event_node = 0;
        record.event.id.bitfield.event_op   = kRocProfVisDmOperationNoOp;
        record.event.timestamp              = event.start - offset;
        record.event.duration =
            static_cast<rocprofvis_dm_duration_t>(event.end - event.start);
        {
            std::lock_guard<std::mutex> lock(string_intern_mutex_);
            record.event.category =
                BindObject()->FuncAddString(BindObject()->trace_object, event.name);
        }
        record.event.symbol = record.event.category;
        record.event.level  = static_cast<rocprofvis_dm_event_level_t>(open_ends.size());

        BindObject()->FuncAddRecord(slice, record);
        open_ends.push_back(event.end);
    }

    BindObject()->FuncCompleteSlice(slice);
    ShowProgress(100 - future->Progress(),
                 "Time slice successfully loaded! (pytorch-hub)", kRPVDbSuccess, future);
    return future->SetPromise(kRocProfVisDmResultSuccess);
}

rocprofvis_dm_result_t
PytorchHubDatabase::BuildComputeQuery(rocprofvis_db_compute_use_case_enum_t,
                                      rocprofvis_db_num_of_params_t,
                                      rocprofvis_db_compute_params_t,
                                      rocprofvis_dm_string_t&)
{
    return kRocProfVisDmResultNotSupported;
}

rocprofvis_dm_result_t
PytorchHubDatabase::BuildTableQuery(rocprofvis_dm_table_use_case_enum_t,
                                    rocprofvis_dm_timestamp_t, rocprofvis_dm_timestamp_t,
                                    rocprofvis_db_num_of_tracks_t,
                                    rocprofvis_db_track_selection_t,
                                    rocprofvis_dm_charptr_t, rocprofvis_dm_charptr_t,
                                    rocprofvis_dm_charptr_t, rocprofvis_dm_charptr_t,
                                    rocprofvis_dm_charptr_t, rocprofvis_dm_sort_order_t,
                                    uint64_t, uint64_t, bool, rocprofvis_dm_string_t&)
{
    return kRocProfVisDmResultNotSupported;
}

rocprofvis_dm_result_t
PytorchHubDatabase::SaveTrimmedData(rocprofvis_dm_timestamp_t, rocprofvis_dm_timestamp_t,
                                    rocprofvis_dm_charptr_t, Future* future)
{
    return future->SetPromise(kRocProfVisDmResultNotSupported);
}

rocprofvis_dm_result_t
PytorchHubDatabase::ReadFlowTraceInfo(rocprofvis_dm_event_id_t, Future* future)
{
    return future->SetPromise(kRocProfVisDmResultNotSupported);
}

rocprofvis_dm_result_t
PytorchHubDatabase::ReadStackTraceInfo(rocprofvis_dm_event_id_t, Future* future)
{
    return future->SetPromise(kRocProfVisDmResultNotSupported);
}

rocprofvis_dm_result_t
PytorchHubDatabase::ReadExtEventInfo(rocprofvis_dm_event_id_t, Future* future)
{
    return future->SetPromise(kRocProfVisDmResultNotSupported);
}

rocprofvis_dm_result_t
PytorchHubDatabase::ExecuteQuery(rocprofvis_dm_charptr_t, rocprofvis_dm_charptr_t,
                                 Future* future)
{
    return future->SetPromise(kRocProfVisDmResultNotSupported);
}

rocprofvis_dm_result_t
PytorchHubDatabase::ExecuteComputeQuery(rocprofvis_db_compute_use_case_enum_t,
                                        rocprofvis_dm_charptr_t, Future* future)
{
    return future->SetPromise(kRocProfVisDmResultNotSupported);
}

}  // namespace DataModel
}  // namespace RocProfVis

#endif
