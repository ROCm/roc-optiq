// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_roofline.h"

namespace RocProfVis
{
namespace Controller
{

constexpr double kMinX = 0.01;
constexpr double kMaxX = 1000.00;

Roofline::Roofline()
: Handle(__kRPVControllerRooflinePropertiesFirst, __kRPVControllerRooflinePropertiesLast) 
{}

Roofline::~Roofline()
{}

rocprofvis_controller_object_type_t Roofline::GetType(void) 
{
    return kRPVControllerObjectTypeRoofline;
}

rocprofvis_result_t Roofline::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(Roofline);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerRooflineNumCeilingsCompute:
            {
                *value = m_ceilings_compute.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerRooflineCeilingComputeTypeIndexed:
            {
                if(index < m_ceilings_compute.size())
                {
                    *value = (uint64_t)m_ceilings_compute[index].type;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineNumCeilingsBandwidth:
            {
                *value = m_ceilings_bandwidth.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerRooflineCeilingBandwidthTypeIndexed:
            {
                if(index < m_ceilings_bandwidth.size())
                {
                    *value = (uint64_t)m_ceilings_bandwidth[index].type;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineNumCeilingsRidge:
            {
                *value = m_ceilings_ridge.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerRooflineCeilingRidgeComputeTypeIndexed:
            {
                if(index < m_ceilings_ridge.size())
                {
                    *value = (uint64_t)m_ceilings_ridge[index].compute_type;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineCeilingRidgeBandwidthTypeIndexed:
            {
                if(index < m_ceilings_ridge.size())
                {
                    *value = (uint64_t)m_ceilings_ridge[index].bandwidth_type;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineNumKernels:
            {
                *value = m_intensities.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerRooflineKernelIdIndexed:
            {
                if(index < m_intensities.size())
                {
                    *value = (uint64_t)m_intensities[index].kernel_id;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineKernelIntensityTypeIndexed:
            {
                if(index < m_intensities.size())
                {
                    *value = (uint64_t)m_intensities[index].type;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
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

rocprofvis_result_t Roofline::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerRooflineCeilingComputeXIndexed:
            {
                if(index < m_ceilings_compute.size())
                {
                    *value = m_ceilings_compute[index].position.x;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineCeilingComputeYIndexed:
            {
                if(index < m_ceilings_compute.size())
                {
                    *value = m_ceilings_compute[index].position.y;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineCeilingBandwidthXIndexed:
            {
                if(index < m_ceilings_bandwidth.size())
                {
                    *value = m_ceilings_bandwidth[index].position.x;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineCeilingBandwidthYIndexed:
            {
                if(index < m_ceilings_bandwidth.size())
                {
                    *value = m_ceilings_bandwidth[index].position.y;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineCeilingRidgeXIndexed:
            {
                if(index < m_ceilings_ridge.size())
                {
                    *value = m_ceilings_ridge[index].position.x;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineCeilingRidgeYIndexed:
            {
                if(index < m_ceilings_ridge.size())
                {
                    *value = m_ceilings_ridge[index].position.y;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineKernelIntensityXIndexed:
            {
                if(index < m_intensities.size())
                {
                    *value = m_intensities[index].position.x;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
                break;
            }
            case kRPVControllerRooflineKernelIntensityYIndexed:
            {
                if(index < m_intensities.size())
                {
                    *value = m_intensities[index].position.y;
                    result = kRocProfVisResultSuccess;
                }
                else
                {
                    result = kRocProfVisResultOutOfRange;
                }
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

rocprofvis_result_t Roofline::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerRooflineNumCeilingsCompute:
        {
            m_ceilings_compute.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerRooflineCeilingComputeTypeIndexed:
        {
            if(index < m_ceilings_compute.size())
            {
                m_ceilings_compute[index].type = (rocprofvis_controller_roofline_ceiling_compute_type_t)value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineNumCeilingsBandwidth:
        {
            m_ceilings_bandwidth.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerRooflineCeilingBandwidthTypeIndexed:
        {
            if(index < m_ceilings_bandwidth.size())
            {
                m_ceilings_bandwidth[index].type = (rocprofvis_controller_roofline_ceiling_bandwidth_type_t)value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineNumCeilingsRidge:
        {
            m_ceilings_ridge.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerRooflineCeilingRidgeComputeTypeIndexed:
        {
            if(index < m_ceilings_ridge.size())
            {
                m_ceilings_ridge[index].compute_type = (rocprofvis_controller_roofline_ceiling_compute_type_t)value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineCeilingRidgeBandwidthTypeIndexed:
        {
            if(index < m_ceilings_ridge.size())
            {
                m_ceilings_ridge[index].bandwidth_type = (rocprofvis_controller_roofline_ceiling_bandwidth_type_t)value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineNumKernels:
        {
            m_intensities.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerRooflineKernelIdIndexed:
        {
            if(index < m_intensities.size())
            {
                m_intensities[index].kernel_id = (uint32_t)value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineKernelIntensityTypeIndexed:
        {
            if(index < m_intensities.size())
            {
                m_intensities[index].type = (rocprofvis_controller_roofline_kernel_intensity_type_t)value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
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

rocprofvis_result_t Roofline::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerRooflineCeilingComputeXIndexed:
        {
            if(index < m_ceilings_compute.size())
            {
                m_ceilings_compute[index].position.x = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineCeilingComputeYIndexed:
        {
            if(index < m_ceilings_compute.size())
            {
                m_ceilings_compute[index].position.y = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineCeilingBandwidthXIndexed:
        {
            if(index < m_ceilings_bandwidth.size())
            {
                m_ceilings_bandwidth[index].position.x = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineCeilingBandwidthYIndexed:
        {
            if(index < m_ceilings_bandwidth.size())
            {
                m_ceilings_bandwidth[index].position.y = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineCeilingRidgeXIndexed:
        {
            if(index < m_ceilings_ridge.size())
            {
                m_ceilings_ridge[index].position.x = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineCeilingRidgeYIndexed:
        {
            if(index < m_ceilings_ridge.size())
            {
                m_ceilings_ridge[index].position.y = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineKernelIntensityXIndexed:
        {
            if(index < m_intensities.size())
            {
                m_intensities[index].position.x = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
            break;
        }
        case kRPVControllerRooflineKernelIntensityYIndexed:
        {
            if(index < m_intensities.size())
            {
                m_intensities[index].position.y = value;
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultOutOfRange;
            }
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

bool Roofline::QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_controller_roofline_ceiling_compute_type_t& out) const
{
    bool valid = true;
    switch(in)
    {
        case kRPVComputeColumnWorkloadRooflineBenchMFMAF8Flops:
        {
            out = kRPVControllerRooflineCeilingComputeMFMAFP8;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchFP16Flops:
        {
            out = kRPVControllerRooflineCeilingComputeVALUFP16;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchMFMAF16Flops:
        {
            out = kRPVControllerRooflineCeilingComputeMFMAFP16;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchMFMABF16Flops:
        {
            out = kRPVControllerRooflineCeilingComputeMFMABF16;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchFP32Flops:
        {
            out = kRPVControllerRooflineCeilingComputeVALUFP32;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchMFMAF32Flops:
        {
            out = kRPVControllerRooflineCeilingComputeMFMAFP32;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchFP64Flops:
        {
            out = kRPVControllerRooflineCeilingComputeVALUFP64;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchMFMAF64Flops:
        {
            out = kRPVControllerRooflineCeilingComputeMFMAFP64;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchI8Ops:
        {
            out = kRPVControllerRooflineCeilingComputeVALUI8;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchMFMAI8Ops:
        {
            out = kRPVControllerRooflineCeilingComputeMFMAI8;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchI32Ops:
        {
            out = kRPVControllerRooflineCeilingComputeVALUI32;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchI64Ops:
        {
            out = kRPVControllerRooflineCeilingComputeVALUI64;
            break;
        }
        default:
        {
            valid = false;
            break;
        }
    }
    return valid;
}

bool Roofline::QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_controller_roofline_ceiling_bandwidth_type_t& out) const
{
    bool valid = true;
    switch(in)
    {
        case kRPVComputeColumnWorkloadRooflineBenchHBMBw:
        {
            out = kRPVControllerRooflineCeilingTypeBandwidthHBM;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchL2Bw:
        {
            out = kRPVControllerRooflineCeilingTypeBandwidthL2;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchL1Bw:
        {
            out = kRPVControllerRooflineCeilingTypeBandwidthL1;
            break;
        }
        case kRPVComputeColumnWorkloadRooflineBenchLDSBw:
        {
            out = kRPVControllerRooflineCeilingTypeBandwidthLDS;
            break;
        }
        default:
        {
            valid = false;
            break;
        }
    }
    return valid;
}

bool Roofline::QueryToPropertyEnum(rocprofvis_db_compute_column_enum_t in, rocprofvis_controller_roofline_kernel_intensity_type_t& out) const
{
    bool valid = true;
    switch(in)
    {
        case kRPVComputeColumnRooflineL1CacheData:
        {
            out = kRPVControllerRooflineKernelIntensityTypeL1;
            break;
        }
        case kRPVComputeColumnRooflineL2CacheData:
        {  
            out = kRPVControllerRooflineKernelIntensityTypeL2;
            break;
        }
        case kRPVComputeColumnRooflineHBMCacheData:
        {
            out = kRPVControllerRooflineKernelIntensityTypeHBM;
            break;
        }
        default:
        {
            valid = false;
            break;
        }
    }
    return valid;
}

double Roofline::MinX() const
{
    return kMinX;
}

double Roofline::MaxX() const
{
    return kMaxX;
}

}
}
