// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_controller.h"
#include "rocprofvis_controller_analysis.h"
#include "system/rocprofvis_controller_table_system.h"
#include <array>
#include <functional>
#include <mutex>
#include <string_view>
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

class Trace;
class SystemTrace;
class Future;
class Array;
class SystemTable;
class EventSearchTable;
class Track;
class Node;
class Processor;
class Process;
class Thread;
class Counter;
class SummaryMetrics;

class Analysis
{
public:
    static Analysis& GetInstance();

    rocprofvis_result_t AsyncFetchTable(SystemTrace* trace, Table& table, Arguments& args, Future& future, Array& array) const;

    rocprofvis_result_t AsyncTableExportCSV(SystemTrace* trace, Table& table, Arguments& args, Future& future, const char* path) const;

    rocprofvis_result_t AsyncFetchQueueUtilization(SystemTrace* trace, Track* track, double start, double end, double* output, Future* future) const;

    rocprofvis_result_t AsyncFetchCounterStatistics(SystemTrace* trace, Track* track, double start, double end, rocprofvis_analysis_counter_statistics_t* output, Future* future) const;

    rocprofvis_result_t AsyncFetchSummary(SystemTrace* trace, Arguments& args, Future* future, SummaryMetrics& output) const;

    rocprofvis_result_t GetSummaryInstanceTable(SystemTrace* trace, rocprofvis_analysis_summary_category_t category, rocprofvis_handle_t** table);

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
    struct TraceData
    {
        EventsTable* instrumented_thread_events_table;
        EventsTable* dispatch_events_table;
        EventsTable* memory_allocation_events_table;
        EventsTable* memory_copy_events_table;
        EventsTable* launch_sample_events_table;
        std::array<EventSearchTable*, __kRPVAnalysisSummaryCategoryLast> summary_instance_tables;
    };

    Analysis();
    ~Analysis();

    rocprofvis_result_t GetOrAllocateEventsTable(EventsTable*& slot, rocprofvis_dm_event_operation_t op, rocprofvis_handle_t** table);

    rocprofvis_result_t FetchCounterStatistics(rocprofvis_dm_trace_t dm_handle, Track* track, double start, double end, rocprofvis_dm_hashed_timestamp_tag_t tag, rocprofvis_analysis_counter_statistics_t* output, uint64_t* active_duration, Future* future) const;

    // file_id, when set, summarizes one source of a compare trace: every query is
    // scoped to that file and counters from the other sources are left out.
    rocprofvis_result_t FetchSummaryMetrics(SystemTrace* trace, double start, double end, const c_optional_uint64_t& file_id, SummaryMetrics& output, Future* future) const;

    rocprofvis_result_t FetchTopEvents(rocprofvis_dm_trace_t dm_handle, const std::vector<rocprofvis_dm_event_operation_t>& ops, const rocprofvis_dm_query_criteria_t& criteria, double start, double end, const std::array<rocprofvis_property_t, __kRPVAnalysisSummaryTopEventsMetricLast>& op_properties, const std::array<rocprofvis_property_t, __kRPVAnalysisSummaryTopEventMetricLast>& event_properties, rocprofvis_property_t event_name_property, SummaryMetrics& output, Future* future) const;

    rocprofvis_result_t FetchCounterStatistics(rocprofvis_dm_trace_t dm_handle, Processor* processor, Counter* counter, double start, double end, SummaryMetrics& output, Future* future) const;
    
    rocprofvis_dm_result_t ExecuteTableQuery(rocprofvis_dm_trace_t dm_handle, const char* query, Future* future, std::function<rocprofvis_dm_result_t(rocprofvis_dm_table_t table, uint64_t rows, uint64_t cols)> callback) const;

    std::unordered_map<Trace*, TraceData> m_data;
    std::mutex m_data_mutex;
};

}
}
