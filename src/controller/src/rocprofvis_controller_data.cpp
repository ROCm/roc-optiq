// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_data.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_core_assert.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <utility>

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
            if (m_owns_object && m_object)
            {
                delete reinterpret_cast<Handle*>(m_object);
            }
            m_owns_object = false;
            m_object      = nullptr;
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

Data::Data(Data&& other) noexcept
: m_type(other.m_type)
{
    m_uint64 = 0;
    operator=(std::move(other));
}

Data& Data::operator=(Data const& other)
{
    if (this != &other)
    {
        Reset();
        m_type = other.m_type;
        switch(m_type)
        {
            case kRPVControllerPrimitiveTypeObject:
            {
                // A copy only borrows the object; ownership is never shared so
                // that the owning Data remains the sole deleter.
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

Data& Data::operator=(Data&& other) noexcept
{
    if (this == &other)
    {
        return *this;
    }
    Reset();
    m_type = other.m_type;
    switch(m_type)
    {
        case kRPVControllerPrimitiveTypeObject:
        {
            m_object             = other.m_object;
            m_owns_object        = other.m_owns_object;
            other.m_object       = nullptr;
            other.m_owns_object  = false;
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
        ROCPROFVIS_ASSERT(m_string);
        if (m_string)
        {
            std::memcpy(m_string, string, length);
        }
    }
}

Data::~Data()
{
    switch(m_type)
    {
        case kRPVControllerPrimitiveTypeObject:
        {
            if (m_owns_object && m_object)
            {
                delete reinterpret_cast<Handle*>(m_object);
            }
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
                m_object      = object;
                m_owns_object = false;
                result        = kRocProfVisResultSuccess;
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

rocprofvis_result_t Data::SetOwnedObject(rocprofvis_handle_t* object)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object)
    {
        switch(m_type)
        {
            case kRPVControllerPrimitiveTypeObject:
            {
                if (m_owns_object && m_object && m_object != object)
                {
                    delete reinterpret_cast<Handle*>(m_object);
                }
                m_object      = object;
                m_owns_object = true;
                result        = kRocProfVisResultSuccess;
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
                *length = static_cast<uint32_t>(m_string ? strlen(m_string) : 0);
                result = kRocProfVisResultSuccess;
            }
            else if (length && string && (*length > 0))
            {
                // *length is the capacity of the caller's buffer. Copy as many bytes
                // as fit, and null-terminate only when there is spare room. This keeps
                // the exact-fit caller (resize(strlen) then pass strlen) working while
                // ensuring oversized buffers (e.g. a fixed char buf[256] later wrapped
                // in std::string(buffer)) get a terminator instead of reading past the
                // copied data into uninitialized memory.
                const size_t src_len  = m_string ? strlen(m_string) : 0;
                const size_t capacity = static_cast<size_t>(*length);
                const size_t copy     = std::min(src_len, capacity);
                if (copy > 0) std::memcpy(string, m_string, copy);
                if (copy < capacity) string[copy] = '\0';
                *length = static_cast<uint32_t>(copy);
                result  = kRocProfVisResultSuccess;
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

                size_t length = strlen(string);
                m_string = (char*)calloc(length+1, 1);
                if (m_string)
                {
                    std::memcpy(m_string, string, length);
                    m_string[length] = '\0';
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

rocprofvis_result_t Data::GetMemoryUsage(uint64_t* value, rocprofvis_common_property_t property)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                *value = 0;
                result = kRocProfVisResultSuccess;
                if(m_type == kRPVControllerPrimitiveTypeObject)
                {
                    result = ((Handle*)m_object)->GetUInt64((rocprofvis_property_t)property, 0, value);
                }
                if(result == kRocProfVisResultSuccess)
                {
                    *value += sizeof(Data);
                }
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Data);
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
