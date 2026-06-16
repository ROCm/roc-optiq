// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_analysis.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_reference.h"
#include "system/rocprofvis_controller_table_system.h"
#include "system/rocprofvis_controller_trace_system.h"
#include "system/rocprofvis_controller_track.h"
#include "rocprofvis_core_assert.h"
#include <vector>

namespace RocProfVis
{
namespace Controller
{
typedef Reference<rocprofvis_controller_arguments_t, Arguments, kRPVControllerObjectTypeArguments> ArgumentsRef;
typedef Reference<rocprofvis_controller_array_t, Array, kRPVControllerObjectTypeArray> ArrayRef;
typedef Reference<rocprofvis_controller_future_t, Future, kRPVControllerObjectTypeFuture> FutureRef;
typedef Reference<rocprofvis_controller_t, SystemTrace, kRPVControllerObjectTypeControllerSystem> SystemTraceRef;
typedef Reference<rocprofvis_controller_table_t, Table, kRPVControllerObjectTypeTable> TableRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;
}
}

extern "C"
{

rocprofvis_result_t rocprofvis_analysis_fetch_queue_utilization(rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track, double start_time, double end_time, rocprofvis_controller_future_t* result, double* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    RocProfVis::Controller::TrackRef track_ref(track);
    RocProfVis::Controller::FutureRef future(result);
    if(trace.IsValid() && future.IsValid() && track_ref.IsValid() && output)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().AsyncFetchQueueUtilization(trace.Get(), track_ref.Get(), start_time, end_time, output, future.Get());
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_get_instrumented_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    if(trace.IsValid() && table)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().GetInstrumentedThreadEventsTable(trace.Get(), table);
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_get_dispatch_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    if(trace.IsValid() && table)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().GetDispatchEventsTable(trace.Get(), table);
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_get_memory_allocation_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    if(trace.IsValid() && table)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().GetMemoryAllocationEventsTable(trace.Get(), table);
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_get_memory_copy_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    if(trace.IsValid() && table)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().GetMemoryCopyEventsTable(trace.Get(), table);
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_get_sampled_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    if(trace.IsValid() && table)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().GetLaunchSampleEventsTable(trace.Get(), table);
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_fetch_table(rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef system_trace(controller);
    RocProfVis::Controller::TableRef table_ref(table);
    RocProfVis::Controller::ArgumentsRef args_ref(args);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::ArrayRef array(output);

    if(system_trace.IsValid() && table_ref.IsValid() && args_ref.IsValid() && future.IsValid() && array.IsValid())
    {
        error = RocProfVis::Controller::Analysis::GetInstance().AsyncFetchTable(system_trace.Get(), *table_ref, *args_ref, *future, *array);
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_table_export_csv(rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, char const* path)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef system_trace(controller);
    RocProfVis::Controller::TableRef table_ref(table);
    RocProfVis::Controller::ArgumentsRef args_ref(args);
    RocProfVis::Controller::FutureRef future(result);

    if (system_trace.IsValid() && table_ref.IsValid() && args_ref.IsValid() && future.IsValid() && path)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().AsyncTableExportCSV(system_trace.Get(), *table_ref, *args_ref, *future, path);
    }
    return error;
}

void rocprofvis_analysis_free_trace_data(rocprofvis_controller_t* controller)
{
    RocProfVis::Controller::SystemTraceRef trace(controller);
    if(trace.IsValid())
    {
        RocProfVis::Controller::Analysis::GetInstance().FreeTraceData(trace.Get());
    }
}

}

namespace RocProfVis
{
namespace Controller
{

Analysis& Analysis::GetInstance()
{
    static Analysis instance;
    return instance;
}

rocprofvis_result_t Analysis::AsyncFetchTable(SystemTrace* trace, Table& table, Arguments& args, Future& future, Array& array) const
{
    rocprofvis_result_t   error     = kRocProfVisResultUnknownError;
    future.Set(JobSystem::Get().IssueJob([trace, &table, &args, &array](Future* future) -> rocprofvis_result_t {
            return table.SetupAndFetch(*trace, args, array, future);
        }, &future));
    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t Analysis::AsyncTableExportCSV(SystemTrace* trace, Table& table, Arguments& args, Future& future, const char* path) const
{
    rocprofvis_result_t   error     = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = trace->GetDMHandle();
    std::string path_str = path;
    future.Set(JobSystem::Get().IssueJob([&table, dm_handle, &args, path_str](Future* future) -> rocprofvis_result_t {
            return table.ExportCSV(dm_handle, args, future, path_str.c_str());
        }, &future));
    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t Analysis::AsyncFetchQueueUtilization(SystemTrace* trace, Track* track, double start, double end, double* output, Future* future) const
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    future->Set(JobSystem::Get().IssueJob([this, trace, track, start, end, output](Future* future) -> rocprofvis_result_t {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        rocprofvis_handle_t* queue_handle = nullptr;
        result = track->GetObject(kRPVControllerTrackQueue, 0, &queue_handle);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        if(queue_handle)
        {
            rocprofvis_dm_result_t dm_result = kRocProfVisDmResultUnknownError;
            uint64_t track_id = 0;
            result = track->GetUInt64(kRPVControllerTrackId, 0, &track_id);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            rocprofvis_dm_track_t dm_track = track->GetDmHandle();
            rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_track, kRPVDMTrackDatabaseHandle, 0);
            ROCPROFVIS_ASSERT(db);            
            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
            ROCPROFVIS_ASSERT(object2wait);
            uint64_t range_start = (uint64_t)floor(start);
            uint64_t range_end = (uint64_t)ceil(end);
            bool     range_empty = true;
            dm_result = rocprofvis_db_read_trace_slice_async(db, range_start, range_end, kRocProfVisDmHashedTimestampTagAnalysis, 1, (rocprofvis_db_track_selection_t)&track_id, object2wait);
            ROCPROFVIS_ASSERT(dm_result == kRocProfVisDmResultSuccess);
            future->AddDependentFuture(object2wait);
            dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
            ROCPROFVIS_ASSERT(dm_result == kRocProfVisDmResultSuccess);
            rocprofvis_dm_slice_t slice = rocprofvis_dm_get_property_as_handle(dm_track, kRPVDMSliceHandleTimed, rocprofvis_dm_hash_combine_timestamp(range_start, range_end, kRocProfVisDmHashedTimestampTagAnalysis));
            ROCPROFVIS_ASSERT(slice);
            if(slice && !future->IsCancelled())
            {
                uint64_t num_records = rocprofvis_dm_get_property_as_uint64(slice, kRPVDMNumberOfRecordsUInt64, 0);
                uint64_t busy_duration = 0;
                uint64_t busy_start = 0;
                uint64_t busy_end   = 0;                
                for(uint64_t i = 0; i < num_records; i++)
                {
                    uint64_t event_start = rocprofvis_dm_get_property_as_uint64(slice, kRPVDMTimestampUInt64Indexed, i);
                    uint64_t event_end = event_start + rocprofvis_dm_get_property_as_int64(slice, kRPVDMEventDurationInt64Indexed, i);
                    if(event_end > event_start)
                    {
                        if(event_start < range_start)
                        {
                            event_start = range_start;
                        }
                        if(event_end > range_end)
                        {
                            event_end = range_end;
                        }
                        if(range_empty)
                        {
                            busy_start = event_start;
                            busy_end   = event_end;
                            range_empty = false;
                        }
                        else if(event_start <= busy_end)
                        {
                            if(event_end > busy_end)
                            {
                                busy_end = event_end;
                            }
                        }
                        else
                        {
                            busy_duration += busy_end - busy_start;
                            busy_start = event_start;
                            busy_end   = event_end;
                        }
                    }
                }
                if(!range_empty)
                {
                    busy_duration += busy_end - busy_start;
                }
                if(busy_duration == 0)
                {
                    *output = 0.0;
                }
                else if(range_start == range_end)
                {
                    *output = 100.0;
                }
                else
                {
                    *output = (double)busy_duration / (double)(range_end - range_start) * 100.0;
                }
            }
            dm_result = rocprofvis_dm_delete_time_slice_handle(trace->GetDMHandle(), track_id, slice);
            ROCPROFVIS_ASSERT(dm_result == kRocProfVisDmResultSuccess);
            future->RemoveDependentFuture(object2wait);
            rocprofvis_db_future_free(object2wait);
        }
        return future->IsCancelled() ? kRocProfVisResultCancelled : result;
    }, future));
    if(future->IsValid())
    {
        result = kRocProfVisResultSuccess;
    }
    return result;
}

rocprofvis_result_t
Analysis::GetInstrumentedThreadEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    return GetOrAllocateEventsTable(m_data[trace].instrumented_thread_events_table, kRocProfVisDmOperationLaunch, table);
}

rocprofvis_result_t
Analysis::GetDispatchEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    return GetOrAllocateEventsTable(m_data[trace].dispatch_events_table, kRocProfVisDmOperationDispatch, table);
}

rocprofvis_result_t
Analysis::GetMemoryAllocationEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    return GetOrAllocateEventsTable(m_data[trace].memory_allocation_events_table, kRocProfVisDmOperationMemoryAllocate, table);
}

rocprofvis_result_t
Analysis::GetMemoryCopyEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    return GetOrAllocateEventsTable(m_data[trace].memory_copy_events_table, kRocProfVisDmOperationMemoryCopy, table);
}

rocprofvis_result_t
Analysis::GetLaunchSampleEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    return GetOrAllocateEventsTable(m_data[trace].launch_sample_events_table, kRocProfVisDmOperationLaunchSample, table);
}

void Analysis::FreeTraceData(Trace* trace)
{
    if(trace && m_data.count(trace) > 0)
    {
        TraceData& data = m_data.at(trace);
        delete data.instrumented_thread_events_table;
        delete data.dispatch_events_table;
        delete data.memory_allocation_events_table;
        delete data.memory_copy_events_table;
        delete data.launch_sample_events_table;
        m_data.erase(trace);
    }
}

Analysis::EventsTable::EventsTable(uint64_t id, rocprofvis_dm_event_operation_t op)
: SystemTable(id)
, m_op(op)
{}

rocprofvis_result_t Analysis::EventsTable::UnpackArguments(Arguments& args, QueryArguments& out) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    std::vector<uint32_t> tracks;
    uint64_t num_tracks = 0;
    double   end_ts     = 0;
    double   start_ts   = 0;
    uint64_t sort_column_index = 2;
    uint64_t sort_order = (uint64_t)kRPVControllerSortOrderDescending;
    rocprofvis_dm_table_use_case_enum_t use_case;
    result = UnpackUseCase(args, use_case);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    result = args.GetDouble(kRPVControllerTableArgsStartTime, 0, &start_ts);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    result = args.GetDouble(kRPVControllerTableArgsEndTime, 0, &end_ts);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    result = args.GetUInt64(kRPVControllerTableArgsNumTracks, 0, &num_tracks);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    if(num_tracks > 0)
    {
        for(uint32_t i = 0; i < num_tracks && (result == kRocProfVisResultSuccess); i++)
        {
            TrackRef track_ref;
            result = args.GetObject(kRPVControllerTableArgsTracksIndexed, i, track_ref.GetHandleAddress());
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            if(track_ref.IsValid())
            {
                uint64_t track_type = 0;
                result = track_ref->GetUInt64(kRPVControllerTrackType, 0, &track_type);
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(kRPVControllerTrackTypeEvents == rocprofvis_controller_track_type_t(track_type))
                {
                    uint64_t track_id = 0;
                    result = track_ref->GetUInt64(kRPVControllerTrackId, 0, &track_id);
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    tracks.push_back((uint32_t)track_id);
                }
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
        }
    }
    const char* group = (m_op == kRocProfVisDmOperationLaunchSample) ? "name, COUNT(*) AS Invocations, SUM(duration) AS DurationTotal" :
        "name, COUNT(*) AS Invocations, SUM(duration) AS DurationTotal, AVG(duration) AS DurationAvg, MIN(duration) AS DurationMin, MAX(duration) AS DurationMax";
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    result = args.GetUInt64(kRPVControllerTableArgsSortColumn, 0, &sort_column_index);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    result = args.GetUInt64(kRPVControllerTableArgsSortOrder, 0, &sort_order);
    out = {"", "__op = " + std::to_string(m_op), group, "name", sort_column_index, (rocprofvis_controller_sort_order_t)sort_order, tracks, {}, use_case, start_ts, end_ts};
    return result;
}

rocprofvis_result_t
Analysis::EventsTable::UnpackUseCase(Arguments& args, rocprofvis_dm_table_use_case_enum_t& out) const
{
    (void)args;
    out = kRPVDMTableUseCaseEventTrackTable;
    return kRocProfVisResultSuccess;
}

Analysis::Analysis()
{}

Analysis::~Analysis()
{
    for(std::pair<Trace* const, TraceData>& trace : m_data)
    {
        FreeTraceData(trace.first);
    }
    m_data.clear();
}

rocprofvis_result_t Analysis::GetOrAllocateEventsTable(EventsTable*& slot, rocprofvis_dm_event_operation_t op, rocprofvis_handle_t** table)
{
    if(!slot)
    {
        slot = new EventsTable(0, op);
    }
    *table = (rocprofvis_handle_t*)slot;
    return slot ? kRocProfVisResultSuccess : kRocProfVisResultUnknownError;
}

}
}