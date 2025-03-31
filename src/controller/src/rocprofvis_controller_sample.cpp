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
: m_data(type)
, m_id(id)
, m_timestamp(timestamp)
{
}

Sample::~Sample()
{
}

rocprofvis_controller_object_type_t Sample::GetType(void) 
{
    return kRPVControllerObjectTypeSample;
}

rocprofvis_result_t Sample::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
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
rocprofvis_result_t Sample::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
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
rocprofvis_result_t Sample::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
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
rocprofvis_result_t Sample::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
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

rocprofvis_result_t Sample::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
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
        {
            result = kRocProfVisResultReadOnlyError;
            break;
        }
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
rocprofvis_result_t Sample::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
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
rocprofvis_result_t Sample::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSampleTrack:
            {
                TrackRef track_ref(value);
                if(track_ref.IsValid())
                {
                    m_track = track_ref.Get();
                    result  = kRocProfVisResultSuccess;
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
rocprofvis_result_t Sample::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length) 
{
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

}
}
