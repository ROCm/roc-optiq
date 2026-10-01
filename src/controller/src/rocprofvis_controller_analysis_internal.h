// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <functional>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "rocprofvis_controller_analysis.h"
#include "system/rocprofvis_controller_table_system.h"

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

    rocprofvis_result_t AsyncFetchCounterStatistics(SystemTrace* trace, Track* track, double start, double end, rocprofvis_analysis_counter_statistics_t* output, Future* future) const;

    rocprofvis_result_t GetInstrumentedThreadEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);
    rocprofvis_result_t GetDispatchEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);
    rocprofvis_result_t GetMemoryAllocationEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);
    rocprofvis_result_t GetMemoryCopyEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);
    rocprofvis_result_t GetLaunchSampleEventsTable(SystemTrace* trace, rocprofvis_handle_t** table);

    // Unlike the Get* calls, the caller owns what comes back and frees it with
    // rocprofvis_controller_table_free. Nothing is cached per trace, so two
    // readers of the same operation do not share a row cache.
    rocprofvis_result_t AllocEventsTable(rocprofvis_dm_event_operation_t op, rocprofvis_handle_t** table);

    void FreeTraceData(Trace* trace);

private:
    class EventsTable : public SystemTable
    {
    public:
        EventsTable(uint64_t id, rocprofvis_dm_event_operation_t op);

    protected:
        rocprofvis_result_t UnpackArguments(Arguments& args, TableArguments*& out) const final;
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
