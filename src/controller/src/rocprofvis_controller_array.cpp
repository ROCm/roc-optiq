// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_array.h"

namespace RocProfVis
{
namespace Controller
{

Array::Array()
{
}

Array::~Array()
{
}

rocprofvis_controller_object_type_t Array::GetType(void) 
{
    return kRPVControllerObjectTypeArray;
}

rocprofvis_result_t Array::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
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
            case kRPVControllerArrayNumEntries:
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
            case kRPVControllerArrayNumEntries:
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
rocprofvis_result_t Array::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
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
                    result = m_array[index].GetString(value, length);
                }
                else
                {
                    result = kRocProfVisResultInvalidArgument;
                }
                break;
            }
            case kRPVControllerArrayNumEntries:
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

rocprofvis_result_t Array::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
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
                result = kRocProfVisResultInvalidEnum;
                break;
            }
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
            case kRPVControllerArrayNumEntries:
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
            case kRPVControllerArrayNumEntries:
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
rocprofvis_result_t Array::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length) 
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
            case kRPVControllerArrayNumEntries:
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
