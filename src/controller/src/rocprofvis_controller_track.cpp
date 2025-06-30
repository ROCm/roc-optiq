// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_controller_trace.h"

#include <algorithm>
#include <cfloat>
#include <cstring>
#include <cmath>
#include <set>

namespace RocProfVis
{
namespace Controller
{

Track::Track(rocprofvis_controller_track_type_t type, uint64_t id, rocprofvis_dm_track_t dm_handle, Trace * ctx)
: m_id(id)
, m_num_elements(0)
, m_type(type)
, m_start_timestamp(DBL_MIN)
, m_end_timestamp(DBL_MAX)
, m_dm_handle(dm_handle)
, m_ctx(ctx)
{

}

Track::~Track()
{
}

rocprofvis_dm_track_t Track::GetDmHandle(void){
    
    return m_dm_handle;
}


Trace* Track::GetContext(void)
{
    return m_ctx;
}

rocprofvis_result_t Track::FetchSegments(double start, double end, void* user_ptr, FetchSegmentsFunc func)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    if(m_start_timestamp <= end && m_end_timestamp >= start)
    {
        if (m_segments.GetSegmentDuration() == 0)
        {
            uint32_t num_segments = (uint32_t)ceil((m_end_timestamp - m_start_timestamp) / kSegmentDuration);
            m_segments.Init(m_start_timestamp, kSegmentDuration, num_segments);
        }

        start = std::max(start, m_start_timestamp);
        end   = std::min(end, m_end_timestamp);

        std::vector<std::pair<uint32_t, uint32_t>> fetch_ranges;

        uint32_t start_index = (uint32_t) floor((start - m_start_timestamp) / kSegmentDuration);
        uint32_t end_index   = (uint32_t) ceil((end - m_start_timestamp) / kSegmentDuration);

        for(uint32_t i = start_index; i < end_index; i++)
        {
            if(!m_segments.IsValid(i))
            {
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

        if(fetch_ranges.size())
        {
            for(auto& range : fetch_ranges)
            {
                double fetch_start = m_start_timestamp + (range.first * kSegmentDuration);
                double fetch_end   = m_start_timestamp + ((range.second + 1) * kSegmentDuration);

                result = FetchFromDataModel(fetch_start, fetch_end);
                if (result == kRocProfVisResultSuccess)
                {
                    for(uint32_t i = range.first; i <= range.second; i++)
                    {
                        m_segments.SetValid(i);
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
            result = kRocProfVisResultSuccess;
        }

        if(result == kRocProfVisResultSuccess)
        {
            result = m_segments.FetchSegments(start, end, user_ptr, func);
        }
    }

    return result;
}

rocprofvis_result_t Track::DeleteSegment(void* target, uint32_t lod)
{
    rocprofvis_result_t result = kRocProfVisResultSuccess;
    result = m_segments.Remove((Segment*)target);
    return result;
}

struct FetchEventsArgs
{
    Array*             m_array;
    uint64_t*          m_index;
    std::unordered_set<uint64_t> m_event_ids;
    SegmentLRUParams lru_params;
};

rocprofvis_result_t Track::Fetch(double start, double end, Array& array, uint64_t& index)
{
    std::unique_lock lock(m_ctx->GetMemoryManager()->GetMemoryManagerMutex());
    FetchEventsArgs args;
    args.m_array = &array;
    args.m_index = &index;
    args.lru_params.m_owner = this;
    args.lru_params.m_ctx   = GetContext();
    args.lru_params.m_lod      = 0;
    array.SetContext(GetContext());

    rocprofvis_result_t result = FetchSegments(start, end, &args, [](double start, double end, Segment& segment, void* user_ptr) -> rocprofvis_result_t
    {
        FetchEventsArgs* args = (FetchEventsArgs*) user_ptr;
        rocprofvis_result_t result = segment.Fetch(start, end, args->m_array->GetVector(), *args->m_index, &args->m_event_ids, &args->lru_params);
        return result;
    });
        

    return result;
}

rocprofvis_result_t Track::FetchFromDataModel(double start, double end)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
    if(nullptr != object2wait)
    {
        rocprofvis_dm_trace_t trace = rocprofvis_dm_get_property_as_handle(
            m_dm_handle, kRPVDMTrackTraceHandle, 0);
        rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(
            m_dm_handle, kRPVDMTrackDatabaseHandle, 0);
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

        if(kRocProfVisDmResultSuccess ==
           rocprofvis_db_read_trace_slice_async(db, (uint64_t) fetch_start, (uint64_t) fetch_end, 1,
                                                (rocprofvis_db_track_selection_t) &m_id,
                                                object2wait))
        {
            if(kRocProfVisDmResultSuccess ==
               rocprofvis_db_future_wait(object2wait, UINT64_MAX))
            {
                rocprofvis_dm_slice_t slice = rocprofvis_dm_get_property_as_handle(
                    m_dm_handle, kRPVDMSliceHandleTimed, fetch_start);
                if(nullptr != slice)
                {
                    uint64_t num_records = rocprofvis_dm_get_property_as_uint64(
                        slice, kRPVDMNumberOfRecordsUInt64, 0);

                    if(num_records > 0)
                    {
                        switch(dm_track_type)
                        {
                            case kRocProfVisDmRegionTrack:
                            case kRocProfVisDmKernelTrack:
                            {
                                uint64_t index = 0;

                                for(int i = 0; i < num_records; i++)
                                {
                                    double timestamp =
                                        (double) rocprofvis_dm_get_property_as_uint64(
                                            slice, kRPVDMTimestampUInt64Indexed, i);
                                    double duration =
                                        (double) rocprofvis_dm_get_property_as_int64(
                                            slice, kRPVDMEventDurationInt64Indexed, i);
                                    if(duration < 0) continue;

                                    uint64_t event_id =
                                        rocprofvis_dm_get_property_as_uint64(
                                            slice, kRPVDMEventIdUInt64Indexed, i);
                                    Event* new_event =
                                        m_ctx->GetMemoryManager()->NewEvent(
                                            event_id, timestamp,
                                                                 timestamp + duration);
                                    if(new_event)
                                    {
                                        result = new_event->SetUInt64(
                                            kRPVControllerEventLevel, 0,
                                            rocprofvis_dm_get_property_as_uint64(
                                                slice, kRPVDMEventLevelUInt64Indexed, i));
                                        result = new_event->SetString(
                                            kRPVControllerEventCategory, 0,
                                            rocprofvis_dm_get_property_as_charptr(
                                                slice,
                                                kRPVDMEventTypeStringCharPtrIndexed, i),
                                            0);
                                        ROCPROFVIS_ASSERT(result ==
                                                          kRocProfVisResultSuccess);

                                        result = new_event->SetString(
                                            kRPVControllerEventName, 0,
                                            rocprofvis_dm_get_property_as_charptr(
                                                slice,
                                                kRPVDMEventSymbolStringCharPtrIndexed, i),
                                            0);
                                        ROCPROFVIS_ASSERT(result ==
                                                          kRocProfVisResultSuccess);

                                        result = SetObject(
                                            kRPVControllerTrackEntry, index++,
                                            (rocprofvis_handle_t*) new_event);
                                        ROCPROFVIS_ASSERT(result ==
                                                          kRocProfVisResultSuccess);
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
                                uint64_t index = 0;
                                uint64_t sample_id = 0;
                                for(int i = 0; i < num_records; i++)
                                {
                                    double timestamp =
                                        (double) rocprofvis_dm_get_property_as_uint64(
                                            slice, kRPVDMTimestampUInt64Indexed, i);

                                    Sample* new_sample =
                                        new Sample(kRPVControllerPrimitiveTypeDouble,
                                                   sample_id++, timestamp);
                                    if(new_sample)
                                    {
                                        new_sample->SetDouble(
                                            kRPVControllerSampleValue, 0,
                                            rocprofvis_dm_get_property_as_double(
                                                slice, kRPVDMPmcValueDoubleIndexed, i));
                                        SetObject(
                                            kRPVControllerTrackEntry, index++,
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
                    rocprofvis_dm_delete_time_slice(trace, fetch_start, fetch_end);
                    
                    
                }
            }
        }
        rocprofvis_db_future_free(object2wait);
    }
    return kRocProfVisResultSuccess;
}

rocprofvis_controller_object_type_t Track::GetType(void)
{
    return kRPVControllerObjectTypeTrack;
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
                *value = sizeof(Track);
                result = kRocProfVisResultSuccess;
                for(auto& pair : m_segments.GetSegments())
                {
                    *value += sizeof(pair);
                    uint64_t entry_size = 0;
                    result = pair.second->GetMemoryUsage(&entry_size, (rocprofvis_common_property_t)property);
                    if (result == kRocProfVisResultSuccess)
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
                *value = m_num_elements;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTrackExtDataNumberOfEntries:
            {
                *value = rocprofvis_dm_get_property_as_uint64(
                    m_dm_handle, kRPVDMNumberOfTrackExtDataRecordsUInt64, 0);
                break;
            }
            case kRPVControllerTrackMinTimestamp:
            case kRPVControllerTrackMaxTimestamp:
            case kRPVControllerTrackEntry:
            case kRPVControllerTrackName:
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

rocprofvis_result_t Track::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
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
            case kRPVControllerTrackId:
            case kRPVControllerTrackType:
            case kRPVControllerTrackNumberOfEntries:
            case kRPVControllerTrackEntry:
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
rocprofvis_result_t Track::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value)
{
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

rocprofvis_result_t Track::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTrackName:
        {
            if (length && (!value || *length == 0))
            {
                *length = m_name.length();
                result = kRocProfVisResultSuccess;
            }
            else if (value && length && *length > 0)
            {
                strncpy(value, m_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            break;
        }

        case kRPVControllerTrackExtDataCategoryIndexed:
        {
            char* str = rocprofvis_dm_get_property_as_charptr(
                    m_dm_handle, kRPVDMTrackExtDataCategoryCharPtrIndexed, 0);
            if (length && (!value || *length == 0))
            {
                *length = strlen(str);
                result = kRocProfVisResultSuccess;
            }
            else if (value && length && *length > 0)
            {
                strncpy(value, str, *length);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerTrackExtDataNameIndexed:
        {
            char* str = rocprofvis_dm_get_property_as_charptr(
                    m_dm_handle, kRPVDMTrackExtDataNameCharPtrIndexed, 0);
            if (length && (!value || *length == 0))
            {
                *length = strlen(str);
                result = kRocProfVisResultSuccess;
            }
            else if (value && length && *length > 0)
            {
                strncpy(value, str, *length);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerTrackExtDataValueIndexed:
        {
            char* str = rocprofvis_dm_get_property_as_charptr(
                    m_dm_handle, kRPVDMTrackExtDataValueCharPtrIndexed, 0);
            if (length && (!value || *length == 0))
            {
                *length = strlen(str);
                result = kRocProfVisResultSuccess;
            }
            else if (value && length && *length > 0)
            {
                strncpy(value, str, *length);
                result = kRocProfVisResultSuccess;
            }
            break;
        }

        case kRPVControllerTrackMinTimestamp:
        case kRPVControllerTrackMaxTimestamp:
        case kRPVControllerTrackId:
        case kRPVControllerTrackType:
        case kRPVControllerTrackNumberOfEntries:
        case kRPVControllerTrackEntry:
        case kRPVControllerTrackExtDataNumberOfEntries:
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

rocprofvis_result_t Track::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
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
            m_type = (rocprofvis_controller_track_type_t)value;
            break;
        }
        case kRPVControllerTrackNumberOfEntries:
        {
            m_num_elements = value;
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
rocprofvis_result_t Track::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerTrackMinTimestamp:
        {
            m_start_timestamp = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerTrackMaxTimestamp:
        {
            m_end_timestamp = value;
            result = kRocProfVisResultSuccess;
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
rocprofvis_result_t Track::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerTrackEntry:
            {
                // Start & end timestamps must be configured
                ROCPROFVIS_ASSERT(m_start_timestamp >= 0.0 && m_start_timestamp < m_end_timestamp);

                Handle* object = (Handle*)value;
                auto object_type = object->GetType();
                if (((m_type == kRPVControllerTrackTypeEvents) && (object_type == kRPVControllerObjectTypeEvent))
                    || ((m_type == kRPVControllerTrackTypeSamples) && (object_type == kRPVControllerObjectTypeSample)))
                {
                    rocprofvis_property_t start_ts_property;
                    rocprofvis_property_t end_ts_property;
                    uint64_t              level = 0;
                    uint64_t              event_id = 0;
                    if (object_type == kRPVControllerObjectTypeEvent)
                    {  
                        result = object->GetUInt64(kRPVControllerEventLevel, 0, &level);
                        result = object->GetUInt64(kRPVControllerEventId, 0, &event_id);
                        start_ts_property = kRPVControllerEventStartTimestamp;
                        end_ts_property = kRPVControllerEventEndTimestamp;
                    }
                    else
                    {
                        start_ts_property = kRPVControllerSampleTimestamp;
                        end_ts_property = kRPVControllerSampleTimestamp;
                    }

                    typedef struct
                    {
                        double start;
                        double end;
                    } timestamp_pair;
                    timestamp_pair timestamp = { 0 };
                    result = object->GetDouble(start_ts_property, 0, &timestamp.start);
                    if (result == kRocProfVisResultSuccess)
                    {
                        result = object->GetDouble(end_ts_property, 0, &timestamp.end);
                    }
                    if (result == kRocProfVisResultSuccess)
                    {
                        if(timestamp.start >= m_start_timestamp &&
                           timestamp.end <= m_end_timestamp)
                        {
                            timestamp_pair relative  = { timestamp.start - m_start_timestamp,
                                                    timestamp.end - m_start_timestamp };
                            timestamp_pair num_segments = {floor(relative.start / kSegmentDuration),
                                                    floor(relative.end / kSegmentDuration)};
                            
                            for (double current_segment = num_segments.start; current_segment <= num_segments.end; current_segment++)
                            {
                                double segment_start =
                                    m_start_timestamp +
                                    (current_segment * kSegmentDuration);

                                if(m_segments.GetSegments().find(segment_start) ==
                                   m_segments.GetSegments().end())
                                {
                                    double segment_end = segment_start + kSegmentDuration;
                                    std::unique_ptr<Segment> segment =
                                        std::make_unique<Segment>(m_type, m_ctx);
                                    segment->SetStartEndTimestamps(segment_start,
                                                                   segment_end);
                                    segment->SetMinTimestamp(timestamp.start);
                                    segment->SetMaxTimestamp(timestamp.end);                                 
                                    m_segments.Insert(segment_start, std::move(segment));
                                    result =
                                        (m_segments.GetSegments().find(segment_start) !=
                                         m_segments.GetSegments().end())
                                            ? kRocProfVisResultSuccess
                                            : kRocProfVisResultMemoryAllocError;
                                }

                                if(result == kRocProfVisResultSuccess)
                                {
                                    std::shared_ptr<Segment>& segment =
                                        m_segments.GetSegments()[segment_start];
                                    segment->SetMinTimestamp(
                                        std::min(segment->GetMinTimestamp(), timestamp.start));
                                    segment->SetMaxTimestamp(std::max(
                                        segment->GetMaxTimestamp(), timestamp.end));
                                    if(current_segment == num_segments.start)
                                    {
                                        segment->Insert(timestamp.start, level, object);
                                    }
                                    else
                                    {
                                        if (object_type == kRPVControllerObjectTypeEvent)
                                        {
                                            Event* event = (Event*) object;
                                            Event* event_duplicate = m_ctx->GetMemoryManager()->NewEvent(event);
                                            segment->Insert(timestamp.start, level, event_duplicate);
                                            //spdlog::debug("Add duplicate event with id = {}", event_id);
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
rocprofvis_result_t Track::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
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

}
}
