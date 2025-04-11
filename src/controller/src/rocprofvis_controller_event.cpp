// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include <cstring>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;

Event::Event(uint64_t id, double start_ts, double end_ts)
: m_id(id)
, m_track(nullptr)
, m_start_timestamp(start_ts)
, m_end_timestamp(end_ts)
, m_name("")
, m_category("")
{
}

Event::~Event()
{ 
#ifdef JSON_SUPPORT
    if (m_track && m_track->GetDmHandle() == nullptr && strlen(m_name) > 0)
    {
        free((char*)m_name);
    }
#endif
}

rocprofvis_controller_object_type_t Event::GetType(void) 
{
    return kRPVControllerObjectTypeEvent;
}

rocprofvis_result_t Event::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Event);
                *value += strlen(m_name) + 1;
                *value += strlen(m_category) + 1;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumCallstackEntries:
            {
                // todo : direct data model access
                *value = 0;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumInputFlowControl:
            {
                // todo : direct data model access
                *value = 0;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumOutputFlowControl:
            {
                // todo : direct data model access
                *value = 0;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumChildren:
            {
                *value = 0;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventTrack:
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventName:
            case kRPVControllerEventInputFlowControlIndexed:
            case kRPVControllerEventOutputFlowControlIndexed:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
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
rocprofvis_result_t Event::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerEventStartTimestamp:
            {
                *value = m_start_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventEndTimestamp:
            {
                *value = m_end_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventId:
            case kRPVControllerEventNumCallstackEntries:
            case kRPVControllerEventNumInputFlowControl:
            case kRPVControllerEventNumOutputFlowControl:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventTrack:
            case kRPVControllerEventName:
            case kRPVControllerEventInputFlowControlIndexed:
            case kRPVControllerEventOutputFlowControlIndexed:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
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
rocprofvis_result_t Event::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerEventTrack:
            {
                *value = (rocprofvis_handle_t*)m_track;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventInputFlowControlIndexed:
            {
                // todo : direct data model access
                result = kRocProfVisResultOutOfRange;
                break;
            }
            case kRPVControllerEventOutputFlowControlIndexed:
            {
                // todo : direct data model access
                result = kRocProfVisResultOutOfRange;
                break;
            }
            case kRPVControllerEventCallstackEntryIndexed:
            {
                // todo : direct data model access
                result = kRocProfVisResultOutOfRange;

                break;
            }
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventId:
            case kRPVControllerEventNumCallstackEntries:
            case kRPVControllerEventNumInputFlowControl:
            case kRPVControllerEventNumOutputFlowControl:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventName:
            case kRPVControllerEventChildIndexed:
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
rocprofvis_result_t Event::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventName:
        {
            if (length && (!value || *length == 0))
            {
                *length = strlen(m_name) + strlen(m_category)+1;
                result = kRocProfVisResultSuccess;
            }
            else if (length && value && *length > 0)
            {
                std::string full_name = m_category;
                full_name += " ";
                full_name += m_name; 
                strncpy(value, full_name.c_str(), *length);
       
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
            break;
        }
        case kRPVControllerEventCategory:
        {
            if (length && (!value || *length == 0))
            {
                *length = strlen(m_category);
                result = kRocProfVisResultSuccess;
            }
            else if (length && value && *length > 0)
            {
                strncpy(value, m_category, *length);
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
            break;
        }
        case kRPVControllerEventTrack:
        case kRPVControllerEventStartTimestamp:
        case kRPVControllerEventEndTimestamp:
        case kRPVControllerEventId:
        case kRPVControllerEventNumCallstackEntries:
        case kRPVControllerEventNumInputFlowControl:
        case kRPVControllerEventNumOutputFlowControl:
        case kRPVControllerEventNumChildren:
        case kRPVControllerEventInputFlowControlIndexed:
        case kRPVControllerEventOutputFlowControlIndexed:
        case kRPVControllerEventChildIndexed:
        case kRPVControllerEventCallstackEntryIndexed:
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

rocprofvis_result_t Event::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventId:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerEventNumCallstackEntries:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        case kRPVControllerEventNumInputFlowControl:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        case kRPVControllerEventNumOutputFlowControl:
        {
            result = kRocProfVisResultInvalidType;
            break;
        }
        case kRPVControllerEventNumChildren:
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
        case kRPVControllerEventTrack:
        case kRPVControllerEventStartTimestamp:
        case kRPVControllerEventEndTimestamp:
        case kRPVControllerEventName:
        case kRPVControllerEventInputFlowControlIndexed:
        case kRPVControllerEventOutputFlowControlIndexed:
        case kRPVControllerEventChildIndexed:
        case kRPVControllerEventCallstackEntryIndexed:
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
rocprofvis_result_t Event::SetDouble(rocprofvis_property_t property, uint64_t index, double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventStartTimestamp:
        {
            m_start_timestamp = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerEventEndTimestamp:
        {
            m_end_timestamp = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerEventId:
        case kRPVControllerEventNumCallstackEntries:
        case kRPVControllerEventNumInputFlowControl:
        case kRPVControllerEventNumOutputFlowControl:
        case kRPVControllerEventNumChildren:
        case kRPVControllerEventTrack:
        case kRPVControllerEventName:
        case kRPVControllerEventInputFlowControlIndexed:
        case kRPVControllerEventOutputFlowControlIndexed:
        case kRPVControllerEventChildIndexed:
        case kRPVControllerEventCallstackEntryIndexed:
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
rocprofvis_result_t Event::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerEventTrack:
            {
                TrackRef track_ref(value);
                if(track_ref.IsValid())
                {
                    m_track = track_ref.Get();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerEventInputFlowControlIndexed:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            case kRPVControllerEventOutputFlowControlIndexed:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            case kRPVControllerEventCallstackEntryIndexed:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventId:
            case kRPVControllerEventNumCallstackEntries:
            case kRPVControllerEventNumInputFlowControl:
            case kRPVControllerEventNumOutputFlowControl:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventName:
            case kRPVControllerEventChildIndexed:
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
rocprofvis_result_t Event::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerEventName:
            {
#ifdef JSON_SUPPORT
                if(m_track && m_track->GetDmHandle()==nullptr)
                {
                    if(length > 0)
                    {
                        char* name = (char*) calloc(length + 1, 1);
                        if(name)
                        {
                            strcpy(name, value);
                            m_name = name;
                        }
                    }
                }
                else
#endif
                {
                    m_name = value;
                }
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventCategory:
            {
                m_category = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventTrack:
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventId:
            case kRPVControllerEventNumCallstackEntries:
            case kRPVControllerEventNumInputFlowControl:
            case kRPVControllerEventNumOutputFlowControl:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventInputFlowControlIndexed:
            case kRPVControllerEventOutputFlowControlIndexed:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
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
