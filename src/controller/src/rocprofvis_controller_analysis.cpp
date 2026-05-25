// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_analysis.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_reference.h"
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

typedef Reference<rocprofvis_controller_future_t, Future, kRPVControllerObjectTypeFuture> FutureRef;
typedef Reference<rocprofvis_controller_t, SystemTrace, kRPVControllerObjectTypeControllerSystem> SystemTraceRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;

}  // namespace Controller
}

extern "C"
{

rocprofvis_result_t rocprofvis_controller_analysis_fetch_queue_utilization(rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track, double start_time, double end_time, rocprofvis_controller_future_t* result, double* output)
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

Analysis::Analysis()
{}

Analysis::~Analysis()
{}

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
            uint64_t track_id = 0;
            result = track->GetUInt64(kRPVControllerTrackId, 0, &track_id);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(trace->GetDMHandle(), kRPVDMDatabaseHandle, 0);
            ROCPROFVIS_ASSERT(db);
            char* query = nullptr;
            double busy_duration = 0.0;
            uint64_t event_count = 0;
            rocprofvis_dm_result_t dm_result = rocprofvis_db_build_table_query(db, kRPVDMTableUseCaseAnalysis, start, end, 1, (rocprofvis_db_track_selection_t)&track_id, nullptr, nullptr,
                                                                                "SUM(duration) AS total_duration, COUNT(*) AS event_count", "__trackId",
                                                                                nullptr, kRPVDMSortOrderAsc, 0, nullptr, 0, 0, false, &query);
            if(dm_result == kRocProfVisDmResultSuccess && strlen(query) > 0)
            {
                result = ExecuteQuery(trace, query, "Coarse queue utilization", *future, [start, end, &busy_duration, &event_count](const QueryDataStore& store) -> rocprofvis_result_t {
                    rocprofvis_result_t result = kRocProfVisResultUnknownError;
                    if(store.rows.size() == 1 && store.columns.count("total_duration") > 0 && store.columns.count("event_count") > 0)
                    {
                        const char* duration = store.rows[0][store.columns.at("total_duration")];
                        const char* count = store.rows[0][store.columns.at("event_count")];
                        if(duration && strlen(duration) > 0 && count && strlen(count) > 0)
                        {
                            busy_duration = strtod(duration, NULL);
                            event_count = strtoull(count, NULL, 10);
                            result = kRocProfVisResultSuccess;                        
                        }
                    }
                    return result;
                });
            }
            free(query);
            if(dm_result == kRocProfVisDmResultSuccess && result == kRocProfVisResultSuccess && event_count > 0)
            {
                dm_result = rocprofvis_db_build_table_query(db, kRPVDMTableUseCaseAnalysis, start, end, 1, (rocprofvis_db_track_selection_t)&track_id, nullptr, nullptr, nullptr, nullptr,
                                                                                nullptr, kRPVDMSortOrderAsc, 0, nullptr, 1, 0, false, &query);
                if(dm_result == kRocProfVisDmResultSuccess && strlen(query) > 0)
                {
                    result = ExecuteQuery(trace, query, "Fine queue utilization", *future, [start, end, &busy_duration](const QueryDataStore& store) -> rocprofvis_result_t {
                        rocprofvis_result_t result = kRocProfVisResultUnknownError;
                        if(store.rows.size() == 1 && store.columns.count("start") > 0)
                        {
                            const char* data = store.rows[0][store.columns.at("start")];
                            if(data && strlen(data) > 0)
                            {
                                double head = strtod(data, NULL);
                                if(head < start)
                                {
                                    busy_duration -= (start - head);
                                }
                                result = kRocProfVisResultSuccess;                        
                            }
                        }
                        return result;
                    });
                }
                free(query);
                dm_result = rocprofvis_db_build_table_query(db, kRPVDMTableUseCaseAnalysis, start, end, 1, (rocprofvis_db_track_selection_t)&track_id, nullptr, nullptr, nullptr, nullptr,
                                                                                    nullptr, kRPVDMSortOrderAsc, 0, nullptr, 1, event_count - 1, false, &query);
                if(dm_result == kRocProfVisDmResultSuccess && strlen(query) > 0)
                {
                    result = ExecuteQuery(trace, query, "Fine queue utilization", *future, [start, end, &busy_duration](const QueryDataStore& store) -> rocprofvis_result_t {
                        rocprofvis_result_t result = kRocProfVisResultUnknownError;
                        if(store.rows.size() == 1 && store.columns.count("end") > 0)
                        {
                            const char* data = store.rows[0][store.columns.at("end")];
                            if(data && strlen(data) > 0)
                            {
                                double tail = strtod(data, NULL);
                                if(end < tail)
                                {
                                    busy_duration -= (tail - end);
                                }
                                result = kRocProfVisResultSuccess;                        
                            }
                        }
                        return result;
                    });
                }
                free(query);
            }
            if(dm_result == kRocProfVisDmResultSuccess)
            {
                if(result == kRocProfVisResultSuccess)
                {
                    *output = busy_duration / (end - start) * 100.0;
                }
                else if(result == kRocProfVisResultNotLoaded)
                {
                    *output = 0.0;
                    result = kRocProfVisResultSuccess;
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

rocprofvis_result_t Analysis::ExecuteQuery(Trace* trace, const char* query, const char* description, Future& future, QueryCallback callback) const
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    ROCPROFVIS_ASSERT(trace && query && description);
    rocprofvis_dm_handle_t dm_handle = trace->GetDMHandle();
    ROCPROFVIS_ASSERT(dm_handle);
    rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
    ROCPROFVIS_ASSERT(db);
    rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
    if(object2wait)
    {
        rocprofvis_dm_table_id_t table_id = 0;
        rocprofvis_dm_result_t dm_result = rocprofvis_db_execute_query_async(db, query, description, object2wait, &table_id);
        if(dm_result == kRocProfVisDmResultSuccess)
        {
            future.AddDependentFuture(object2wait);
            dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
            if(dm_result == kRocProfVisDmResultSuccess)
            {
                uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                if(num_tables > 0)
                {
                    rocprofvis_dm_table_t table = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMTableHandleByID, table_id);
                    if(table)
                    {
                        if(future.IsCancelled())
                        {
                            result = kRocProfVisResultCancelled;
                        }
                        else
                        {
                            const char* table_query = rocprofvis_dm_get_property_as_charptr(table, kRPVDMExtTableQueryCharPtr, 0);
                            uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(table, kRPVDMNumberOfTableRowsUInt64, 0);
                            uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(table, kRPVDMNumberOfTableColumnsUInt64, 0);
                            if(table_query && strcmp(table_query, query) == 0)
                            {
                                QueryDataStore data_store;
                                for(uint64_t c = 0; c < num_columns; c++)
                                {
                                    data_store.columns[rocprofvis_dm_get_property_as_charptr(table, kRPVDMExtTableColumnNameCharPtrIndexed, c)] = c;
                                }
                                data_store.rows.resize(num_rows);
                                bool rows_ok = true;
                                for(uint64_t r = 0; r < num_rows && rows_ok; r++)
                                {
                                    rocprofvis_dm_table_row_t table_row = rocprofvis_dm_get_property_as_handle(table, kRPVDMExtTableRowHandleIndexed, r);
                                    if(table_row)
                                    {
                                        uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(table_row, kRPVDMNumberOfTableRowCellsUInt64, 0);
                                        if(num_cells == num_columns)
                                        {
                                            data_store.rows[r].resize(num_columns);
                                            for(uint64_t c = 0; c < num_columns; c++)
                                            {
                                                data_store.rows[r][c] = rocprofvis_dm_get_property_as_charptr(table_row, kRPVDMExtTableRowCellValueCharPtrIndexed, c);
                                            }
                                        }
                                        else
                                        {
                                            rows_ok = false;
                                        }
                                    }
                                    else
                                    {
                                        rows_ok = false;
                                    }
                                }
                                if(rows_ok)
                                {
                                    result = callback(data_store);
                                }
                                else
                                {
                                    result = kRocProfVisResultUnknownError;
                                }
                            }
                            else
                            {
                                result = kRocProfVisResultUnknownError;
                            }
                        }
                        rocprofvis_dm_delete_table_at(dm_handle, table_id);
                    }
                    else
                    {
                        result = kRocProfVisResultUnknownError;
                    }
                }
                else
                {
                    result = kRocProfVisResultUnknownError;
                }
            }
            else if(dm_result == kRocProfVisDmResultDbAccessFailed)
            {
                result = kRocProfVisResultNotLoaded;
            }
            else
            {
                result = kRocProfVisResultUnknownError;
            }
            future.RemoveDependentFuture(object2wait);
        }
        else
        {
            result = kRocProfVisResultUnknownError;
        }
        rocprofvis_db_future_free(object2wait);
    }
    else
    {
        result = kRocProfVisResultMemoryAllocError;
    }
    return result;
}

}
}