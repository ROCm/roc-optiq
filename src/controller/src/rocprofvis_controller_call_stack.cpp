// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_call_stack.h"
#include "rocprofvis_controller_reference.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_callstack_t, CallStack, kRPVControllerObjectTypeCallstack> CallStackRef;

CallStack::CallStack(const char* symbol, const char* args, const char* line)
: Handle(__kRPVControllerCallstackPropertiesFirst,
         __kRPVControllerCallstackPropertiesLast)
, m_symbol(symbol)
, m_args(args)
, m_line(line)
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
                uint64_t sym_size = 0;
                uint64_t args_size = 0;
                uint64_t line_size = 0;
                
                *value = sizeof(CallStack);
                result = kRocProfVisResultSuccess;

                if (result == kRocProfVisResultSuccess)
                {
                    result = m_symbol.GetUInt64(&sym_size);
                }
                
                if (result == kRocProfVisResultSuccess)
                {
                    result = m_args.GetUInt64(&args_size);
                }
                
                if (result == kRocProfVisResultSuccess)
                {
                    result = m_line.GetUInt64(&line_size);
                }

                if (result == kRocProfVisResultSuccess)
                {
                    *value += sym_size;
                    *value += args_size;
                    *value += line_size;
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
        case kRPVControllerCallstackFunction:
            result = m_symbol.GetString(value, length);
            break;
        case kRPVControllerCallstackArguments:
            result = m_args.GetString(value, length);
            break;
        case kRPVControllerCallstackLine:
            result = m_line.GetString(value, length);
            break;
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
