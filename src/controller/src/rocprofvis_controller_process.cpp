// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_process.h"
#include "rocprofvis_controller_thread.h"
#include "rocprofvis_controller_queue.h"
#include "rocprofvis_controller_stream.h"
#include "rocprofvis_controller_counter.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_controller_reference.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_thread_t, Thread, kRPVControllerObjectTypeThread>
    ThreadRef;
typedef Reference<rocprofvis_controller_queue_t, Queue, kRPVControllerObjectTypeQueue>
    QueueRef;
typedef Reference<rocprofvis_controller_stream_t, Stream, kRPVControllerObjectTypeStream>
    StreamRef;
typedef Reference<rocprofvis_controller_counter_t, Counter, kRPVControllerObjectTypeCounter>
    CounterRef;

Process::Process()
{
}

Process::~Process()
{
    for (auto* thread : m_threads)
    {
        delete thread;
    }
    for(auto* queue : m_queues)
    {
        delete queue;
    }
    for(auto* stream : m_streams)
    {
        delete stream;
    }
    for(auto* counter : m_counters)
    {
        delete counter;
    }
}

rocprofvis_controller_object_type_t
Process::GetType(void)
{
    return kRPVControllerObjectTypeProcess;
}

rocprofvis_result_t
Process::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessNodeId:
            {
                *value = m_node_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessParentId:
            {
                *value = m_parent_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessNumThreads:
            {
                *value = m_threads.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessNumQueues:
            {
                *value = m_queues.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessNumStreams:
            {
                *value = m_streams.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessNumCounters:
            {
                *value = m_counters.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessInitTime:
            case kRPVControllerProcessFinishTime:
            case kRPVControllerProcessStartTime:
            case kRPVControllerProcessEndTime:
            case kRPVControllerProcessCommand:
            case kRPVControllerProcessEnvironment:
            case kRPVControllerProcessExtData:
            case kRPVControllerProcessThreadIndexed:
            case kRPVControllerProcessQueueIndexed:
            case kRPVControllerProcessStreamIndexed:
            case kRPVControllerProcessCounterIndexed:
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
Process::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessInitTime:
            {
                *value = m_init_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessFinishTime:
            {
                *value = m_finish_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessStartTime:
            {
                *value = m_start_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessEndTime:
            {
                *value = m_end_time;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessNumCounters:
            case kRPVControllerProcessId:
            case kRPVControllerProcessNodeId:
            case kRPVControllerProcessParentId:
            case kRPVControllerProcessCommand:
            case kRPVControllerProcessEnvironment:
            case kRPVControllerProcessExtData:
            case kRPVControllerProcessNumThreads:
            case kRPVControllerProcessNumQueues:
            case kRPVControllerProcessNumStreams:
            case kRPVControllerProcessThreadIndexed:
            case kRPVControllerProcessQueueIndexed:
            case kRPVControllerProcessStreamIndexed:
            case kRPVControllerProcessCounterIndexed:
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
Process::GetObject(rocprofvis_property_t property, uint64_t index,
                   rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerProcessThreadIndexed:
            {
                if(index < m_threads.size())
                {
                    *value = (rocprofvis_handle_t*) m_threads[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerProcessQueueIndexed:
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
            case kRPVControllerProcessStreamIndexed:
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
            case kRPVControllerProcessCounterIndexed:
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
            case kRPVControllerProcessId:
            case kRPVControllerProcessNodeId:
            case kRPVControllerProcessParentId:
            case kRPVControllerProcessInitTime:
            case kRPVControllerProcessFinishTime:
            case kRPVControllerProcessStartTime:
            case kRPVControllerProcessEndTime:
            case kRPVControllerProcessCommand:
            case kRPVControllerProcessEnvironment:
            case kRPVControllerProcessExtData:
            case kRPVControllerProcessNumThreads:
            case kRPVControllerProcessNumQueues:
            case kRPVControllerProcessNumStreams:
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
Process::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                   uint32_t* length)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessCommand:
        {
            result = GetStdStringImpl(value, length, m_command);
            break;
        }
        case kRPVControllerProcessEnvironment:
        {
            result = GetStdStringImpl(value, length, m_environment);
            break;
        }
        case kRPVControllerProcessExtData:
        {
            result = GetStdStringImpl(value, length, m_ext_data);
            break;
        }
        case kRPVControllerProcessId:
        case kRPVControllerProcessNodeId:
        case kRPVControllerProcessParentId:
        case kRPVControllerProcessInitTime:
        case kRPVControllerProcessFinishTime:
        case kRPVControllerProcessStartTime:
        case kRPVControllerProcessEndTime:
        case kRPVControllerProcessNumThreads:
        case kRPVControllerProcessNumQueues:
        case kRPVControllerProcessNumStreams:
        case kRPVControllerProcessThreadIndexed:
        case kRPVControllerProcessQueueIndexed:
        case kRPVControllerProcessStreamIndexed:
        case kRPVControllerProcessNumCounters:
        case kRPVControllerProcessCounterIndexed:
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
Process::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessId:
        {
            m_id   = static_cast<uint32_t>(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessNodeId:
        {
            m_node_id =  static_cast<uint32_t>(value);
            result    = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessParentId:
        {
            m_parent_id =  static_cast<uint32_t>(value);
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessNumThreads:
        {
            if(value > m_threads.size())
            {
                m_threads.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessNumQueues:
        {
            if(value > m_queues.size())
            {
                m_queues.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessNumStreams:
        {
            if(value > m_streams.size())
            {
                m_streams.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessNumCounters:
        {
            if(value > m_counters.size())
            {
                m_counters.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessCounterIndexed:
        case kRPVControllerProcessInitTime:
        case kRPVControllerProcessFinishTime:
        case kRPVControllerProcessStartTime:
        case kRPVControllerProcessEndTime:
        case kRPVControllerProcessCommand:
        case kRPVControllerProcessEnvironment:
        case kRPVControllerProcessExtData:
        case kRPVControllerProcessThreadIndexed:
        case kRPVControllerProcessQueueIndexed:
        case kRPVControllerProcessStreamIndexed:
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
Process::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessInitTime:
        {
            m_init_time = value;
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessFinishTime:
        {
            m_finish_time = value;
            result        = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessStartTime:
        {
            m_start_time = value;
            result       = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessEndTime:
        {
            m_end_time = value;
            result     = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerProcessNumCounters:
        case kRPVControllerProcessCounterIndexed:
        case kRPVControllerProcessId:
        case kRPVControllerProcessNodeId:
        case kRPVControllerProcessParentId:
        case kRPVControllerProcessCommand:
        case kRPVControllerProcessEnvironment:
        case kRPVControllerProcessExtData:
        case kRPVControllerProcessNumThreads:
        case kRPVControllerProcessNumQueues:
        case kRPVControllerProcessNumStreams:
        case kRPVControllerProcessThreadIndexed:
        case kRPVControllerProcessQueueIndexed:
        case kRPVControllerProcessStreamIndexed:
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
Process::SetObject(rocprofvis_property_t property, uint64_t index,
                   rocprofvis_handle_t* value)

{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerProcessThreadIndexed:
        {
            ThreadRef ref(value);
            if(ref.IsValid())
            {
                if(index < m_threads.size())
                {
                    m_threads[index] = ref.Get();
                    result           = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
            }
            break;
        }
        case kRPVControllerProcessQueueIndexed:
        {
            QueueRef ref(value);
            if(ref.IsValid())
            {
                if(index < m_queues.size())
                {
                    m_queues[index] = ref.Get();
                    result          = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
            }
            break;
        }
        case kRPVControllerProcessStreamIndexed:
        {
            StreamRef ref(value);
            if(ref.IsValid())
            {
                if(index < m_streams.size())
                {
                    m_streams[index] = ref.Get();
                    result           = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
            }
            break;
        }
        case kRPVControllerProcessCounterIndexed:
        {
            CounterRef ref(value);
            if(ref.IsValid())
            {
                if(index < m_counters.size())
                {
                    m_counters[index] = ref.Get();
                    result           = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
            }
            break;
        }
        case kRPVControllerProcessNumCounters:
        case kRPVControllerProcessId:
        case kRPVControllerProcessNodeId:
        case kRPVControllerProcessParentId:
        case kRPVControllerProcessInitTime:
        case kRPVControllerProcessFinishTime:
        case kRPVControllerProcessStartTime:
        case kRPVControllerProcessEndTime:
        case kRPVControllerProcessCommand:
        case kRPVControllerProcessEnvironment:
        case kRPVControllerProcessExtData:
        case kRPVControllerProcessNumThreads:
        case kRPVControllerProcessNumQueues:
        case kRPVControllerProcessNumStreams:
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
Process::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && *value != 0)
    {
        switch(property)
        {
            case kRPVControllerProcessCommand:
            {
                m_command = value;
                result    = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessEnvironment:
            {
                m_environment = value;
                result        = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessExtData:
            {
                m_ext_data = value;
                result     = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerProcessNumCounters:
            case kRPVControllerProcessCounterIndexed:
            case kRPVControllerProcessId:
            case kRPVControllerProcessNodeId:
            case kRPVControllerProcessParentId:
            case kRPVControllerProcessInitTime:
            case kRPVControllerProcessFinishTime:
            case kRPVControllerProcessStartTime:
            case kRPVControllerProcessEndTime:
            case kRPVControllerProcessNumThreads:
            case kRPVControllerProcessNumQueues:
            case kRPVControllerProcessNumStreams:
            case kRPVControllerProcessThreadIndexed:
            case kRPVControllerProcessQueueIndexed:
            case kRPVControllerProcessStreamIndexed:
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
