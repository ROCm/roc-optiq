// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_event.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;

Event::Event(uint64_t id, double start_ts, double end_ts)
: m_id(id)
, m_start_timestamp(start_ts)
, m_end_timestamp(end_ts)
{
}

Event::~Event()
{

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
            case kRPVControllerEventId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumCallstackEntries:
            {
                *value = m_callstack.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumInputFlowControl:
            {
                *value = m_input_flow_control.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumOutputFlowControl:
            {
                *value = m_output_flow_control.size();
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
                if (index < m_input_flow_control.size())
                {
                    *value = (rocprofvis_handle_t*)m_input_flow_control[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerEventOutputFlowControlIndexed:
            {
                if (index < m_output_flow_control.size())
                {
                    *value = (rocprofvis_handle_t*)m_output_flow_control[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerEventCallstackEntryIndexed:
            {
                if (index < m_callstack.size())
                {
                    *value = (rocprofvis_handle_t*)m_callstack[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
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
                *length = m_name.length();
                result = kRocProfVisResultSuccess;
            }
            else if (length && value && *length > 0)
            {
                strncpy(value, m_name.c_str(), *length);
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
            if (value != m_callstack.size())
            {
                m_callstack.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerEventNumInputFlowControl:
        {
            if (value != m_input_flow_control.size())
            {
                m_input_flow_control.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerEventNumOutputFlowControl:
        {
            if (value != m_output_flow_control.size())
            {
                m_output_flow_control.resize(value);
            }
            result = kRocProfVisResultSuccess;
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
                Handle* handle = (Handle*)value;
                if (handle->GetType() == kRPVControllerObjectTypeFlowControl)
                {
                    if (index < m_input_flow_control.size())
                    {
                        m_input_flow_control[index] = (FlowControl*)value;
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                }
                break;
            }
            case kRPVControllerEventOutputFlowControlIndexed:
            {
                Handle* handle = (Handle*)value;
                if (handle->GetType() == kRPVControllerObjectTypeFlowControl)
                {
                    if (index < m_output_flow_control.size())
                    {
                        m_output_flow_control[index] = (FlowControl*)value;
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                }
                break;
            }
            case kRPVControllerEventCallstackEntryIndexed:
            {
                Handle* handle = (Handle*)value;
                if (handle->GetType() == kRPVControllerObjectTypeCallstack)
                {
                    if (index < m_callstack.size())
                    {
                        m_callstack[index] = (Callstack*)value;
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                }
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
                m_name = value;
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
