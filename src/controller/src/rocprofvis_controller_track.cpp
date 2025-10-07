// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_counter.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_queue.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_stream.h"
#include "rocprofvis_controller_thread.h"
#include "rocprofvis_controller_trace.h"
#include "rocprofvis_core_assert.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstring>
#include <set>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_thread_t, Thread, kRPVControllerObjectTypeThread>
    ThreadRef;
typedef Reference<rocprofvis_controller_queue_t, Queue, kRPVControllerObjectTypeQueue>
    QueueRef;
typedef Reference<rocprofvis_controller_stream_t, Stream, kRPVControllerObjectTypeStream>
    StreamRef;
typedef Reference<rocprofvis_controller_counter_t, Counter,
                  kRPVControllerObjectTypeCounter>
    CounterRef;

Track::Track(rocprofvis_controller_track_type_t type, uint64_t id,
             rocprofvis_dm_track_t dm_handle, Trace* ctx)
: m_id(id)
, m_num_entries(0)
, m_type(type)
, m_start_timestamp(DBL_MIN)
, m_end_timestamp(DBL_MAX)
, m_dm_handle(dm_handle)
, m_thread(nullptr)
, m_queue(nullptr)
, m_stream(nullptr)
, m_counter(nullptr)
, m_ctx(ctx)
{
    s_data_model_load = 0;
}

Track::~Track() {}

rocprofvis_dm_track_t
Track::GetDmHandle(void)
{
    return m_dm_handle;
}

Handle*
Track::GetContext(void)
{
    return m_ctx;
}

SegmentTimeline*
Track::GetSegments()
{
    return &m_segments;
}

rocprofvis_result_t
Track::FetchSegments(double start, double end, void* user_ptr, Future* future,
                     FetchSegmentsFunc func)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    if(m_start_timestamp <= end && m_end_timestamp >= start)
    {
        if(m_segments.GetSegmentDuration() == 0)
        {
            uint32_t num_segments =
                (uint32_t) ceil((m_end_timestamp - m_start_timestamp) / kSegmentDuration);
            m_segments.SetContext(this);
            m_segments.Init(m_start_timestamp, kSegmentDuration, num_segments,
                            m_num_entries);
        }

        start = std::max(start, m_start_timestamp);
        end   = std::min(end, m_end_timestamp);

        std::vector<std::pair<uint32_t, uint32_t>> fetch_ranges;

        uint32_t start_index =
            (uint32_t) floor((start - m_start_timestamp) / kSegmentDuration);
        uint32_t end_index =
            (uint32_t) ceil((end - m_start_timestamp) / kSegmentDuration);

        {
            std::unique_lock lock(*m_segments.GetMutex());
            m_cv.wait(lock, [&] {
                for(uint32_t i = start_index; i < end_index; i++)
                {
                    if(m_segments.IsProcessed(i))
                    {
                        return false;
                    }
                }
                return true;
            });
            for(uint32_t i = start_index; i < end_index; i++)
            {
                if(!m_segments.IsValid(i))
                {
                    m_segments.SetProcessed(i, true);
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
                        m_start_timestamp + (range.first * kSegmentDuration);
                    double fetch_end =
                        m_start_timestamp + ((range.second + 1) * kSegmentDuration);

                    result = FetchFromDataModel(fetch_start, fetch_end, future);
                    spdlog::debug(
                        "FetchFromDataModel for track {} ({}-{}) = {}, cancelled={}",
                        m_id, fetch_start, fetch_end, (uint32_t) result,
                        future->IsCancelled());
                }
                {
                    std::unique_lock lock(*m_segments.GetMutex());
                    for(uint32_t i = range.first; i <= range.second; i++)
                    {
                        m_segments.SetProcessed(i, false);
                        m_segments.SetValid(i, result == kRocProfVisResultSuccess);
                    }
                }
                m_cv.notify_all();
            }
        }
        else
        {
            result = kRocProfVisResultOutOfRange;
        }

        {
            std::unique_lock lock(*m_segments.GetMutex());
            m_cv.wait(lock, [&] {
                for(uint32_t i = start_index; i < end_index; i++)
                {
                    if(m_segments.IsProcessed(i))
                    {
                        return false;
                    }
                }
                return true;
            });
        }

        result = m_segments.FetchSegments(start, end, user_ptr, future, func);
    }

    return result;
}

struct FetchEventsArgs
{
    Array*                       m_array;
    uint64_t*                    m_index;
    std::unordered_set<uint64_t> m_event_ids;
    SegmentLRUParams             lru_params;
};

rocprofvis_result_t
Track::Fetch(double start, double end, Array& array, uint64_t& index, Future* future)
{
    FetchEventsArgs args;
    args.m_array          = &array;
    args.m_index          = &index;
    args.lru_params.m_ctx = (Trace*) GetContext();
    args.lru_params.m_lod = 0;
    array.SetContext(GetContext());

    rocprofvis_result_t result =
        FetchSegments(start, end, &args, future,
                      [](double start, double end, Segment& segment, void* user_ptr,
                         SegmentTimeline* owner) -> rocprofvis_result_t {
                          FetchEventsArgs* args      = (FetchEventsArgs*) user_ptr;
                          args->lru_params.m_owner   = owner;
                          rocprofvis_result_t result = segment.Fetch(
                              start, end, args->m_array->GetVector(), *args->m_index,
                              &args->m_event_ids, &args->lru_params);
                          return result;
                      });

    return result;
}

inline uint64_t
hash_combine(uint64_t a, uint64_t b)
{
    a ^= b + 0x9e3779b97f4a7c15 + (a << 12) + (a >> 4);
    return a;
}

rocprofvis_result_t
Track::FetchFromDataModel(double start, double end, Future* future)
{
    s_data_model_load++;
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;

    rocprofvis_dm_trace_t trace =
        rocprofvis_dm_get_property_as_handle(m_dm_handle, kRPVDMTrackTraceHandle, 0);
    rocprofvis_dm_database_t db =
        rocprofvis_dm_get_property_as_handle(m_dm_handle, kRPVDMTrackDatabaseHandle, 0);
    uint64_t dm_track_type = rocprofvis_dm_get_property_as_uint64(
        m_dm_handle, kRPVDMTrackCategoryEnumUInt64, 0);

    double fetch_start =
        m_start_timestamp +
        (floor((start - m_start_timestamp) / kSegmentDuration) * kSegmentDuration);

    double zerod_end          = end - m_start_timestamp;
    double divided_by_segment = zerod_end / kSegmentDuration;
    double ceil_segment       = ceil(divided_by_segment);
    double rounded_segment    = ceil_segment * kSegmentDuration;
    double fetch_end          = m_start_timestamp + rounded_segment;

    int num_threads = 1;

    std::vector<rocprofvis_db_future_t> futures;
    double time_per_query = (fetch_end - fetch_start) / num_threads;
    for(int i = 0; i < num_threads; i++)
    {
        if(future->IsCancelled()) break;
        rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
        if(nullptr != object2wait)
        {
            if(kRocProfVisDmResultSuccess ==
               rocprofvis_db_read_trace_slice_async(
                   db, (uint64_t) (fetch_start + (i * time_per_query)),
                   (uint64_t) (fetch_start + ((i + 1) * time_per_query)), 1,
                   (rocprofvis_db_track_selection_t) &m_id, object2wait))
            {
                futures.push_back(object2wait);
                future->AddDependentFuture(object2wait);
            }
        }
    }

    for(int i = 0; i < futures.size(); i++)
    {
        if(future->IsCancelled()) break;
        rocprofvis_dm_result_t dm_result =
            rocprofvis_db_future_wait(futures[i], UINT64_MAX);
        if(kRocProfVisDmResultSuccess == dm_result)
        {
            rocprofvis_dm_slice_t slice = rocprofvis_dm_get_property_as_handle(
                m_dm_handle, kRPVDMSliceHandleTimed,
                hash_combine(fetch_start + (i * time_per_query),
                             fetch_start + ((i + 1) * time_per_query)));
            if(nullptr != slice)
            {
                uint64_t num_records = rocprofvis_dm_get_property_as_uint64(
                    slice, kRPVDMNumberOfRecordsUInt64, 0);

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
                            uint64_t index = 0;

                            for(int i = 0; i < num_records; i++)
                            {
                                if(future->IsCancelled()) break;
                                double timestamp =
                                    (double) rocprofvis_dm_get_property_as_uint64(
                                        slice, kRPVDMTimestampUInt64Indexed, i);
                                double duration =
                                    (double) rocprofvis_dm_get_property_as_int64(
                                        slice, kRPVDMEventDurationInt64Indexed, i);
                                if(duration < 0) continue;

                                uint64_t event_id = rocprofvis_dm_get_property_as_uint64(
                                    slice, kRPVDMEventIdUInt64Indexed, i);
                                Event* new_event = m_ctx->GetMemoryManager()->NewEvent(
                                    event_id, timestamp, timestamp + duration,
                                    GetSegments());
                                if(new_event)
                                {
                                    result = new_event->SetUInt64(
                                        kRPVControllerEventLevel, 0,
                                        rocprofvis_dm_get_property_as_uint64(
                                            slice, kRPVDMEventLevelUInt64Indexed, i));
                                    result = new_event->SetString(
                                        kRPVControllerEventCategory, 0,
                                        rocprofvis_dm_get_property_as_charptr(
                                            slice, kRPVDMEventTypeStringCharPtrIndexed,
                                            i),
                                        0);
                                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                                    result = new_event->SetString(
                                        kRPVControllerEventName, 0,
                                        rocprofvis_dm_get_property_as_charptr(
                                            slice, kRPVDMEventSymbolStringCharPtrIndexed,
                                            i),
                                        0);
                                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

                                    result = SetObject(kRPVControllerTrackEntry, index++,
                                                       (rocprofvis_handle_t*) new_event);
                                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                                }
                                else
                                {
                                    result = kRocProfVisResultMemoryAllocError;
                                    break;
                                }
                            }

                            break;
                        }
                        case kRocProfVisDmPmcTrack:
                        {
                            uint64_t index     = 0;
                            uint64_t sample_id = 0;
                            for(int i = 0; i < num_records; i++)
                            {
                                if(future->IsCancelled()) break;
                                double timestamp =
                                    (double) rocprofvis_dm_get_property_as_uint64(
                                        slice, kRPVDMTimestampUInt64Indexed, i);

                                Sample* new_sample = m_ctx->GetMemoryManager()->NewSample(
                                    kRPVControllerPrimitiveTypeDouble, sample_id++,
                                    timestamp, GetSegments());
                                if(new_sample)
                                {
                                    new_sample->SetDouble(
                                        kRPVControllerSampleValue, 0,
                                        rocprofvis_dm_get_property_as_double(
                                            slice, kRPVDMPmcValueDoubleIndexed, i));
                                    SetObject(kRPVControllerTrackEntry, index++,
                                              (rocprofvis_handle_t*) new_sample);
                                }
                                else
                                {
                                    result = kRocProfVisResultMemoryAllocError;
                                    break;
                                }
                            }

                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                }

                if(kRocProfVisDmResultSuccess !=
                   rocprofvis_dm_delete_time_slice_handle(trace, m_id, slice))
                {
                    result = kRocProfVisResultUnknownError;
                }
            }
            else
            {
                result = kRocProfVisResultNotLoaded;
            }
        }
        else
        {
            if(dm_result == kRocProfVisDmResultDbAbort)
            {
                result = kRocProfVisResultCancelled;
            }
            else
            {
                result = kRocProfVisResultUnknownError;
            }
        }
        future->RemoveDependentFuture(futures[i]);
        rocprofvis_db_future_free(futures[i]);
    }
    s_data_model_load--;

    return future->IsCancelled() ? kRocProfVisResultCancelled : result;
}

rocprofvis_controller_object_type_t
Track::GetType(void)
{
    return kRPVControllerObjectTypeTrack;
}

rocprofvis_result_t
Track::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                *value = sizeof(Track);
                result = kRocProfVisResultSuccess;
                for(auto& pair : m_segments.GetSegments())
                {
                    *value += sizeof(pair);
                    uint64_t entry_size = 0;
                    result              = pair.second->GetMemoryUsage(
                        &entry_size, (rocprofvis_common_property_t) property);
                    if(result == kRocProfVisResultSuccess)
                    {
                        *value += entry_size;
                    }
                    else
                    {
                        break;
                    }
                }
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Track);
                result = kRocProfVisResultSuccess;
                for(auto& pair : m_segments.GetSegments())
                {
                    *value += sizeof(pair);
                }
                break;
            }
            case kRPVControllerTrackId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackType:
            {
                *value = m_type;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackNumberOfEntries:
            {
                *value = m_num_entries;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackExtDataNumberOfEntries:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    m_dm_handle, kRPVDMNumberOfTrackExtDataRecordsUInt64, 0);
                break;
            }
            case kRPVControllerTrackNode:
            {
                *value = m_node;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackMinTimestamp:
            case kRPVControllerTrackMaxTimestamp:
            case kRPVControllerTrackEntry:
            case kRPVControllerTrackName:
            case kRPVControllerTrackExtDataCategoryIndexed:
            case kRPVControllerTrackExtDataNameIndexed:
            case kRPVControllerTrackExtDataValueIndexed:
            case kRPVControllerTrackThread:
            case kRPVControllerTrackQueue:
            case kRPVControllerTrackCounter:
            case kRPVControllerTrackStream:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Track::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTrackMinTimestamp:
            {
                *value = m_start_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackMaxTimestamp:
            {
                *value = m_end_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackMinValue:
            {
                *value = m_min_value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackMaxValue:
            {
                *value = m_max_value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackId:
            case kRPVControllerTrackType:
            case kRPVControllerTrackNumberOfEntries:
            case kRPVControllerTrackEntry:
            case kRPVControllerTrackName:
            case kRPVControllerTrackExtDataNumberOfEntries:
            case kRPVControllerTrackExtDataCategoryIndexed:
            case kRPVControllerTrackExtDataNameIndexed:
            case kRPVControllerTrackExtDataValueIndexed:
            case kRPVControllerTrackThread:
            case kRPVControllerTrackQueue:
            case kRPVControllerTrackCounter:
            case kRPVControllerTrackStream:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t
Track::GetObject(rocprofvis_property_t property, uint64_t index,
                 rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
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
                *value = (rocprofvis_handle_t*) m_thread;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackQueue:
            {
                *value = (rocprofvis_handle_t*) m_queue;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackStream:
            {
                *value = (rocprofvis_handle_t*) m_stream;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackCounter:
            {
                *value = (rocprofvis_handle_t*) m_counter;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackId:
            case kRPVControllerTrackType:
            case kRPVControllerTrackNumberOfEntries:
            case kRPVControllerTrackMinTimestamp:
            case kRPVControllerTrackMaxTimestamp:
            case kRPVControllerTrackName:
            case kRPVControllerTrackExtDataNumberOfEntries:
            case kRPVControllerTrackExtDataCategoryIndexed:
            case kRPVControllerTrackExtDataNameIndexed:
            case kRPVControllerTrackExtDataValueIndexed:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
Track::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                 uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTrackName:
        {
            result = GetStdStringImpl(value, length, m_name);
            break;
        }

        case kRPVControllerTrackExtDataCategoryIndexed:
        {
            char* str = rocprofvis_dm_get_property_as_charptr(
                m_dm_handle, kRPVDMTrackExtDataCategoryCharPtrIndexed, index);
            result = GetStringImpl(value, length, str, strlen(str));
            break;
        }
        case kRPVControllerTrackExtDataNameIndexed:
        {
            char* str = rocprofvis_dm_get_property_as_charptr(
                m_dm_handle, kRPVDMTrackExtDataNameCharPtrIndexed, index);
            result = GetStringImpl(value, length, str, strlen(str));
            break;
        }
        case kRPVControllerTrackExtDataValueIndexed:
        {
            char* str = rocprofvis_dm_get_property_as_charptr(
                m_dm_handle, kRPVDMTrackExtDataValueCharPtrIndexed, index);
            result = GetStringImpl(value, length, str, strlen(str));
            break;
        }

        case kRPVControllerTrackMinTimestamp:
        case kRPVControllerTrackMaxTimestamp:
        case kRPVControllerTrackId:
        case kRPVControllerTrackType:
        case kRPVControllerTrackNumberOfEntries:
        case kRPVControllerTrackEntry:
        case kRPVControllerTrackExtDataNumberOfEntries:
        case kRPVControllerTrackThread:
        case kRPVControllerTrackQueue:
        case kRPVControllerTrackCounter:
        case kRPVControllerTrackStream:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}

rocprofvis_result_t
Track::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
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
            m_type = (rocprofvis_controller_track_type_t) value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackNumberOfEntries:
        {
            m_num_entries = value;
            result        = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackNode:
        {
            m_node = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackMinTimestamp:
        case kRPVControllerTrackMaxTimestamp:
        case kRPVControllerTrackEntry:
        case kRPVControllerTrackName:
        case kRPVControllerTrackExtDataNumberOfEntries:
        case kRPVControllerTrackExtDataCategoryIndexed:
        case kRPVControllerTrackExtDataNameIndexed:
        case kRPVControllerTrackExtDataValueIndexed:
        case kRPVControllerTrackThread:
        case kRPVControllerTrackQueue:
        case kRPVControllerTrackCounter:
        case kRPVControllerTrackStream:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}
rocprofvis_result_t
Track::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTrackMinTimestamp:
        {
            m_start_timestamp = value;
            result            = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackMaxTimestamp:
        {
            m_end_timestamp = value;
            result          = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackMinValue:
        {
            m_min_value = value;
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackMaxValue:
        {
            m_max_value = value;
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackEntry:
        case kRPVControllerTrackId:
        case kRPVControllerTrackType:
        case kRPVControllerTrackNumberOfEntries:
        case kRPVControllerTrackName:
        case kRPVControllerTrackExtDataNumberOfEntries:
        case kRPVControllerTrackExtDataCategoryIndexed:
        case kRPVControllerTrackExtDataNameIndexed:
        case kRPVControllerTrackExtDataValueIndexed:
        case kRPVControllerTrackThread:
        case kRPVControllerTrackQueue:
        case kRPVControllerTrackCounter:
        case kRPVControllerTrackStream:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        default:
        {
            result = kRocProfVisResultInvalidEnum;
            break;
        }
    }
    return result;
}
rocprofvis_result_t
Track::SetObject(rocprofvis_property_t property, uint64_t index,
                 rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTrackEntry:
            {
                // Start & end timestamps must be configured
                ROCPROFVIS_ASSERT(m_start_timestamp >= 0.0 &&
                                  m_start_timestamp < m_end_timestamp);
                Handle* object      = (Handle*) value;
                auto    object_type = object->GetType();
                if(((m_type == kRPVControllerTrackTypeEvents) &&
                    (object_type == kRPVControllerObjectTypeEvent)) ||
                   ((m_type == kRPVControllerTrackTypeSamples) &&
                    (object_type == kRPVControllerObjectTypeSample)))
                {
                    rocprofvis_property_t start_ts_property;
                    rocprofvis_property_t end_ts_property;
                    uint64_t              level    = 0;
                    uint64_t              event_id = 0;
                    if(object_type == kRPVControllerObjectTypeEvent)
                    {
                        result = object->GetUInt64(kRPVControllerEventLevel, 0, &level);
                        result = object->GetUInt64(kRPVControllerEventId, 0, &event_id);
                        start_ts_property = kRPVControllerEventStartTimestamp;
                        end_ts_property   = kRPVControllerEventEndTimestamp;
                    }
                    else
                    {
                        start_ts_property = kRPVControllerSampleTimestamp;
                        end_ts_property   = kRPVControllerSampleTimestamp;
                    }

                    typedef struct
                    {
                        double start;
                        double end;
                    } timestamp_pair;
                    timestamp_pair timestamp = { 0 };
                    result = object->GetDouble(start_ts_property, 0, &timestamp.start);
                    if(result == kRocProfVisResultSuccess)
                    {
                        result = object->GetDouble(end_ts_property, 0, &timestamp.end);
                    }
                    if(result == kRocProfVisResultSuccess)
                    {
                        if(timestamp.start >= m_start_timestamp &&
                           timestamp.end <= m_end_timestamp)
                        {
                            timestamp_pair relative = {
                                timestamp.start - m_start_timestamp,
                                timestamp.end - m_start_timestamp
                            };
                            timestamp_pair num_segments = {
                                floor(relative.start / kSegmentDuration),
                                floor(relative.end / kSegmentDuration)
                            };

                            std::unique_lock lock(*m_segments.GetMutex());
                            for(double current_segment = num_segments.start;
                                current_segment <= num_segments.end; current_segment++)
                            {
                                double segment_start =
                                    m_start_timestamp +
                                    (current_segment * kSegmentDuration);

                                if(m_segments.GetSegments().find(segment_start) ==
                                   m_segments.GetSegments().end())
                                {
                                    double segment_end = segment_start + kSegmentDuration;
                                    std::unique_ptr<Segment> segment =
                                        std::make_unique<Segment>(m_type, &m_segments);
                                    segment->SetStartEndTimestamps(segment_start,
                                                                   segment_end);
                                    segment->SetMinTimestamp(timestamp.start);
                                    segment->SetMaxTimestamp(timestamp.end);
                                    result = m_segments.Insert(segment_start,
                                                               std::move(segment));
                                    if(result == kRocProfVisResultDuplicate)
                                    {
                                        spdlog::warn("Segment already exists at {}",
                                                     segment_start);
                                        result = kRocProfVisResultSuccess;
                                    }
                                }

                                if(result == kRocProfVisResultSuccess)
                                {
                                    std::unique_ptr<Segment>& segment =
                                        m_segments.GetSegments()[segment_start];
                                    segment->SetMinTimestamp(std::min(
                                        segment->GetMinTimestamp(), timestamp.start));
                                    segment->SetMaxTimestamp(std::max(
                                        segment->GetMaxTimestamp(), timestamp.end));
                                    if(current_segment == num_segments.start)
                                    {
                                        segment->Insert(timestamp.start, level, object);
                                    }
                                    else
                                    {
                                        if(object_type == kRPVControllerObjectTypeEvent)
                                        {
                                            Event* event = (Event*) object;
                                            segment->Insert(timestamp.start, level,
                                                            event);
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
                    m_thread = ref.Get();
                    result   = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerTrackQueue:
            {
                QueueRef ref(value);
                if(ref.IsValid())
                {
                    m_queue = ref.Get();
                    result  = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerTrackStream:
            {
                StreamRef ref(value);
                if(ref.IsValid())
                {
                    m_stream = ref.Get();
                    result   = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerTrackCounter:
            {
                CounterRef ref(value);
                if(ref.IsValid())
                {
                    m_counter = ref.Get();
                    result    = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerTrackId:
            case kRPVControllerTrackType:
            case kRPVControllerTrackNumberOfEntries:
            case kRPVControllerTrackMinTimestamp:
            case kRPVControllerTrackMaxTimestamp:
            case kRPVControllerTrackName:
            case kRPVControllerTrackExtDataNumberOfEntries:
            case kRPVControllerTrackExtDataCategoryIndexed:
            case kRPVControllerTrackExtDataNameIndexed:
            case kRPVControllerTrackExtDataValueIndexed:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t
Track::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                 uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTrackName:
            {
                m_name = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackMinTimestamp:
            case kRPVControllerTrackMaxTimestamp:
            case kRPVControllerTrackId:
            case kRPVControllerTrackType:
            case kRPVControllerTrackNumberOfEntries:
            case kRPVControllerTrackEntry:
            case kRPVControllerTrackExtDataNumberOfEntries:
            case kRPVControllerTrackExtDataCategoryIndexed:
            case kRPVControllerTrackExtDataNameIndexed:
            case kRPVControllerTrackExtDataValueIndexed:
            case kRPVControllerTrackThread:
            case kRPVControllerTrackQueue:
            case kRPVControllerTrackCounter:
            case kRPVControllerTrackStream:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

}  // namespace Controller
}  // namespace RocProfVis
