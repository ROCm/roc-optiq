// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_sample_lod.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <exception>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_sample_t, Sample, kRPVControllerObjectTypeSample> SampleRef;

void SampleLOD::CalculateChildValues(void)
{
    m_child_min                 = DBL_MAX;
    m_child_max                 = DBL_MIN;
    m_child_mean                = 0;
    m_child_median              = 0;
    m_child_min_timestamp       = DBL_MAX;
    m_child_max_timestamp       = DBL_MIN;
    std::vector<double> values;
    double total_duration = 0;
    for(Sample* sample : m_children)
    {
        if(sample)
        {
            const double timestamp     = sample->GetTimestamp();
            const double end_timestamp = sample->GetEndTimestamp();
            const double value         = sample->GetValue();
            const double duration      = end_timestamp - timestamp;
            m_child_min_timestamp      = std::min(m_child_min_timestamp, timestamp);
            m_child_max_timestamp      = std::max(m_child_max_timestamp, timestamp);
            m_child_min                = std::min(m_child_min, value);
            m_child_max                = std::max(m_child_max, value);
            m_child_mean += value * duration;
            total_duration += duration;
            values.push_back(value);
        }
    }
    if(values.empty())
    {
        m_child_min           = 0;
        m_child_max           = 0;
        m_child_min_timestamp = 0;
        m_child_max_timestamp = 0;
        SetValue(0);
        return;
    }
    size_t num_children = values.size();
    if(num_children & 0x1)
    {
        m_child_median = values[num_children / 2];
    }
    else
    {
        size_t lower = std::max(num_children / 2, static_cast<size_t>(1)) - 1;
        size_t upper = std::min(num_children / 2, values.size() - 1);

        m_child_median = (values[lower] + values[upper]) / 2;
    }
    m_child_mean = total_duration != 0 ? m_child_mean / total_duration : 0;
    SetValue(m_child_mean);
}

SampleLOD::SampleLOD(uint64_t id, double timestamp)
: Sample(id, timestamp)
, m_child_min(0)
, m_child_mean(0)
, m_child_median(0)
, m_child_max(0)
, m_child_min_timestamp(0)
, m_child_max_timestamp(0)
{}

SampleLOD::SampleLOD(uint64_t id, double timestamp, std::vector<Sample*>& children)
: Sample(id, timestamp)
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

rocprofvis_result_t
SampleLOD::GetInclusiveMemoryUsage(uint64_t* value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        // Child samples are borrowed; only the pointer buffer belongs to this LOD.
        *value = sizeof(SampleLOD) + m_children.capacity() * sizeof(Sample*);
        result = kRocProfVisResultSuccess;
    }
    return result;
}

rocprofvis_result_t
SampleLOD::GetChild(uint64_t index, Sample** value) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        if(index < m_children.size())
        {
            *value = m_children[index];
            result = kRocProfVisResultSuccess;
        }
        else
        {
            result = kRocProfVisResultOutOfRange;
        }
    }
    return result;
}

rocprofvis_result_t
SampleLOD::SetChild(uint64_t index, Sample* child)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(child)
    {
        if(index < m_children.size())
        {
            m_children[index] = child;
            result            = kRocProfVisResultSuccess;
        }
        else
        {
            result = kRocProfVisResultOutOfRange;
        }
    }
    return result;
}

rocprofvis_result_t
SampleLOD::SetNumberOfChildren(uint64_t count)
{
    rocprofvis_result_t result = kRocProfVisResultMemoryAllocError;
    try
    {
        if(const bool size_changed = (count != m_children.size()); size_changed)
        {
            m_children.resize(count);
            CalculateChildValues();
        }
        result = kRocProfVisResultSuccess;
    } catch(std::exception const&)
    {
        result = kRocProfVisResultMemoryAllocError;
    }
    return result;
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
                *value = GetNumberOfChildren();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
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
                *value = GetChildMin();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMean:
            {
                *value = GetChildMean();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMedian:
            {
                *value = GetChildMedian();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMax:
            {
                *value = GetChildMax();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMinTimestamp:
            {
                *value = GetChildMinTimestamp();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleChildMaxTimestamp:
            {
                *value = GetChildMaxTimestamp();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSampleTimestamp:
            case kRPVControllerSampleValue:
            case kRPVControllerSampleId:
            case kRPVControllerSampleType:
            case kRPVControllerSampleNumChildren:
            case kRPVControllerSampleChildIndex:
            case kRPVControllerSampleEndTimestamp:
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
                Sample* child = nullptr;
                result        = GetChild(index, &child);
                if(result == kRocProfVisResultSuccess)
                {
                    *value = reinterpret_cast<rocprofvis_handle_t*>(child);
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
rocprofvis_result_t SampleLOD::SetUInt64(rocprofvis_property_t property, uint64_t index,
                                uint64_t value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSampleNumChildren:
        {
            result = SetNumberOfChildren(value);
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
            SetChildMin(value);
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMean:
        {
            SetChildMean(value);
            result       = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMedian:
        {
            SetChildMedian(value);
            result         = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMax:
        {
            SetChildMax(value);
            result      = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMinTimestamp:
        {
            SetChildMinTimestamp(value);
            result                = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleChildMaxTimestamp:
        {
            SetChildMaxTimestamp(value);
            result                = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSampleValue:
        case kRPVControllerSampleId:
        case kRPVControllerSampleType:
        case kRPVControllerSampleNumChildren:
        case kRPVControllerSampleChildIndex:
        case kRPVControllerSampleTimestamp:
        case kRPVControllerSampleEndTimestamp:
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
                if(index >= GetNumberOfChildren())
                {
                    result = kRocProfVisResultOutOfRange;
                }
                else
                {
                    SampleRef sample_ref(value);
                    if(sample_ref.IsValid())
                    {
                        result = SetChild(index, sample_ref.Get());
                    }
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
                                char const* value) 
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
                result = Sample::SetString(property, index, value);
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
