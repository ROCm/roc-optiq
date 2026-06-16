// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "system/rocprofvis_controller_table_system.h"
#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#ifdef __cplusplus
extern "C"
{
#endif

/*
* Calculates the queue utilization for the specified track within the given time range.
* @param controller The system trace controller instance.
* @param track The handle track to analyze.
* @param start_time The start time in ns of the analysis range.
* @param end_time The end time in ns of the analysis range.
* @param result The future object to store the result.
* @param output The output value to write the queue utilization.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_fetch_queue_utilization(rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track, double start_time, double end_time, rocprofvis_controller_future_t* result, double* output);

/*
* Returns the instrumented-thread events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_instrumented_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the dispatch events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_dispatch_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the memory-allocation events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_memory_allocation_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the memory-copy events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_memory_copy_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Returns the sampled events table.
* @param controller The system trace controller instance.
* @param table Out-param that receives the table handle.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_get_sampled_events_table(rocprofvis_controller_t* controller, rocprofvis_handle_t** table);

/*
* Fetches rows for an analysis table.
* @param controller The system trace controller instance.
* @param table A table handle from one of the rocprofvis_analysis_get_*_events_table getters.
* @param args Query arguments.
* @param result The future object used to wait for completion.
* @param output Array that receives the fetched rows.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_fetch_table(rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, rocprofvis_controller_array_t* output);

/*
* Exports an analysis table to a CSV file.
* @param controller The system trace controller instance.
* @param table A table handle from one of the rocprofvis_analysis_get_*_events_table getters.
* @param args Query arguments.
* @param result The future object used to wait for completion.
* @param path Filesystem path where the CSV file will be written.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_analysis_table_export_csv(rocprofvis_controller_t* controller, rocprofvis_controller_table_t* table, rocprofvis_controller_arguments_t* args, rocprofvis_controller_future_t* result, char const* path);

/*
* Releases the analysis data associated with the given controller. Call when
* closing the controller.
* @param controller The system trace controller instance.
*/
void rocprofvis_analysis_free_trace_data(rocprofvis_controller_t* controller);

#ifdef __cplusplus
}
#endif

namespace RocProfVis
{
namespace Controller
{

class Trace;
class SystemTrace;
class Future;
class Array;
class SystemTable;
class Track;

class Analysis
{
public:
    static Analysis& GetInstance();

    rocprofvis_result_t AsyncFetchTable(SystemTrace* trace, Table& table, Arguments& args, Future& future, Array& array) const;

    rocprofvis_result_t AsyncTableExportCSV(SystemTrace* trace, Table& table, Arguments& args, Future& future, const char* path) const;

    rocprofvis_result_t AsyncFetchQueueUtilization(SystemTrace* trace, Track* track, double start, double end, double* output, Future* future) const;

    rocprofvis_result_t GetInstrumentedThreadEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);
    rocprofvis_result_t GetDispatchEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);
    rocprofvis_result_t GetMemoryAllocationEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);
    rocprofvis_result_t GetMemoryCopyEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);
    rocprofvis_result_t GetLaunchSampleEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);

    void FreeTraceData(Trace* trace);

private:
    class EventsTable : public SystemTable
    {
    public:
        EventsTable(uint64_t id, rocprofvis_dm_event_operation_t op);

    protected:
        rocprofvis_result_t UnpackArguments(Arguments& args, QueryArguments& out) const final;
        rocprofvis_result_t UnpackUseCase(Arguments& args, rocprofvis_dm_table_use_case_enum_t& out) const final;

    private:
        rocprofvis_dm_event_operation_t m_op;
    };
    struct QueryDataStore
    {
        std::unordered_map<std::string_view, uint64_t> columns;
        std::vector<std::vector<const char*>> rows;
    };
    typedef std::function<rocprofvis_result_t(const QueryDataStore&)> QueryCallback;
    struct TraceData
    {
        EventsTable* instrumented_thread_events_table;
        EventsTable* dispatch_events_table;
        EventsTable* memory_allocation_events_table;
        EventsTable* memory_copy_events_table;
        EventsTable* launch_sample_events_table;
    };

    Analysis();
    ~Analysis();

    rocprofvis_result_t GetOrAllocateEventsTable(EventsTable*& slot, rocprofvis_dm_event_operation_t op, rocprofvis_handle_t** table);

    std::unordered_map<Trace*, TraceData> m_data;
};

}
}
