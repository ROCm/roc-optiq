// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_thread.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack>
    TrackRef;

Thread::Thread()
: m_start_time(0)
, m_end_time(0)
, m_track(nullptr)
, m_id(0)
, m_tid(0)
, m_node_id(0)
, m_process_id(0)
, m_parent_id(0)
{}

Thread::~Thread() {}

rocprofvis_controller_object_type_t
Thread::GetType(void)
{
    return kRPVControllerObjectTypeThread;
}

rocprofvis_result_t
Thread::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerThreadId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadProcessId:
            {
                *value = m_process_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadParentId:
            {
                *value = m_parent_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadTid:
            {
                *value = m_tid;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadName:
            case kRPVControllerThreadExtData:
            case kRPVControllerThreadStartTime:
            case kRPVControllerThreadEndTime:
            case kRPVControllerThreadTrack:
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
Thread::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerThreadStartTime:
            {
                *value = m_start_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadEndTime:
            {
                *value = m_end_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadId:
            case kRPVControllerThreadNodeId:
            case kRPVControllerThreadProcessId:
            case kRPVControllerThreadParentId:
            case kRPVControllerThreadTid:
            case kRPVControllerThreadName:
            case kRPVControllerThreadExtData:
            case kRPVControllerThreadTrack:
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
Thread::GetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerThreadTrack:
            {
                *value = (rocprofvis_handle_t*) m_track;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadId:
            case kRPVControllerThreadNodeId:
            case kRPVControllerThreadProcessId:
            case kRPVControllerThreadParentId:
            case kRPVControllerThreadTid:
            case kRPVControllerThreadName:
            case kRPVControllerThreadExtData:
            case kRPVControllerThreadStartTime:
            case kRPVControllerThreadEndTime:
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
Thread::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                  uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerThreadExtData:
        {
            if(value && length && *length)
            {
                strncpy(value, m_ext_data.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_ext_data.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerThreadId:
        case kRPVControllerThreadNodeId:
        case kRPVControllerThreadProcessId:
        case kRPVControllerThreadParentId:
        case kRPVControllerThreadTid:
        case kRPVControllerThreadStartTime:
        case kRPVControllerThreadEndTime:
        case kRPVControllerThreadTrack:
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
Thread::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadId:
        {
            m_id   = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadNodeId:
        {
            m_node_id = value;
            result    = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadProcessId:
        {
            m_process_id = value;
            result       = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadParentId:
        {
            m_parent_id = value;
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadTid:
        {
            m_tid  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadName:
        case kRPVControllerThreadExtData:
        case kRPVControllerThreadStartTime:
        case kRPVControllerThreadEndTime:
        case kRPVControllerThreadTrack:
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
Thread::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadStartTime:
        {
            m_start_time = value;
            result       = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadEndTime:
        {
            m_end_time = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadId:
        case kRPVControllerThreadNodeId:
        case kRPVControllerThreadProcessId:
        case kRPVControllerThreadParentId:
        case kRPVControllerThreadTid:
        case kRPVControllerThreadName:
        case kRPVControllerThreadExtData:
        case kRPVControllerThreadTrack:
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
Thread::SetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerThreadTrack:
            {
                TrackRef ref(value);
                if (ref.IsValid())
                {
                    m_track = ref.Get();
                    result  = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerThreadId:
            case kRPVControllerThreadNodeId:
            case kRPVControllerThreadProcessId:
            case kRPVControllerThreadParentId:
            case kRPVControllerThreadTid:
            case kRPVControllerThreadName:
            case kRPVControllerThreadExtData:
            case kRPVControllerThreadStartTime:
            case kRPVControllerThreadEndTime:
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
Thread::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                  uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadName:
        {
            m_name = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadExtData:
        {
            m_ext_data = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadId:
        case kRPVControllerThreadNodeId:
        case kRPVControllerThreadProcessId:
        case kRPVControllerThreadParentId:
        case kRPVControllerThreadTid:
        case kRPVControllerThreadStartTime:
        case kRPVControllerThreadEndTime:
        case kRPVControllerThreadTrack:
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

}
}
