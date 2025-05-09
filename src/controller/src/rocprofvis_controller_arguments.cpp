// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_arguments.h"

namespace RocProfVis
{
namespace Controller
{

Arguments::Arguments()
{
}

Arguments::~Arguments()
{
}

rocprofvis_controller_object_type_t Arguments::GetType(void) 
{
    return kRPVControllerObjectTypeArguments;
}

rocprofvis_result_t Arguments::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            default:
            {
                result = m_args[property].GetUInt64(value);
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t Arguments::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            default:
            {
                result = m_args[property].GetDouble(value);
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t Arguments::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            default:
            {
                result = m_args[property].GetObject(value);
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t Arguments::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            default:
            {
                result = m_args[property].GetString(value, length);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Arguments::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            default:
            {
                m_args[property].SetType(kRPVControllerPrimitiveTypeUInt64);
                result = m_args[property].SetUInt64(value);
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t Arguments::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            default:
            {
                m_args[property].SetType(kRPVControllerPrimitiveTypeDouble);
                result = m_args[property].SetDouble(value);
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t Arguments::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            default:
            {
                m_args[property].SetType(kRPVControllerPrimitiveTypeObject);
                result = m_args[property].SetObject(value);
                break;
            }
        }
    }
    return result;
}
rocprofvis_result_t Arguments::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            default:
            {
                m_args[property].SetType(kRPVControllerPrimitiveTypeString);
                result = m_args[property].SetString(value);
                break;
            }
        }
    }
    return result;
}

}
}
