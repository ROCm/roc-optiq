// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_processor.h"
#include "rocprofvis_controller_stream.h"
#include "rocprofvis_controller_queue.h"
#include "rocprofvis_controller_counter.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_queue_t, Queue, kRPVControllerObjectTypeQueue>
    QueueRef;
typedef Reference<rocprofvis_controller_stream_t, Stream, kRPVControllerObjectTypeStream>
    StreamRef;
typedef Reference<rocprofvis_controller_counter_t, Counter, kRPVControllerObjectTypeCounter>
    CounterRef;

Processor::Processor()
: Handle(__kRPVControllerProcessorPropertiesFirst,
         __kRPVControllerProcessorPropertiesLast)
, m_type(kRPVControllerProcessorTypeUndefined)
{}

Processor::~Processor() {}

rocprofvis_controller_object_type_t
Processor::GetType(void)
{
    return kRPVControllerObjectTypeProcessor;
}

rocprofvis_result_t
Processor::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessorId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorTypeIndex:
            {
                *value = m_type_index;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorIndex:
            {
                *value = m_index;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorLogicalIndex:
            {
                *value = m_logical_index;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorNumQueues:
            {
                *value = m_queues.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorNumStreams:
            {
                *value = m_streams.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorNumCounters:
            {
                *value = m_counters.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorType:
            {
                *value = static_cast<uint64_t>(m_type);
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
Processor::GetObject(rocprofvis_property_t property, uint64_t index,
                     rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessorQueueIndexed:
            {
                if(index < m_queues.size())
                {
                    *value = (rocprofvis_handle_t*) m_queues[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerProcessorStreamIndexed:
            {
                if(index < m_streams.size())
                {
                    *value = (rocprofvis_handle_t*) m_streams[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerProcessorCounterIndexed:
            {
                if(index < m_counters.size())
                {
                    *value = (rocprofvis_handle_t*) m_counters[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
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
Processor::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                     uint32_t* length)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorName:
        {
            result = GetStdStringImpl(value, length, m_name);
            break;
        }
        case kRPVControllerProcessorModelName:
        {
            result = GetStdStringImpl(value, length, m_model_name);
            break;
        }
        case kRPVControllerProcessorUserName:
        {
            result = GetStdStringImpl(value, length, m_user_name);
            break;
        }
        case kRPVControllerProcessorVendorName:
        {
            result = GetStdStringImpl(value, length, m_vendor_name);
            break;
        }
        case kRPVControllerProcessorProductName:
        {
            result = GetStdStringImpl(value, length, m_product_name);
            break;
        }
        case kRPVControllerProcessorExtData:
        {
            result = GetStdStringImpl(value, length, m_ext_data);
            break;
        }
        case kRPVControllerProcessorUUID:
        {
            result = GetStdStringImpl(value, length, m_uuid);
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
Processor::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessorId:
        {
            m_id   = static_cast<uint32_t>(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorTypeIndex:
        {
            m_type_index = static_cast<uint32_t>(value);
            result       = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorIndex:
        {
            m_index = static_cast<uint32_t>(value);
            result  = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorLogicalIndex:
        {
            m_logical_index = static_cast<uint32_t>(value);
            result          = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorNodeId:
        {
            m_node_id = value;
            result    = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessorNumQueues:
        {
            if(m_queues.size() != value)
            {
                for(uint64_t i = value; i < m_queues.size(); i++)
                {
                    delete m_queues[i];
                    m_queues[i] = nullptr;
                }
                m_queues.resize(value);
                result = m_queues.size() == value ? kRocProfVisResultSuccess
                                                  : kRocProfVisResultMemoryAllocError;
            }
            else
            {
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorNumStreams:
        {
            if(m_streams.size() != value)
            {
                for(uint64_t i = value; i < m_streams.size(); i++)
                {
                    delete m_streams[i];
                    m_streams[i] = nullptr;
                }
                m_streams.resize(value);
                result = m_streams.size() == value ? kRocProfVisResultSuccess
                                                  : kRocProfVisResultMemoryAllocError;
            }
            else
            {
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorNumCounters:
        {
            if(m_counters.size() != value)
            {
                for(uint64_t i = value; i < m_counters.size(); i++)
                {
                    delete m_counters[i];
                    m_counters[i] = nullptr;
                }
                m_counters.resize(value);
                result = m_counters.size() == value ? kRocProfVisResultSuccess
                                                   : kRocProfVisResultMemoryAllocError;
            }
            else
            {
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerProcessorType:
        {
            m_type = static_cast<rocprofvis_controller_processor_type_t>(value);
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
Processor::SetObject(rocprofvis_property_t property, uint64_t index,
                     rocprofvis_handle_t* value)

{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessorQueueIndexed:
            {
                if (index < m_queues.size())
                {
                    QueueRef ref(value);
                    if(ref.IsValid())
                    {
                        m_queues[index] = ref.Get();
                        result = kRocProfVisResultSuccess;
                    }
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerProcessorStreamIndexed:
            {
                if(index < m_streams.size())
                {
                    StreamRef ref(value);
                    if(ref.IsValid())
                    {
                        m_streams[index] = ref.Get();
                        result          = kRocProfVisResultSuccess;
                    }
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerProcessorCounterIndexed:
            {
                if(index < m_counters.size())
                {
                    CounterRef ref(value);
                    if(ref.IsValid())
                    {
                        m_counters[index] = ref.Get();
                        result           = kRocProfVisResultSuccess;
                    }
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
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
Processor::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && *value != 0)
    {
        switch(property)
        {
            case kRPVControllerProcessorName:
            {
                m_name = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorModelName:
            {
                m_model_name = value;
                result       = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorUserName:
            {
                m_user_name = value;
                result      = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorVendorName:
            {
                m_vendor_name = value;
                result        = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorProductName:
            {
                m_product_name = value;
                result         = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorExtData:
            {
                m_ext_data = value;
                result     = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessorUUID:
            {
                m_uuid = value;
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

}
}
