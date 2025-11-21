// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

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
, m_data(0)
, m_next_data(0)
, m_next_timestamp(0)
, m_timestamp(timestamp)
{}

Sample& Sample::operator=(Sample&& other)
{
    m_data            = other.m_data;
    m_timestamp       = other.m_timestamp;
    m_next_data       = other.m_next_data;
    m_next_timestamp  = other.m_next_timestamp;
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
                *value = m_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleType:
            {
                *value = kRPVControllerPrimitiveTypeDouble;
                result = kRocProfVisResultSuccess;
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
                *value = m_data;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleNextTimestamp:
            {
                *value = m_next_timestamp;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleNextValue:
            {
                *value = m_next_data;
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
                *value = nullptr;
                result = kRocProfVisResultNotSupported;
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
            m_data = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleNextValue:
        {
            m_next_data = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleNextTimestamp:
        {
            m_next_timestamp = value;
            result = kRocProfVisResultSuccess;
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
