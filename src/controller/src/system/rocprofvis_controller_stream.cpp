// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_stream.h"
#include "rocprofvis_controller_processor.h"
#include "rocprofvis_controller_queue.h"
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
typedef Reference<rocprofvis_controller_processor_t, Processor,
                  kRPVControllerObjectTypeProcessor>
    ProcessorRef;
typedef Reference<rocprofvis_controller_queue_t, Queue,
                  kRPVControllerObjectTypeQueue>
    QueueRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack>
    TrackRef;

Stream::Stream()
: Handle(__kRPVControllerStreamPropertiesFirst, __kRPVControllerStreamPropertiesLast)
, m_node(nullptr)
, m_process(nullptr)
, m_processor(nullptr)
, m_track(nullptr)
, m_id(0)
{}

Stream::~Stream() {}

rocprofvis_controller_object_type_t Stream::GetType(void)
{
    return kRPVControllerObjectTypeStream;
}

rocprofvis_result_t Stream::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    (void) index;
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
            case kRPVControllerStreamNumQueues:
            {
                *value = m_queues.size();
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


rocprofvis_result_t Stream::GetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerStreamNode:
            {
                *value = (rocprofvis_handle_t*) m_node;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerStreamProcess:
            {
                *value = (rocprofvis_handle_t*) m_process;
                result = kRocProfVisResultSuccess;
                break;
            }
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
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Stream::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                  uint32_t* length)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamName:
        {
            result = GetStdStringImpl(value, length, m_name);
            break;
        }
        case kRPVControllerStreamExtData:
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

rocprofvis_result_t Stream::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerStreamId:
        {
            m_id  = static_cast<uint32_t>(value);
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
        default:
        {
            result = UnhandledProperty(property);
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
            case kRPVControllerStreamNode:
            {
                NodeRef ref(value);
                if(ref.IsValid())
                {
                    m_node = ref.Get();
                    result  = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerStreamProcess:
            {
                ProcessRef ref(value);
                if(ref.IsValid())
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

rocprofvis_result_t Stream::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    (void) index;
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
