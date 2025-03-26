// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_track.h"

#include <cassert>
#include <algorithm>

namespace RocProfVis
{
namespace Controller
{

Track::Track(rocprofvis_controller_track_type_t type, uint64_t id)
: m_id(id)
, m_num_elements(0)
, m_type(type)
, m_start_timestamp(DBL_MIN)
, m_end_timestamp(DBL_MAX)
{

}

Track::~Track()
{
    for (auto& pair : m_segments)
    {
        delete pair.second;
    }
}

rocprofvis_result_t Track::Fetch(uint32_t lod, double start, double end, Array& array, uint64_t& index)
{
    rocprofvis_result_t result = kRocProfVisResultOutOfRange;
    if((start <= m_start_timestamp && end >= m_end_timestamp) || (start >= m_start_timestamp && start < m_end_timestamp) || (end > m_start_timestamp && end <= m_end_timestamp))
    {
        auto lower = std::lower_bound(m_segments.begin(), m_segments.end(), std::max(m_start_timestamp, start), [](std::pair<double, Segment*> const& pair, double const& start) -> bool
        {
            bool result = (pair.second->GetMaxTimestamp() < start);
            return result;
        });
        auto upper = std::upper_bound(m_segments.begin(), m_segments.end(), std::min(m_end_timestamp, end), [](double const& end, std::pair<double, Segment*> const& pair) -> bool
        {
            bool result = (end <= pair.first);
            return result;
        });
        while (lower != upper)
        {
            result = lower->second->Fetch(lod, start, end, array, index);
            if (result == kRocProfVisResultSuccess)
            {
                ++lower;
            }
            else if (result == kRocProfVisResultOutOfRange)
            {
                ++lower;
            }
            else
            {
                break;
            }
        }
    }
    return result;
}

rocprofvis_controller_object_type_t Track::GetType(void)
{
    return kRPVControllerObjectTypeTrack;
}

rocprofvis_result_t Track::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (value)
    {
        switch(property)
        {
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
            case kRPVControllerTrackMinTimestamp:
            case kRPVControllerTrackMaxTimestamp:
            case kRPVControllerTrackEntry:
            case kRPVControllerTrackName:
            default:
            {
                result = kRocProfVisResultNotSupported;
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
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
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
        case kRPVControllerTrackMinTimestamp:
        case kRPVControllerTrackMaxTimestamp:
        case kRPVControllerTrackId:
        case kRPVControllerTrackType:
        case kRPVControllerTrackNumberOfEntries:
        case kRPVControllerTrackEntry:
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
            break;
        }
        case kRPVControllerTrackMaxTimestamp:
        {
            m_end_timestamp = value;
            break;
        }
        case kRPVControllerTrackEntry:
        case kRPVControllerTrackId:
        case kRPVControllerTrackType:
        case kRPVControllerTrackNumberOfEntries:
        case kRPVControllerTrackName:
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
                assert(m_start_timestamp >= 0.0 && m_start_timestamp < m_end_timestamp);

                Handle* object = (Handle*)value;
                auto object_type = object->GetType();
                if (((m_type == kRPVControllerTrackTypeEvents) && (object_type == kRPVControllerObjectTypeEvent))
                    || ((m_type == kRPVControllerTrackTypeSamples) && (object_type == kRPVControllerObjectTypeSample)))
                {
                    rocprofvis_property_t property;
                    if (object_type == kRPVControllerObjectTypeEvent)
                    {
                        property = kRPVControllerEventStartTimestamp;
                    }
                    else
                    {
                        property = kRPVControllerSampleTimestamp;
                    }

                    double timestamp = 0;
                    result = object->GetDouble(property, 0, &timestamp);
                    if (result == kRocProfVisResultSuccess)
                    {
                        if (timestamp >= m_start_timestamp && timestamp <= m_end_timestamp)
                        {
                            double segment_duration = 10000.0;
                            double relative = (timestamp - m_start_timestamp);
                            double num_segments = floor(relative / segment_duration);
                            double segment_start = m_start_timestamp + (num_segments * segment_duration);

                            if (m_segments.find(segment_start) == m_segments.end())
                            {
                                double segment_end = segment_start + segment_duration;
                                Segment* segment = new Segment(m_type);
                                segment->SetStartEndTimestamps(segment_start, segment_end);
                                segment->SetMinTimestamp(timestamp); 
                                if (object_type == kRPVControllerObjectTypeEvent)
                                {
                                    property = kRPVControllerEventStartTimestamp;
                                    double end_timestamp = timestamp;
                                    object->GetDouble(kRPVControllerEventEndTimestamp, 0, &end_timestamp);
                                    segment->SetMaxTimestamp(end_timestamp);
                                }
                                else
                                {
                                    segment->SetMaxTimestamp(timestamp);
                                }
                                m_segments.insert(std::make_pair(segment_start, segment));
                                result = (m_segments.find(segment_start) != m_segments.end()) ? kRocProfVisResultSuccess : kRocProfVisResultMemoryAllocError;
                            }

                            if (result == kRocProfVisResultSuccess)
                            {
                                Segment* segment = m_segments[segment_start];
                                segment->SetMinTimestamp(std::min(segment->GetMinTimestamp(), timestamp));
                                double end_timestamp = timestamp;
                                if (object_type == kRPVControllerObjectTypeEvent)
                                {   
                                    object->GetDouble(kRPVControllerEventEndTimestamp, 0, &end_timestamp); 
                                }
                                segment->SetMaxTimestamp(std::max(segment->GetMaxTimestamp(), end_timestamp));
                                segment->Insert(timestamp, object);
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
