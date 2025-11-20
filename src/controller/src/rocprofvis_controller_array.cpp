// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_trace.h"

namespace RocProfVis
{
namespace Controller
{

Array::Array()
: Handle(__kRPVControllerArrayPropertiesFirst, __kRPVControllerArrayPropertiesLast)
{
    m_ctx = nullptr;
}

Array::~Array() {}

std::vector<Data>& Array::GetVector(void)
{
    return m_array;
}

rocprofvis_controller_object_type_t Array::GetType(void) 
{
    return kRPVControllerObjectTypeArray;
}

void Array::SetContext(Handle* ctx)
{
    m_ctx = (Trace*)ctx;
}

Handle* Array::GetContext(void) {
    return m_ctx;
}

rocprofvis_result_t Array::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Array);
                result = kRocProfVisResultSuccess;
                for(auto& it : m_array)
                {
                    uint64_t entry_size = 0;
                    result = it.GetMemoryUsage(&entry_size, (rocprofvis_common_property_t)property);
                    if (result == kRocProfVisResultSuccess)
                    {
                        *value += entry_size;
                    }
                    else
                    {
                        break;
                    }
                }
                break;
            }
            case kRPVControllerArrayEntryIndexed:
            {
                if(index < m_array.size())
                {
                    result = m_array[index].GetUInt64(value);
                }
                else
                {
                    result = kRocProfVisResultInvalidArgument;
                }
                break;
            }
            case kRPVControllerArrayNumEntries:
            {
                *value = m_array.size();
                result = kRocProfVisResultSuccess;
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
rocprofvis_result_t Array::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerArrayEntryIndexed:
            {
                if(index < m_array.size())
                {
                    result = m_array[index].GetDouble(value);
                }
                else
                {
                    result = kRocProfVisResultInvalidArgument;
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
rocprofvis_result_t Array::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerArrayEntryIndexed:
            {
                if(index < m_array.size())
                {
                    result = m_array[index].GetObject(value);
                }
                else
                {
                    result = kRocProfVisResultInvalidArgument;
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
rocprofvis_result_t Array::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerArrayEntryIndexed:
            {
                if(index < m_array.size())
                {
                    result = m_array[index].GetString(value, length);
                }
                else
                {
                    result = kRocProfVisResultInvalidArgument;
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

rocprofvis_result_t Array::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerArrayEntryIndexed:
        {
            if(index < m_array.size())
            {
                result = m_array[index].SetUInt64(value);
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerArrayNumEntries:
        {
            if(value != m_array.size())
            {
                m_array.resize(value);
                result = m_array.size() == value ? kRocProfVisResultSuccess : kRocProfVisResultMemoryAllocError;
            }
            else
            {
                result = kRocProfVisResultSuccess;
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
rocprofvis_result_t Array::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerArrayEntryIndexed:
            {
                if(index < m_array.size())
                {
                    result = m_array[index].SetDouble(value);
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
rocprofvis_result_t Array::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerArrayEntryIndexed:
            {
                if(index < m_array.size())
                {
                    m_array[index].SetType(kRPVControllerPrimitiveTypeObject);
                    result = m_array[index].SetObject(value);
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
rocprofvis_result_t Array::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerArrayEntryIndexed:
            {
                if(index < m_array.size())
                {
                    result = m_array[index].SetString(value);
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

}
}
