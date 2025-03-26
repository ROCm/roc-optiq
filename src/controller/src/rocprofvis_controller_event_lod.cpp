// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_event_lod.h"
#include "rocprofvis_controller_reference.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_event_t, Event, kRPVControllerObjectTypeEvent> EventRef;

EventLOD::EventLOD(uint64_t id, double start_ts, double end_ts)
: Event(id, start_ts, end_ts)
{
}

EventLOD::~EventLOD()
{
}

rocprofvis_controller_object_type_t EventLOD::GetType(void) 
{
    return kRPVControllerObjectTypeEvent;
}

rocprofvis_result_t EventLOD::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerEventNumChildren:
            {
                *value = m_children.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventId:
            case kRPVControllerEventNumCallstackEntries:
            case kRPVControllerEventNumInputFlowControl:
            case kRPVControllerEventNumOutputFlowControl:
            case kRPVControllerEventTrack:
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventName:
            case kRPVControllerEventInputFlowControlIndexed:
            case kRPVControllerEventOutputFlowControlIndexed:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
            default:
            {
                result = Event::GetUInt64(property, index, value);
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t EventLOD::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    return Event::GetDouble(property, index, value);
}
rocprofvis_result_t EventLOD::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerEventChildIndexed:
            {
                if(index < m_children.size())
                {
                    *value = (rocprofvis_handle_t*) m_children[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
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
            case kRPVControllerEventName:
            case kRPVControllerEventInputFlowControlIndexed:
            case kRPVControllerEventOutputFlowControlIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
            default:
            {
                result = Event::GetObject(property, index, value);
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t EventLOD::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    return Event::GetString(property, index, value, length);
}

rocprofvis_result_t EventLOD::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventNumChildren:
        {
            if(value != m_children.size())
            {
                m_children.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerEventId:
        case kRPVControllerEventNumCallstackEntries:
        case kRPVControllerEventNumInputFlowControl:
        case kRPVControllerEventNumOutputFlowControl:
        case kRPVControllerEventTrack:
        case kRPVControllerEventStartTimestamp:
        case kRPVControllerEventEndTimestamp:
        case kRPVControllerEventName:
        case kRPVControllerEventInputFlowControlIndexed:
        case kRPVControllerEventOutputFlowControlIndexed:
        case kRPVControllerEventChildIndexed:
        case kRPVControllerEventCallstackEntryIndexed:
        default:
        {
            result = Event::SetUInt64(property, index, value);
            break;
        }
    }
    return result;
}
rocprofvis_result_t EventLOD::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
{
    return Event::SetDouble(property, index, value);
}
rocprofvis_result_t EventLOD::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerEventChildIndexed:
        {
            if(index < m_children.size())
            {
                EventRef event_ref(value);
                if(event_ref.IsValid())
                {
                    m_children[index] = event_ref.Get();
                    result            = kRocProfVisResultSuccess;
                }
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
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
        case kRPVControllerEventName:
        case kRPVControllerEventInputFlowControlIndexed:
        case kRPVControllerEventOutputFlowControlIndexed:
        case kRPVControllerEventCallstackEntryIndexed:
        default:
        {
            result = Event::SetObject(property, index, value);
            break;
        }
    }
    return result;
}
rocprofvis_result_t EventLOD::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length) 
{
    return Event::SetString(property, index, value, length);
}

}
}
