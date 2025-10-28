// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_sample.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;

Sample::Sample(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp)
: Handle(__kRPVControllerSamplePropertiesFirst, __kRPVControllerSamplePropertiesLast)
, m_data(type)
, m_id(id)
, m_timestamp(timestamp)
{}

Sample& Sample::operator=(Sample&& other)
{
    m_id              = other.m_id;
    m_data            = other.m_data;
    m_timestamp       = other.m_timestamp;   
    return *this;
}

Sample::~Sample() {}

rocprofvis_controller_object_type_t Sample::GetType(void) 
{
    return kRPVControllerObjectTypeSample;
}

rocprofvis_result_t Sample::GetUInt64(rocprofvis_property_t property, uint64_t index,
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
                *value = sizeof(Sample);
                result = kRocProfVisResultSuccess;
                break;
            }
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
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Sample::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSampleChildMin:
            {
                *value = 0;
                result = kRocProfVisResultNotSupported;
                break;
            }
            case kRPVControllerSampleChildMean:
            {
                *value = 0;
                result = kRocProfVisResultNotSupported;
                break;
            }
            case kRPVControllerSampleChildMedian:
            {
                *value = 0;
                result = kRocProfVisResultNotSupported;
                break;
            }
            case kRPVControllerSampleChildMax:
            {
                *value = 0;
                result = kRocProfVisResultNotSupported;
                break;
            }
            case kRPVControllerSampleChildMinTimestamp:
            {
                *value = 0;
                result = kRocProfVisResultNotSupported;
                break;
            }
            case kRPVControllerSampleChildMaxTimestamp:
            {
                *value = 0;
                result = kRocProfVisResultNotSupported;
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
            default:
            {
                result = UnhandledProperty(property);
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Sample::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSampleChildIndex:
            {
                *value = nullptr;
                result = kRocProfVisResultNotSupported;
                break;
            }
            case kRPVControllerSampleTrack:
            {
                *value = nullptr;
                result = kRocProfVisResultNotSupported;
                break;
            }
            case kRPVControllerSampleValue:
            {
                result = m_data.GetObject(value);
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


rocprofvis_result_t Sample::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSampleValue:
        {
            result = m_data.GetString(value, length);
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


rocprofvis_result_t Sample::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSampleValue:
        {
            result = m_data.SetUInt64(value);
            break;
        }
        case kRPVControllerSampleId:
        {
            result = kRocProfVisResultReadOnlyError;
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

rocprofvis_result_t Sample::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSampleValue:
        {
            result = m_data.SetDouble(value);
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

rocprofvis_result_t
Sample::SetObject(rocprofvis_property_t property, uint64_t index,
                  rocprofvis_handle_t* value)
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSampleValue:
            {
                result = m_data.SetObject(value);
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

rocprofvis_result_t 
Sample::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSampleValue:
            {
                result = m_data.SetString(value);
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
