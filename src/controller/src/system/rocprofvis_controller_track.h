// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_segment.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <cfloat>
#include <condition_variable>
#include <map>
#include <string>
#include <memory>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Array;
class Thread;
class Queue;
class Stream;
class SystemTrace;
class Counter;
class Future;

class Track : public Handle
{
public:
    Track(rocprofvis_controller_track_type_t type, uint64_t id, rocprofvis_dm_track_t dm_handle, SystemTrace* ctx);

    virtual ~Track();

    rocprofvis_result_t FetchSegments(double start, double end, void* user_ptr, Future* future, FetchSegmentsFunc func);
    rocprofvis_result_t Fetch(double start, double end, Array& array, uint64_t& index, Future* future);

    rocprofvis_controller_object_type_t GetType(void) final;
    rocprofvis_controller_track_type_t GetTrackType() const;
    rocprofvis_dm_track_t GetDmHandle(void) const;

    uint64_t GetId() const;
    uint64_t GetNumberOfEntries() const;
    uint64_t GetExtDataNumberOfEntries() const;
    uint64_t GetNodeId() const;
    uint64_t GetAgentIdOrPid() const;
    uint64_t GetQueueIdOrTid() const;
    uint64_t GetNumberOfOperationTypes() const;
    rocprofvis_dm_event_operation_t GetOperationType(uint64_t index) const;

    double GetStartTimestamp() const;
    double GetEndTimestamp() const;
    double GetMinValue() const;
    double GetMaxValue() const;

    const std::string& GetCategory() const;
    const std::string& GetMainName() const;
    const std::string& GetSubName() const;

    void SetTrackType(rocprofvis_controller_track_type_t type);
    void SetDmHandle(rocprofvis_dm_track_t dm_handle);

    void SetId(uint64_t id);
    void SetNumberOfEntries(uint64_t number_of_entries);
    void SetNodeId(uint64_t node_id);
    void SetAgentIdOrPid(uint64_t agent_id_or_pid);
    void SetQueueIdOrTid(uint64_t queue_id_or_tid);
    void SetNumberOfOperationTypes(uint64_t number_of_operation_types);
    void SetOperationType(uint64_t index,
                          rocprofvis_dm_event_operation_t operation_type);

    void SetStartTimestamp(double start_timestamp);
    void SetEndTimestamp(double end_timestamp);
    void SetMinValue(double min_value);
    void SetMaxValue(double max_value);

    void SetCategory(const std::string& category);
    void SetMainName(const std::string& main_name);
    void SetSubName(const std::string& sub_name);

    std::string GetExtDataCategory(uint64_t index) const;
    std::string GetExtDataName(uint64_t index) const;
    std::string GetExtDataValue(uint64_t index) const;

    Thread*  GetThread()  const;
    Queue*   GetQueue()   const;
    Stream*  GetStream()  const;
    Counter* GetCounter() const;
    
    void     SetThread(Thread* thread);
    void     SetQueue(Queue* queue);
    void     SetStream(Stream* stream);
    void     SetCounter(Counter* counter);

    rocprofvis_result_t GetInclusiveMemoryUsage(uint64_t* value);

    Handle* GetContext(void) override;
    SegmentTimeline* GetSegments();
    rocprofvis_result_t GetBucketValues(size_t buckets_num, Array& array);

    rocprofvis_result_t FillBounds();
    rocprofvis_result_t FillMetadata();
    rocprofvis_result_t FillTopologyIds();

private:
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) override final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) override final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) override final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) override final;  // use GetThread/GetQueue/GetStream/GetCounter instead

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) override final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) override final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value) override final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) override final;  // use SetThread/SetQueue/SetStream/SetCounter instead

    struct fetch_range_t
    {
        double start;
        double end;
    };

    struct trace_read_request_t
    {
        rocprofvis_db_future_t future;
        double                 start;
        double                 end;
    };
    struct track_bounds_t
    {
        uint64_t num_entries     = 0;
        double   start_timestamp = DBL_MIN;
        double   end_timestamp   = DBL_MAX;
        double   min_value       = 0.0;
        double   max_value       = 0.0;
    };

    struct track_metadata_t
    {
        std::string category;
        std::string main_name;
        std::string sub_name;
        std::vector<rocprofvis_dm_event_operation_t> operation_types;
    };

    struct track_topology_ids_t
    {
        uint64_t node_id         = 0;
        uint64_t agent_id_or_pid = 0;
        uint64_t queue_id_or_tid = 0;
    };

    struct track_topology_links_t
    {
        Thread*  thread  = nullptr;
        Queue*   queue   = nullptr;
        Stream*  stream  = nullptr;
        Counter* counter = nullptr;
    };

    uint64_t                           m_id;
    rocprofvis_controller_track_type_t m_type;
    rocprofvis_dm_track_t              m_dm_handle;
    SystemTrace*                       m_ctx;
    track_bounds_t                     m_bounds;
    track_metadata_t                   m_metadata;
    track_topology_ids_t               m_topology_ids;
    track_topology_links_t             m_topology_links;
    SegmentTimeline                    m_segments;
    std::condition_variable_any        m_state_changed;

private:

    fetch_range_t CalculateFetchRange(double start, double end) const;

    std::vector<trace_read_request_t> ScheduleTraceReadRequests(
        rocprofvis_dm_database_t database, const fetch_range_t& fetch_range,
        int num_requests, Future* future);

    rocprofvis_result_t WaitForAndProcessTraceReadRequests(
        rocprofvis_dm_trace_t trace, uint64_t dm_track_type,
        const std::vector<trace_read_request_t>& requests, Future* future);

    rocprofvis_result_t ProcessTraceReadRequest(
        rocprofvis_dm_trace_t trace, uint64_t dm_track_type,
        const trace_read_request_t& request, Future* future);

    rocprofvis_result_t ProcessEventRecords(
        rocprofvis_dm_slice_t data, uint64_t num_records, Future* future);

    rocprofvis_result_t ProcessPmcSampleRecords(
        rocprofvis_dm_slice_t data, uint64_t num_records, Future* future);

    rocprofvis_result_t FetchFromDataModel(double start, double end, Future* future);

    uint32_t GetNumberOfEventsForTimeRange(double start, double end);
};

}
}
