// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_sample_lod.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"

#include <cfloat>
#include <cmath>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_sample_t, Sample, kRPVControllerObjectTypeSample> SampleRef;

void SampleLOD::CalculateChildValues(void)
{
    ROCPROFVIS_ASSERT(m_children.size() > 0);
    m_child_min                 = DBL_MAX;
    m_child_max                 = DBL_MIN;
    m_child_mean                = 0;
    m_child_median              = 0;
    m_child_min_timestamp       = DBL_MAX;
    m_child_max_timestamp       = DBL_MIN;
    uint64_t            entries = 0;
    std::vector<double> values;
    for(Sample* sample : m_children)
    {
        if(sample)
        {
            double timestamp = 0;
            double value     = 0;
            rocprofvis_result_t result = sample->GetDouble( kRPVControllerSampleTimestamp, 0, &timestamp);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            result = sample->GetDouble(kRPVControllerSampleValue, 0, &value);
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
            {
                m_child_min_timestamp = std::min(m_child_min_timestamp, timestamp);
                m_child_max_timestamp = std::max(m_child_max_timestamp, timestamp);
                m_child_min           = std::min(m_child_min, value);
                m_child_max           = std::max(m_child_max, value);
                m_child_mean += value;
                entries++;
                values.push_back(value);
            }
        }
    }
    uint64_t num_children = values.size();
    if(num_children & 0x1)
    {
        m_child_median = values[num_children / 2];
    }
    else
    {
        uint64_t lower = std::max(num_children / 2, (uint64_t)1) - 1;
        uint64_t upper = std::min(num_children / 2, values.size() - 1);

        m_child_median = (values[lower] + values[upper]) / 2;
    }
    m_child_mean /= entries;
    auto result = SetDouble(kRPVControllerSampleValue, 0, m_child_mean);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
}

SampleLOD::SampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp)
: Sample(type, id, timestamp)
, m_child_min(0)
, m_child_mean(0)
, m_child_median(0)
, m_child_max(0)
, m_child_min_timestamp(0)
, m_child_max_timestamp(0)
{}

SampleLOD::SampleLOD(rocprofvis_controller_primitive_type_t type, uint64_t id, double timestamp,
            std::vector<Sample*>& children)
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

SampleLOD& SampleLOD::operator=(SampleLOD&& other)
{
    m_children  = std::move(other.m_children);
    m_id = other.m_id;
    m_data = other.m_data;
    m_timestamp = other.m_timestamp;
    m_child_min = 0;
    m_child_mean = 0; 
    m_child_median = 0;
    m_child_max    = 0;
    m_child_min_timestamp = 0;
    m_child_max_timestamp = 0;

    return *this;
}

SampleLOD::~SampleLOD()
{
}

rocprofvis_controller_object_type_t SampleLOD::GetType(void) 
{
    return kRPVControllerObjectTypeSample;
}

size_t
SampleLOD::GetNumChildren()
{
    return m_children.size();
}

rocprofvis_result_t SampleLOD::GetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
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
rocprofvis_result_t SampleLOD::GetDouble(rocprofvis_property_t property, uint64_t index,
                                double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
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
rocprofvis_result_t SampleLOD::GetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSampleChildIndex:
            {
                if(index < m_children.size())
                {
                    *value = (rocprofvis_handle_t*) m_children[index];
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
rocprofvis_result_t SampleLOD::GetString(rocprofvis_property_t property, uint64_t index,
                                char* value, uint32_t* length) 
{
    return Sample::GetString(property, index, value, length);
}

rocprofvis_result_t SampleLOD::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSampleNumChildren:
        {
            if(value != m_children.size())
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
rocprofvis_result_t SampleLOD::SetDouble(rocprofvis_property_t property, uint64_t index,
                                double value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSampleChildMin:
        {
            m_child_min = value;
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMean:
        {
            m_child_mean = value;
            result       = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMedian:
        {
            m_child_median = value;
            result         = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMax:
        {
            m_child_max = value;
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMinTimestamp:
        {
            m_child_min_timestamp = value;
            result                = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMaxTimestamp:
        {
            m_child_max_timestamp = value;
            result                = kRocProfVisResultSuccess;
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
rocprofvis_result_t SampleLOD::SetObject(rocprofvis_property_t property, uint64_t index,
                                rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSampleChildIndex:
            {
                if(index < m_children.size())
                {
                    SampleRef sample_ref(value);
                    if(sample_ref.IsValid())
                    {
                        m_children[index] = sample_ref.Get();
                        result            = kRocProfVisResultSuccess;
                    }
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
rocprofvis_result_t SampleLOD::SetString(rocprofvis_property_t property, uint64_t index,
                                char const* value, uint32_t length) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
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

}
}
