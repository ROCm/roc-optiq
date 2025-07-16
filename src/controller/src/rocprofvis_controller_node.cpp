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

Node::Node() {}

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
            case kRPVControllerNodeHostName:
            case kRPVControllerNodeDomainName:
            case kRPVControllerNodeOSName:
            case kRPVControllerNodeOSRelease:
            case kRPVControllerNodeOSVersion:
            case kRPVControllerNodeHardwareName:
            case kRPVControllerNodeMachineId:
            case kRPVControllerNodeMachineGuid:
            case kRPVControllerNodeProcessorIndexed:
            case kRPVControllerNodeProcessIndexed:
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
Node::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeId:
        case kRPVControllerNodeHostName:
        case kRPVControllerNodeDomainName:
        case kRPVControllerNodeOSName:
        case kRPVControllerNodeOSRelease:
        case kRPVControllerNodeOSVersion:
        case kRPVControllerNodeHardwareName:
        case kRPVControllerNodeMachineId:
        case kRPVControllerNodeMachineGuid:
        case kRPVControllerNodeNumProcessors:
        case kRPVControllerNodeProcessorIndexed:
        case kRPVControllerNodeNumProcesses:
        case kRPVControllerNodeProcessIndexed:
        case kRPVControllerNodeHash:
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
            case kRPVControllerNodeId:
            case kRPVControllerNodeHostName:
            case kRPVControllerNodeDomainName:
            case kRPVControllerNodeOSName:
            case kRPVControllerNodeOSRelease:
            case kRPVControllerNodeOSVersion:
            case kRPVControllerNodeHardwareName:
            case kRPVControllerNodeMachineId:
            case kRPVControllerNodeMachineGuid:
            case kRPVControllerNodeNumProcessors:
            case kRPVControllerNodeNumProcesses:
            case kRPVControllerNodeHash:
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
Node::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeHostName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_host_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_host_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeDomainName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_domain_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_domain_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeOSName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_os_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_os_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeOSRelease:
        {
            if(value && length && *length)
            {
                strncpy(value, m_os_release.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_os_release.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeOSVersion:
        {
            if(value && length && *length)
            {
                strncpy(value, m_os_version.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_os_version.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeHardwareName:
        {
            if(value && length && *length)
            {
                strncpy(value, m_hardware_name.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_hardware_name.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeMachineId:
        {
            if(value && length && *length)
            {
                strncpy(value, m_machine_id.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_machine_id.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeMachineGuid:
        {
            if(value && length && *length)
            {
                strncpy(value, m_guid.c_str(), *length);
                result = kRocProfVisResultSuccess;
            }
            else if(length)
            {
                *length = m_guid.length();
                result  = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerNodeId:
        case kRPVControllerNodeNumProcessors:
        case kRPVControllerNodeProcessorIndexed:
        case kRPVControllerNodeNumProcesses:
        case kRPVControllerNodeProcessIndexed:
        case kRPVControllerNodeHash:
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
Node::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeId:
        {
            m_id   = value;
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
        case kRPVControllerNodeHostName:
        case kRPVControllerNodeDomainName:
        case kRPVControllerNodeOSName:
        case kRPVControllerNodeOSRelease:
        case kRPVControllerNodeOSVersion:
        case kRPVControllerNodeHardwareName:
        case kRPVControllerNodeMachineId:
        case kRPVControllerNodeMachineGuid:
        case kRPVControllerNodeProcessorIndexed:
        case kRPVControllerNodeProcessIndexed:
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
Node::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerNodeId:
        case kRPVControllerNodeHostName:
        case kRPVControllerNodeDomainName:
        case kRPVControllerNodeOSName:
        case kRPVControllerNodeOSRelease:
        case kRPVControllerNodeOSVersion:
        case kRPVControllerNodeHardwareName:
        case kRPVControllerNodeMachineId:
        case kRPVControllerNodeHash:
        case kRPVControllerNodeMachineGuid:
        case kRPVControllerNodeNumProcessors:
        case kRPVControllerNodeProcessorIndexed:
        case kRPVControllerNodeNumProcesses:
        case kRPVControllerNodeProcessIndexed:
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
        case kRPVControllerNodeId:
        case kRPVControllerNodeHostName:
        case kRPVControllerNodeDomainName:
        case kRPVControllerNodeOSName:
        case kRPVControllerNodeOSRelease:
        case kRPVControllerNodeOSVersion:
        case kRPVControllerNodeHardwareName:
        case kRPVControllerNodeMachineId:
        case kRPVControllerNodeHash:
        case kRPVControllerNodeMachineGuid:
        case kRPVControllerNodeNumProcessors:
        case kRPVControllerNodeNumProcesses:
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
Node::SetString(rocprofvis_property_t property, uint64_t index, char const* value,
                uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value && length)
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
            case kRPVControllerNodeHash:
            case kRPVControllerNodeId:
            case kRPVControllerNodeNumProcessors:
            case kRPVControllerNodeProcessorIndexed:
            case kRPVControllerNodeNumProcesses:
            case kRPVControllerNodeProcessIndexed:
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
