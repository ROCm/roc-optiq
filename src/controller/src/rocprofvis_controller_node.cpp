// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_node.h"
#include "rocprofvis_controller_process.h"
#include "rocprofvis_controller_processor.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_processor_t, Processor,
                  kRPVControllerObjectTypeProcessor>
    ProcessorRef;
typedef Reference<rocprofvis_controller_process_t, Process,
                  kRPVControllerObjectTypeProcess>
    ProcessRef;

Node::Node()
: Handle(__kRPVControllerNodePropertiesFirst, __kRPVControllerNodePropertiesLast)
{}

Node::~Node()
{
    for(auto* proc : m_processors)
    {
        delete proc;
    }
}

rocprofvis_controller_object_type_t
Node::GetType(void)
{
    return kRPVControllerObjectTypeNode;
}

rocprofvis_result_t
Node::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerNodeId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeNumProcessors:
            {
                *value = m_processors.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeNumProcesses:
            {
                *value = m_processes.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeHash:
            {
                *value = m_hash;
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
Node::GetObject(rocprofvis_property_t property, uint64_t index,
                rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerNodeProcessorIndexed:
            {
                if(m_processors.size() > index)
                {
                    *value = (rocprofvis_handle_t*) m_processors[index];
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerNodeProcessIndexed:
            {
                if(m_processes.size() > index)
                {
                    *value = (rocprofvis_handle_t*) m_processes[index];
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
Node::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                uint32_t* length)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeHostName:
        {
            result = GetStdStringImpl(value, length, m_host_name);
            break;
        }
        case kRPVControllerNodeDomainName:
        {
            result = GetStdStringImpl(value, length, m_domain_name);
            break;
        }
        case kRPVControllerNodeOSName:
        {
            result = GetStdStringImpl(value, length, m_os_name);
            break;
        }
        case kRPVControllerNodeOSRelease:
        {
            result = GetStdStringImpl(value, length, m_os_release);
            break;
        }
        case kRPVControllerNodeOSVersion:
        {
            result = GetStdStringImpl(value, length, m_os_version);
            break;
        }
        case kRPVControllerNodeHardwareName:
        {
            result = GetStdStringImpl(value, length, m_hardware_name);
            break;
        }
        case kRPVControllerNodeMachineId:
        {
            result = GetStdStringImpl(value, length, m_machine_id);
            break;
        }
        case kRPVControllerNodeMachineGuid:
        {
            result = GetStdStringImpl(value, length, m_guid);
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
Node::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeId:
        {
            m_id   = static_cast<uint32_t>(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerNodeNumProcessors:
        {
            if(m_processors.size() < value)
            {
                m_processors.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerNodeNumProcesses:
        {
            if(m_processes.size() < value)
            {
                m_processes.resize(value);
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerNodeHash:
        {
            m_hash = value;
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
Node::SetObject(rocprofvis_property_t property, uint64_t index,
                rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeProcessorIndexed:
        {
            if(m_processors.size() > index)
            {
                ProcessorRef ref(value);
                if(ref.IsValid())
                {
                    m_processors[index] = ref.Get();
                    result              = kRocProfVisResultSuccess;
                }
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerNodeProcessIndexed:
        {
            if(m_processes.size() > index)
            {
                ProcessRef ref(value);
                if(ref.IsValid())
                {
                    m_processes[index] = ref.Get();
                    result             = kRocProfVisResultSuccess;
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
    return result;
}

rocprofvis_result_t
Node::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && *value != 0)
    {
        switch(property)
        {
            case kRPVControllerNodeHostName:
            {
                m_host_name = value;
                result      = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeDomainName:
            {
                m_domain_name = value;
                result        = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeOSName:
            {
                m_os_name = value;
                result    = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeOSRelease:
            {
                m_os_release = value;
                result       = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeOSVersion:
            {
                m_os_version = value;
                result       = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeHardwareName:
            {
                m_hardware_name = value;
                result          = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeMachineId:
            {
                m_machine_id = value;
                result       = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNodeMachineGuid:
            {
                m_guid = value;
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
