// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_topology.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_controller_trace_system.h"
#include "rocprofvis_controller_future.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <set>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_thread_t, Thread, kRPVControllerObjectTypeThread> ThreadRef;
typedef Reference<rocprofvis_controller_queue_t, Queue, kRPVControllerObjectTypeQueue> QueueRef;
typedef Reference<rocprofvis_controller_stream_t, Stream, kRPVControllerObjectTypeStream> StreamRef;
typedef Reference<rocprofvis_controller_counter_t, Counter, kRPVControllerObjectTypeCounter> CounterRef;

Track::Track(rocprofvis_controller_track_type_t type, uint64_t id, rocprofvis_dm_track_t dm_handle, SystemTrace * ctx)
: Handle(__kRPVControllerTrackPropertiesFirst, __kRPVControllerTrackPropertiesLast)
, m_id(id)
, m_type(type)
, m_dm_handle(dm_handle)
, m_ctx(ctx)
{ 
}

Track::~Track()
{
}

rocprofvis_dm_track_t Track::GetDmHandle(void) const
{
    return m_dm_handle;
}

uint64_t
Track::GetId() const
{
    return m_id;
}

rocprofvis_controller_track_type_t
Track::GetTrackType() const
{
    return m_type;
}

uint64_t
Track::GetNumberOfEntries() const
{
    return m_bounds.num_entries;
}

uint64_t
Track::GetNodeId() const
{
    return m_topology_ids.node_id;
}

uint64_t
Track::GetAgentIdOrPid() const
{
    return m_topology_ids.agent_id_or_pid;
}

uint64_t
Track::GetQueueIdOrTid() const
{
    return m_topology_ids.queue_id_or_tid;
}

uint64_t
Track::GetNumberOfOperationTypes() const
{
    return m_metadata.operation_types.size();
}

rocprofvis_dm_event_operation_t
Track::GetOperationType(uint64_t index) const
{
    ROCPROFVIS_ASSERT(index < m_metadata.operation_types.size());
    return m_metadata.operation_types[index];
}

double
Track::GetStartTimestamp() const
{
    return m_bounds.start_timestamp;
}

double
Track::GetEndTimestamp() const
{
    return m_bounds.end_timestamp;
}

double
Track::GetMinValue() const
{
    return m_bounds.min_value;
}

double
Track::GetMaxValue() const
{
    return m_bounds.max_value;
}

const std::string&
Track::GetCategory() const
{
    return m_metadata.category;
}

const std::string&
Track::GetMainName() const
{
    return m_metadata.main_name;
}

const std::string&
Track::GetSubName() const
{
    return m_metadata.sub_name;
}

void
Track::SetTrackType(rocprofvis_controller_track_type_t type)
{
    m_type = type;
}

void
Track::SetDmHandle(rocprofvis_dm_track_t dm_handle)
{
    m_dm_handle = dm_handle;
}

void
Track::SetId(uint64_t id)
{
    m_id = id;
}

void
Track::SetNumberOfEntries(uint64_t number_of_entries)
{
    m_bounds.num_entries = number_of_entries;
}

void
Track::SetNodeId(uint64_t node_id)
{
    m_topology_ids.node_id = node_id;
}

void
Track::SetAgentIdOrPid(uint64_t agent_id_or_pid)
{
    m_topology_ids.agent_id_or_pid = agent_id_or_pid;
}

void
Track::SetQueueIdOrTid(uint64_t queue_id_or_tid)
{
    m_topology_ids.queue_id_or_tid = queue_id_or_tid;
}

void
Track::SetNumberOfOperationTypes(uint64_t number_of_operation_types)
{
    m_metadata.operation_types.resize(number_of_operation_types);
}

void
Track::SetOperationType(uint64_t index,
                        rocprofvis_dm_event_operation_t operation_type)
{
    ROCPROFVIS_ASSERT(index < m_metadata.operation_types.size());
    m_metadata.operation_types[index] = operation_type;
}

void
Track::SetStartTimestamp(double start_timestamp)
{
    m_bounds.start_timestamp = start_timestamp;
}

void
Track::SetEndTimestamp(double end_timestamp)
{
    m_bounds.end_timestamp = end_timestamp;
}

void
Track::SetMinValue(double min_value)
{
    m_bounds.min_value = min_value;
}

void
Track::SetMaxValue(double max_value)
{
    m_bounds.max_value = max_value;
}

void
Track::SetCategory(const std::string& category)
{
    m_metadata.category = category;
}

void
Track::SetMainName(const std::string& main_name)
{
    m_metadata.main_name = main_name;
}

void
Track::SetSubName(const std::string& sub_name)
{
    m_metadata.sub_name = sub_name;
}

uint64_t
Track::GetExtDataNumberOfEntries() const
{
    return rocprofvis_dm_get_property_as_uint64(
        GetDmHandle(), kRPVDMNumberOfTrackExtDataRecordsUInt64, 0);
}

std::string
Track::GetExtDataCategory(uint64_t index) const
{
    return rocprofvis_dm_get_property_as_charptr(
        GetDmHandle(), kRPVDMTrackExtDataCategoryCharPtrIndexed, index);
}

std::string
Track::GetExtDataName(uint64_t index) const
{
    return rocprofvis_dm_get_property_as_charptr(
        GetDmHandle(), kRPVDMTrackExtDataNameCharPtrIndexed, index);
}

std::string
Track::GetExtDataValue(uint64_t index) const
{
    return rocprofvis_dm_get_property_as_charptr(
        GetDmHandle(), kRPVDMTrackExtDataValueCharPtrIndexed, index);
}

Handle* Track::GetContext(void)
{
    return m_ctx;
}

SegmentTimeline*
Track::GetSegments()
{
    return &m_segments;
}

Thread*
Track::GetThread() const
{
    return m_topology_links.thread;
}

Queue*
Track::GetQueue() const
{
    return m_topology_links.queue;
}

Stream*
Track::GetStream() const
{
    return m_topology_links.stream;
}

Counter*
Track::GetCounter() const
{
    return m_topology_links.counter;
}

void
Track::SetThread(Thread* thread)
{
    m_topology_links.thread = thread;
}

void
Track::SetQueue(Queue* queue)
{
    m_topology_links.queue = queue;
}

void
Track::SetStream(Stream* stream)
{
    m_topology_links.stream = stream;
}

void
Track::SetCounter(Counter* counter)
{
    m_topology_links.counter = counter;
}

rocprofvis_result_t Track::GetBucketValues(size_t buckets_num, Array& array) {

    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    for (int i = 0; i < buckets_num; i++)
    {
        uint64_t value = rocprofvis_dm_get_property_as_uint64(
            GetDmHandle(), kRPVDMTrackHistogramBucketValueDoubleIndexed, i);
        result = array.SetDouble(kRPVControllerArrayEntryIndexed, i, static_cast<double>(value));
    }
    return result;
}

rocprofvis_result_t Track::FetchSegments(double start, double end, void* user_ptr, Future* future, FetchSegmentsFunc func)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    if(GetStartTimestamp() <= end && GetEndTimestamp() >= start)
    {
        if(GetSegments()->GetSegmentDuration() == 0)
        {
            uint32_t num_segments = (uint32_t)ceil(
                (GetEndTimestamp() - GetStartTimestamp()) / kSegmentDuration);
            GetSegments()->SetContext(this);
            GetSegments()->Init(
                GetStartTimestamp(), kSegmentDuration, num_segments,
                GetNumberOfEntries());
        }

        start = std::max(start, GetStartTimestamp());
        end   = std::min(end, GetEndTimestamp());

        std::vector<std::pair<uint32_t, uint32_t>> fetch_ranges;

        uint32_t start_index = (uint32_t) floor(
            (start - GetStartTimestamp()) / kSegmentDuration);
        uint32_t end_index = (uint32_t) ceil(
            (end - GetStartTimestamp()) / kSegmentDuration);

        {
            std::unique_lock lock(*GetSegments()->GetMutex());
            m_state_changed.wait(lock, [&] {
                for(uint32_t i = start_index; i < end_index; i++)
                {
                    if(GetSegments()->IsProcessed(i))
                    {
                        return false;
                    }
                }
                return true;
            });
            for(uint32_t i = start_index; i < end_index; i++)
            {
                if(!GetSegments()->IsValid(i))
                {
                    GetSegments()->SetProcessed(i, true);
                    if(fetch_ranges.size())
                    {
                        auto& last_range = fetch_ranges.back();
                        if(last_range.second == i - 1)
                        {
                            last_range.second = i;
                        }
                        else
                        {
                            fetch_ranges.push_back(std::make_pair(i, i));
                        }
                    }
                    else
                    {
                        fetch_ranges.push_back(std::make_pair(i, i));
                    }
                }
            }
        }

        if(fetch_ranges.size())
        {
            for(auto& range : fetch_ranges)
            {
                if(future->IsCancelled())
                {
                    result = kRocProfVisResultCancelled;
                }
                else
                {
                    double fetch_start =
                        GetStartTimestamp() + (range.first * kSegmentDuration);
                    double fetch_end =
                        GetStartTimestamp() + ((range.second + 1) * kSegmentDuration);

                    result = FetchFromDataModel(fetch_start, fetch_end, future);
                    spdlog::debug("FetchFromDataModel for track {} ({}-{}) = {}, cancelled={}",GetId(),fetch_start, fetch_end,(uint32_t)result, future->IsCancelled());

                }
                {
                    std::unique_lock lock(*GetSegments()->GetMutex());
                    for(uint32_t i = range.first; i <= range.second; i++)
                    {
                        GetSegments()->SetProcessed(i, false);
                        GetSegments()->SetValid(i, result == kRocProfVisResultSuccess );
                    }
                }
                m_state_changed.notify_all();
            }
        }
        else
        {
            result = kRocProfVisResultOutOfRange;
        }

        {
            std::unique_lock lock(*GetSegments()->GetMutex());
            m_state_changed.wait(lock, [&] {
                for(uint32_t i = start_index; i < end_index; i++)
                {
                    if(GetSegments()->IsProcessed(i))
                    {
                        return false;
                    }
                }
                return true;
            });
        }

        // Walking the segments would report kRocProfVisResultOutOfRange for a
        // cancelled fetch, because the segments it needs were never populated.
        // The cancellation has to survive so the view knows the data is missing
        // and must be re-requested when the track comes back into view.
        if(result != kRocProfVisResultCancelled)
        {
            result = GetSegments()->FetchSegments(start, end, user_ptr, future, func);
        }
    }

    return result;
}

struct FetchEventsArgs
{
    Array*             m_array;
    uint64_t*          m_index;
    std::unordered_set<uint64_t> m_event_ids;
    SegmentLRUParams lru_params;
};

rocprofvis_result_t Track::Fetch(double start, double end, Array& array, uint64_t& index, Future* future)
{
    FetchEventsArgs args;
    args.m_array = &array;
    args.m_index = &index;
    args.lru_params.m_ctx   = (SystemTrace*)GetContext();
    args.lru_params.m_lod      = 0;
    array.SetContext(GetContext());

    rocprofvis_result_t result = FetchSegments(start, end, &args, future, [](double start, double end, Segment& segment, void* user_ptr, SegmentTimeline* owner) -> rocprofvis_result_t
    {
        FetchEventsArgs* args = (FetchEventsArgs*) user_ptr;
        args->lru_params.m_owner   = owner;
        rocprofvis_result_t result = segment.Fetch(start, end, args->m_array->GetVector(), *args->m_index, &args->m_event_ids, &args->lru_params);
        return result;
    });

    return result;
}

rocprofvis_result_t Track::FetchFromDataModel(double start, double end, Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;

    rocprofvis_dm_trace_t trace = rocprofvis_dm_get_property_as_handle(
        GetDmHandle(), kRPVDMTrackTraceHandle, 0);
    rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(
        GetDmHandle(), kRPVDMTrackDatabaseHandle, 0);
    uint64_t dm_track_type = rocprofvis_dm_get_property_as_uint64(
        GetDmHandle(), kRPVDMTrackCategoryEnumUInt64, 0);

    const fetch_range_t fetch_range = CalculateFetchRange(start, end);
    constexpr uint32_t thread_max_events = 1000000;
    constexpr uint32_t max_threads_per_range = 2;
    uint32_t num_events_per_range =
        GetNumberOfEventsForTimeRange(fetch_range.start, fetch_range.end);
    if (num_events_per_range == 0 && kRocProfVisDmPmcTrack!=dm_track_type)
        return kRocProfVisResultSuccess;
    int num_threads = (num_events_per_range + thread_max_events) / thread_max_events;
    if (num_threads > max_threads_per_range)
        num_threads = max_threads_per_range;

    std::vector<trace_read_request_t> requests =
        ScheduleTraceReadRequests(db, fetch_range, num_threads, future);

    result = WaitForAndProcessTraceReadRequests(trace, dm_track_type, requests, future);

    return future->IsCancelled() ? kRocProfVisResultCancelled : result;
}

Track::fetch_range_t
Track::CalculateFetchRange(double start, double end) const
{
    fetch_range_t range;
    range.start = GetStartTimestamp() +
                  (std::floor((start - GetStartTimestamp()) / kSegmentDuration) *
                   kSegmentDuration);
    range.end = GetStartTimestamp() +
                (std::ceil((end - GetStartTimestamp()) / kSegmentDuration) *
                 kSegmentDuration);
    return range;
}

uint32_t
Track::GetNumberOfEventsForTimeRange(double start, double end)
{
    rocprofvis_dm_trace_t trace =
        rocprofvis_dm_get_property_as_handle(
            GetDmHandle(), kRPVDMTrackTraceHandle, 0);
    uint64_t start_time =
        rocprofvis_dm_get_property_as_uint64(trace, kRPVDMStartTimeUInt64, 0);
    size_t bucket_size =
        rocprofvis_dm_get_property_as_uint64(trace, kRPVDMHistogramBucketSize, 0);
    uint64_t start_bucket = static_cast<uint64_t>((start - start_time) / bucket_size);
    uint64_t end_bucket =
        static_cast<uint64_t>(((end - start_time) + bucket_size) / bucket_size);
    uint32_t num_events = 0;
    for(uint64_t i = start_bucket; i <= end_bucket; i++)
    {
        num_events += static_cast<uint32_t>(rocprofvis_dm_get_property_as_uint64(
            GetDmHandle(), kRPVDMTrackHistogramBucketEventDensityUInt64Indexed, i));
    }
    return num_events;
}

std::vector<Track::trace_read_request_t>
Track::ScheduleTraceReadRequests(rocprofvis_dm_database_t database,
                                 const fetch_range_t& fetch_range,
                                 int num_requests,
                                 Future* future)
{
    std::vector<trace_read_request_t> requests;
    const double time_per_request =
        (fetch_range.end - fetch_range.start) / num_requests;

    for(int i = 0; i < num_requests; i++)
    {
        if(future->IsCancelled())
        {
            break;
        }

        const double request_start = fetch_range.start + (i * time_per_request);
        const double request_end   = fetch_range.start + ((i + 1) * time_per_request);
        rocprofvis_db_future_t db_future = rocprofvis_db_future_alloc(nullptr);
        if(nullptr != db_future)
        {
            if(kRocProfVisDmResultSuccess ==
               rocprofvis_db_read_trace_slice_async(
                   database, (uint64_t)request_start, (uint64_t)request_end,
                   kRocProfVisDmHashedTimestampTagTrackSlice, 1,
                   (rocprofvis_db_track_selection_t)&m_id, db_future))
            {
                requests.push_back({db_future, request_start, request_end});
                future->AddDependentFuture(db_future);
            }
            else
            {
                rocprofvis_db_future_free(db_future);
            }
        }
    }

    return requests;
}

rocprofvis_result_t
Track::WaitForAndProcessTraceReadRequests(
    rocprofvis_dm_trace_t trace,
    uint64_t dm_track_type,
    const std::vector<trace_read_request_t>& requests,
    Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;

    for(const trace_read_request_t& request : requests)
    {
        result = ProcessTraceReadRequest(trace, dm_track_type, request, future);
        future->RemoveDependentFuture(request.future);
        rocprofvis_db_future_free(request.future);
    }

    return result;
}

rocprofvis_result_t
Track::ProcessTraceReadRequest(rocprofvis_dm_trace_t trace,
                               uint64_t dm_track_type,
                               const trace_read_request_t& request,
                               Future* future)
{
    const rocprofvis_dm_result_t dm_result =
        rocprofvis_db_future_wait(request.future, UINT64_MAX);
    if(kRocProfVisDmResultSuccess != dm_result)
    {
        return dm_result == kRocProfVisDmResultDbAbort ?
                   kRocProfVisResultCancelled :
                   kRocProfVisResultUnknownError;
    }

    rocprofvis_dm_slice_t data = rocprofvis_dm_get_property_as_handle(
        GetDmHandle(), kRPVDMSliceHandleTimed,
        rocprofvis_dm_hash_combine_timestamp(
            request.start, request.end, kRocProfVisDmHashedTimestampTagTrackSlice));
    if(nullptr == data)
    {
        return kRocProfVisResultNotLoaded;
    }

    rocprofvis_result_t result = kRocProfVisResultSuccess;
    const uint64_t num_records = rocprofvis_dm_get_property_as_uint64(
        data, kRPVDMNumberOfRecordsUInt64, 0);
    if(num_records > 0)
    {
        switch(dm_track_type)
        {
            case kRocProfVisDmRegionTrack:
            case kRocProfVisDmRegionMainTrack:
            case kRocProfVisDmRegionSampleTrack:
            case kRocProfVisDmKernelDispatchTrack:
            case kRocProfVisDmMemoryAllocationTrack:
            case kRocProfVisDmMemoryCopyTrack:
            case kRocProfVisDmStreamTrack:
            {
                result = ProcessEventRecords(data, num_records, future);
                break;
            }
            case kRocProfVisDmPmcTrack:
            {
                result = ProcessPmcSampleRecords(data, num_records, future);
                break;
            }
            default:
            {
                break;
            }
        }
    }

    if(kRocProfVisDmResultSuccess !=
       rocprofvis_dm_delete_time_slice_handle(
           trace, static_cast<rocprofvis_dm_track_id_t>(GetId()), data))
    {
        result = kRocProfVisResultUnknownError;
    }

    return result;
}

rocprofvis_result_t
Track::ProcessEventRecords(rocprofvis_dm_slice_t data,
                           uint64_t num_records,
                           Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultSuccess;
    uint64_t index = 0;

    for(int record_index = 0; record_index < num_records; record_index++)
    {
        if(future->IsCancelled())
        {
            result = kRocProfVisResultCancelled;
            break;
        }

        double timestamp = (double)rocprofvis_dm_get_property_as_uint64(
            data, kRPVDMTimestampUInt64Indexed, record_index);
        double duration = (double)rocprofvis_dm_get_property_as_int64(
            data, kRPVDMEventDurationInt64Indexed, record_index);
        if(duration < 0)
        {
            continue;
        }

        uint64_t event_id = rocprofvis_dm_get_property_as_uint64(
            data, kRPVDMEventIdUInt64Indexed, record_index);
        Event* new_event = static_cast<SystemTrace*>(GetContext())
                               ->GetMemoryManager()->NewEvent(
            event_id, timestamp, timestamp + duration, GetSegments());
        if(new_event)
        {
            result = new_event->SetUInt64(
                kRPVControllerEventLevel, 0,
                rocprofvis_dm_get_property_as_uint64(
                    data, kRPVDMEventLevelUInt64Indexed, record_index));
            if(result == kRocProfVisResultSuccess)
            {
                result = new_event->SetString(
                    kRPVControllerEventCategory, 0,
                    rocprofvis_dm_get_property_as_charptr(
                        data, kRPVDMEventTypeStringCharPtrIndexed, record_index));
            }
            if(result == kRocProfVisResultSuccess)
            {
                result = new_event->SetString(
                    kRPVControllerEventName, 0,
                    rocprofvis_dm_get_property_as_charptr(
                        data, kRPVDMEventSymbolStringCharPtrIndexed, record_index));
            }
            if(result == kRocProfVisResultSuccess)
            {
                result = SetObject(
                    kRPVControllerTrackEntry, index++,
                    (rocprofvis_handle_t*)new_event);
            }
            if(result == kRocProfVisResultOutOfRange)
            {
                spdlog::warn(
                    "Track::FetchFromDataModel: Skipping Event "
                    "id {} on track id {}, event is out of range",
                    event_id, GetId());
            }
            else
            {
                ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                if(result != kRocProfVisResultSuccess)
                {
                    break;
                }
            }
        }
        else
        {
            result = kRocProfVisResultMemoryAllocError;
            break;
        }
    }

    return result;
}

rocprofvis_result_t
Track::ProcessPmcSampleRecords(rocprofvis_dm_slice_t data,
                               uint64_t num_records,
                               Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultSuccess;
    uint64_t index = 0;
    uint64_t sample_id = 0;
    double timestamp = (double)rocprofvis_dm_get_property_as_uint64(
        data, kRPVDMTimestampUInt64Indexed, 0);
    double value = rocprofvis_dm_get_property_as_double(
        data, kRPVDMPmcValueDoubleIndexed, 0);
    double last_timestamp = timestamp;
    double last_value = value;

    for(int record_index = 1; record_index < num_records; record_index++)
    {
        if(future->IsCancelled())
        {
            result = kRocProfVisResultCancelled;
            break;
        }

        timestamp = (record_index == num_records) ? last_timestamp :
            (double)rocprofvis_dm_get_property_as_uint64(
                data, kRPVDMTimestampUInt64Indexed, record_index);
        value = (record_index == num_records) ? last_value :
            rocprofvis_dm_get_property_as_double(
                data, kRPVDMPmcValueDoubleIndexed, record_index);
        if(timestamp <= last_timestamp)
        {
            continue;
        }

        Sample* new_sample = static_cast<SystemTrace*>(GetContext())
                                 ->GetMemoryManager()->NewSample(
            kRPVControllerPrimitiveTypeDouble, sample_id++, last_timestamp, GetSegments());
        if(new_sample)
        {
            new_sample->SetDouble(kRPVControllerSampleValue, 0, last_value);
            new_sample->SetDouble(kRPVControllerSampleEndTimestamp, 0, timestamp);
            SetObject(
                kRPVControllerTrackEntry, index++, (rocprofvis_handle_t*)new_sample);
        }
        else
        {
            result = kRocProfVisResultMemoryAllocError;
            break;
        }
        last_value = value;
        last_timestamp = timestamp;
    }

    return result;
}

rocprofvis_controller_object_type_t Track::GetType(void)
{
    return kRPVControllerObjectTypeTrack;
}

rocprofvis_result_t
Track::GetInclusiveMemoryUsage(uint64_t* value)
{
    if(value == nullptr)
    {
        return kRocProfVisResultInvalidArgument;
    }

    *value = sizeof(Track);
    rocprofvis_result_t result = kRocProfVisResultSuccess;
    for(auto& pair : GetSegments()->GetSegments())
    {
        *value += sizeof(pair);
        uint64_t entry_size = 0;
        result = pair.second->GetMemoryUsage(
            &entry_size, kRPVControllerCommonMemoryUsageInclusive);
        if(result != kRocProfVisResultSuccess)
        {
            break;
        }
        *value += entry_size;
    }

    return result;
}

rocprofvis_result_t Track::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                result = GetInclusiveMemoryUsage(value);
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Track);
                result = kRocProfVisResultSuccess;
                for(auto& pair : GetSegments()->GetSegments())
                {
                    *value += sizeof(pair);
                }
                break;
            }
            case kRPVControllerTrackId:
            {
                *value = GetId();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackType:
            {
                *value = GetTrackType();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackNumberOfEntries:
            {
                *value = GetNumberOfEntries();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackExtDataNumberOfEntries:
            {
                *value = GetExtDataNumberOfEntries();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackNode:
            {
                *value = GetNodeId();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackAgentIdOrPid:
            {
                *value = GetAgentIdOrPid();
                result = kRocProfVisResultSuccess;
                break;
            }     
            case kRPVControllerTrackQueueIdOrTid:
            {
                *value = GetQueueIdOrTid();
                result = kRocProfVisResultSuccess;
                break;
            }  
            case kRPVControllerTrackHistogramBucketDensityIndexed:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    GetDmHandle(), kRPVDMTrackHistogramBucketEventDensityUInt64Indexed, index);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackNumberOfOperationTypes:
            {
                *value = GetNumberOfOperationTypes();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackOperationTypeIndexed:
            {
                if(index < GetNumberOfOperationTypes())
                {
                    *value = GetOperationType(index);
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerTrackInstanceId:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    GetDmHandle(), kRPVDMTrackInstanceIdUInt64, 0);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackFileId:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    GetDmHandle(), kRPVDMTrackFileIdUInt64, 0);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackOrderRanking:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    GetDmHandle(), kRPVDMTrackOrderRankingUInt64, 0);
                result = kRocProfVisResultSuccess;
                break;
            }
        
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Track::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerTrackMinTimestamp:
            {
                *value = GetStartTimestamp();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackMaxTimestamp:
            {
                *value = GetEndTimestamp();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackMinValue:
            {
                *value = GetMinValue();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackMaxValue:
            {
                *value = GetMaxValue();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackHistogramBucketValueIndexed:
            {
                *value = rocprofvis_dm_get_property_as_double(
                    GetDmHandle(), kRPVDMTrackHistogramBucketValueDoubleIndexed, index);
                result = kRocProfVisResultSuccess;
                break;
            }  
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Track::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerTrackEntry:
            {
                result = kRocProfVisResultNotSupported;
                break;
            }
            case kRPVControllerTrackThread:
            {
                *value = (rocprofvis_handle_t*)GetThread();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackQueue:
            {
                *value = (rocprofvis_handle_t*)GetQueue();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackStream:
            {
                *value = (rocprofvis_handle_t*)GetStream();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackCounter:
            {
                *value = (rocprofvis_handle_t*)GetCounter();
                result = kRocProfVisResultSuccess;
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Track::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTrackCategory:
        {
            result = GetStdStringImpl(value, length, GetCategory());
            break;
        }
        case kRPVControllerTrackMainName:
        {
            result = GetStdStringImpl(value, length, GetMainName());
            break;
        }
        case kRPVControllerTrackSubName:
        {
            result = GetStdStringImpl(value, length, GetSubName());
            break;
        }
        case kRPVControllerTrackExtDataCategoryIndexed:
        {
            result = GetStdStringImpl(value, length, GetExtDataCategory(index));
            break;
        }
        case kRPVControllerTrackExtDataNameIndexed:
        {
            result = GetStdStringImpl(value, length, GetExtDataName(index));
            break;
        }
        case kRPVControllerTrackExtDataValueIndexed:
        {
            result = GetStdStringImpl(value, length, GetExtDataValue(index));
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t Track::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTrackId:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerTrackType:
        {
            SetTrackType(static_cast<rocprofvis_controller_track_type_t>(value));
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackNumberOfEntries:
        {
            SetNumberOfEntries(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackNode:
        {
            SetNodeId(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackAgentIdOrPid:
        {
            SetAgentIdOrPid(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackQueueIdOrTid:
        {
            SetQueueIdOrTid(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackNumberOfOperationTypes:
        {
            SetNumberOfOperationTypes(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackOperationTypeIndexed:
        {
            if(index < GetNumberOfOperationTypes())
            {
                SetOperationType(
                    index, static_cast<rocprofvis_dm_event_operation_t>(value));
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t Track::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTrackMinTimestamp:
        {
            SetStartTimestamp(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackMaxTimestamp:
        {
            SetEndTimestamp(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackMinValue:
        {
            SetMinValue(value);
            result            = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackMaxValue:
        {
            SetMaxValue(value);
            result          = kRocProfVisResultSuccess;
            break;
        }
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t Track::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerTrackEntry:
            {
                // Start & end timestamps must be configured
                ROCPROFVIS_ASSERT(GetStartTimestamp() >= 0.0 &&
                                  GetStartTimestamp() < GetEndTimestamp());
                Handle* object = (Handle*)value;
                auto object_type = object->GetType();
                if (((GetTrackType() == kRPVControllerTrackTypeEvents) && (object_type == kRPVControllerObjectTypeEvent))
                    || ((GetTrackType() == kRPVControllerTrackTypeSamples) && (object_type == kRPVControllerObjectTypeSample)))
                {
                    uint64_t              level = 0;
                    uint64_t              event_id = 0;

                    std::pair<double, double> timestamp = { 0,0 };
                    double sample_value = 0.0;
                    if (object_type == kRPVControllerObjectTypeEvent)
                    {  
                        result = object->GetUInt64(kRPVControllerEventLevel, 0, &level);
                        result = object->GetUInt64(kRPVControllerEventId, 0, &event_id);
                        result = object->GetDouble(kRPVControllerEventStartTimestamp, 0, &timestamp.first);
                        if (result == kRocProfVisResultSuccess)
                        {
                            result = object->GetDouble(kRPVControllerEventEndTimestamp, 0, &timestamp.second);
                        }
                    }
                    else
                    {
                        result = object->GetDouble(kRPVControllerSampleTimestamp, 0, &timestamp.first);
                        if (result == kRocProfVisResultSuccess)
                        {
                            result = object->GetDouble(kRPVControllerSampleEndTimestamp, 0, &timestamp.second);
                            if (result == kRocProfVisResultSuccess)
                            {
                                result = object->GetDouble(kRPVControllerSampleValue, 0, &sample_value);
                            }
                        }
                    }

                    if (result == kRocProfVisResultSuccess)
                    {
                        if(timestamp.first >= GetStartTimestamp() &&
                           timestamp.second <= GetEndTimestamp())
                        {
                            std::pair<double, double> relative  = { timestamp.first - GetStartTimestamp(),
                                                    timestamp.second - GetStartTimestamp() };
                            std::pair<double, double> range = {floor(relative.first / kSegmentDuration),
                                                    floor(relative.second / kSegmentDuration)};
                            
                            std::unique_lock lock(*GetSegments()->GetMutex());

                            for (double current_segment = range.first; current_segment <= range.second; current_segment++)
                            {

                                double segment_start =
                                    GetStartTimestamp() +
                                    (current_segment * kSegmentDuration);
                                double segment_end = segment_start + kSegmentDuration;

                                if(GetSegments()->GetSegments().find(segment_start) ==
                                   GetSegments()->GetSegments().end())
                                {

                                    std::unique_ptr<Segment> segment =
                                        std::make_unique<Segment>(GetTrackType(), GetSegments());
                                    segment->SetStartEndTimestamps(segment_start,
                                                                   segment_end);
                                    segment->SetMinTimestamp(timestamp.first);
                                    segment->SetMaxTimestamp(timestamp.second);
                                    result = GetSegments()->Insert(segment_start,
                                                                   std::move(segment));
                                    if(result == kRocProfVisResultDuplicate) {
                                        spdlog::warn("Segment already exists at {}",
                                                segment_start);
                                        result = kRocProfVisResultSuccess;
                                    }
                                }

                                if(result == kRocProfVisResultSuccess)
                                {

                                    std::unique_ptr<Segment>& segment =
                                        GetSegments()->GetSegments()[segment_start];
                                    segment->SetMinTimestamp(
                                        std::min(segment->GetMinTimestamp(), timestamp.first));
                                    segment->SetMaxTimestamp(std::max(
                                        segment->GetMaxTimestamp(), timestamp.second));
                                    if (range.second-range.first == 0)
                                    {
                                        segment->Insert(timestamp.first, static_cast<uint8_t>(level), object);
                                    } else
                                    if (object_type == kRPVControllerObjectTypeEvent)
                                    {
                                        if (current_segment == range.first)
                                        {
                                            segment->Insert(timestamp.first, static_cast<uint8_t>(level), object);
                                        }
                                        else
                                        {
                                            if (object_type == kRPVControllerObjectTypeEvent)
                                            {
                                                Event* event = (Event*)object;
                                                segment->Insert(timestamp.first, static_cast<uint8_t>(level), event);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (current_segment == range.first)
                                        {
                                            segment->Insert(timestamp.first, static_cast<uint8_t>(level), object);
                                        }
                                    }
                                }
                            }
                        }
                        else
                        {
                            result = kRocProfVisResultOutOfRange;
                        }
                    }
                }
                break;
            }
            case kRPVControllerTrackThread:
            {
                ThreadRef ref(value);
                if(ref.IsValid())
                {
                    SetThread(ref.Get());
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerTrackQueue:
            {
                QueueRef ref(value);
                if(ref.IsValid())
                {
                    SetQueue(ref.Get());
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerTrackStream:
            {
                StreamRef ref(value);
                if(ref.IsValid())
                {
                    SetStream(ref.Get());
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerTrackCounter:
            {
                CounterRef ref(value);
                if(ref.IsValid())
                {
                    SetCounter(ref.Get());
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Track::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerTrackCategory:
            {
                SetCategory(value);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackMainName:
            {
                SetMainName(value);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackSubName:
            {
                SetSubName(value);
                result = kRocProfVisResultSuccess;
                break;
            }
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Track::FillBounds()
{
    track_bounds_t bounds;
    uint64_t       start_timestamp = 0;
    uint64_t       end_timestamp   = 0;

    rocprofvis_dm_result_t dm_result = rocprofvis_dm_get_property_as_uint64(
        GetDmHandle(), kRPVDMTrackNumRecordsUInt64, 0, &bounds.num_entries);
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        dm_result = rocprofvis_dm_get_property_as_uint64(
            GetDmHandle(), kRPVDMTrackMinimumTimestampUInt64, 0, &start_timestamp);
    }
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        dm_result = rocprofvis_dm_get_property_as_uint64(
            GetDmHandle(), kRPVDMTrackMaximumTimestampUInt64, 0, &end_timestamp);
    }
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        dm_result = rocprofvis_dm_get_property_as_double(
            GetDmHandle(), kRPVDMTrackMinimumValueDouble, 0, &bounds.min_value);
    }
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        dm_result = rocprofvis_dm_get_property_as_double(
            GetDmHandle(), kRPVDMTrackMaximumValueDouble, 0, &bounds.max_value);
    }

    if(dm_result == kRocProfVisDmResultSuccess &&
       GetTrackType() == kRPVControllerTrackTypeSamples)
    {
        rocprofvis_dm_trace_t trace = nullptr;
        dm_result = rocprofvis_dm_get_property_as_handle(
            GetDmHandle(), kRPVDMTrackTraceHandle, 0, &trace);
        if(dm_result == kRocProfVisDmResultSuccess)
        {
            dm_result = rocprofvis_dm_get_property_as_uint64(
                trace, kRPVDMEndTimeUInt64, 0, &end_timestamp);
        }
    }

    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        bounds.start_timestamp = static_cast<double>(start_timestamp);
        bounds.end_timestamp   = static_cast<double>(end_timestamp);
        SetNumberOfEntries(bounds.num_entries);
        SetStartTimestamp(bounds.start_timestamp);
        SetEndTimestamp(bounds.end_timestamp);
        SetMinValue(bounds.min_value);
        SetMaxValue(bounds.max_value);
        result = kRocProfVisResultSuccess;
    }

    return result;
}

rocprofvis_result_t
Track::FillMetadata()
{
    track_metadata_t metadata;
    char*            category      = nullptr;
    char*            main_name     = nullptr;
    char*            sub_name      = nullptr;
    uint64_t         dm_track_type = 0;

    rocprofvis_dm_result_t dm_result = rocprofvis_dm_get_property_as_charptr(
        GetDmHandle(), kRPVDMTrackCategoryEnumCharPtr, 0, &category);
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        dm_result = rocprofvis_dm_get_property_as_charptr(
            GetDmHandle(), kRPVDMTrackMainProcessNameCharPtr, 0, &main_name);
    }
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        dm_result = rocprofvis_dm_get_property_as_charptr(
            GetDmHandle(), kRPVDMTrackSubProcessNameCharPtr, 0, &sub_name);
    }
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        dm_result = rocprofvis_dm_get_property_as_uint64(
            GetDmHandle(), kRPVDMTrackCategoryEnumUInt64, 0, &dm_track_type);
    }

    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(dm_result == kRocProfVisDmResultSuccess && category && main_name && sub_name)
    {
        metadata.category  = category;
        metadata.main_name = main_name;
        metadata.sub_name  = sub_name;

        switch(static_cast<rocprofvis_dm_track_category_t>(dm_track_type))
        {
            case kRocProfVisDmPmcTrack:
            {
                metadata.operation_types = { kRocProfVisDmOperationNoOp };
                break;
            }
            case kRocProfVisDmRegionTrack:
            {
                metadata.operation_types = { kRocProfVisDmOperationLaunch,
                                             kRocProfVisDmOperationLaunchSample };
                break;
            }
            case kRocProfVisDmKernelDispatchTrack:
            {
                metadata.operation_types = { kRocProfVisDmOperationDispatch };
                break;
            }
            case kRocProfVisDmMemoryAllocationTrack:
            {
                metadata.operation_types = { kRocProfVisDmOperationMemoryAllocate };
                break;
            }
            case kRocProfVisDmMemoryCopyTrack:
            {
                metadata.operation_types = { kRocProfVisDmOperationMemoryCopy };
                break;
            }
            case kRocProfVisDmStreamTrack:
            {
                metadata.operation_types = { kRocProfVisDmOperationLaunch,
                                             kRocProfVisDmOperationDispatch,
                                             kRocProfVisDmOperationMemoryAllocate,
                                             kRocProfVisDmOperationMemoryCopy,
                                             kRocProfVisDmOperationLaunchSample };
                break;
            }
            case kRocProfVisDmRegionMainTrack:
            {
                metadata.operation_types = { kRocProfVisDmOperationLaunch };
                break;
            }
            case kRocProfVisDmRegionSampleTrack:
            {
                metadata.operation_types = { kRocProfVisDmOperationLaunchSample };
                break;
            }
            default:
            {
                metadata.operation_types = { kRocProfVisDmMultipleOperations };
                break;
            }
        }

        SetCategory(metadata.category);
        SetMainName(metadata.main_name);
        SetSubName(metadata.sub_name);
        SetNumberOfOperationTypes(metadata.operation_types.size());
        for(uint64_t index = 0; index < metadata.operation_types.size(); ++index)
        {
            SetOperationType(index, metadata.operation_types[index]);
        }
        result = kRocProfVisResultSuccess;
    }

    return result;
}

rocprofvis_result_t
Track::FillTopologyIds()
{
    track_topology_ids_t topology_ids;

    rocprofvis_dm_result_t dm_result = rocprofvis_dm_get_property_as_uint64(
        GetDmHandle(), kRPVDMTrackNodeIdUInt64, 0, &topology_ids.node_id);
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        dm_result = rocprofvis_dm_get_property_as_uint64(
            GetDmHandle(), kRPVDMTrackProcessIdUInt64, 0,
            &topology_ids.agent_id_or_pid);
    }
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        dm_result = rocprofvis_dm_get_property_as_uint64(
            GetDmHandle(), kRPVDMTrackSubProcessIdUInt64, 0,
            &topology_ids.queue_id_or_tid);
    }

    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(dm_result == kRocProfVisDmResultSuccess)
    {
        SetNodeId(topology_ids.node_id);
        SetAgentIdOrPid(topology_ids.agent_id_or_pid);
        SetQueueIdOrTid(topology_ids.queue_id_or_tid);
        result = kRocProfVisResultSuccess;
    }

    return result;
}

}
}
