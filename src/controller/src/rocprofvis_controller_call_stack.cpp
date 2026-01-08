// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_call_stack.h"
#include "rocprofvis_controller_reference.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_callstack_t, CallStack, kRPVControllerObjectTypeCallstack> CallStackRef;

CallStack::CallStack(const char* file, const char* pc, const char* name, const char* line_name, const char* line_address)
: m_file(file)
, m_pc(pc)
, m_name(name)
, m_line_name(line_name)
, m_line_address(line_address)
, Handle(__kRPVControllerCallstackPropertiesFirst,
         __kRPVControllerCallstackPropertiesLast)
{}

CallStack::~CallStack() {}

rocprofvis_controller_object_type_t CallStack::GetType(void) 
{
    return kRPVControllerObjectTypeCallstack;
}

rocprofvis_result_t CallStack::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                uint64_t file_size = 0;
                uint64_t pc_size = 0;
                uint64_t name_size = 0;
                uint64_t line_name_size = 0;
                uint64_t line_address_size = 0;
                
                *value = sizeof(CallStack);
                result = kRocProfVisResultSuccess;

                if (result == kRocProfVisResultSuccess)
                {
                    result = m_file.GetUInt64(&file_size);
                }
                
                if (result == kRocProfVisResultSuccess)
                {
                    result = m_pc.GetUInt64(&pc_size);
                }
                
                if (result == kRocProfVisResultSuccess)
                {
                    result = m_name.GetUInt64(&name_size);
                }

                if (result == kRocProfVisResultSuccess)
                {
                    result = m_line_name.GetUInt64(&line_name_size);
                }

                if (result == kRocProfVisResultSuccess)
                {
                    result = m_line_address.GetUInt64(&line_address_size);
                }

                if (result == kRocProfVisResultSuccess)
                {
                    *value += file_size + pc_size + name_size + line_name_size + line_address_size;
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

rocprofvis_result_t CallStack::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    (void) index;    
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerCallstackFile:
        {
            result = m_file.GetString(value, length);
            break;
        }
        case kRPVControllerCallstackPc:
        {       
            result = m_pc.GetString(value, length);
            break;
        }
        case kRPVControllerCallstackName:
        {       
            result = m_name.GetString(value, length);
            break;
        }
        case kRPVControllerCallstackLineName:
        {
            result = m_line_name.GetString(value, length);
            break; 
        }
        case kRPVControllerCallstackLineAddress:
        {
            result = m_line_address.GetString(value, length);
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
