// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_data.h"

#include <cstdlib>
#include <cstring>
#include <cassert>

namespace RocProfVis
{
namespace Controller
{

void Data::Reset()
{
    switch(m_type)
    {
        case kRPVControllerPrimitiveTypeObject:
        {
            break;
        }
        case kRPVControllerPrimitiveTypeString:
        {
            if (m_string)
            {
                free(m_string);
            }
            break;
        }
        case kRPVControllerPrimitiveTypeUInt64:
        case kRPVControllerPrimitiveTypeDouble:
        default:
        {
            break;
        }
    }
}

Data::Data()
: m_type(kRPVControllerPrimitiveTypeUInt64)
{
    m_uint64 = 0;
}

Data::Data(Data const& other)
: m_type(other.m_type)
{
    m_uint64 = 0;
    operator=(other);
}

Data::Data(Data&& other)
: m_type(other.m_type)
{
    operator=(other);
}

Data& Data::operator=(Data const& other)
{
    if (this != &other)
    {
        Reset();
        switch(m_type)
        {
            case kRPVControllerPrimitiveTypeObject:
            {
                SetObject(other.m_object);
                break;
            }
            case kRPVControllerPrimitiveTypeString:
            {
                SetString(other.m_string);
                break;
            }
            case kRPVControllerPrimitiveTypeUInt64:
            {
                SetUInt64(other.m_uint64);
                break;
            }
            case kRPVControllerPrimitiveTypeDouble:
            {
                SetDouble(other.m_double);
                break;
            }
            default:
            {
                break;
            }
        }
    }
    return *this;
}

Data& Data::operator=(Data&& other)
{
    Reset();
    switch(m_type)
    {
        case kRPVControllerPrimitiveTypeObject:
        {
            m_object = other.m_object;
            other.m_object = nullptr;
            break;
        }
        case kRPVControllerPrimitiveTypeString:
        {
            m_string = other.m_string;
            other.m_string = nullptr;
            break;
        }
        case kRPVControllerPrimitiveTypeUInt64:
        {
            m_uint64 = other.m_uint64;
            other.m_uint64 = 0;
            break;
        }
        case kRPVControllerPrimitiveTypeDouble:
        {
            m_double = other.m_double;
            other.m_double = 0;
            break;
        }
        default:
        {
            break;
        }
    }
    return *this;
}

Data::Data(rocprofvis_controller_primitive_type_t type)
: m_type(type)
{
    m_uint64 = 0;
}

Data::Data(double val)
: m_type(kRPVControllerPrimitiveTypeDouble)
, m_double(val)
{
}

Data::Data(uint64_t val)
: m_type(kRPVControllerPrimitiveTypeUInt64)
, m_uint64(val)
{
}

Data::Data(rocprofvis_handle_t* object)
: m_type(kRPVControllerPrimitiveTypeObject)
, m_object(object)
{
}

Data::Data(char const* const string)
: m_type(kRPVControllerPrimitiveTypeString)
, m_string(nullptr)
{
    if (string)
    {
        size_t length = strlen(string);
        m_string = (char*)calloc(length+1, 1);
        assert(m_string);
        if (m_string)
        {
            strcpy(m_string, string);
        }
    }
}

Data::~Data()
{
    switch(m_type)
    {
        case kRPVControllerPrimitiveTypeObject:
        {
            break;
        }
        case kRPVControllerPrimitiveTypeString:
        {
            if (m_string)
            {
                free(m_string);
            }
            break;
        }
        case kRPVControllerPrimitiveTypeUInt64:
        case kRPVControllerPrimitiveTypeDouble:
        default:
        {
            break;
        }
    }
}

rocprofvis_controller_primitive_type_t Data::GetType(void) const
{
    return m_type;
}

void Data::SetType(rocprofvis_controller_primitive_type_t type)
{
    m_type = type;
}

rocprofvis_result_t Data::GetObject(rocprofvis_handle_t** object)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object)
    {
        switch(m_type)
        {
            case kRPVControllerPrimitiveTypeObject:
            {
                *object = m_object;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPrimitiveTypeString:
            case kRPVControllerPrimitiveTypeUInt64:
            case kRPVControllerPrimitiveTypeDouble:
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

rocprofvis_result_t Data::SetObject(rocprofvis_handle_t* object)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object)
    {
        switch(m_type)
        {
            case kRPVControllerPrimitiveTypeObject:
            {
                m_object = object;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPrimitiveTypeString:
            case kRPVControllerPrimitiveTypeUInt64:
            case kRPVControllerPrimitiveTypeDouble:
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

rocprofvis_result_t Data::GetString(char* string, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(m_type)
    {
        case kRPVControllerPrimitiveTypeString:
        {
            if (length && (!string || (*length == 0)))
            {
                *length = m_string ? strlen(m_string) : 0;
                result = kRocProfVisResultSuccess;
            }
            else if (length && string && (*length > 0))
            {
                strncpy(string, m_string, *length);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerPrimitiveTypeObject:
        case kRPVControllerPrimitiveTypeUInt64:
        case kRPVControllerPrimitiveTypeDouble:
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

rocprofvis_result_t Data::SetString(char const* string)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(m_type)
    {
        case kRPVControllerPrimitiveTypeString:
        {
            if (string)
            {
                if (m_string)
                {
                    free(m_string);
                    m_string = nullptr;
                }

                uint32_t length = strlen(string);
                m_string = (char*)calloc(length+1, 1);
                if (m_string)
                {
                    strncpy(m_string, string, length);
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultMemoryAllocError;
                }
            }
            break;
        }
        case kRPVControllerPrimitiveTypeObject:
        case kRPVControllerPrimitiveTypeUInt64:
        case kRPVControllerPrimitiveTypeDouble:
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

rocprofvis_result_t Data::GetUInt64(uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(m_type)
        {
            case kRPVControllerPrimitiveTypeUInt64:
            {
                *value = m_uint64;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPrimitiveTypeString:
            case kRPVControllerPrimitiveTypeObject:
            case kRPVControllerPrimitiveTypeDouble:
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

rocprofvis_result_t Data::SetUInt64(uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(m_type)
    {
        case kRPVControllerPrimitiveTypeUInt64:
        {
            m_uint64 = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPrimitiveTypeString:
        case kRPVControllerPrimitiveTypeObject:
        case kRPVControllerPrimitiveTypeDouble:
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

rocprofvis_result_t Data::GetDouble(double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(m_type)
        {
            case kRPVControllerPrimitiveTypeDouble:
            {
                *value = m_double;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerPrimitiveTypeObject:
            case kRPVControllerPrimitiveTypeString:
            case kRPVControllerPrimitiveTypeUInt64:
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

rocprofvis_result_t Data::SetDouble(double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(m_type)
    {
        case kRPVControllerPrimitiveTypeDouble:
        {
            m_double = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerPrimitiveTypeObject:
        case kRPVControllerPrimitiveTypeString:
        case kRPVControllerPrimitiveTypeUInt64:
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

}
}
