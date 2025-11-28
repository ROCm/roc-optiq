// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_summary_metrics.h"
#include <algorithm>
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

constexpr const char* KERNEL_PADING_NAME = "Others";

SummaryMetrics::SummaryMetrics()
: Handle(__kRPVControllerSummaryMetricPropertiesFirst, __kRPVControllerSummaryMetricPropertiesLast)
, m_aggregation_level(kRPVControllerSummaryAggregationLevelTrace)
, m_id(std::nullopt)
, m_name(std::nullopt)
, m_processor_type(std::nullopt)
, m_processor_type_idx(std::nullopt)
, m_gpu(std::nullopt)
, m_cpu(std::nullopt)
{}

SummaryMetrics::~SummaryMetrics() 
{}

rocprofvis_controller_object_type_t SummaryMetrics::GetType(void) 
{
    return kRPVControllerObjectTypeSummaryMetrics;
}

rocprofvis_result_t SummaryMetrics::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                *value = sizeof(SummaryMetrics);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSummaryMetricPropertyAggregationLevel:
            {
                *value = (uint64_t)m_aggregation_level;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSummaryMetricPropertyNumSubMetrics:
            {
                *value = m_sub_metrics.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerSummaryMetricPropertyId:
            {
                if(m_id)
                {
                    *value = m_id.value();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyProcessorType:
            {
                if(m_processor_type)
                {
                    *value = m_processor_type.value();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyProcessorTypeIndex:
            {
                if(m_processor_type_idx)
                {
                    *value = m_processor_type_idx.value();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyNumKernels:
            {
                if(m_gpu)
                {
                    *value = m_gpu.value().top_kernels.size();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyKernelInvocationsIndexed:
            {
                if(m_gpu && index < m_gpu.value().top_kernels.size())
                {
                    *value = m_gpu.value().top_kernels[index].invocations;
                    result = kRocProfVisResultSuccess;
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

rocprofvis_result_t SummaryMetrics::GetDouble(rocprofvis_property_t property, uint64_t index, double* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSummaryMetricPropertyGpuGfxUtil:
            {
                if(m_gpu && m_gpu.value().gfx_util)
                {
                    *value = m_gpu.value().gfx_util.value();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyGpuMemUtil:
            {
                if(m_gpu && m_gpu.value().mem_util)
                {
                    *value = m_gpu.value().mem_util.value();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyKernelsExecTimeTotal:
            {
                if(m_gpu)
                {
                    *value = m_gpu.value().kernel_exec_time_total;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyKernelExecTimeSumIndexed:
            {
                if(m_gpu && index < m_gpu.value().top_kernels.size())
                {
                    *value = m_gpu.value().top_kernels[index].exec_time_sum;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyKernelExecTimeMinIndexed:
            {
                if(m_gpu && index < m_gpu.value().top_kernels.size())
                {
                    *value = m_gpu.value().top_kernels[index].exec_time_min;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyKernelExecTimeMaxIndexed:
            {
                if(m_gpu && index < m_gpu.value().top_kernels.size())
                {
                    *value = m_gpu.value().top_kernels[index].exec_time_max;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyKernelExecTimePctIndexed:
            {
                if(m_gpu && index < m_gpu.value().top_kernels.size())
                {
                    *value = m_gpu.value().top_kernels[index].exec_time_pct;
                    result = kRocProfVisResultSuccess;
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

rocprofvis_result_t SummaryMetrics::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSummaryMetricPropertySubMetricsIndexed:
            {
                if(index < m_sub_metrics.size())
                {
                    *value = (rocprofvis_handle_t*)&m_sub_metrics[index];
                    result = kRocProfVisResultSuccess;
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

rocprofvis_result_t SummaryMetrics::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerSummaryMetricPropertyName:
            {
                if(m_name)
                {
                    result = GetStdStringImpl(value, length, m_name.value());
                }
                break;
            }
            case kRPVControllerSummaryMetricPropertyKernelNameIndexed:
            {
                if(m_gpu && index < m_gpu.value().top_kernels.size())
                {
                    result = GetStdStringImpl(value, length, m_gpu.value().top_kernels[index].name);
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

rocprofvis_result_t SummaryMetrics::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSummaryMetricPropertyAggregationLevel:
        {
            m_aggregation_level = (rocprofvis_controller_summary_aggregation_level_t)value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSummaryMetricPropertyNumSubMetrics:
        {
            m_sub_metrics.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSummaryMetricPropertyId:
        {
            m_id = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSummaryMetricPropertyNumKernels:
        {
            if(m_gpu)
            {
                m_gpu.value().top_kernels.resize(value);
            }
            else
            {
                m_gpu = std::make_optional<GPUMetrics>({std::nullopt, std::nullopt, std::vector<KernelMetrics>(value)});
            }
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSummaryMetricPropertyProcessorType:
        {
            m_processor_type = (rocprofvis_controller_processor_type_t)value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSummaryMetricPropertyProcessorTypeIndex:
        {
            m_processor_type_idx = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSummaryMetricPropertyKernelInvocationsIndexed:
        {
            if(m_gpu && index < m_gpu.value().top_kernels.size())
            {
                m_gpu.value().top_kernels[index].invocations = value;
                result = kRocProfVisResultSuccess;
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

rocprofvis_result_t SummaryMetrics::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSummaryMetricPropertyGpuGfxUtil:
        {
            if(m_gpu)
            {
                m_gpu.value().gfx_util = static_cast<float>(value);
            }
            else
            {
                m_gpu = GPUMetrics{static_cast<float>(value), std::nullopt, {}};
            }
            ;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSummaryMetricPropertyGpuMemUtil:
        {
            if(m_gpu)
            {
                m_gpu.value().mem_util = static_cast<float>(value);
            }
            else
            {
                m_gpu = GPUMetrics{std::nullopt, static_cast<float>(value), {}};
            }
            ;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSummaryMetricPropertyKernelsExecTimeTotal:
        {
            if(m_gpu)
            {
                m_gpu.value().kernel_exec_time_total = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerSummaryMetricPropertyKernelExecTimeSumIndexed:
        {
            if(m_gpu && index < m_gpu.value().top_kernels.size())
            {
                m_gpu.value().top_kernels[index].exec_time_sum = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerSummaryMetricPropertyKernelExecTimeMinIndexed:
        {
            if(m_gpu && index < m_gpu.value().top_kernels.size())
            {
                m_gpu.value().top_kernels[index].exec_time_min = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerSummaryMetricPropertyKernelExecTimeMaxIndexed:
        {
            if(m_gpu && index < m_gpu.value().top_kernels.size())
            {
                m_gpu.value().top_kernels[index].exec_time_max = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVControllerSummaryMetricPropertyKernelExecTimePctIndexed:
        {
            if(m_gpu && index < m_gpu.value().top_kernels.size())
            {
                m_gpu.value().top_kernels[index].exec_time_pct = value;
                result = kRocProfVisResultSuccess;
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

rocprofvis_result_t SummaryMetrics::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSummaryMetricPropertySubMetricsIndexed:
            {
                if(index < m_sub_metrics.size())
                {
                    m_sub_metrics[index] = *(SummaryMetrics*)value;
                    result = kRocProfVisResultSuccess;
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

rocprofvis_result_t SummaryMetrics::SetString(rocprofvis_property_t property, uint64_t index, char const* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    switch(property)
    {
        case kRPVControllerSummaryMetricPropertyName:
        {
            m_name = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVControllerSummaryMetricPropertyKernelNameIndexed:
        {
            if(m_gpu && index < m_gpu.value().top_kernels.size())
            {
                m_gpu.value().top_kernels[index].name = value;
                result = kRocProfVisResultSuccess;
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

rocprofvis_result_t SummaryMetrics::AggregateSubMetrics()
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(m_sub_metrics.empty())
    {
        result = kRocProfVisResultSuccess;
    }
    else
    {
        std::unordered_map<std::string_view, KernelMetrics> kernel_map;
        for(SummaryMetrics& sub : m_sub_metrics)
        {
            result = sub.AggregateSubMetrics();
            if(result == kRocProfVisResultSuccess)
            {
                if(m_gpu)
                {
                    if(sub.m_gpu)
                    {
                        m_gpu.value().gfx_util = AddOptionals(m_gpu.value().gfx_util, sub.m_gpu.value().gfx_util);
                        m_gpu.value().mem_util = AddOptionals(m_gpu.value().mem_util, sub.m_gpu.value().mem_util);
                        m_gpu.value().kernel_exec_time_total += sub.m_gpu.value().kernel_exec_time_total;
                    }
                }
                else
                {
                    m_gpu = sub.m_gpu;
                    m_gpu.value().top_kernels.clear();
                }
                for(const KernelMetrics& kernel : sub.m_gpu.value().top_kernels)
                {
                    if(kernel.name != KERNEL_PADING_NAME)
                    {
                        if(kernel_map.count(kernel.name))
                        {
                            kernel_map[kernel.name].invocations += kernel.invocations;
                            kernel_map[kernel.name].exec_time_sum += kernel.exec_time_sum;
                            kernel_map[kernel.name].exec_time_min = std::min(kernel_map[kernel.name].exec_time_min, kernel.exec_time_min);
                            kernel_map[kernel.name].exec_time_max = std::max(kernel_map[kernel.name].exec_time_max, kernel.exec_time_max);
                        }
                        else
                        {
                            kernel_map[kernel.name] = kernel;
                        }    
                    }                       
                }
            }
        }
        if(m_gpu)
        {
            if(m_gpu.value().gfx_util)
            {
                m_gpu.value().gfx_util.value() /= m_sub_metrics.size();
            }
            if(m_gpu.value().mem_util)
            {
                m_gpu.value().mem_util.value() /= m_sub_metrics.size();
            }
            for(const std::pair<std::string_view, KernelMetrics>& kernel : kernel_map)
            {
                m_gpu.value().top_kernels.emplace_back(kernel.second);
            }
            std::sort(m_gpu.value().top_kernels.begin(), m_gpu.value().top_kernels.end(), [](const KernelMetrics& a, const KernelMetrics& b){
                return a.exec_time_sum > b.exec_time_sum;
            });
            m_gpu.value().top_kernels.resize(std::min(m_gpu.value().top_kernels.size(), (size_t)10));
            for(KernelMetrics& kernel : m_gpu.value().top_kernels)
            {
                kernel.exec_time_pct = kernel.exec_time_sum / m_gpu.value().kernel_exec_time_total;
            }
            PadTopKernels();
        }
    }
    return result;
}

void SummaryMetrics::PadTopKernels()
{
    if(m_gpu)
    {
        double exec_time_sum = 0.0;
        for(const KernelMetrics& kernel : m_gpu.value().top_kernels)
        {
            exec_time_sum += kernel.exec_time_sum;
        }
        if(exec_time_sum < m_gpu.value().kernel_exec_time_total)
        {
            m_gpu.value().top_kernels.emplace_back(KernelMetrics{KERNEL_PADING_NAME, 0, m_gpu.value().kernel_exec_time_total - exec_time_sum, 0.0, 0.0, 
                                                   (float)((m_gpu.value().kernel_exec_time_total - exec_time_sum) / m_gpu.value().kernel_exec_time_total)});
        }
    }    
}

void SummaryMetrics::Reset()
{
    m_aggregation_level = kRPVControllerSummaryAggregationLevelTrace;
    m_id = std::nullopt;
    m_name = std::nullopt;
    m_processor_type = std::nullopt;
    m_processor_type_idx = std::nullopt;
    m_gpu = std::nullopt;
    m_cpu = std::nullopt;
    m_sub_metrics.clear();
}

bool SummaryMetrics::Empty() const
{
    return !(m_gpu || m_cpu || !m_sub_metrics.empty());
}

}
}
