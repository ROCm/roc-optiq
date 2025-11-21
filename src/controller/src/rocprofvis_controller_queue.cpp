// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_queue.h"
#include "rocprofvis_controller_processor.h"
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
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack>
    TrackRef;

Queue::Queue()
: Handle(__kRPVControllerQueuePropertiesFirst, __kRPVControllerQueuePropertiesLast)
, m_node(nullptr)
, m_process(nullptr)
, m_processor(nullptr)
, m_track(nullptr)
, m_id(0)
{}

Queue::~Queue() {}

rocprofvis_controller_object_type_t Queue::GetType(void)
{
    return kRPVControllerObjectTypeQueue;
}

rocprofvis_result_t Queue::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerQueueId:
            {
                *value = m_id;
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

rocprofvis_result_t Queue::GetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t** value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerQueueNode:
            {
                *value = (rocprofvis_handle_t*) m_node;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerQueueProcess:
            {
                *value = (rocprofvis_handle_t*) m_process;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerQueueProcessor:
            {
                *value = (rocprofvis_handle_t*) m_processor;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerQueueTrack:
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

rocprofvis_result_t Queue::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                  uint32_t* length)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueName:
        {
            result = GetStdStringImpl(value, length, m_name);
            break;
        }
        case kRPVControllerQueueExtData:
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

rocprofvis_result_t Queue::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueId:
        {
            m_id  = static_cast<uint32_t>(value);
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

rocprofvis_result_t Queue::SetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerQueueProcessor:
            {
                ProcessorRef ref(value);
                if (ref.IsValid())
                {
                    m_processor = ref.Get();
                    result      = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerQueueTrack:
            {
                TrackRef ref(value);
                if (ref.IsValid())
                {
                    m_track = ref.Get();
                    result      = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerQueueNode:
            {
                NodeRef ref(value);
                if (ref.IsValid())
                {
                    m_node = ref.Get();
                    result      = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerQueueProcess:
            {
                ProcessRef ref(value);
                if (ref.IsValid())
                {
                    m_process = ref.Get();
                    result      = kRocProfVisResultSuccess;
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

rocprofvis_result_t Queue::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerQueueName:
        {
            m_name = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerQueueExtData:
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
