// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller.h"

#include <cassert>
#include <cstring>
#include <string>
#include <future>
#include <map>
#include <vector>
#include <algorithm>

#include "rocprofvis_trace.h"

namespace RocProfVis
{
namespace Controller
{

class Callstack;
class FlowControl;
class Sample;
class Event;
class SampleLOD;
class EventLOD;

template<typename Pointer, typename Type, unsigned Enum>
class Reference
{
public:
    Reference(Pointer* ptr)
    : m_object(nullptr)
    {
        Type* t = (Type*)ptr;
        m_object = (t && t->GetType() == Enum) ? t : nullptr; 
    }

    ~Reference()
    {
    }

    bool IsValid(void) const
    {
        return m_object != nullptr;
    }

    Type* Get(void)
    {
        return m_object;
    }

    Type& operator*()
    {
        assert(m_object);
        return *m_object;
    }

    Type* operator->()
    {
        assert(m_object);
        return m_object;
    }

private:
    Type* m_object;
};

class Data
{
    void Reset()
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

public:
    Data()
    : m_type(kRPVControllerPrimitiveTypeUInt64)
    {
        m_uint64 = 0;
    }

    Data(Data const& other)
    : m_type(other.m_type)
    {
        m_uint64 = 0;
        operator=(other);
    }

    Data(Data&& other)
    : m_type(other.m_type)
    {
        operator=(other);
    }

    Data& operator=(Data const& other)
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

    Data& operator=(Data&& other)
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

    Data(rocprofvis_controller_primitive_type_t type)
    : m_type(type)
    {
        m_uint64 = 0;
    }

    Data(double val)
    : m_type(kRPVControllerPrimitiveTypeDouble)
    , m_double(val)
    {
    }

    Data(uint64_t val)
    : m_type(kRPVControllerPrimitiveTypeUInt64)
    , m_uint64(val)
    {
    }

    Data(rocprofvis_handle_t* object)
    : m_type(kRPVControllerPrimitiveTypeObject)
    , m_object(object)
    {
    }

    Data(char const* const string)
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

    ~Data()
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

    rocprofvis_controller_primitive_type_t GetType(void) const
    {
        return m_type;
    }

    void SetType(rocprofvis_controller_primitive_type_t type)
    {
        m_type = type;
    }

    rocprofvis_result_t GetObject(rocprofvis_handle_t** object)
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

    rocprofvis_result_t SetObject(rocprofvis_handle_t* object)
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

    rocprofvis_result_t GetString(char* string, uint32_t* length)
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

    rocprofvis_result_t SetString(char const* string)
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

    rocprofvis_result_t GetUInt64(uint64_t* value)
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

    rocprofvis_result_t SetUInt64(uint64_t value)
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

    rocprofvis_result_t GetDouble(double* value)
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

    rocprofvis_result_t SetDouble(double value)
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
private:
    rocprofvis_controller_primitive_type_t m_type;
    union
    {
        rocprofvis_handle_t* m_object;
        char* m_string;
        uint64_t m_uint64;
        double m_double;
    };
};

// All controller handles inherit from this and implement this API
class Handle
{
public:
    Handle() {}
    virtual ~Handle() {}

    virtual rocprofvis_controller_object_type_t GetType(void) = 0;

    // Handlers for getters.
    virtual rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) = 0;
    virtual rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) = 0;
    virtual rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) = 0;
    virtual rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) = 0;

    virtual rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) = 0;
    virtual rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) = 0;
    virtual rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) = 0;
    virtual rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) = 0;
};

class Array : public Handle
{
public:
    Array() {}
    virtual ~Array() {}

    rocprofvis_controller_object_type_t GetType(void) final
    {
        return kRPVControllerObjectTypeArray;
    }

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerArrayEntryIndexed:
                {
                    if (index < m_array.size())
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
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerArrayEntryIndexed:
                {
                    if (index < m_array.size())
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
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerArrayEntryIndexed:
                {
                    if (index < m_array.size())
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
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerArrayEntryIndexed:
                {
                    if (index < m_array.size())
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

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerArrayEntryIndexed:
                {
                    if (index < m_array.size())
                    {
                        result = m_array[index].SetUInt64(value);
                    }
                    else
                    {
                        result = kRocProfVisResultInvalidArgument;   
                    }
                    break;
                }
                case kRPVControllerArrayNumEntries:
                {
                    if (value != m_array.size())
                    {
                        m_array.resize(value);
                        result = kRocProfVisResultSuccess;
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
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerArrayEntryIndexed:
                {
                    if (index < m_array.size())
                    {
                        result = m_array[index].SetDouble(value);
                    }
                    else
                    {
                        result = kRocProfVisResultInvalidArgument;   
                    }
                    break;
                }
                case kRPVControllerArrayNumEntries:
                {
                    result = kRocProfVisResultReadOnlyError;
                    break;
                }
                default:
                {
                    result = kRocProfVisResultInvalidArgument;
                    break;
                }
            }
        }
        return result;
    }
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerArrayEntryIndexed:
                {
                    if (index < m_array.size())
                    {
                        m_array[index].SetType(kRPVControllerPrimitiveTypeObject);
                        result = m_array[index].SetObject(value);
                    }
                    else
                    {
                        result = kRocProfVisResultInvalidArgument;   
                    }
                    break;
                }
                case kRPVControllerArrayNumEntries:
                {
                    result = kRocProfVisResultReadOnlyError;
                    break;
                }
                default:
                {
                    result = kRocProfVisResultInvalidArgument;
                    break;
                }
            }
        }
        return result;
    }
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerArrayEntryIndexed:
                {
                    if (index < m_array.size())
                    {
                        result = m_array[index].SetString(value);
                    }
                    else
                    {
                        result = kRocProfVisResultInvalidArgument;   
                    }
                    break;
                }
                case kRPVControllerArrayNumEntries:
                {
                    result = kRocProfVisResultReadOnlyError;
                    break;
                }
                default:
                {
                    result = kRocProfVisResultInvalidArgument;
                    break;
                }
            }
        }
        return result;
    }
private:
    std::vector<Data> m_array;
};

class Sample : public Handle
{
public:
    Sample(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp)
    : m_data(type)
    , m_id(id)
    , m_timestamp(timestamp)
    {
    }

    virtual ~Sample()
    {
    }

    rocprofvis_controller_object_type_t GetType(void) override
    {
        return kRPVControllerObjectTypeSample;
    }

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleId:
                {
                    *value = m_id;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleType:
                {
                    *value = m_data.GetType();
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleValue:
                {
                    result = m_data.GetUInt64(value);
                    break;
                }
                case kRPVControllerSampleNumChildren:
                {
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildIndex:
                case kRPVControllerSampleChildMin:
                case kRPVControllerSampleChildMean:
                case kRPVControllerSampleChildMedian:
                case kRPVControllerSampleChildMax:
                case kRPVControllerSampleChildMinTimestamp:
                case kRPVControllerSampleChildMaxTimestamp:
                case kRPVControllerSampleTimestamp:
                case kRPVControllerSampleTrack:
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
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleChildMin:
                {
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMean:
                {
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMedian:
                {
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMax:
                {
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMinTimestamp:
                {
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMaxTimestamp:
                {
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleTimestamp:
                {
                    *value = m_timestamp;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleValue:
                {
                    result = m_data.GetDouble(value);
                    break;
                }
                case kRPVControllerSampleId:
                case kRPVControllerSampleType:
                case kRPVControllerSampleNumChildren:
                case kRPVControllerSampleChildIndex:
                case kRPVControllerSampleTrack:
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
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleChildIndex:
                {
                    *value = nullptr;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleTrack:
                {
                    *value = nullptr;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleValue:
                {
                    result = m_data.GetObject(value);
                    break;
                }
                case kRPVControllerSampleId:
                case kRPVControllerSampleType:
                case kRPVControllerSampleNumChildren:
                case kRPVControllerSampleChildMin:
                case kRPVControllerSampleChildMean:
                case kRPVControllerSampleChildMedian:
                case kRPVControllerSampleChildMax:
                case kRPVControllerSampleChildMinTimestamp:
                case kRPVControllerSampleChildMaxTimestamp:
                case kRPVControllerSampleTimestamp:
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
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerSampleValue:
            case kRPVControllerSampleId:
            case kRPVControllerSampleType:
            case kRPVControllerSampleNumChildren:
            case kRPVControllerSampleChildIndex:
            case kRPVControllerSampleChildMin:
            case kRPVControllerSampleChildMean:
            case kRPVControllerSampleChildMedian:
            case kRPVControllerSampleChildMax:
            case kRPVControllerSampleChildMinTimestamp:
            case kRPVControllerSampleChildMaxTimestamp:
            case kRPVControllerSampleTimestamp:
            case kRPVControllerSampleTrack:
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

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerSampleValue:
            {
                result = m_data.SetUInt64(value);
                break;
            }
            case kRPVControllerSampleId:
            case kRPVControllerSampleType:
            case kRPVControllerSampleNumChildren:
            case kRPVControllerSampleChildIndex:
            case kRPVControllerSampleChildMin:
            case kRPVControllerSampleChildMean:
            case kRPVControllerSampleChildMedian:
            case kRPVControllerSampleChildMax:
            case kRPVControllerSampleChildMinTimestamp:
            case kRPVControllerSampleChildMaxTimestamp:
            case kRPVControllerSampleTimestamp:
            case kRPVControllerSampleTrack:
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
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerSampleValue:
            {
                result = m_data.SetDouble(value);
                break;
            }
            case kRPVControllerSampleId:
            case kRPVControllerSampleType:
            case kRPVControllerSampleNumChildren:
            case kRPVControllerSampleChildIndex:
            case kRPVControllerSampleChildMin:
            case kRPVControllerSampleChildMean:
            case kRPVControllerSampleChildMedian:
            case kRPVControllerSampleChildMax:
            case kRPVControllerSampleChildMinTimestamp:
            case kRPVControllerSampleChildMaxTimestamp:
            case kRPVControllerSampleTimestamp:
            case kRPVControllerSampleTrack:
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
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleTrack:
                {
                    Handle* handle = (Handle*)value;
                    if (handle->GetType() == kRPVControllerObjectTypeTrack)
                    {
                        m_track = (Track*)value;
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultInvalidType;
                    }
                    break;
                }
                case kRPVControllerSampleValue:
                case kRPVControllerSampleId:
                case kRPVControllerSampleType:
                case kRPVControllerSampleNumChildren:
                case kRPVControllerSampleChildIndex:
                case kRPVControllerSampleChildMin:
                case kRPVControllerSampleChildMean:
                case kRPVControllerSampleChildMedian:
                case kRPVControllerSampleChildMax:
                case kRPVControllerSampleChildMinTimestamp:
                case kRPVControllerSampleChildMaxTimestamp:
                case kRPVControllerSampleTimestamp:
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
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleValue:
                {
                    result = m_data.SetString(value);
                    break;
                }
                case kRPVControllerSampleId:
                case kRPVControllerSampleType:
                case kRPVControllerSampleNumChildren:
                case kRPVControllerSampleChildIndex:
                case kRPVControllerSampleChildMin:
                case kRPVControllerSampleChildMean:
                case kRPVControllerSampleChildMedian:
                case kRPVControllerSampleChildMax:
                case kRPVControllerSampleChildMinTimestamp:
                case kRPVControllerSampleChildMaxTimestamp:
                case kRPVControllerSampleTimestamp:
                case kRPVControllerSampleTrack:
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

private:
    Data m_data;
    uint64_t m_id;
    class Track* m_track;
    double m_timestamp;
};

class SampleLOD : public Sample
{
    void CalculateChildValues(void)
    {
        m_child_min = DBL_MAX;
        m_child_max = DBL_MIN;
        m_child_mean = 0;
        m_child_median = 0;
        m_child_min_timestamp = DBL_MAX;
        m_child_max_timestamp = DBL_MIN;
        uint64_t entries = 0;
        std::vector<double> values;
        for (Sample* sample : m_children)
        {
            if (sample)
            {
                double timestamp = 0;
                double value = 0;
                if (sample->GetDouble(kRPVControllerSampleTimestamp, 0, &timestamp) == kRocProfVisResultSuccess
                    && sample->GetDouble(kRPVControllerSampleValue, 0, &value) == kRocProfVisResultSuccess)
                {
                    m_child_min_timestamp = std::min(m_child_min_timestamp, timestamp);
                    m_child_max_timestamp = std::max(m_child_max_timestamp, timestamp);
                    m_child_min = std::min(m_child_min, value);
                    m_child_max = std::max(m_child_max, value);
                    m_child_mean += value;
                    entries++;
                    values.push_back(value);
                }
            }
        }
        uint64_t num_children = values.size();
        if (num_children & 0x1)
        {
            m_child_median = values[num_children / 2];
        }
        else
        {
            m_child_median = (values[num_children / 2] + values[(num_children / 2) + 1]) / 2;
        }
        m_child_mean /= entries;
    }

public:
    SampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp)
    : Sample(type, id, timestamp)
    , m_child_min(0)
    , m_child_mean(0)
    , m_child_median(0)
    , m_child_max(0)
    , m_child_min_timestamp(0)
    , m_child_max_timestamp(0)
    {
    }

    SampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp, std::vector<Sample*>& children)
    : Sample(type, id, timestamp)
    , m_children(children)
    , m_child_min(0)
    , m_child_mean(0)
    , m_child_median(0)
    , m_child_max(0)
    , m_child_min_timestamp(0)
    , m_child_max_timestamp(0)
    {
        // Calculate the child min/mean/median/max/mints/maxts
        CalculateChildValues();
    }

    virtual ~SampleLOD()
    {
    }

    rocprofvis_controller_object_type_t GetType(void) final
    {
        return kRPVControllerObjectTypeSample;
    }

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleNumChildren:
                {
                    *value = m_children.size();
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleId:
                case kRPVControllerSampleType:
                case kRPVControllerSampleValue:
                case kRPVControllerSampleChildIndex:
                case kRPVControllerSampleChildMin:
                case kRPVControllerSampleChildMean:
                case kRPVControllerSampleChildMedian:
                case kRPVControllerSampleChildMax:
                case kRPVControllerSampleChildMinTimestamp:
                case kRPVControllerSampleChildMaxTimestamp:
                case kRPVControllerSampleTimestamp:
                case kRPVControllerSampleTrack:
                {
                    result = Sample::GetUInt64(property, index, value);
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
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleChildMin:
                {
                    *value = m_child_min;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMean:
                {
                    *value = m_child_mean;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMedian:
                {
                    *value = m_child_median;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMax:
                {
                    *value = m_child_max;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMinTimestamp:
                {
                    *value = m_child_min_timestamp;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleChildMaxTimestamp:
                {
                    *value = m_child_max_timestamp;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerSampleTimestamp:
                case kRPVControllerSampleValue:
                case kRPVControllerSampleId:
                case kRPVControllerSampleType:
                case kRPVControllerSampleNumChildren:
                case kRPVControllerSampleChildIndex:
                case kRPVControllerSampleTrack:
                {
                    result = Sample::GetDouble(property, index, value);
                    break;
                }
                default:
                {
                    break;
                }
            }
        }
        return result;
    }
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleChildIndex:
                {
                    if (index < m_children.size())
                    {
                        *value = (rocprofvis_handle_t*)m_children[index];
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                    break;
                }
                case kRPVControllerSampleTrack:
                case kRPVControllerSampleValue:
                case kRPVControllerSampleId:
                case kRPVControllerSampleType:
                case kRPVControllerSampleNumChildren:
                case kRPVControllerSampleChildMin:
                case kRPVControllerSampleChildMean:
                case kRPVControllerSampleChildMedian:
                case kRPVControllerSampleChildMax:
                case kRPVControllerSampleChildMinTimestamp:
                case kRPVControllerSampleChildMaxTimestamp:
                case kRPVControllerSampleTimestamp:
                {
                    result = Sample::GetObject(property, index, value);
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
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final
    {
        return Sample::GetString(property, index, value, length);
    }

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerSampleNumChildren:
            {
                if (value != m_children.size())
                {
                    m_children.resize(value);
                }
                CalculateChildValues();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildIndex:
            case kRPVControllerSampleChildMin:
            case kRPVControllerSampleChildMean:
            case kRPVControllerSampleChildMedian:
            case kRPVControllerSampleChildMax:
            case kRPVControllerSampleChildMinTimestamp:
            case kRPVControllerSampleChildMaxTimestamp:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            case kRPVControllerSampleValue:
            case kRPVControllerSampleId:
            case kRPVControllerSampleType:
            case kRPVControllerSampleTimestamp:
            case kRPVControllerSampleTrack:
            {
                result = Sample::SetUInt64(property, index, value);
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
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerSampleChildMin:
            {
                m_child_min = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMean:
            {
                m_child_mean = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMedian:
            {
                m_child_median = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMax:
            {
                m_child_max = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMinTimestamp:
            {
                m_child_min_timestamp = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMaxTimestamp:
            {
                m_child_max_timestamp = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleValue:
            case kRPVControllerSampleId:
            case kRPVControllerSampleType:
            case kRPVControllerSampleNumChildren:
            case kRPVControllerSampleChildIndex:
            case kRPVControllerSampleTimestamp:
            case kRPVControllerSampleTrack:
            {
                result = Sample::SetDouble(property, index, value);
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
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleChildIndex:
                {
                    if (index < m_children.size())
                    {
                        m_children[index] = (Sample*)value;                    
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                    break;
                }
                case kRPVControllerSampleValue:
                case kRPVControllerSampleId:
                case kRPVControllerSampleType:
                case kRPVControllerSampleNumChildren:
                case kRPVControllerSampleChildMin:
                case kRPVControllerSampleChildMean:
                case kRPVControllerSampleChildMedian:
                case kRPVControllerSampleChildMax:
                case kRPVControllerSampleChildMinTimestamp:
                case kRPVControllerSampleChildMaxTimestamp:
                case kRPVControllerSampleTimestamp:
                case kRPVControllerSampleTrack:
                {
                    result = Sample::SetObject(property, index, value);
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
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerSampleValue:
                case kRPVControllerSampleId:
                case kRPVControllerSampleType:
                case kRPVControllerSampleNumChildren:
                case kRPVControllerSampleChildIndex:
                case kRPVControllerSampleChildMin:
                case kRPVControllerSampleChildMean:
                case kRPVControllerSampleChildMedian:
                case kRPVControllerSampleChildMax:
                case kRPVControllerSampleChildMinTimestamp:
                case kRPVControllerSampleChildMaxTimestamp:
                case kRPVControllerSampleTimestamp:
                case kRPVControllerSampleTrack:
                {
                    result = Sample::SetString(property, index, value, length);
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

private:
    std::vector<Sample*> m_children;
    double m_child_min;
    double m_child_mean;
    double m_child_median;
    double m_child_max;
    double m_child_min_timestamp;
    double m_child_max_timestamp;
};

class Event : public Handle
{
public:
    Event(uint64_t id, double start_ts, double end_ts)
    : m_id(id)
    , m_start_timestamp(start_ts)
    , m_end_timestamp(end_ts)
    {
    }

    virtual ~Event()
    {

    }

    rocprofvis_controller_object_type_t GetType(void) override
    {
        return kRPVControllerObjectTypeEvent;
    }

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerEventId:
                {
                    *value = m_id;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventNumCallstackEntries:
                {
                    *value = m_callstack.size();
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventNumInputFlowControl:
                {
                    *value = m_input_flow_control.size();
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventNumOutputFlowControl:
                {
                    *value = m_output_flow_control.size();
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventNumChildren:
                {
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventTrack:
                case kRPVControllerEventStartTimestamp:
                case kRPVControllerEventEndTimestamp:
                case kRPVControllerEventName:
                case kRPVControllerEventInputFlowControlIndexed:
                case kRPVControllerEventOutputFlowControlIndexed:
                case kRPVControllerEventChildIndexed:
                case kRPVControllerEventCallstackEntryIndexed:
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
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerEventStartTimestamp:
                {
                    *value = m_start_timestamp;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventEndTimestamp:
                {
                    *value = m_end_timestamp;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventId:
                case kRPVControllerEventNumCallstackEntries:
                case kRPVControllerEventNumInputFlowControl:
                case kRPVControllerEventNumOutputFlowControl:
                case kRPVControllerEventNumChildren:
                case kRPVControllerEventTrack:
                case kRPVControllerEventName:
                case kRPVControllerEventInputFlowControlIndexed:
                case kRPVControllerEventOutputFlowControlIndexed:
                case kRPVControllerEventChildIndexed:
                case kRPVControllerEventCallstackEntryIndexed:
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
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerEventTrack:
                {
                    *value = (rocprofvis_handle_t*)m_track;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventInputFlowControlIndexed:
                {
                    if (index < m_input_flow_control.size())
                    {
                        *value = (rocprofvis_handle_t*)m_input_flow_control[index];
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                    break;
                }
                case kRPVControllerEventOutputFlowControlIndexed:
                {
                    if (index < m_output_flow_control.size())
                    {
                        *value = (rocprofvis_handle_t*)m_output_flow_control[index];
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                    break;
                }
                case kRPVControllerEventCallstackEntryIndexed:
                {
                    if (index < m_callstack.size())
                    {
                        *value = (rocprofvis_handle_t*)m_callstack[index];
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                    break;
                }
                case kRPVControllerEventStartTimestamp:
                case kRPVControllerEventEndTimestamp:
                case kRPVControllerEventId:
                case kRPVControllerEventNumCallstackEntries:
                case kRPVControllerEventNumInputFlowControl:
                case kRPVControllerEventNumOutputFlowControl:
                case kRPVControllerEventNumChildren:
                case kRPVControllerEventName:
                case kRPVControllerEventChildIndexed:
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
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerEventName:
                {
                    if (length && (*value || *length == 0))
                    {
                        *length = m_name.length();
                        result = kRocProfVisResultSuccess;
                    }
                    else if (length && value && *length > 0)
                    {
                        strncpy(value, m_name.c_str(), *length);
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultInvalidArgument;
                    }
                    break;
                }
                case kRPVControllerEventTrack:
                case kRPVControllerEventStartTimestamp:
                case kRPVControllerEventEndTimestamp:
                case kRPVControllerEventId:
                case kRPVControllerEventNumCallstackEntries:
                case kRPVControllerEventNumInputFlowControl:
                case kRPVControllerEventNumOutputFlowControl:
                case kRPVControllerEventNumChildren:
                case kRPVControllerEventInputFlowControlIndexed:
                case kRPVControllerEventOutputFlowControlIndexed:
                case kRPVControllerEventChildIndexed:
                case kRPVControllerEventCallstackEntryIndexed:
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

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerEventId:
            {
                m_id = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumCallstackEntries:
            {
                if (value != m_callstack.size())
                {
                    m_callstack.resize(value);
                }
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumInputFlowControl:
            {
                if (value != m_input_flow_control.size())
                {
                    m_input_flow_control.resize(value);
                }
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumOutputFlowControl:
            {
                if (value != m_output_flow_control.size())
                {
                    m_output_flow_control.resize(value);
                }
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventNumChildren:
            {
                result = kRocProfVisResultReadOnlyError;
                break;
            }
            case kRPVControllerEventTrack:
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventName:
            case kRPVControllerEventInputFlowControlIndexed:
            case kRPVControllerEventOutputFlowControlIndexed:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
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
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerEventStartTimestamp:
            {
                m_start_timestamp = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventEndTimestamp:
            {
                m_end_timestamp = value;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventId:
            case kRPVControllerEventNumCallstackEntries:
            case kRPVControllerEventNumInputFlowControl:
            case kRPVControllerEventNumOutputFlowControl:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventTrack:
            case kRPVControllerEventName:
            case kRPVControllerEventInputFlowControlIndexed:
            case kRPVControllerEventOutputFlowControlIndexed:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
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
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerEventTrack:
                {
                    Handle* handle = (Handle*)value;
                    if (handle->GetType() == kRPVControllerObjectTypeTrack)
                    {
                        m_track = (Track*)value;
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultInvalidType;
                    }
                    break;
                }
                case kRPVControllerEventInputFlowControlIndexed:
                {
                    Handle* handle = (Handle*)value;
                    if (handle->GetType() == kRPVControllerObjectTypeFlowControl)
                    {
                        if (index < m_input_flow_control.size())
                        {
                            m_input_flow_control[index] = (FlowControl*)value;
                            result = kRocProfVisResultSuccess;
                        }
                        else
                        {
                            result = kRocProfVisResultOutOfRange;
                        }
                    }
                    break;
                }
                case kRPVControllerEventOutputFlowControlIndexed:
                {
                    Handle* handle = (Handle*)value;
                    if (handle->GetType() == kRPVControllerObjectTypeFlowControl)
                    {
                        if (index < m_output_flow_control.size())
                        {
                            m_output_flow_control[index] = (FlowControl*)value;
                            result = kRocProfVisResultSuccess;
                        }
                        else
                        {
                            result = kRocProfVisResultOutOfRange;
                        }
                    }
                    break;
                }
                case kRPVControllerEventCallstackEntryIndexed:
                {
                    Handle* handle = (Handle*)value;
                    if (handle->GetType() == kRPVControllerObjectTypeCallstack)
                    {
                        if (index < m_callstack.size())
                        {
                            m_callstack[index] = (Callstack*)value;
                            result = kRocProfVisResultSuccess;
                        }
                        else
                        {
                            result = kRocProfVisResultOutOfRange;
                        }
                    }
                    break;
                }
                case kRPVControllerEventStartTimestamp:
                case kRPVControllerEventEndTimestamp:
                case kRPVControllerEventId:
                case kRPVControllerEventNumCallstackEntries:
                case kRPVControllerEventNumInputFlowControl:
                case kRPVControllerEventNumOutputFlowControl:
                case kRPVControllerEventNumChildren:
                case kRPVControllerEventName:
                case kRPVControllerEventChildIndexed:
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
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) override
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerEventName:
                {
                    m_name = value;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventTrack:
                case kRPVControllerEventStartTimestamp:
                case kRPVControllerEventEndTimestamp:
                case kRPVControllerEventId:
                case kRPVControllerEventNumCallstackEntries:
                case kRPVControllerEventNumInputFlowControl:
                case kRPVControllerEventNumOutputFlowControl:
                case kRPVControllerEventNumChildren:
                case kRPVControllerEventInputFlowControlIndexed:
                case kRPVControllerEventOutputFlowControlIndexed:
                case kRPVControllerEventChildIndexed:
                case kRPVControllerEventCallstackEntryIndexed:
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

private:
    Track* m_track;
    uint64_t m_id;
    double m_start_timestamp;
    double m_end_timestamp;
    std::string m_name;
    std::vector<Callstack*> m_callstack;
    std::vector<FlowControl*> m_input_flow_control;
    std::vector<FlowControl*> m_output_flow_control;
};

class EventLOD : public Event
{
public:
    EventLOD(uint64_t id, double start_ts, double end_ts)
    : Event(id, start_ts, end_ts)
    {
    }

    virtual ~EventLOD()
    {

    }

    rocprofvis_controller_object_type_t GetType(void) final
    {
        return kRPVControllerObjectTypeEvent;
    }

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerEventNumChildren:
                {
                    *value = m_children.size();
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventId:
                case kRPVControllerEventNumCallstackEntries:
                case kRPVControllerEventNumInputFlowControl:
                case kRPVControllerEventNumOutputFlowControl:
                case kRPVControllerEventTrack:
                case kRPVControllerEventStartTimestamp:
                case kRPVControllerEventEndTimestamp:
                case kRPVControllerEventName:
                case kRPVControllerEventInputFlowControlIndexed:
                case kRPVControllerEventOutputFlowControlIndexed:
                case kRPVControllerEventChildIndexed:
                case kRPVControllerEventCallstackEntryIndexed:
                default:
                {
                    result = Event::GetUInt64(property, index, value);
                    break;
                }
            }
        }
        return result;
    }
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final
    {
        return Event::GetDouble(property, index, value);
    }
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerEventChildIndexed:
                {
                    if (index < m_children.size())
                    {
                        *value = (rocprofvis_handle_t*)m_children[index];
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                    break;
                }
                case kRPVControllerEventTrack:
                case kRPVControllerEventStartTimestamp:
                case kRPVControllerEventEndTimestamp:
                case kRPVControllerEventId:
                case kRPVControllerEventNumCallstackEntries:
                case kRPVControllerEventNumInputFlowControl:
                case kRPVControllerEventNumOutputFlowControl:
                case kRPVControllerEventNumChildren:
                case kRPVControllerEventName:
                case kRPVControllerEventInputFlowControlIndexed:
                case kRPVControllerEventOutputFlowControlIndexed:
                case kRPVControllerEventCallstackEntryIndexed:
                default:
                {
                    result = Event::GetObject(property, index, value);
                    break;
                }
            }
        }
        return result;
    }
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final
    {
        return Event::GetString(property, index, value, length);
    }

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerEventNumChildren:
            {
                if (value != m_children.size())
                {
                    m_children.resize(value);
                }
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerEventId:
            case kRPVControllerEventNumCallstackEntries:
            case kRPVControllerEventNumInputFlowControl:
            case kRPVControllerEventNumOutputFlowControl:
            case kRPVControllerEventTrack:
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventName:
            case kRPVControllerEventInputFlowControlIndexed:
            case kRPVControllerEventOutputFlowControlIndexed:
            case kRPVControllerEventChildIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
            default:
            {
                result = Event::SetUInt64(property, index, value);
                break;
            }
        }
        return result;
    }
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final
    {
        return Event::SetDouble(property, index, value);
    }
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerEventChildIndexed:
            {
                Handle* handle = (Handle*)value;
                if (index < m_children.size() && (handle->GetType() == kRPVControllerObjectTypeEvent))
                {
                    m_children[index] = (Event*)value;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerEventTrack:
            case kRPVControllerEventStartTimestamp:
            case kRPVControllerEventEndTimestamp:
            case kRPVControllerEventId:
            case kRPVControllerEventNumCallstackEntries:
            case kRPVControllerEventNumInputFlowControl:
            case kRPVControllerEventNumOutputFlowControl:
            case kRPVControllerEventNumChildren:
            case kRPVControllerEventName:
            case kRPVControllerEventInputFlowControlIndexed:
            case kRPVControllerEventOutputFlowControlIndexed:
            case kRPVControllerEventCallstackEntryIndexed:
            default:
            {
                result = Event::SetObject(property, index, value);
                break;
            }
        }
        return result;
    }
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final
    {
        return Event::SetString(property, index, value, length);
    }

private:
    std::vector<Event*> m_children;
};

class Segment
{
    class LOD
    {
    public:
        LOD()
        : m_valid(false)
        {
        }

        LOD(LOD const& other)
        { 
            operator=(other);
        }

        LOD(LOD&& other)
        {
            operator=(other);
        }
        
        ~LOD()
        {
            for (auto& pair : m_entries)
            {
                delete pair.second;
            }
        }

        LOD& operator=(LOD const& other)
        {
            if (this != &other)
            {
                m_entries = other.m_entries;
                m_valid   = other.m_valid;
            }
            return *this;
        }

        LOD& operator=(LOD&& other)
        {
            if(this != &other)
            {
                m_entries = other.m_entries;
                m_valid   = other.m_valid;
            }
            return *this;
        }

        std::multimap<double, Handle*>& GetEntries()
        {
            return m_entries;
        }

        void SetValid(bool valid)
        {
            m_valid = valid;
        }

        bool IsValid() const
        {
            return m_valid;
        }

    private:
        std::multimap<double, Handle*> m_entries;
        bool m_valid;
    };

    void GenerateEventLOD(std::vector<Event*>& events, double event_start, double event_end, uint32_t lod_to_generate)
    {
        Event* new_event = new EventLOD(0, event_start, event_end);
        if (new_event)
        {
            std::multimap<double, Handle*>& new_events = m_lods[lod_to_generate]->GetEntries();
            new_events.insert(std::make_pair(event_start, new_event));
        }
    }

    void GenerateSampleLOD(std::vector<Sample*>& samples, rocprofvis_controller_primitive_type_t type, double timestamp, uint32_t lod_to_generate)
    {
        SampleLOD* new_sample = new SampleLOD(type, 0, timestamp, samples);
        if (new_sample)
        {
            std::multimap<double, Handle*>& new_samples = m_lods[lod_to_generate]->GetEntries();
            new_samples.insert(std::make_pair(timestamp, new_sample));
        }
    }

public:
    Segment()
    : m_start_timestamp(0.0)
    , m_end_timestamp(0.0)
    , m_min_timestamp(0.0)
    , m_max_timestamp(0.0)
    , m_type(kRPVControllerTrackTypeSamples)
    {
    }

    Segment(rocprofvis_controller_track_type_t type)
    : m_start_timestamp(0.0)
    , m_end_timestamp(0.0)
    , m_min_timestamp(0.0)
    , m_max_timestamp(0.0)
    , m_type(type)
    {
    }

    ~Segment()
    { 
        for (auto& pair : m_lods)
        {
            delete pair.second;
        }
    }

    void GenerateLOD(uint32_t lod_to_generate)
    {
        if (lod_to_generate > 0)
        {
            uint32_t previous_lod = (uint32_t)(lod_to_generate - 1);
            LOD* lod = m_lods[previous_lod];
            std::multimap<double, Handle*>& values = lod->GetEntries();
            if(!lod->IsValid() && values.size() > 1)
            {
                double scale = 1.0;
                for(uint32_t i = 0; i < lod_to_generate; i++)
                {
                    scale *= 1000.0;
                }
                double min_ts = m_min_timestamp;
                double max_ts = m_min_timestamp + scale;
                if (m_type = kRPVControllerTrackTypeEvents)
                {
                    std::vector<Event*> events;
                    for(auto& pair : values)
                    {
                        Event* event = (Event*)pair.second;
                        if (event)
                        {
                            double event_start = pair.first;
                            double event_end = 0.0;
                            if (event->GetDouble(kRPVControllerEventEndTimestamp, 0, &event_end) == kRocProfVisResultSuccess)
                            {
                                if ((event_start >= min_ts && event_start <= max_ts)
                                && (event_end >= min_ts && event_end <= max_ts))
                                {
                                    // Merge into the current event
                                    events.push_back(event);
                                }
                                else
                                {
                                    // We assume that the events are ordered by time, so this must at least end after the current event
                                    assert(event_end > max_ts);

                                    // Generate the stub event for any populated events.
                                    if ((events.front()->GetDouble(kRPVControllerEventStartTimestamp, 0, &event_start) == kRocProfVisResultSuccess)
                                    && (events.back()->GetDouble(kRPVControllerEventEndTimestamp, 0, &event_end) == kRocProfVisResultSuccess))
                                    {
                                        GenerateEventLOD(events, event_start, event_end, lod_to_generate);
                                    }

                                    // Create a new event & increment the search
                                    min_ts = std::min(min_ts + scale, m_max_timestamp);
                                    max_ts = std::min(max_ts + scale, m_max_timestamp);
                                    events.clear();
                                    events.push_back(event);
                                }
                            }
                        }
                    }
                
                    if (events.size())
                    {
                        double event_start = 0.0;
                        double event_end = 0.0;
                        if ((events.front()->GetDouble(kRPVControllerEventStartTimestamp, 0, &event_start) == kRocProfVisResultSuccess)
                        && (events.back()->GetDouble(kRPVControllerEventEndTimestamp, 0, &event_end) == kRocProfVisResultSuccess))
                        {
                            GenerateEventLOD(events, event_start, event_end, lod_to_generate);
                        }
                    }
                }
                else
                {
                    std::vector<Sample*> samples;
                    for(auto& pair : values)
                    {
                        Sample* sample = (Sample*)pair.second;
                        if (sample)
                        {
                            double sample_start = pair.first;
                            {
                                if (sample_start >= min_ts && sample_start <= max_ts)
                                {
                                    // Merge into the current sample
                                    samples.push_back(sample);
                                }
                                else
                                {
                                    // We assume that the events are ordered by time, so this must at least end after the current sample
                                    assert(sample_start > max_ts);

                                    // Generate the stub event for any populated events.
                                    uint64_t type = 0;
                                    if (sample->GetUInt64(kRPVControllerSampleType, 0, &type) == kRocProfVisResultSuccess)
                                    {
                                        GenerateSampleLOD(samples, (rocprofvis_controller_primitive_type_t)type, sample_start, lod_to_generate);
                                    }

                                    // Create a new sample & increment the search
                                    min_ts = std::min(min_ts + scale, m_max_timestamp);
                                    max_ts = std::min(max_ts + scale, m_max_timestamp);
                                    samples.clear();
                                    samples.push_back(sample);
                                }
                            }
                        }
                    }
                }

                lod->SetValid(true);
            }
        }
    }

    double GetStartTimestamp()
    {
        return m_start_timestamp;
    }
    double GetEndTimestamp()
    {
        return m_end_timestamp;
    }
    double GetMinTimestamp()
    {
        return m_min_timestamp;
    }
    double GetMaxTimestamp()
    {
        return m_max_timestamp;
    }

    void SetStartEndTimestamps(double start, double end)
    {
        m_start_timestamp = start;
        m_end_timestamp = end;
    }

    void SetMinTimestamp(double value)
    {
        m_min_timestamp = value;
    }

    void SetMaxTimestamp(double value)
    {
        m_max_timestamp = value;
    }

    void Insert(double timestamp, Handle* event)
    {
        if (m_lods.find(0) == m_lods.end())
        {
            // LOD0 is always valid or nothing will work.
            m_lods[0] = new LOD();
            m_lods[0]->SetValid(true);
        }
        m_lods[0]->GetEntries().insert(std::make_pair(timestamp, event));
    }

    rocprofvis_result_t Fetch(uint32_t lod, double start, double end, Array& array, uint64_t& index)
    {
        rocprofvis_result_t result = kRocProfVisResultOutOfRange;
        if (m_lods.find(lod) != m_lods.end()
           && (start <= m_start_timestamp && end >= m_end_timestamp) || (start >= m_start_timestamp && start < m_end_timestamp) || (end > m_start_timestamp && end <= m_end_timestamp))
        {
            auto& lod_ref = m_lods[lod];
            auto& entries = lod_ref->GetEntries();
            auto lower = std::lower_bound(entries.begin(), entries.end(), start, [](std::pair<double, Handle*> const& pair, double const& start) -> bool
            {
                double max_ts = pair.first;
                pair.second->GetDouble(kRPVControllerEventStartTimestamp, 0, &max_ts);

                bool result = max_ts < start;
                return result;
            });
            auto upper = std::upper_bound(entries.begin(), entries.end(), end, [](double const& end, std::pair<double, Handle*> const& pair) -> bool
            {
                bool result = end <= pair.first;
                return result;
            });
            while (lower != upper)
            {
                result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, index + 1);
                if(result == kRocProfVisResultSuccess)
                {
                    result = array.SetObject(kRPVControllerArrayEntryIndexed, index++, (rocprofvis_handle_t*)lower->second);
                    if(result == kRocProfVisResultSuccess)
                    {
                        ++lower;
                    }
                    else
                    {
                        break;
                    }
                }
                else
                {
                    break;
                }
            }
        }
        return result;
    }

private:
    std::map<uint32_t, LOD*> m_lods;
    double m_start_timestamp;
    double m_end_timestamp;
    double m_min_timestamp;
    double m_max_timestamp;
    rocprofvis_controller_track_type_t m_type;
};

class Track : public Handle
{
public:
    Track(rocprofvis_controller_track_type_t type)
    : m_id(0)
    , m_num_elements(0)
    , m_type(type)
    , m_start_timestamp(DBL_MIN)
    , m_end_timestamp(DBL_MAX)
    {

    }

    virtual ~Track()
    {
        for (auto& pair : m_segments)
        {
            delete pair.second;
        }
    }

    rocprofvis_result_t Fetch(uint32_t lod, double start, double end, Array& array, uint64_t& index)
    {
        rocprofvis_result_t result = kRocProfVisResultOutOfRange;
        if((start <= m_start_timestamp && end >= m_end_timestamp) || (start >= m_start_timestamp && start < m_end_timestamp) || (end > m_start_timestamp && end <= m_end_timestamp))
        {
            auto lower = std::lower_bound(m_segments.begin(), m_segments.end(), std::max(m_start_timestamp, start), [](std::pair<double, Segment*> const& pair, double const& start) -> bool
            {
                bool result = (pair.second->GetMaxTimestamp() < start);
                return result;
            });
            auto upper = std::upper_bound(m_segments.begin(), m_segments.end(), std::min(m_end_timestamp, end), [](double const& end, std::pair<double, Segment*> const& pair) -> bool
            {
                bool result = (end <= pair.first);
                return result;
            });
            while (lower != upper)
            {
                result = lower->second->Fetch(lod, start, end, array, index);
                if (result == kRocProfVisResultSuccess)
                {
                    ++lower;
                }
                else if (result == kRocProfVisResultOutOfRange)
                {
                    ++lower;
                }
                else
                {
                    break;
                }
            }
        }
        return result;
    }

    rocprofvis_controller_object_type_t GetType(void) final
    {
        return kRPVControllerObjectTypeTrack;
    }

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerTrackId:
                {
                    *value = m_id;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTrackType:
                {
                    *value = m_type;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTrackNumberOfEntries:
                {
                    *value = m_num_elements;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTrackGraph:
                case kRPVControllerTrackMinTimestamp:
                case kRPVControllerTrackMaxTimestamp:
                case kRPVControllerTrackEntry:
                case kRPVControllerTrackName:
                default:
                {
                    result = kRocProfVisResultNotSupported;
                    break;
                }
            }
        }
        return result;
    }
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerTrackMinTimestamp:
                {
                    *value = m_start_timestamp;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTrackMaxTimestamp:
                {
                    *value = m_end_timestamp;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTrackId:
                case kRPVControllerTrackType:
                case kRPVControllerTrackNumberOfEntries:
                case kRPVControllerTrackGraph:
                case kRPVControllerTrackEntry:
                case kRPVControllerTrackName:
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
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final
    {
        assert(0);
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerTrackGraph:
                {
                    *value = nullptr;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTrackEntry:
                {
                    result = kRocProfVisResultNotSupported;
                    break;
                }
                case kRPVControllerTrackId:
                case kRPVControllerTrackType:
                case kRPVControllerTrackNumberOfEntries:
                case kRPVControllerTrackMinTimestamp:
                case kRPVControllerTrackMaxTimestamp:
                case kRPVControllerTrackName:
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
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerTrackName:
            {
                if (length && (!value || *length == 0))
                {
                    *length = m_name.length();
                    result = kRocProfVisResultSuccess;
                }
                else if (value && length && *length > 0)
                {
                    strncpy(value, m_name.c_str(), *length);
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerTrackMinTimestamp:
            case kRPVControllerTrackMaxTimestamp:
            case kRPVControllerTrackId:
            case kRPVControllerTrackType:
            case kRPVControllerTrackNumberOfEntries:
            case kRPVControllerTrackGraph:
            case kRPVControllerTrackEntry:
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

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerTrackId:
            {
                m_id = value;
                break;
            }
            case kRPVControllerTrackType:
            {
                m_type = (rocprofvis_controller_track_type_t)value;
                break;
            }
            case kRPVControllerTrackNumberOfEntries:
            {
                m_num_elements = value;
                break;
            }
            case kRPVControllerTrackMinTimestamp:
            case kRPVControllerTrackMaxTimestamp:
            case kRPVControllerTrackGraph:
            case kRPVControllerTrackEntry:
            case kRPVControllerTrackName:
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
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        switch(property)
        {
            case kRPVControllerTrackMinTimestamp:
            {
                m_start_timestamp = value;
                break;
            }
            case kRPVControllerTrackMaxTimestamp:
            {
                m_end_timestamp = value;
                break;
            }
            case kRPVControllerTrackGraph:
            case kRPVControllerTrackEntry:
            case kRPVControllerTrackId:
            case kRPVControllerTrackType:
            case kRPVControllerTrackNumberOfEntries:
            case kRPVControllerTrackName:
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
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerTrackGraph:
                {
                    assert(0);
                    result = kRocProfVisResultUnknownError;
                    break;
                }
                case kRPVControllerTrackEntry:
                {
                    // Start & end timestamps must be configured
                    assert(m_start_timestamp >= 0.0 && m_start_timestamp < m_end_timestamp);

                    Handle* object = (Handle*)value;
                    auto object_type = object->GetType();
                    if (((m_type == kRPVControllerTrackTypeEvents) && (object_type == kRPVControllerObjectTypeEvent))
                        || ((m_type == kRPVControllerTrackTypeSamples) && (object_type == kRPVControllerObjectTypeSample)))
                    {
                        rocprofvis_property_t property;
                        if (object_type == kRPVControllerObjectTypeEvent)
                        {
                            property = kRPVControllerEventStartTimestamp;
                        }
                        else
                        {
                            property = kRPVControllerSampleTimestamp;
                        }

                        double timestamp = 0;
                        result = object->GetDouble(property, 0, &timestamp);
                        if (result == kRocProfVisResultSuccess)
                        {
                            if (timestamp >= m_start_timestamp && timestamp <= m_end_timestamp)
                            {
                                double segment_duration = 10000.0;
                                double relative = (timestamp - m_start_timestamp);
                                double num_segments = floor(relative / segment_duration);
                                double segment_start = m_start_timestamp + (num_segments * segment_duration);

                                if (m_segments.find(segment_start) == m_segments.end())
                                {
                                    double segment_end = segment_start + segment_duration;
                                    Segment* segment = new Segment;
                                    segment->SetStartEndTimestamps(segment_start, segment_end);
                                    segment->SetMinTimestamp(timestamp); 
                                    if (object_type == kRPVControllerObjectTypeEvent)
                                    {
                                        property = kRPVControllerEventStartTimestamp;
                                        double end_timestamp = timestamp;
                                        object->GetDouble(kRPVControllerEventEndTimestamp, 0, &end_timestamp);
                                        segment->SetMaxTimestamp(end_timestamp);
                                    }
                                    else
                                    {
                                        segment->SetMaxTimestamp(timestamp);
                                    }
                                    m_segments.insert(std::make_pair(segment_start, segment));
                                    result = (m_segments.find(segment_start) != m_segments.end()) ? kRocProfVisResultSuccess : kRocProfVisResultMemoryAllocError;
                                }

                                if (result == kRocProfVisResultSuccess)
                                {
                                    Segment* segment = m_segments[segment_start];
                                    segment->SetMinTimestamp(std::min(segment->GetMinTimestamp(), timestamp));
                                    double end_timestamp = timestamp;
                                    if (object_type == kRPVControllerObjectTypeEvent)
                                    {   
                                        object->GetDouble(kRPVControllerEventEndTimestamp, 0, &end_timestamp); 
                                    }
                                    segment->SetMaxTimestamp(std::max(segment->GetMaxTimestamp(), end_timestamp));
                                    segment->Insert(timestamp, object);
                                }
                            }
                            else
                            {
                                result = kRocProfVisResultOutOfRange;
                            }
                        }
                    }
                    break;
                }
                case kRPVControllerTrackId:
                case kRPVControllerTrackType:
                case kRPVControllerTrackNumberOfEntries:
                case kRPVControllerTrackMinTimestamp:
                case kRPVControllerTrackMaxTimestamp:
                case kRPVControllerTrackName:
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
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value)
        {
            switch(property)
            {
                case kRPVControllerTrackName:
                {
                    m_name = value;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTrackMinTimestamp:
                case kRPVControllerTrackMaxTimestamp:
                case kRPVControllerTrackId:
                case kRPVControllerTrackType:
                case kRPVControllerTrackNumberOfEntries:
                case kRPVControllerTrackGraph:
                case kRPVControllerTrackEntry:
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

private:
    uint64_t m_id;
    uint64_t m_num_elements;
    rocprofvis_controller_track_type_t m_type;
    std::map<double, Segment*> m_segments;
    double m_start_timestamp;
    double m_end_timestamp;
    std::string m_name;
};

class Future : public Handle
{
public:
    Future() {}
    virtual ~Future() {}

    rocprofvis_controller_object_type_t GetType(void) final
    {
        return kRPVControllerObjectTypeFuture;
    }

    void Set(std::future<rocprofvis_result_t>&& future)
    {
        assert(future.valid() && !m_future.valid());
        m_future = std::move(future);
    }

    bool IsValid() const
    {
        return m_future.valid();
    }

    rocprofvis_result_t Wait(float timeout)
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if (timeout == FLT_MAX)
        {
            m_future.wait();
            result = kRocProfVisResultSuccess;
        }
        else
        {
            std::chrono::microseconds time =
                std::chrono::microseconds(uint64_t(timeout * 1000000.0));
            std::future_status   status = m_future.wait_for(time);
            if (status == std::future_status::timeout || status == std::future_status::deferred)
            {
                result = kRocProfVisResultTimeout;
            }
            else
            {
                result = kRocProfVisResultSuccess;
            }
        }
        return result;
    }

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if (value && (property == kRPVControllerFutureResult))
        {
            switch (property)
            {
                case kRPVControllerFutureResult:
                {
                    if(m_future.valid())
                    {
                        *value = m_future.get();
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultUnknownError;
                    }
                    break;
                }
                case kRPVControllerFutureType:
                {
                    *value = m_object.GetType();
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerFutureObject:
                {
                    result = m_object.GetUInt64(value);
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
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if(value && (property == kRPVControllerFutureResult))
        {
            switch(property)
            {
                case kRPVControllerFutureObject:
                {
                    result = m_object.GetDouble(value);
                    break;
                }
                case kRPVControllerFutureResult:
                case kRPVControllerFutureType:
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
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if(value && (property == kRPVControllerFutureResult))
        {
            switch(property)
            {
                case kRPVControllerFutureObject:
                {
                    result = m_object.GetObject(value);
                    break;
                }
                case kRPVControllerFutureResult:
                case kRPVControllerFutureType:
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
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final
    {
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
        if(value && (property == kRPVControllerFutureResult))
        {
            switch(property)
            {
                case kRPVControllerFutureObject:
                {
                    result = m_object.GetString(value, length);
                    break;
                }
                case kRPVControllerFutureResult:
                case kRPVControllerFutureType:
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

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
private:
    std::future<rocprofvis_result_t> m_future;
    Data m_object;
};

class Timeline : public Handle
{
public:
    Timeline()
    : m_id(0)
    {
    }

    ~Timeline()
    {
        for(Track* track : m_tracks)
        {
            delete track;
        }
    }

    rocprofvis_result_t AsyncFetch(Track& track, Future& future, Array& array,
                                   double start, double end)
    {
        rocprofvis_result_t error = kRocProfVisResultUnknownError;

        future.Set(std::async(
            std::launch::async, [&track, &array, start, end]() -> rocprofvis_result_t {
                rocprofvis_result_t result = kRocProfVisResultUnknownError;
                uint64_t            index  = 0;
                result                     = track.Fetch(0, start, end, array, index);
                return result;
            }));

        if(future.IsValid())
        {
            error = kRocProfVisResultSuccess;
        }

        return error;
    }

    rocprofvis_result_t Load(char const* const               filename,
                             RocProfVis::Controller::Future& future)
    {
        assert(filename && strlen(filename));

        rocprofvis_result_t result   = kRocProfVisResultInvalidArgument;
        std::string         filepath = filename;
        future.Set(
            std::async(std::launch::async, [this, filepath]() -> rocprofvis_result_t {
                rocprofvis_result_t     result = kRocProfVisResultUnknownError;
                rocprofvis_trace_data_t trace_object;
                std::future<bool>       future = rocprofvis_trace_async_load_json_trace(
                    filepath.c_str(), trace_object);
                if(future.valid())
                {
                    future.wait();
                    result = future.get() ? kRocProfVisResultSuccess
                                          : kRocProfVisResultUnknownError;

                    if(result == kRocProfVisResultSuccess)
                    {
                        uint64_t event_id  = 0;
                        uint64_t sample_id = 0;
                        uint64_t track_id  = 0;
                        for(auto& pair : trace_object.m_trace_data)
                        {
                            std::string const& name = pair.first;
                            auto const&        data = pair.second;

                            for(auto& thread : data.m_threads)
                            {
                                auto   type  = thread.second.m_events.size()
                                                   ? kRPVControllerTrackTypeEvents
                                                   : kRPVControllerTrackTypeSamples;
                                Track* track = new Track(type);
                                track->SetString(kRPVControllerTrackName, 0,
                                                 thread.first.c_str(),
                                                 thread.first.length());
                                track->SetUInt64(kRPVControllerTrackId, 0, track_id++);
                                switch(type)
                                {
                                    case kRPVControllerTrackTypeEvents:
                                    {
                                        track->SetUInt64(
                                            kRPVControllerTrackNumberOfEntries, 0,
                                            thread.second.m_events.size());

                                        double min_ts = DBL_MAX;
                                        double max_ts = DBL_MIN;
                                        for(auto& event : thread.second.m_events)
                                        {
                                            min_ts = std::min(event.m_start_ts, min_ts);
                                            max_ts = std::max(event.m_start_ts +
                                                                  event.m_duration,
                                                              max_ts);
                                        }
                                        track->SetDouble(kRPVControllerTrackMinTimestamp,
                                                         0, min_ts);
                                        track->SetDouble(kRPVControllerTrackMaxTimestamp,
                                                         0, max_ts);

                                        uint64_t index = 0;
                                        for(auto& event : thread.second.m_events)
                                        {
                                            Event* new_event = new Event(
                                                event_id++, event.m_start_ts,
                                                event.m_start_ts + event.m_duration);
                                            if(new_event)
                                            {
                                                result = new_event->SetObject(
                                                    kRPVControllerEventTrack, 0,
                                                    (rocprofvis_handle_t*) track);
                                                assert(result ==
                                                       kRocProfVisResultSuccess);

                                                result = new_event->SetString(
                                                    kRPVControllerEventName, 0,
                                                    event.m_name.c_str(),
                                                    event.m_name.length());
                                                assert(result ==
                                                       kRocProfVisResultSuccess);

                                                result = track->SetObject(
                                                    kRPVControllerTrackEntry, index++,
                                                    (rocprofvis_handle_t*) new_event);
                                                assert(result ==
                                                       kRocProfVisResultSuccess);
                                            }
                                        }
                                        m_tracks.push_back(track);
                                        break;
                                    }
                                    case kRPVControllerTrackTypeSamples:
                                    {
                                        track->SetUInt64(
                                            kRPVControllerTrackNumberOfEntries, 0,
                                            thread.second.m_events.size());

                                        double min_ts = DBL_MAX;
                                        double max_ts = DBL_MIN;
                                        for(auto& sample : thread.second.m_counters)
                                        {
                                            min_ts = std::min(min_ts, sample.m_start_ts);
                                            max_ts = std::max(max_ts, sample.m_start_ts);
                                        }
                                        track->SetDouble(kRPVControllerTrackMinTimestamp,
                                                         0, min_ts);
                                        track->SetDouble(kRPVControllerTrackMaxTimestamp,
                                                         0, max_ts);

                                        uint64_t index = 0;
                                        for(auto& sample : thread.second.m_counters)
                                        {
                                            Sample* new_sample = new Sample(
                                                kRPVControllerPrimitiveTypeDouble,
                                                sample_id++, sample.m_start_ts);
                                            new_sample->SetObject(
                                                kRPVControllerSampleTrack, 0,
                                                (rocprofvis_handle_t*) track);
                                            track->SetObject(
                                                kRPVControllerTrackEntry, index++,
                                                (rocprofvis_handle_t*) new_sample);
                                        }
                                        m_tracks.push_back(track);
                                        break;
                                    }
                                    default:
                                    {
                                        break;
                                    }
                                }
                            }
                        }
                    }
                }
                return result;
            }));

        if(future.IsValid())
        {
            result = kRocProfVisResultSuccess;
        }

        return result;
    }

    rocprofvis_controller_object_type_t GetType(void) final
    {
        return kRPVControllerObjectTypeTimeline;
    }

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if(value)
        {
            switch(property)
            {
                case kRPVControllerTimelineId:
                {
                    *value = m_id;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTimelineNumTracks:
                {
                    *value = m_tracks.size();
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTimelineMinTimestamp:
                case kRPVControllerTimelineMaxTimestamp:
                case kRPVControllerTimelineTrackIndexed:
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
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index,
                                  double* value) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if(value)
        {
            switch(property)
            {
                case kRPVControllerTimelineMinTimestamp:
                {
                    if(m_tracks.size())
                    {
                        result = m_tracks.front()->GetDouble(
                            kRPVControllerTrackMinTimestamp, 0, value);
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                    break;
                }
                case kRPVControllerTimelineMaxTimestamp:
                {
                    if(m_tracks.size())
                    {
                        result = m_tracks.back()->GetDouble(
                            kRPVControllerTrackMaxTimestamp, 0, value);
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                    break;
                }
                case kRPVControllerTimelineId:
                case kRPVControllerTimelineNumTracks:
                case kRPVControllerTimelineTrackIndexed:
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
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t** value) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if(value)
        {
            switch(property)
            {
                case kRPVControllerTimelineTrackIndexed:
                {
                    if (index < m_tracks.size())
                    {
                        *value = (rocprofvis_handle_t*) m_tracks[index];
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultOutOfRange;
                    }
                    break;
                }
                case kRPVControllerTimelineId:
                case kRPVControllerTimelineNumTracks:
                case kRPVControllerTimelineMinTimestamp:
                case kRPVControllerTimelineMaxTimestamp:
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
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index,
                                  char* value, uint32_t* length) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if(value)
        {
            switch(property)
            {
                case kRPVControllerTimelineId:
                case kRPVControllerTimelineNumTracks:
                case kRPVControllerTimelineMinTimestamp:
                case kRPVControllerTimelineMaxTimestamp:
                case kRPVControllerTimelineTrackIndexed:
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

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index,
                                  uint64_t value) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index,
                                  double value) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index,
                                  rocprofvis_handle_t* value) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index,
                                  char const* value, uint32_t length) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }

private:
    uint64_t            m_id;
    std::vector<Track*> m_tracks;
};

class Trace : public Handle
{
public:
    Trace()
    : m_id(0)
    , m_timeline(nullptr)
    {
    }

    virtual ~Trace()
    {
        delete m_timeline;
    }

    rocprofvis_result_t Load(char const* const filename, RocProfVis::Controller::Future& future)
    {
        assert (filename && strlen(filename));
        
        rocprofvis_result_t result = kRocProfVisResultInvalidArgument;

        m_timeline = new Timeline();
        if (m_timeline)
        {
            result = m_timeline->Load(filename, future);
        }
        else
        {
            result = kRocProfVisResultMemoryAllocError;
        }

        return result;
    }

    rocprofvis_result_t AsyncFetch(Track& track, Future& future, Array& array,
                                   double start, double end)
    {
        rocprofvis_result_t error = kRocProfVisResultUnknownError;
        if(m_timeline)
        {
            error = m_timeline->AsyncFetch(track, future, array, start, end);
        }
        return error;
    }

    rocprofvis_controller_object_type_t GetType(void) final
    {
        return kRPVControllerObjectTypeController;
    }

    // Handlers for getters.
    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final
    {
        assert(0);
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerId:
                {
                    *value = m_id;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerNumAnalysisView:
                {
                    *value = 0;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerTimeline:
                case kRPVControllerEventTable:
                case kRPVControllerAnalysisViewIndexed:
                default:
                {
                    result = kRocProfVisResultNotSupported;
                    break;
                }
            }
        }
        return result;
    }
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final
    {
        assert(0);
        return kRocProfVisResultNotSupported;
    }
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final
    {
        rocprofvis_result_t result = kRocProfVisResultUnknownError;
        if (value)
        {
            switch (property)
            {
                case kRPVControllerTimeline:
                {
                    *value = (rocprofvis_handle_t*)m_timeline;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerEventTable:
                {
                    assert(0);
                    *value = nullptr;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerAnalysisViewIndexed:
                {
                    assert(0);
                    *value = nullptr;
                    result = kRocProfVisResultSuccess;
                    break;
                }
                case kRPVControllerId:
                case kRPVControllerNumAnalysisView:
                default:
                {
                    result = kRocProfVisResultNotSupported;
                    break;
                }
            }
        }
        return result;
    }
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final
    {
        assert(0);
        return kRocProfVisResultNotSupported;
    }

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length) final
    {
        assert(0);
        return kRocProfVisResultReadOnlyError;
    }

private:
    uint64_t m_id;
    Timeline* m_timeline;
};

typedef Reference<rocprofvis_controller_t, Trace, kRPVControllerObjectTypeController> TraceRef;
typedef Reference<rocprofvis_controller_timeline_t, Timeline, kRPVControllerObjectTypeTimeline> TimelineRef;
typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;
typedef Reference<rocprofvis_controller_event_t, Event, kRPVControllerObjectTypeEvent> EventRef;
typedef Reference<rocprofvis_controller_sample_t, Sample, kRPVControllerObjectTypeSample> SampleRef;
typedef Reference<rocprofvis_controller_array_t, Array, kRPVControllerObjectTypeArray> ArrayRef;
typedef Reference<rocprofvis_controller_future_t, Future, kRPVControllerObjectTypeFuture> FutureRef;
}
}

extern "C"
{
rocprofvis_result_t rocprofvis_controller_get_uint64(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->GetUInt64(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_get_double(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->GetDouble(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_get_object(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->GetObject(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_set_object(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->SetObject(property, index, value);
    }
    return result;
}
rocprofvis_result_t rocprofvis_controller_get_string(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (object && value)
    {
        RocProfVis::Controller::Handle* handle = (RocProfVis::Controller::Handle*)object;
        result = handle->GetString(property, index, value, length);
    }
    return result;
}
rocprofvis_controller_t* rocprofvis_controller_alloc()
{
    rocprofvis_controller_t* controller = nullptr;
    RocProfVis::Controller::Trace* trace = new RocProfVis::Controller::Trace();
    if (trace)
    {
        controller = (rocprofvis_controller_t*) trace;
    }
    return controller;
}
rocprofvis_result_t rocprofvis_controller_load_async(rocprofvis_controller_t* controller, char const* const filename, rocprofvis_controller_future_t* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;

    RocProfVis::Controller::TraceRef trace(controller);
    RocProfVis::Controller::FutureRef future_ref(future);
    if(trace.IsValid() && future_ref.IsValid() && filename && strlen(filename))
    {
        result = trace->Load(filename, *future_ref);
    }

    return result;
}
rocprofvis_controller_future_t* rocprofvis_controller_future_alloc(void)
{
    rocprofvis_controller_future_t* future = (rocprofvis_controller_future_t*)new RocProfVis::Controller::Future();
    return future;
}
rocprofvis_controller_array_t* rocprofvis_controller_array_alloc(uint32_t initial_size)
{
    RocProfVis::Controller::Array* array = new RocProfVis::Controller::Array();
    rocprofvis_result_t result = array->SetUInt64(kRPVControllerArrayNumEntries, 0, initial_size);
    assert(result == kRocProfVisResultSuccess);
    return (rocprofvis_controller_array_t*)array;
}
rocprofvis_result_t rocprofvis_controller_future_wait(rocprofvis_controller_future_t* object, float timeout)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    RocProfVis::Controller::FutureRef future(object);
    if (future.IsValid())
    {
        result = future->Wait(timeout);
    }
    return result;
}

rocprofvis_result_t rocprofvis_controller_track_fetch_async(
    rocprofvis_controller_t* controller, rocprofvis_controller_track_t* track,
    double start_time, double end_time, rocprofvis_controller_future_t* result,
    rocprofvis_controller_array_t* output)
{
    rocprofvis_result_t error = kRocProfVisResultInvalidArgument;
    RocProfVis::Controller::TraceRef trace(controller);
    RocProfVis::Controller::TrackRef track_ref(track);
    RocProfVis::Controller::FutureRef future(result);
    RocProfVis::Controller::ArrayRef array(output);
    if (trace.IsValid() && track_ref.IsValid() && future.IsValid() && array.IsValid())
    {
        error = trace->AsyncFetch(*track_ref, *future, *array, start_time, end_time);
    }
    return error;
}
void rocprofvis_controller_array_free(rocprofvis_controller_array_t* object)
{
    RocProfVis::Controller::ArrayRef array(object);
    if (array.IsValid())
    {
        delete array.Get();
    }
}
void rocprofvis_controller_future_free(rocprofvis_controller_future_t* object)
{
    RocProfVis::Controller::FutureRef future(object);
    if (future.IsValid())
    {
        delete future.Get();
    }
}
void rocprofvis_controller_free(rocprofvis_controller_t* object)
{
    RocProfVis::Controller::TraceRef trace(object);
    if (trace.IsValid())
    {
        delete trace.Get();
    }
}
}