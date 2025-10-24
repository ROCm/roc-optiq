// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_arguments.h"

namespace RocProfVis
{
namespace Controller
{

Arguments::Arguments()
: Handle(0, 0)
{}

Arguments::~Arguments() {}

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
        auto it = m_args.find(property);
        if (it != m_args.end() && it->second.size() > index)
        {
            result = it->second[index].GetUInt64(value);
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
        auto it = m_args.find(property);
        if (it != m_args.end() && it->second.size() > index)
        {
            result = it->second[index].GetDouble(value);
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
        auto it = m_args.find(property);
        if (it != m_args.end() && it->second.size() > index)
        {
            result = it->second[index].GetObject(value);
        }
    }
    return result;
}
rocprofvis_result_t Arguments::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    auto it = m_args.find(property);
    if (it != m_args.end() && it->second.size() > index)
    {
        result = it->second[index].GetString(value, length);
    }
    return result;
}

rocprofvis_result_t Arguments::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(m_args[property].size() < index + 1)
    {
        m_args[property].resize(index + 1);
    }
    m_args[property][index].SetType(kRPVControllerPrimitiveTypeUInt64);
    result = m_args[property][index].SetUInt64(value);
    return result;
}
rocprofvis_result_t Arguments::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(m_args[property].size() < index + 1)
    {
        m_args[property].resize(index + 1);
    }
    m_args[property][index].SetType(kRPVControllerPrimitiveTypeDouble);
    result = m_args[property][index].SetDouble(value);
    return result;
}
rocprofvis_result_t Arguments::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        if(m_args[property].size() < index + 1)
        {
            m_args[property].resize(index + 1);
        }
        m_args[property][index].SetType(kRPVControllerPrimitiveTypeObject);
        result = m_args[property][index].SetObject(value);
    }
    return result;
}
rocprofvis_result_t Arguments::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        if(m_args[property].size() < index + 1)
        {
            m_args[property].resize(index + 1);
        }
        m_args[property][index].SetType(kRPVControllerPrimitiveTypeString);
        result = m_args[property][index].SetString(value);
    }
    return result;
}

}
}
