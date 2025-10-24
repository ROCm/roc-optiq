// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_thread.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_node.h"
#include "rocprofvis_controller_process.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_process_t, Process, kRPVControllerObjectTypeProcess> ProcessRef;
typedef Reference<rocprofvis_controller_node_t, Node, kRPVControllerObjectTypeNode> NodeRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack>
    TrackRef;

Thread::Thread()
: Handle(__kRPVControllerThreadPropertiesFirst, __kRPVControllerThreadPropertiesLast)
, m_start_time(0)
, m_end_time(0)
, m_node(nullptr)
, m_process(nullptr)
, m_track(nullptr)
, m_id(0)
, m_tid(0)
, m_parent_id(0)
, m_thread_type(kRPVControllerThreadTypeUndefined)
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
    (void) index;
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
            case kRPVControllerThreadType:
            {
                *value = m_thread_type;
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
Thread::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    (void) index;
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
Thread::GetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t** value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerThreadNode:
            {
                *value = (rocprofvis_handle_t*) m_node;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadProcess:
            {
                *value = (rocprofvis_handle_t*) m_process;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerThreadTrack:
            {
                *value = (rocprofvis_handle_t*) m_track;
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
Thread::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                  uint32_t* length)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadName:
        {
            result = GetStdStringImpl(value, length, m_name);
            break;
        }
        case kRPVControllerThreadExtData:
        {
            result = GetStdStringImpl(value, length, m_ext_data);
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

rocprofvis_result_t
Thread::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerThreadId:
        {
            m_id   = static_cast<uint32_t>(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadParentId:
        {
            m_parent_id = static_cast<uint32_t>(value);;
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadTid:
        {
            m_tid  = static_cast<uint32_t>(value);;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerThreadType:
        {
            m_thread_type = (rocprofvis_controller_thread_type_t)value;
            result = kRocProfVisResultSuccess;
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

rocprofvis_result_t
Thread::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    (void) index;
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
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

rocprofvis_result_t
Thread::SetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t* value)
{
    (void) index;
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
            case kRPVControllerThreadNode:
            {
                NodeRef ref(value);
                if (ref.IsValid())
                {
                    m_node = ref.Get();
                    result  = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerThreadProcess:
            {
                ProcessRef ref(value);
                if (ref.IsValid())
                {
                    m_process = ref.Get();
                    result  = kRocProfVisResultSuccess;
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

rocprofvis_result_t
Thread::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    (void) index;
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
        default:
        {
            result = UnhandledProperty(property);
            break;
        }
    }
    return result;
}

}
}
