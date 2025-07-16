// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_stream.h"
#include "rocprofvis_controller_processor.h"
#include "rocprofvis_controller_queue.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_processor_t, Processor,
                  kRPVControllerObjectTypeProcessor>
    ProcessorRef;
typedef Reference<rocprofvis_controller_queue_t, Queue,
                  kRPVControllerObjectTypeQueue>
    QueueRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack>
    TrackRef;

Stream::Stream()
: m_processor(nullptr)
, m_track(nullptr)
, m_id(0)
, m_node_id(0)
, m_process_id(0)
{

}

Stream::~Stream() {

}

rocprofvis_controller_object_type_t Stream::GetType(void)
{
    return kRPVControllerObjectTypeStream;
}

rocprofvis_result_t Stream::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerStreamId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamProcessId:
            {
                *value = m_process_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamNumQueues:
            {
                *value = m_queues.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamName:
            case kRPVControllerStreamExtData:
            case kRPVControllerStreamProcessor:
            case kRPVControllerStreamQueueIndexed:
            case kRPVControllerStreamTrack:
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

rocprofvis_result_t Stream::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerStreamId:
            case kRPVControllerStreamNodeId:
            case kRPVControllerStreamProcessId:
            case kRPVControllerStreamName:
            case kRPVControllerStreamExtData:
            case kRPVControllerStreamProcessor:
            case kRPVControllerStreamNumQueues:
            case kRPVControllerStreamQueueIndexed:
            case kRPVControllerStreamTrack:
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

rocprofvis_result_t Stream::GetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerStreamProcessor:
            {
                *value = (rocprofvis_handle_t*) m_processor;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamQueueIndexed:
            {
                if(index < m_queues.size())
                {
                    *value = (rocprofvis_handle_t*) m_queues[index];
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerStreamTrack:
            {
                *value = (rocprofvis_handle_t*) m_track;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamId:
            case kRPVControllerStreamNodeId:
            case kRPVControllerStreamProcessId:
            case kRPVControllerStreamName:
            case kRPVControllerStreamExtData:
            case kRPVControllerStreamNumQueues:
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

rocprofvis_result_t Stream::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                  uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamName:
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
        case kRPVControllerStreamExtData:
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
        case kRPVControllerStreamTrack:
        case kRPVControllerStreamId:
        case kRPVControllerStreamNodeId:
        case kRPVControllerStreamProcessId:
        case kRPVControllerStreamProcessor:
        case kRPVControllerStreamQueueIndexed:
        case kRPVControllerStreamNumQueues:
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

rocprofvis_result_t Stream::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamId:
        {
            m_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamNodeId:
        {
            m_node_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamProcessId:
        {
            m_process_id  = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamNumQueues:
        {
            if (value > m_queues.size())
            {
                m_queues.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamTrack:
        case kRPVControllerStreamName:
        case kRPVControllerStreamExtData:
        case kRPVControllerStreamProcessor:
        case kRPVControllerStreamQueueIndexed:
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

rocprofvis_result_t Stream::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamTrack:
        case kRPVControllerStreamId:
        case kRPVControllerStreamNodeId:
        case kRPVControllerStreamProcessId:
        case kRPVControllerStreamName:
        case kRPVControllerStreamExtData:
        case kRPVControllerStreamProcessor:
        case kRPVControllerStreamQueueIndexed:
        case kRPVControllerStreamNumQueues:
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

rocprofvis_result_t Stream::SetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerStreamTrack:
            {
                TrackRef ref(value);
                if(ref.IsValid())
                {
                    m_track = ref.Get();
                    result  = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerStreamProcessor:
            {
                ProcessorRef ref(value);
                if(ref.IsValid())
                {
                    m_processor = ref.Get();
                    result      = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerStreamQueueIndexed:
            {
                QueueRef ref(value);
                if(ref.IsValid() && index < m_queues.size())
                {
                    m_queues[index] = ref.Get();
                    result      = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerStreamId:
            case kRPVControllerStreamNodeId:
            case kRPVControllerStreamProcessId:
            case kRPVControllerStreamName:
            case kRPVControllerStreamExtData:
            case kRPVControllerStreamNumQueues:
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

rocprofvis_result_t Stream::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                  uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamName:
        {
            m_name = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamExtData:
        {
            m_ext_data = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerStreamTrack:
        case kRPVControllerStreamId:
        case kRPVControllerStreamNodeId:
        case kRPVControllerStreamProcessId:
        case kRPVControllerStreamProcessor:
        case kRPVControllerStreamQueueIndexed:
        case kRPVControllerStreamNumQueues:
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
