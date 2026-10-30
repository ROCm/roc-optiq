// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_analysis_internal.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_reference.h"
#include "system/rocprofvis_controller_summary_metrics.h"
#include "system/rocprofvis_controller_table_system.h"
#include "system/rocprofvis_controller_table_system_search.h"
#include "system/rocprofvis_controller_topology.h"
#include "system/rocprofvis_controller_trace_system.h"
#include "system/rocprofvis_controller_track.h"
#include "rocprofvis_core_assert.h"
#include <cstdlib>
#include <cstring>
#include <vector>

namespace RocProfVis
{
namespace Controller
{
typedef Reference<rocprofvis_controller_arguments_t, Arguments, kRPVControllerObjectTypeArguments> ArgumentsRef;
typedef Reference<rocprofvis_controller_array_t, Array, kRPVControllerObjectTypeArray> ArrayRef;
typedef Reference<rocprofvis_controller_future_t, Future, kRPVControllerObjectTypeFuture> FutureRef;
typedef Reference<rocprofvis_controller_t, SystemTrace, kRPVControllerObjectTypeControllerSystem> SystemTraceRef;
typedef Reference<rocprofvis_controller_summary_metrics_t, SummaryMetrics, kRPVControllerObjectTypeSummaryMetrics> SummaryMetricsRef;
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

rocprofvis_result_t rocprofvis_analysis_fetch_counter_statistics(rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track, double start_time, double end_time, rocprofvis_controller_future_t* result, rocprofvis_analysis_counter_statistics_t* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    RocProfVis::Controller::TrackRef track_ref(track);
    RocProfVis::Controller::FutureRef future(result);
    if(trace.IsValid() && future.IsValid() && track_ref.IsValid() && output)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().AsyncFetchCounterStatistics(trace.Get(), track_ref.Get(), start_time, end_time, output, future.Get());
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_fetch_summary(rocprofvis_controller_t* controller, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, rocprofvis_controller_summary_metrics_t* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    RocProfVis::Controller::ArgumentsRef args_ref(args);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::SummaryMetricsRef output_ref(output);
    if(trace.IsValid() && args_ref.IsValid() && future.IsValid() && output_ref.IsValid())
    {
        error = RocProfVis::Controller::Analysis::GetInstance().AsyncFetchSummary(trace.Get(), *args_ref, future.Get(), *output_ref);
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_get_summary_kernel_instance_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    if(trace.IsValid() && table)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().GetSummaryInstanceTable(trace.Get(), kRPVAnalysisSummaryCategoryKernel, table);
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_get_summary_memory_instance_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    if(trace.IsValid() && table)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().GetSummaryInstanceTable(trace.Get(), kRPVAnalysisSummaryCategoryMemory, table);
    }
    return error;
}

rocprofvis_result_t rocprofvis_analysis_get_summary_thread_instance_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::SystemTraceRef trace(controller);
    if(trace.IsValid() && table)
    {
        error = RocProfVis::Controller::Analysis::GetInstance().GetSummaryInstanceTable(trace.Get(), kRPVAnalysisSummaryCategoryThread, table);
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

rocprofvis_result_t rocprofvis_analysis_events_table_alloc(uint64_t op, rocprofvis_handle_t** table)
{
    return RocProfVis::Controller::Analysis::GetInstance().AllocEventsTable(
        static_cast<rocprofvis_dm_event_operation_t>(op), table);
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
        if(track->GetQueue())
        {
            uint64_t               track_id  = track->GetId();
            rocprofvis_dm_track_t dm_track = track->GetDmHandle();
            rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_track, kRPVDMTrackDatabaseHandle, 0);
            ROCPROFVIS_ASSERT(db);            
            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
            ROCPROFVIS_ASSERT(object2wait);
            uint64_t range_start = (uint64_t)floor(start);
            uint64_t range_end = (uint64_t)ceil(end);
            bool     range_empty = true;
            rocprofvis_dm_result_t dm_result   = kRocProfVisDmResultUnknownError;
            dm_result = rocprofvis_db_read_trace_slice_async(db, range_start, range_end, kRocProfVisDmHashedTimestampTagAnalysis, 1, (rocprofvis_db_track_selection_t)&track_id, object2wait);
            ROCPROFVIS_ASSERT(dm_result == kRocProfVisDmResultSuccess);
            future->AddDependentFuture(object2wait);
            dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
            ROCPROFVIS_ASSERT(dm_result == kRocProfVisDmResultSuccess || dm_result == kRocProfVisDmResultDbAbort);
            rocprofvis_dm_slice_t slice = rocprofvis_dm_get_property_as_handle(dm_track, kRPVDMSliceHandleTimed, rocprofvis_dm_hash_combine_timestamp(range_start, range_end, kRocProfVisDmHashedTimestampTagAnalysis));
            if(!future->IsCancelled())
            {
                ROCPROFVIS_ASSERT(slice);
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
            if(!future->IsCancelled())
            {
                ROCPROFVIS_ASSERT(dm_result == kRocProfVisDmResultSuccess);
            }
            future->RemoveDependentFuture(object2wait);
            rocprofvis_db_future_free(object2wait);
            result = kRocProfVisResultSuccess;
        }
        return future->IsCancelled() ? kRocProfVisResultCancelled : result;
    }, future));
    if(future->IsValid())
    {
        result = kRocProfVisResultSuccess;
    }
    return result;
}

rocprofvis_result_t Analysis::AsyncFetchCounterStatistics(SystemTrace* trace, Track* track, double start, double end, rocprofvis_analysis_counter_statistics_t* output, Future* future) const
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    future->Set(JobSystem::Get().IssueJob([this, trace, track, start, end, output](Future* future) -> rocprofvis_result_t {
        return FetchCounterStatistics(trace->GetDMHandle(), track, start, end, kRocProfVisDmHashedTimestampTagAnalysis, output, nullptr, future);
    }, future));
    if(future->IsValid())
    {
        result = kRocProfVisResultSuccess;
    }
    return result;
}

rocprofvis_result_t Analysis::AsyncFetchSummary(SystemTrace* trace, Arguments& args, Future* future, SummaryMetrics& output) const
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    future->Set(JobSystem::Get().IssueJob([this, trace, &args, &output](Future* future) -> rocprofvis_result_t {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        double start = -1.0;
        double end = -1.0;
        result = args.GetDouble(kRPVAnalysisSummaryArgsStartTimestamp, 0, &start);
        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetDouble(kRPVAnalysisSummaryArgsEndTimestamp, 0, &end);
            if(result == kRocProfVisResultSuccess)
            {
                SummaryMetrics metrics;
                result = FetchSummaryMetrics(trace, start, end, metrics, future);
                if(result == kRocProfVisResultSuccess && !future->IsCancelled())
                {
                    output = metrics;
                }
            }
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
Analysis::GetSummaryInstanceTable(SystemTrace* trace, rocprofvis_analysis_summary_category_t category, rocprofvis_handle_t** table)
{
    ROCPROFVIS_ASSERT(category < __kRPVAnalysisSummaryCategoryLast);
    std::lock_guard<std::mutex> lock(m_data_mutex);
    EventSearchTable*& slot = m_data[trace].summary_instance_tables[category];
    if(!slot)
    {
        slot = new EventSearchTable(0);
    }
    *table = (rocprofvis_handle_t*)slot;
    return slot ? kRocProfVisResultSuccess : kRocProfVisResultUnknownError;
}

rocprofvis_result_t
Analysis::GetInstrumentedThreadEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    std::lock_guard<std::mutex> lock(m_data_mutex);
    return GetOrAllocateEventsTable(m_data[trace].instrumented_thread_events_table, kRocProfVisDmOperationLaunch, table);
}

rocprofvis_result_t
Analysis::GetDispatchEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    std::lock_guard<std::mutex> lock(m_data_mutex);
    return GetOrAllocateEventsTable(m_data[trace].dispatch_events_table, kRocProfVisDmOperationDispatch, table);
}

rocprofvis_result_t
Analysis::GetMemoryAllocationEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    std::lock_guard<std::mutex> lock(m_data_mutex);
    return GetOrAllocateEventsTable(m_data[trace].memory_allocation_events_table, kRocProfVisDmOperationMemoryAllocate, table);
}

rocprofvis_result_t
Analysis::GetMemoryCopyEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    std::lock_guard<std::mutex> lock(m_data_mutex);
    return GetOrAllocateEventsTable(m_data[trace].memory_copy_events_table, kRocProfVisDmOperationMemoryCopy, table);
}

rocprofvis_result_t
Analysis::GetLaunchSampleEventsTable(SystemTrace* trace, rocprofvis_handle_t** table)
{
    std::lock_guard<std::mutex> lock(m_data_mutex);
    return GetOrAllocateEventsTable(m_data[trace].launch_sample_events_table, kRocProfVisDmOperationLaunchSample, table);
}

rocprofvis_result_t Analysis::AllocEventsTable(rocprofvis_dm_event_operation_t op, rocprofvis_handle_t** table)
{
    if(!table)
    {
        return kRocProfVisResultInvalidArgument;
    }
    *table = (rocprofvis_handle_t*)new EventsTable(0, op);
    return kRocProfVisResultSuccess;
}

void Analysis::FreeTraceData(Trace* trace)
{
    std::lock_guard<std::mutex> lock(m_data_mutex);
    if(trace && m_data.count(trace) > 0)
    {
        TraceData& data = m_data.at(trace);
        delete data.instrumented_thread_events_table;
        delete data.dispatch_events_table;
        delete data.memory_allocation_events_table;
        delete data.memory_copy_events_table;
        delete data.launch_sample_events_table;
        for(EventSearchTable* summary_instance_table : data.summary_instance_tables)
        {
            delete summary_instance_table;
        }
        m_data.erase(trace);
    }
}

Analysis::EventsTable::EventsTable(uint64_t id, rocprofvis_dm_event_operation_t op)
: SystemTable(id)
, m_op(op)
{}

rocprofvis_result_t Analysis::EventsTable::UnpackArguments(Arguments& args, TableArguments*& out) const
{
    if(!out)
    {
        out = new SystemTableArguments();
    }
    SystemTableArguments* sys_out = (SystemTableArguments*)out;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    std::vector<uint32_t> tracks;
    uint64_t num_tracks = 0;
    double   end_ts     = 0;
    double   start_ts   = 0;
    uint64_t sort_column_index = 2;
    uint64_t sort_order = (uint64_t)kRPVControllerSortOrderDescending;
    rocprofvis_dm_table_use_case_enum_t use_case;
    c_optional_uint64_t node_id;
    c_optional_uint64_t agent_id;
    c_optional_uint64_t process_id;
    c_optional_uint64_t thread_id;
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
                rocprofvis_controller_track_type_t track_type =
                    track_ref->GetTrackType();
                if(kRPVControllerTrackTypeEvents == track_type)
                {
                    uint64_t track_id = track_ref->GetId();
                    tracks.push_back((uint32_t)track_id);
                }
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
        }
    }
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    result = args.GetUInt64(kRPVControllerTableArgsSortColumn, 0, &sort_column_index);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    result = args.GetUInt64(kRPVControllerTableArgsSortOrder, 0, &sort_order);
    sys_out->m_sort_column = sort_column_index;
    sys_out->m_sort_order = (rocprofvis_controller_sort_order_t)sort_order;
    if (kRocProfVisResultSuccess == args.GetUInt64(kRPVControllerTableArgsNodeId, 0, &node_id.value))
    {
        node_id.has_value = true;
    }
    if (kRocProfVisResultSuccess == args.GetUInt64(kRPVControllerTableArgsAgentId, 0, &agent_id.value))
    {
        agent_id.has_value = true;
    }
    if (kRocProfVisResultSuccess == args.GetUInt64(kRPVControllerTableArgsProcessId, 0, &process_id.value))
    {
        process_id.has_value = true;
    }
    if (kRocProfVisResultSuccess == args.GetUInt64(kRPVControllerTableArgsThreadId, 0, &thread_id.value))
    {
        thread_id.has_value = true;
    }
    sys_out->m_source_filter = node_id.has_value || agent_id.has_value || process_id.has_value || thread_id.has_value ? std::make_optional(rocprofvis_dm_query_criteria_t{ node_id, agent_id, process_id, thread_id }) : std::nullopt;
    sys_out->m_filter = "__op = " + std::to_string(m_op);
    uint32_t filter_length = 0;
    if(kRocProfVisResultSuccess ==
           args.GetString(kRPVControllerTableArgsFilter, 0, nullptr, &filter_length) &&
       filter_length > 0)
    {
        std::string filter(filter_length, '\0');
        if(kRocProfVisResultSuccess ==
           args.GetString(kRPVControllerTableArgsFilter, 0, filter.data(), &filter_length))
        {
            sys_out->m_filter += " AND (" + filter + ")";
        }
    }
    sys_out->m_group = (m_op == kRocProfVisDmOperationLaunchSample) ? "name, COUNT(*) AS Invocations, SUM(duration) AS DurationTotal" :
        "name, COUNT(*) AS Invocations, SUM(duration) AS DurationTotal, AVG(duration) AS DurationAvg, MIN(duration) AS DurationMin, MAX(duration) AS DurationMax";
    sys_out->m_group_cols = "name";
    sys_out->m_tracks = std::move(tracks);
    sys_out->m_use_case = use_case;
    sys_out->m_start_ts = start_ts;
    sys_out->m_end_ts = end_ts;
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
    while(!m_data.empty())
    {
        FreeTraceData(m_data.begin()->first);
    }
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

rocprofvis_result_t Analysis::FetchCounterStatistics(rocprofvis_dm_trace_t dm_handle, Track* track, double start, double end, rocprofvis_dm_hashed_timestamp_tag_t tag, rocprofvis_analysis_counter_statistics_t* output, uint64_t* active_duration, Future* future) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(track->GetCounter())
    {
        rocprofvis_dm_result_t   dm_result = kRocProfVisDmResultUnknownError;
        uint64_t                 track_id = track->GetId();
        rocprofvis_dm_track_t    dm_track = track->GetDmHandle();
        rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_track, kRPVDMTrackDatabaseHandle, 0);
        ROCPROFVIS_ASSERT(db);
        rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
        ROCPROFVIS_ASSERT(object2wait);
        uint64_t range_start = (uint64_t)floor(start);
        uint64_t range_end = (uint64_t)ceil(end);
        dm_result = rocprofvis_db_read_trace_pmc_slice_async(db, range_start, range_end, tag, (rocprofvis_db_track_selection_t)&track_id, true, false, object2wait);
        ROCPROFVIS_ASSERT(dm_result == kRocProfVisDmResultSuccess);
        future->AddDependentFuture(object2wait);
        dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
        ROCPROFVIS_ASSERT(dm_result == kRocProfVisDmResultSuccess || dm_result == kRocProfVisDmResultDbAbort);
        rocprofvis_dm_slice_t slice = rocprofvis_dm_get_property_as_handle(dm_track, kRPVDMSliceHandleTimed, rocprofvis_dm_hash_combine_timestamp(range_start, range_end, tag));
        if(!future->IsCancelled())
        {
            ROCPROFVIS_ASSERT(slice);
            uint64_t num_records = rocprofvis_dm_get_property_as_uint64(slice, kRPVDMNumberOfRecordsUInt64, 0);
            double min_value = 0.0;
            double max_value = 0.0;
            double mean_value = 0.0;
            double std_dev = 0.0;
            double sum = 0.0;
            double sum_sq = 0.0;
            double total_duration = 0.0;
            bool range_empty = true;
            for(uint64_t i = 0; i < num_records; i++)
            {
                double value = rocprofvis_dm_get_property_as_double(slice, kRPVDMPmcValueDoubleIndexed, i);
                uint64_t sample_start = rocprofvis_dm_get_property_as_uint64(slice, kRPVDMTimestampUInt64Indexed, i);
                uint64_t sample_end = (i + 1 < num_records) ? rocprofvis_dm_get_property_as_uint64(slice, kRPVDMTimestampUInt64Indexed, i + 1) : range_end;
                if(sample_start < range_start)
                {
                    sample_start = range_start;
                }
                if(sample_end > range_end)
                {
                    sample_end = range_end;
                }
                if(sample_end > sample_start)
                {
                    double duration = (double)(sample_end - sample_start);
                    sum += value * duration;
                    sum_sq += value * value * duration;
                    total_duration += sample_end - sample_start;
                    if(range_empty)
                    {
                        min_value = value;
                        max_value = value;
                        range_empty = false;
                    }
                    else
                    {
                        if(value < min_value)
                        {
                            min_value = value;
                        }
                        if(value > max_value)
                        {
                            max_value = value;
                        }
                    }
                }
            }
            if(total_duration > 0.0)
            {
                mean_value = sum / total_duration;
                double variance = sum_sq / total_duration - mean_value * mean_value;
                std_dev = (variance > 0.0) ? sqrt(variance) : 0.0;
            }
            output->min_value = min_value;
            output->max_value = max_value;
            output->mean_value = mean_value;
            output->std_dev = std_dev;
            if(active_duration)
            {
                *active_duration = total_duration;
            }
        }
        dm_result = rocprofvis_dm_delete_time_slice_handle(dm_handle, track_id, slice);
        if(!future->IsCancelled())
        {
            ROCPROFVIS_ASSERT(dm_result == kRocProfVisDmResultSuccess);
        }
        future->RemoveDependentFuture(object2wait);
        rocprofvis_db_future_free(object2wait);
        result = kRocProfVisResultSuccess;
    }
    return future->IsCancelled() ? kRocProfVisResultCancelled : result;
}

rocprofvis_result_t Analysis::FetchSummaryMetrics(SystemTrace* trace, double start, double end, SummaryMetrics& output, Future* future) const
{
    rocprofvis_result_t   result    = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = trace->GetDMHandle();
    output.Reset();
    future->ResetProgress();
    output.SetObject(kRPVAnalysisSummaryMetricPropertyMetadata, 0, (rocprofvis_handle_t*)trace);
    uint64_t num_nodes = 0;
    result = trace->GetUInt64(kRPVControllerSystemNumNodes, 0, &num_nodes);
    if(result == kRocProfVisResultSuccess)
    {
        double node_weight = num_nodes > 0 ? 100.0 / num_nodes : 0.0;
        for(int i = 0; i < num_nodes && !future->IsCancelled(); i++)
        {
            rocprofvis_handle_t* node_handle = nullptr;
            result = trace->GetObject(kRPVControllerSystemNodeIndexed, i, &node_handle);
            if(result == kRocProfVisResultSuccess)
            {
                Node* node = (Node*)node_handle;
                if(node)
                {
                    uint64_t uint64_data = 0;
                    SummaryMetrics node_metrics;
                    rocprofvis_dm_query_criteria_t node_criteria;
                    node_metrics.SetUInt64(kRPVAnalysisSummaryMetricPropertyAggregationLevel, 0, kRPVAnalysisSummaryAggregationLevelNode);
                    result = node->GetUInt64(kRPVControllerNodeId, 0, &uint64_data);
                    if(result == kRocProfVisResultSuccess)
                    {
                        node_criteria.node_id = uint64_data;
                        result = node_metrics.SetObject(kRPVAnalysisSummaryMetricPropertyMetadata, 0, (rocprofvis_handle_t*)node);
                        if(result == kRocProfVisResultSuccess)
                        {                        
                            uint64_t num_processors = 0;
                            uint64_t num_processes = 0;
                            node->GetUInt64(kRPVControllerNodeNumProcessors, 0, &num_processors);
                            node->GetUInt64(kRPVControllerNodeNumProcesses, 0, &num_processes);
                            double item_weight = (num_processors + num_processes) > 0 ? node_weight / (num_processors + num_processes) : 0.0;
                            if(result == kRocProfVisResultSuccess)
                            {
                                rocprofvis_handle_t* processor_handle = nullptr;
                                for(int j = 0; j < num_processors && !future->IsCancelled(); j++)
                                {
                                    result = node->GetObject(kRPVControllerNodeProcessorIndexed, j, &processor_handle);
                                    if(result == kRocProfVisResultSuccess)
                                    {
                                        Processor* processor = (Processor*)processor_handle;
                                        if(processor)
                                        {
                                            SummaryMetrics processor_metrics;
                                            rocprofvis_dm_query_criteria_t processor_criteria = node_criteria;
                                            uint64_t processor_type = kRPVControllerProcessorTypeUndefined;
                                            processor_metrics.SetUInt64(kRPVAnalysisSummaryMetricPropertyAggregationLevel, 0, kRPVAnalysisSummaryAggregationLevelProcessor);
                                            result = processor_metrics.SetObject(kRPVAnalysisSummaryMetricPropertyMetadata, 0, (rocprofvis_handle_t*)processor);
                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                result = processor->GetUInt64(kRPVControllerProcessorId, 0, &uint64_data);
                                            }
                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                processor_criteria.agent_id = uint64_data;
                                                result = processor->GetUInt64(kRPVControllerProcessorType, 0, &processor_type);
                                            }
                                            if(result == kRocProfVisResultSuccess)
                                            {                                                
                                                if(processor_type == kRPVControllerProcessorTypeGPU)
                                                {
                                                    result = FetchTopEvents(
                                                        dm_handle,
                                                        { kRocProfVisDmOperationDispatch },
                                                        processor_criteria, start, end,
                                                        { kRPVAnalysisSummaryGpuPropertyNumTopDispatchEvents,
                                                          kRPVAnalysisSummaryGpuPropertyDispatchEventsInvocationsTotal,
                                                          kRPVAnalysisSummaryGpuPropertyDispatchEventsDurationTotal },
                                                        { kRPVAnalysisSummaryGpuPropertyTopDispatchEventInvocationsIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationSumIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationMinIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationMaxIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationPctIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationAvgIndexed },
                                                        kRPVAnalysisSummaryGpuPropertyTopDispatchEventNameIndexed,
                                                        processor_metrics, future);
                                                    if(result != kRocProfVisResultSuccess)
                                                    {
                                                        return future->IsCancelled()
                                                                   ? kRocProfVisResultCancelled
                                                                   : result;
                                                    }
                                                    result = FetchTopEvents(
                                                        dm_handle,
                                                        { kRocProfVisDmOperationMemoryAllocate,
                                                          kRocProfVisDmOperationMemoryCopy },
                                                        processor_criteria, start, end,
                                                        { kRPVAnalysisSummaryGpuPropertyNumTopMemoryEvents,
                                                          kRPVAnalysisSummaryGpuPropertyMemoryEventsInvocationsTotal,
                                                          kRPVAnalysisSummaryGpuPropertyMemoryEventsDurationTotal },
                                                        { kRPVAnalysisSummaryGpuPropertyTopMemoryEventInvocationsIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationSumIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationMinIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationMaxIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationPctIndexed,
                                                          kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationAvgIndexed },
                                                        kRPVAnalysisSummaryGpuPropertyTopMemoryEventNameIndexed,
                                                        processor_metrics, future);
                                                    if(result != kRocProfVisResultSuccess)
                                                    {
                                                        return future->IsCancelled()
                                                                   ? kRocProfVisResultCancelled
                                                                   : result;
                                                    }
                                                }
                                                else if(processor_type == kRPVControllerProcessorTypeCPU)
                                                {
                                                    result = FetchTopEvents(
                                                        dm_handle,
                                                        { kRocProfVisDmOperationMemoryAllocate,
                                                          kRocProfVisDmOperationMemoryCopy },
                                                        processor_criteria, start, end,
                                                        { kRPVAnalysisSummaryCpuPropertyNumTopMemoryEvents,
                                                          kRPVAnalysisSummaryCpuPropertyMemoryEventsInvocationsTotal,
                                                          kRPVAnalysisSummaryCpuPropertyMemoryEventsDurationTotal },
                                                        { kRPVAnalysisSummaryCpuPropertyTopMemoryEventInvocationsIndexed,
                                                          kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationSumIndexed,
                                                          kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationMinIndexed,
                                                          kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationMaxIndexed,
                                                          kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationPctIndexed,
                                                          kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationAvgIndexed },
                                                        kRPVAnalysisSummaryCpuPropertyTopMemoryEventNameIndexed,
                                                        processor_metrics, future);
                                                    if(result != kRocProfVisResultSuccess)
                                                    {
                                                        return future->IsCancelled()
                                                                   ? kRocProfVisResultCancelled
                                                                   : result;
                                                    }
                                                }
                                                uint64_t num_counters = 0;
                                                result = processor->GetUInt64(kRPVControllerProcessorNumCounters, 0, &num_counters);
                                                if(result == kRocProfVisResultSuccess)
                                                {
                                                    for(int k = 0; k < num_counters && !future->IsCancelled(); k++)
                                                    {
                                                        rocprofvis_handle_t* counter_handle = nullptr;
                                                        result = processor->GetObject(kRPVControllerProcessorCounterIndexed, k, &counter_handle);
                                                        if(result == kRocProfVisResultSuccess)
                                                        {
                                                            Counter* counter = (Counter*)counter_handle;
                                                            if(counter)
                                                            {
                                                                result =
                                                                    FetchCounterStatistics(
                                                                        dm_handle,
                                                                        processor,
                                                                        counter, start,
                                                                        end,
                                                                        processor_metrics,
                                                                        future);
                                                                if(result !=
                                                                   kRocProfVisResultSuccess)
                                                                {
                                                                    return future->IsCancelled()
                                                                               ? kRocProfVisResultCancelled
                                                                               : result;
                                                                }
                                                            }
                                                            else
                                                            {
                                                                result = kRocProfVisResultUnknownError;
                                                                break;
                                                            }
                                                        }
                                                        else
                                                        {
                                                            break;
                                                        }
                                                    }
                                                }
                                                else
                                                {
                                                    break;
                                                }
                                            }
                                            else
                                            {
                                                break;
                                            }
                                            if(result == kRocProfVisResultSuccess && !processor_metrics.Empty())
                                            {
                                                result = node_metrics.GetUInt64(kRPVAnalysisSummaryMetricPropertyNumSubMetrics, 0, &uint64_data);
                                                if(result == kRocProfVisResultSuccess)
                                                {
                                                    node_metrics.SetUInt64(kRPVAnalysisSummaryMetricPropertyNumSubMetrics, 0, uint64_data + 1);
                                                    node_metrics.SetObject(kRPVAnalysisSummaryMetricPropertySubMetricsIndexed, uint64_data, (rocprofvis_handle_t*)&processor_metrics);
                                                }
                                            }
                                            future->SetUInt64(kRPVControllerFutureProgressPercentage, 0, (uint64_t)(i * node_weight + (j + 1) * item_weight));
                                        }
                                        else
                                        {
                                            result = kRocProfVisResultUnknownError;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                break;
                            }
                            rocprofvis_handle_t* process_handle = nullptr;
                            for(int j = 0; j < num_processes && !future->IsCancelled(); j++)
                            {
                                result = node->GetObject(kRPVControllerNodeProcessIndexed, j, &process_handle);
                                if(result == kRocProfVisResultSuccess)
                                {
                                    Process* process = (Process*)process_handle;
                                    if(process)
                                    {
                                        std::unordered_set<uint64_t> unique_threads;
                                        SummaryMetrics process_metrics;
                                        rocprofvis_dm_query_criteria_t process_criteria = node_criteria;
                                        process_metrics.SetUInt64(kRPVAnalysisSummaryMetricPropertyAggregationLevel, 0, kRPVAnalysisSummaryAggregationLevelProcess);
                                        result = process_metrics.SetObject(kRPVAnalysisSummaryMetricPropertyMetadata, 0, (rocprofvis_handle_t*)process);
                                        if(result == kRocProfVisResultSuccess)
                                        {
                                            result = process->GetUInt64(kRPVControllerProcessId, 0, &uint64_data);
                                        }
                                        if(result == kRocProfVisResultSuccess)
                                        {
                                            process_criteria.process_id = uint64_data;
                                            uint64_t num_threads = 0;
                                            result = process->GetUInt64(kRPVControllerProcessNumThreads, 0, &num_threads);
                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                rocprofvis_handle_t* thread_handle = nullptr;
                                                for(int k = 0; k < num_threads && !future->IsCancelled(); k++)
                                                {
                                                    result = process->GetObject(kRPVControllerProcessThreadIndexed, k, &thread_handle);
                                                    if(result == kRocProfVisResultSuccess)
                                                    {
                                                        Thread* thread = (Thread*)thread_handle;
                                                        if(thread)
                                                        {
                                                            uint64_t thread_id = 0;
                                                            result = thread->GetUInt64(kRPVControllerThreadId, 0, &thread_id);
                                                            if(result == kRocProfVisResultSuccess && !unique_threads.count(thread_id))
                                                            {
                                                                SummaryMetrics thread_metrics;
                                                                thread_metrics.SetUInt64(kRPVAnalysisSummaryMetricPropertyAggregationLevel, 0, kRPVAnalysisSummaryAggregationLevelThread);
                                                                result = thread_metrics.SetObject(kRPVAnalysisSummaryMetricPropertyMetadata, 0, (rocprofvis_handle_t*)thread);
                                                                if(result == kRocProfVisResultSuccess)
                                                                {
                                                                    rocprofvis_dm_query_criteria_t thread_criteria = process_criteria;
                                                                    thread_criteria.thread_id = thread_id;
                                                                    unique_threads.insert(thread_id);
                                                                    result = FetchTopEvents(
                                                                        dm_handle,
                                                                        { kRocProfVisDmOperationLaunch,
                                                                          kRocProfVisDmOperationLaunchSample },
                                                                        thread_criteria,
                                                                        start, end,
                                                                        { kRPVAnalysisSummaryProcessThreadPropertyNumTopLaunchEvents,
                                                                          kRPVAnalysisSummaryProcessThreadPropertyLaunchEventsInvocationsTotal,
                                                                          kRPVAnalysisSummaryProcessThreadPropertyLaunchEventsDurationTotal },
                                                                        { kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventInvocationsIndexed,
                                                                          kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationSumIndexed,
                                                                          kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationMinIndexed,
                                                                          kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationMaxIndexed,
                                                                          kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationPctIndexed,
                                                                          kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationAvgIndexed },
                                                                        kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventNameIndexed,
                                                                        thread_metrics,
                                                                        future);
                                                                    if(result !=
                                                                       kRocProfVisResultSuccess)
                                                                    {
                                                                        return future->IsCancelled()
                                                                                   ? kRocProfVisResultCancelled
                                                                                   : result;
                                                                    }
                                                                }
                                                                else
                                                                {
                                                                    break;
                                                                }
                                                                if(result == kRocProfVisResultSuccess && !thread_metrics.Empty())
                                                                {
                                                                    result = process_metrics.GetUInt64(kRPVAnalysisSummaryMetricPropertyNumSubMetrics, 0, &uint64_data);
                                                                    if(result == kRocProfVisResultSuccess)
                                                                    {
                                                                        process_metrics.SetUInt64(kRPVAnalysisSummaryMetricPropertyNumSubMetrics, 0, uint64_data + 1);
                                                                        process_metrics.SetObject(kRPVAnalysisSummaryMetricPropertySubMetricsIndexed, uint64_data, (rocprofvis_handle_t*)&thread_metrics);
                                                                    }
                                                                }
                                                            }                                                            
                                                        }
                                                        else
                                                        {
                                                            result = kRocProfVisResultUnknownError;
                                                            break;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        break;
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            break;
                                        }
                                        if(result == kRocProfVisResultSuccess && !process_metrics.Empty())
                                        {
                                            result = node_metrics.GetUInt64(kRPVAnalysisSummaryMetricPropertyNumSubMetrics, 0, &uint64_data);
                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                node_metrics.SetUInt64(kRPVAnalysisSummaryMetricPropertyNumSubMetrics, 0, uint64_data + 1);
                                                node_metrics.SetObject(kRPVAnalysisSummaryMetricPropertySubMetricsIndexed, uint64_data, (rocprofvis_handle_t*)&process_metrics);
                                            }
                                        }
                                        future->SetUInt64(kRPVControllerFutureProgressPercentage, 0, (uint64_t)(i * node_weight + (num_processors + j + 1) * item_weight));
                                    }
                                    else
                                    {
                                        result = kRocProfVisResultUnknownError;
                                        break;
                                    }
                                }
                                else
                                {
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        break;
                    }
                    if(result == kRocProfVisResultSuccess && !node_metrics.Empty())
                    {
                        result = output.GetUInt64(kRPVAnalysisSummaryMetricPropertyNumSubMetrics, 0, &uint64_data);
                        if(result == kRocProfVisResultSuccess)
                        {
                            output.SetUInt64(kRPVAnalysisSummaryMetricPropertyNumSubMetrics, 0, uint64_data + 1);
                            output.SetObject(kRPVAnalysisSummaryMetricPropertySubMetricsIndexed, uint64_data, (rocprofvis_handle_t*)&node_metrics);
                        }
                        else
                        {
                            result = kRocProfVisResultUnknownError;
                            break;
                        }
                    }
                    future->SetUInt64(kRPVControllerFutureProgressPercentage, 0, (uint64_t)((i + 1) * node_weight));
                }
                else
                {
                    result = kRocProfVisResultUnknownError;
                    break;
                }
            }
            else
            {
                break;
            }
        }
    }
    if(result == kRocProfVisResultSuccess)
    {
        output.AggregateSubMetrics();
        future->SetUInt64(kRPVControllerFutureProgressPercentage, 0, 100);
    }
    return future->IsCancelled() ? kRocProfVisResultCancelled : result;
}

rocprofvis_result_t Analysis::FetchTopEvents(rocprofvis_dm_trace_t dm_handle, const std::vector<rocprofvis_dm_event_operation_t>& ops, const rocprofvis_dm_query_criteria_t& criteria, double start, double end, const std::array<rocprofvis_property_t, __kRPVAnalysisSummaryTopEventsMetricLast>& op_properties, const std::array<rocprofvis_property_t, __kRPVAnalysisSummaryTopEventMetricLast>& event_properties, rocprofvis_property_t event_name_property, SummaryMetrics& output, Future* future) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(!future->IsCancelled())
    {
        rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
        if(db)
        {
            std::vector<uint32_t> tracks(ops.size());
            for(size_t i = 0; i < ops.size(); i++)
            {
                tracks[i] = TABLE_QUERY_PACK_OP_TYPE(ops[i]);
            }
            char* query = nullptr;
            rocprofvis_dm_result_t dm_result = kRocProfVisDmResultUnknownError;
            dm_result = rocprofvis_db_build_table_query(db, kRPVDMTableUseCaseEventTrackTable,
                                                        (rocprofvis_dm_timestamp_t)start, (rocprofvis_dm_timestamp_t)end,
                                                        tracks.size(), (rocprofvis_db_track_selection_t)tracks.data(),
                                                        (rocprofvis_dm_query_criteria_t*)&criteria, nullptr,
                                                        "name, COUNT(*) AS num_invocations, MIN(duration) AS min_duration, MAX(duration) AS max_duration, SUM(duration) AS total_duration", "name",
                                                        "total_duration", kRPVDMSortOrderDesc,
                                                        UINT32_MAX, 0, false, &query);
            if(dm_result == kRocProfVisDmResultSuccess)
            {
                dm_result = ExecuteTableQuery(dm_handle, query, future, [&output, &op_properties, &event_properties, event_name_property](rocprofvis_dm_table_t table, uint64_t rows, uint64_t cols) -> rocprofvis_dm_result_t{
                    rocprofvis_dm_result_t result = kRocProfVisDmResultUnknownError;
                    if(rows > 0)
                    {
                        uint64_t invocations_total = 0;
                        uint64_t exec_time_total = 0;
                        output.SetUInt64(op_properties[kRPVAnalysisSummaryTopEventsMetricNumTopEvents], 0, rows);
                        for(uint64_t i = 0; i < rows; i++)
                        {
                            rocprofvis_dm_table_row_t table_row = rocprofvis_dm_get_property_as_handle(table, kRPVDMExtTableRowHandleIndexed, i);
                            if(table_row != nullptr)
                            {
                                uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(table_row, kRPVDMNumberOfTableRowCellsUInt64, 0);
                                if(num_cells == cols)
                                {
                                    for(uint64_t j = 0; j < num_cells; j++)
                                    {
                                        char const* col_name = rocprofvis_dm_get_property_as_charptr(table, kRPVDMExtTableColumnNameCharPtrIndexed, j);
                                        char const* value = rocprofvis_dm_get_property_as_charptr(table_row, kRPVDMExtTableRowCellValueCharPtrIndexed, j);
                                        if(strcmp(col_name, "name") == 0)
                                        {
                                            output.SetString(event_name_property, i, value);
                                        }
                                        else if(strcmp(col_name, "num_invocations") == 0)
                                        {
                                            uint64_t num_invocations = strtoull(value, NULL, 10);
                                            output.SetUInt64(event_properties[kRPVAnalysisSummaryTopEventMetricInvocations], i, num_invocations);
                                            invocations_total += num_invocations;
                                        }
                                        else if(strcmp(col_name, "total_duration") == 0)
                                        {
                                            uint64_t total_duration = strtoull(value, NULL, 10);
                                            output.SetUInt64(event_properties[kRPVAnalysisSummaryTopEventMetricDurationSum], i, total_duration);
                                            exec_time_total += total_duration;
                                        }
                                        else if(strcmp(col_name, "min_duration") == 0)
                                        {
                                            output.SetUInt64(event_properties[kRPVAnalysisSummaryTopEventMetricDurationMin], i, strtoull(value, NULL, 10));
                                        }
                                        else if(strcmp(col_name, "max_duration") == 0)
                                        {
                                            output.SetUInt64(event_properties[kRPVAnalysisSummaryTopEventMetricDurationMax], i, strtoull(value, NULL, 10));
                                        }
                                    }
                                }
                            }
                        }
                        if(invocations_total)
                        {
                            uint64_t total_duration = 0;
                            for(uint64_t i = 0; i < rows; i++)
                            {
                                if(kRocProfVisResultSuccess == output.GetUInt64(event_properties[kRPVAnalysisSummaryTopEventMetricDurationSum], i, &total_duration))
                                {
                                    output.SetDouble(event_properties[kRPVAnalysisSummaryTopEventMetricDurationPct], i, exec_time_total ? (double)(total_duration) / exec_time_total : 0.0);
                                }
                            }
                            output.SetUInt64(op_properties[kRPVAnalysisSummaryTopEventsMetricInvocations], 0, invocations_total);
                            output.SetUInt64(op_properties[kRPVAnalysisSummaryTopEventsMetricDurationSum], 0, exec_time_total);
                            result = kRocProfVisDmResultSuccess;
                        }
                    }
                    return result;
                });
                free(query);
            }
            result = (dm_result == kRocProfVisDmResultSuccess) ? kRocProfVisResultSuccess : kRocProfVisResultUnknownError;
        }
    }
    return future->IsCancelled() ? kRocProfVisResultCancelled : result;
}

rocprofvis_result_t Analysis::FetchCounterStatistics(rocprofvis_dm_trace_t dm_handle, Processor* processor, Counter* counter, double start, double end, SummaryMetrics& output, Future* future) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(processor)
    {
        uint64_t processor_type = kRPVControllerProcessorTypeUndefined;
        result = processor->GetUInt64(kRPVControllerProcessorType, 0, &processor_type);
        if(result == kRocProfVisResultSuccess)
        {
            if(processor_type == kRPVControllerProcessorTypeGPU || processor_type == kRPVControllerProcessorTypeCPU)
            {
                rocprofvis_handle_t* track_handle = nullptr;
                result = counter->GetObject(kRPVControllerCounterTrack, 0, &track_handle);
                if(result == kRocProfVisResultSuccess)
                {
                    if(track_handle)
                    {
                        Track* track = (Track*)track_handle;
                        if(track->GetNumberOfEntries())
                        {
                            rocprofvis_analysis_counter_statistics_t stats = {};
                            uint64_t duration = 0;
                            result = FetchCounterStatistics(dm_handle, track, start, end, kRocProfVisDmHashedTimestampTagSummary, &stats, &duration, future);
                            // A counter with no active time in the range has nothing to weight its statistics by, so it is left out of the summary.
                            if(result == kRocProfVisResultSuccess && duration > 0)
                            {
                                bool is_cpu = processor_type == kRPVControllerProcessorTypeCPU;
                                uint64_t num_counters = 0;
                                output.GetUInt64(is_cpu ? kRPVAnalysisSummaryCpuPropertyNumCounters : kRPVAnalysisSummaryGpuPropertyNumCounters, 0, &num_counters);
                                output.SetUInt64(is_cpu ? kRPVAnalysisSummaryCpuPropertyNumCounters : kRPVAnalysisSummaryGpuPropertyNumCounters, 0, num_counters + 1);
                                output.SetObject(is_cpu ? kRPVAnalysisSummaryCpuPropertyCounterIndexed : kRPVAnalysisSummaryGpuPropertyCounterIndexed, num_counters, (rocprofvis_handle_t*)counter);
                                output.SetDouble(is_cpu ? kRPVAnalysisSummaryCpuPropertyCounterMinIndexed : kRPVAnalysisSummaryGpuPropertyCounterMinIndexed, num_counters, stats.min_value);
                                output.SetDouble(is_cpu ? kRPVAnalysisSummaryCpuPropertyCounterMaxIndexed : kRPVAnalysisSummaryGpuPropertyCounterMaxIndexed, num_counters, stats.max_value);
                                output.SetDouble(is_cpu ? kRPVAnalysisSummaryCpuPropertyCounterMeanIndexed : kRPVAnalysisSummaryGpuPropertyCounterMeanIndexed, num_counters, stats.mean_value);
                                output.SetDouble(is_cpu ? kRPVAnalysisSummaryCpuPropertyCounterStdDevIndexed : kRPVAnalysisSummaryGpuPropertyCounterStdDevIndexed, num_counters, stats.std_dev);
                                output.SetUInt64(is_cpu ? kRPVAnalysisSummaryCpuPropertyCounterDurationIndexed : kRPVAnalysisSummaryGpuPropertyCounterDurationIndexed, num_counters, duration);
                            }
                        }
                        else
                        {
                            result = kRocProfVisResultSuccess;
                        }
                    }
                    else
                    {
                        result = kRocProfVisResultUnknownError;
                    }
                }
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
        }
    }
    return result;
}

rocprofvis_dm_result_t Analysis::ExecuteTableQuery(rocprofvis_dm_trace_t dm_handle, const char* query, Future* future, std::function<rocprofvis_dm_result_t(rocprofvis_dm_table_t table, uint64_t rows, uint64_t cols)> callback) const
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultUnknownError;
    if(query)
    {
        if(strlen(query))
        {
            rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
            if(db)
            {
                rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
                if(object2wait)
                {
                    rocprofvis_dm_table_id_t table_id = 0;
                    result = rocprofvis_db_execute_query_async(db, query, "Analysis::ExecuteTableQuery", object2wait, &table_id);
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        future->AddDependentFuture(object2wait);
                        result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
                        if(result == kRocProfVisDmResultSuccess)
                        {
                            uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                            if(num_tables > 0)
                            {
                                rocprofvis_dm_table_t table = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMTableHandleByID, table_id);
                                if(table)
                                {
                                    if(!future->IsCancelled())
                                    {
                                        const char* table_query = rocprofvis_dm_get_property_as_charptr(table, kRPVDMExtTableQueryCharPtr, 0);
                                        uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(table, kRPVDMNumberOfTableRowsUInt64, 0);
                                        uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(table, kRPVDMNumberOfTableColumnsUInt64, 0);
                                        if(strcmp(table_query, query) == 0)
                                        {
                                            result = num_rows ? callback(table, num_rows, num_columns) : kRocProfVisDmResultSuccess;
                                        }
                                        else
                                        {
                                            result = kRocProfVisDmResultUnknownError;
                                        }
                                    }
                                }
                            }
                        }
                        future->RemoveDependentFuture(object2wait);
                    }
                    rocprofvis_db_future_free(object2wait);
                    rocprofvis_dm_delete_table_at(dm_handle, table_id);
                }
            }
        }
        else
        {
            result = kRocProfVisDmResultSuccess;
        }
    }
    return result;
}

}
}
