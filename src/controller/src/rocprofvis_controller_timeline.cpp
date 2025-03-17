// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_timeline.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_sample.h"

#include "rocprofvis_trace.h"

#include <cassert>

namespace RocProfVis
{
namespace Controller
{

Timeline::Timeline()
: m_id(0)
{
}

Timeline::~Timeline()
{
    for(Track* track : m_tracks)
    {
        delete track;
    }
}

rocprofvis_result_t Timeline::AsyncFetch(Track& track, Future& future, Array& array,
                                double start, double end)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;

    future.Set(std::async(
        std::launch::async, [&track, &array, start, end]() -> rocprofvis_result_t {
            rocprofvis_result_t result = kRocProfVisResultUnknownError;
            uint64_t            index  = 0;
            result                     = track.Fetch(0, start, end, array, index);
            return result;
        }));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t Timeline::Load(char const* const filename, Future& future)
{
    assert(filename && strlen(filename));

    rocprofvis_result_t result   = kRocProfVisResultInvalidArgument;
    std::string         filepath = filename;
    future.Set(
        std::async(std::launch::async, [this, filepath]() -> rocprofvis_result_t {
            rocprofvis_result_t     result = kRocProfVisResultUnknownError;
            rocprofvis_trace_data_t trace_object;
            std::future<bool>       future = rocprofvis_trace_async_load_json_trace(
                filepath.c_str(), trace_object);
            if(future.valid())
            {
                future.wait();
                result = future.get() ? kRocProfVisResultSuccess
                                        : kRocProfVisResultUnknownError;

                if(result == kRocProfVisResultSuccess)
                {
                    uint64_t event_id  = 0;
                    uint64_t sample_id = 0;
                    uint64_t track_id  = 0;
                    for(auto& pair : trace_object.m_trace_data)
                    {
                        std::string const& name = pair.first;
                        auto const&        data = pair.second;

                        for(auto& thread : data.m_threads)
                        {
                            auto   type  = thread.second.m_events.size()
                                                ? kRPVControllerTrackTypeEvents
                                                : kRPVControllerTrackTypeSamples;
                            Track* track = new Track(type);
                            track->SetString(kRPVControllerTrackName, 0,
                                                thread.first.c_str(),
                                                thread.first.length());
                            track->SetUInt64(kRPVControllerTrackId, 0, track_id++);
                            switch(type)
                            {
                                case kRPVControllerTrackTypeEvents:
                                {
                                    track->SetUInt64(
                                        kRPVControllerTrackNumberOfEntries, 0,
                                        thread.second.m_events.size());

                                    double min_ts = DBL_MAX;
                                    double max_ts = DBL_MIN;
                                    for(auto& event : thread.second.m_events)
                                    {
                                        min_ts = std::min(event.m_start_ts, min_ts);
                                        max_ts = std::max(event.m_start_ts +
                                                                event.m_duration,
                                                            max_ts);
                                    }
                                    track->SetDouble(kRPVControllerTrackMinTimestamp,
                                                        0, min_ts);
                                    track->SetDouble(kRPVControllerTrackMaxTimestamp,
                                                        0, max_ts);

                                    uint64_t index = 0;
                                    for(auto& event : thread.second.m_events)
                                    {
                                        Event* new_event = new Event(
                                            event_id++, event.m_start_ts,
                                            event.m_start_ts + event.m_duration);
                                        if(new_event)
                                        {
                                            result = new_event->SetObject(
                                                kRPVControllerEventTrack, 0,
                                                (rocprofvis_handle_t*) track);
                                            assert(result ==
                                                    kRocProfVisResultSuccess);

                                            result = new_event->SetString(
                                                kRPVControllerEventName, 0,
                                                event.m_name.c_str(),
                                                event.m_name.length());
                                            assert(result ==
                                                    kRocProfVisResultSuccess);

                                            result = track->SetObject(
                                                kRPVControllerTrackEntry, index++,
                                                (rocprofvis_handle_t*) new_event);
                                            assert(result ==
                                                    kRocProfVisResultSuccess);
                                        }
                                    }
                                    m_tracks.push_back(track);
                                    break;
                                }
                                case kRPVControllerTrackTypeSamples:
                                {
                                    track->SetUInt64(
                                        kRPVControllerTrackNumberOfEntries, 0,
                                        thread.second.m_events.size());

                                    double min_ts = DBL_MAX;
                                    double max_ts = DBL_MIN;
                                    for(auto& sample : thread.second.m_counters)
                                    {
                                        min_ts = std::min(min_ts, sample.m_start_ts);
                                        max_ts = std::max(max_ts, sample.m_start_ts);
                                    }
                                    track->SetDouble(kRPVControllerTrackMinTimestamp,
                                                        0, min_ts);
                                    track->SetDouble(kRPVControllerTrackMaxTimestamp,
                                                        0, max_ts);

                                    uint64_t index = 0;
                                    for(auto& sample : thread.second.m_counters)
                                    {
                                        Sample* new_sample = new Sample(
                                            kRPVControllerPrimitiveTypeDouble,
                                            sample_id++, sample.m_start_ts);
                                        new_sample->SetObject(
                                            kRPVControllerSampleTrack, 0,
                                            (rocprofvis_handle_t*) track);
                                        new_sample->SetDouble(
                                            kRPVControllerSampleValue, 0,
                                            sample.m_value);
                                        track->SetObject(
                                            kRPVControllerTrackEntry, index++,
                                            (rocprofvis_handle_t*) new_sample);
                                    }
                                    m_tracks.push_back(track);
                                    break;
                                }
                                default:
                                {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
            return result;
        }));

    if(future.IsValid())
    {
        result = kRocProfVisResultSuccess;
    }

    return result;
}

rocprofvis_controller_object_type_t Timeline::GetType(void) 
{
    return kRPVControllerObjectTypeTimeline;
}

rocprofvis_result_t Timeline::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTimelineId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTimelineNumTracks:
            {
                *value = m_tracks.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTimelineMinTimestamp:
            case kRPVControllerTimelineMaxTimestamp:
            case kRPVControllerTimelineTrackIndexed:
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
rocprofvis_result_t Timeline::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTimelineMinTimestamp:
            {
                if(m_tracks.size())
                {
                    result = m_tracks.front()->GetDouble(
                        kRPVControllerTrackMinTimestamp, 0, value);
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerTimelineMaxTimestamp:
            {
                if(m_tracks.size())
                {
                    result = m_tracks.back()->GetDouble(
                        kRPVControllerTrackMaxTimestamp, 0, value);
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerTimelineId:
            case kRPVControllerTimelineNumTracks:
            case kRPVControllerTimelineTrackIndexed:
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
rocprofvis_result_t Timeline::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTimelineTrackIndexed:
            {
                if(index < m_tracks.size())
                {
                    *value = (rocprofvis_handle_t*) m_tracks[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerTimelineId:
            case kRPVControllerTimelineNumTracks:
            case kRPVControllerTimelineMinTimestamp:
            case kRPVControllerTimelineMaxTimestamp:
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
rocprofvis_result_t Timeline::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTimelineId:
            case kRPVControllerTimelineNumTracks:
            case kRPVControllerTimelineMinTimestamp:
            case kRPVControllerTimelineMaxTimestamp:
            case kRPVControllerTimelineTrackIndexed:
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

rocprofvis_result_t Timeline::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}
rocprofvis_result_t Timeline::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}
rocprofvis_result_t Timeline::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}
rocprofvis_result_t Timeline::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length) 
{
    assert(0);
    return kRocProfVisResultReadOnlyError;
}

}
}
