// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_sample.h"

namespace RocProfVis
{
namespace Controller
{

Sample::Sample(uint64_t id, double timestamp)
: Handle(__kRPVControllerSamplePropertiesFirst, __kRPVControllerSamplePropertiesLast)
, m_data(0)
, m_end_timestamp(0)
, m_timestamp(timestamp)
{
    (void) id;
}

Sample& Sample::operator=(Sample&& other)
{
    m_data            = other.m_data;
    m_timestamp       = other.m_timestamp;
    m_end_timestamp  = other.m_end_timestamp;
    return *this;
}

rocprofvis_result_t
Sample::GetInclusiveMemoryUsage(uint64_t* value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        *value = sizeof(Sample);
        result = kRocProfVisResultSuccess;
    }
    return result;
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
                result = GetInclusiveMemoryUsage(value);
                break;
            }
            case kRPVControllerSampleId:
            {
                *value = GetId();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleType:
            {
                *value = GetValueType();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleNumChildren:
            {
                *value = GetNumberOfChildren();
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
                *value = GetTimestamp();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleValue:
            {
                *value = GetValue();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleEndTimestamp:
            {
                *value = GetEndTimestamp();
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


rocprofvis_result_t Sample::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSampleValue:
        {
            SetValue(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleEndTimestamp:
        {
            SetEndTimestamp(value);
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

}
}
