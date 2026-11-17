// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_summary_metrics.h"
#include "rocprofvis_controller_topology.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>

namespace RocProfVis
{
namespace Controller
{

constexpr const char* TOP_EVENT_REMAINDER_NAME = "Others";
constexpr uint64_t    DEFAULT_TOP_EVENTS_LIMIT = 10;

SummaryMetrics::SummaryMetrics()
: Handle(__kRPVAnalysisSummaryMetricPropertiesFirst, __kRPVAnalysisSummaryProcessThreadPropertiesLast)
, m_aggregation_level(kRPVAnalysisSummaryAggregationLevelTrace)
, m_metadata{ nullptr }
, m_gpu(std::nullopt)
, m_cpu(std::nullopt)
, m_memory(std::nullopt)
, m_launch(std::nullopt)
, m_top_events_limit(DEFAULT_TOP_EVENTS_LIMIT)
{}

void SummaryMetrics::SetTopEventsLimit(uint64_t limit)
{
    m_top_events_limit = limit;
}

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
            case kRPVAnalysisSummaryMetricPropertyAggregationLevel:
            {
                *value = (uint64_t)m_aggregation_level;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVAnalysisSummaryMetricPropertyNumSubMetrics:
            {
                *value = m_sub_metrics.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyNumTopDispatchEvents:
            {
                if(m_gpu)
                {
                    *value = m_gpu.value().dispatch.top.size();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopDispatchEventInvocationsIndexed:
            {
                if(m_gpu && index < m_gpu.value().dispatch.top.size())
                {
                    *value = m_gpu.value().dispatch.top[index].invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyDispatchEventsInvocationsTotal:
            {
                if(m_gpu)
                {
                    *value = m_gpu.value().dispatch.op_invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyDispatchEventsDurationTotal:
            {
                if(m_gpu)
                {
                    *value = m_gpu.value().dispatch.op_duration;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationSumIndexed:
            {
                if(m_gpu && index < m_gpu.value().dispatch.top.size())
                {
                    *value = m_gpu.value().dispatch.top[index].duration_sum;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationMinIndexed:
            {
                if(m_gpu && index < m_gpu.value().dispatch.top.size())
                {
                    *value = m_gpu.value().dispatch.top[index].duration_min;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationMaxIndexed:
            {
                if(m_gpu && index < m_gpu.value().dispatch.top.size())
                {
                    *value = m_gpu.value().dispatch.top[index].duration_max;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyNumTopMemoryEvents:
            {
                if(m_gpu)
                {
                    *value = m_gpu.value().memory.top.size();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyMemoryEventsInvocationsTotal:
            {
                if(m_gpu)
                {
                    *value = m_gpu.value().memory.op_invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyMemoryEventsDurationTotal:
            {
                if(m_gpu)
                {
                    *value = m_gpu.value().memory.op_duration;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopMemoryEventInvocationsIndexed:
            {
                if(m_gpu && index < m_gpu.value().memory.top.size())
                {
                    *value = m_gpu.value().memory.top[index].invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationSumIndexed:
            {
                if(m_gpu && index < m_gpu.value().memory.top.size())
                {
                    *value = m_gpu.value().memory.top[index].duration_sum;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationMinIndexed:
            {
                if(m_gpu && index < m_gpu.value().memory.top.size())
                {
                    *value = m_gpu.value().memory.top[index].duration_min;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationMaxIndexed:
            {
                if(m_gpu && index < m_gpu.value().memory.top.size())
                {
                    *value = m_gpu.value().memory.top[index].duration_max;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyNumCounters:
            {
                if(m_gpu)
                {
                    *value = m_gpu.value().counters.size();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyCounterDurationIndexed:
            {
                if(m_gpu && index < m_gpu.value().counters.size())
                {
                    *value = m_gpu.value().counters[index].duration;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyNumTopMemoryEvents:
            {
                if(m_cpu)
                {
                    *value = m_cpu.value().memory.top.size();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyMemoryEventsInvocationsTotal:
            {
                if(m_cpu)
                {
                    *value = m_cpu.value().memory.op_invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyMemoryEventsDurationTotal:
            {
                if(m_cpu)
                {
                    *value = m_cpu.value().memory.op_duration;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyTopMemoryEventInvocationsIndexed:
            {
                if(m_cpu && index < m_cpu.value().memory.top.size())
                {
                    *value = m_cpu.value().memory.top[index].invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationSumIndexed:
            {
                if(m_cpu && index < m_cpu.value().memory.top.size())
                {
                    *value = m_cpu.value().memory.top[index].duration_sum;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationMinIndexed:
            {
                if(m_cpu && index < m_cpu.value().memory.top.size())
                {
                    *value = m_cpu.value().memory.top[index].duration_min;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationMaxIndexed:
            {
                if(m_cpu && index < m_cpu.value().memory.top.size())
                {
                    *value = m_cpu.value().memory.top[index].duration_max;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyNumCounters:
            {
                if(m_cpu)
                {
                    *value = m_cpu.value().counters.size();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyCounterDurationIndexed:
            {
                if(m_cpu && index < m_cpu.value().counters.size())
                {
                    *value = m_cpu.value().counters[index].duration;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyNumTopMemoryEvents:
            {
                if(m_memory)
                {
                    *value = m_memory.value().top.size();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyMemoryEventsInvocationsTotal:
            {
                if(m_memory)
                {
                    *value = m_memory.value().op_invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyMemoryEventsDurationTotal:
            {
                if(m_memory)
                {
                    *value = m_memory.value().op_duration;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventInvocationsIndexed:
            {
                if(m_memory && index < m_memory.value().top.size())
                {
                    *value = m_memory.value().top[index].invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationSumIndexed:
            {
                if(m_memory && index < m_memory.value().top.size())
                {
                    *value = m_memory.value().top[index].duration_sum;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationMinIndexed:
            {
                if(m_memory && index < m_memory.value().top.size())
                {
                    *value = m_memory.value().top[index].duration_min;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationMaxIndexed:
            {
                if(m_memory && index < m_memory.value().top.size())
                {
                    *value = m_memory.value().top[index].duration_max;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyNumTopLaunchEvents:
            {
                if(m_launch)
                {
                    *value = m_launch.value().top.size();
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyLaunchEventsInvocationsTotal:
            {
                if(m_launch)
                {
                    *value = m_launch.value().op_invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyLaunchEventsDurationTotal:
            {
                if(m_launch)
                {
                    *value = m_launch.value().op_duration;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventInvocationsIndexed:
            {
                if(m_launch && index < m_launch.value().top.size())
                {
                    *value = m_launch.value().top[index].invocations;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationSumIndexed:
            {
                if(m_launch && index < m_launch.value().top.size())
                {
                    *value = m_launch.value().top[index].duration_sum;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationMinIndexed:
            {
                if(m_launch && index < m_launch.value().top.size())
                {
                    *value = m_launch.value().top[index].duration_min;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationMaxIndexed:
            {
                if(m_launch && index < m_launch.value().top.size())
                {
                    *value = m_launch.value().top[index].duration_max;
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
            case kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationPctIndexed:
            {
                if(m_gpu && index < m_gpu.value().dispatch.top.size())
                {
                    *value = m_gpu.value().dispatch.top[index].duration_pct_of_total;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationAvgIndexed:
            {
                if(m_gpu && index < m_gpu.value().dispatch.top.size())
                {
                    *value = m_gpu.value().dispatch.top[index].invocations ? (double)m_gpu.value().dispatch.top[index].duration_sum / m_gpu.value().dispatch.top[index].invocations : 0.0;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationPctIndexed:
            {
                if(m_gpu && index < m_gpu.value().memory.top.size())
                {
                    *value = m_gpu.value().memory.top[index].duration_pct_of_total;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationAvgIndexed:
            {
                if(m_gpu && index < m_gpu.value().memory.top.size())
                {
                    *value = m_gpu.value().memory.top[index].invocations ? (double)m_gpu.value().memory.top[index].duration_sum / m_gpu.value().memory.top[index].invocations : 0.0;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyCounterMinIndexed:
            {
                if(m_gpu && index < m_gpu.value().counters.size())
                {
                    *value = m_gpu.value().counters[index].stats.min_value;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyCounterMaxIndexed:
            {
                if(m_gpu && index < m_gpu.value().counters.size())
                {
                    *value = m_gpu.value().counters[index].stats.max_value;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyCounterMeanIndexed:
            {
                if(m_gpu && index < m_gpu.value().counters.size())
                {
                    *value = m_gpu.value().counters[index].stats.mean_value;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyCounterStdDevIndexed:
            {
                if(m_gpu && index < m_gpu.value().counters.size())
                {
                    *value = m_gpu.value().counters[index].stats.std_dev;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationPctIndexed:
            {
                if(m_cpu && index < m_cpu.value().memory.top.size())
                {
                    *value = m_cpu.value().memory.top[index].duration_pct_of_total;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationAvgIndexed:
            {
                if(m_cpu && index < m_cpu.value().memory.top.size())
                {
                    *value = m_cpu.value().memory.top[index].invocations ? (double)m_cpu.value().memory.top[index].duration_sum / m_cpu.value().memory.top[index].invocations : 0.0;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyCounterMinIndexed:
            {
                if(m_cpu && index < m_cpu.value().counters.size())
                {
                    *value = m_cpu.value().counters[index].stats.min_value;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyCounterMaxIndexed:
            {
                if(m_cpu && index < m_cpu.value().counters.size())
                {
                    *value = m_cpu.value().counters[index].stats.max_value;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyCounterMeanIndexed:
            {
                if(m_cpu && index < m_cpu.value().counters.size())
                {
                    *value = m_cpu.value().counters[index].stats.mean_value;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyCounterStdDevIndexed:
            {
                if(m_cpu && index < m_cpu.value().counters.size())
                {
                    *value = m_cpu.value().counters[index].stats.std_dev;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationPctIndexed:
            {
                if(m_memory && index < m_memory.value().top.size())
                {
                    *value = m_memory.value().top[index].duration_pct_of_total;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationAvgIndexed:
            {
                if(m_memory && index < m_memory.value().top.size())
                {
                    *value = m_memory.value().top[index].invocations ? (double)m_memory.value().top[index].duration_sum / m_memory.value().top[index].invocations : 0.0;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationPctIndexed:
            {
                if(m_launch && index < m_launch.value().top.size())
                {
                    *value = m_launch.value().top[index].duration_pct_of_total;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationAvgIndexed:
            {
                if(m_launch && index < m_launch.value().top.size())
                {
                    *value = m_launch.value().top[index].invocations ? (double)m_launch.value().top[index].duration_sum / m_launch.value().top[index].invocations : 0.0;
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
            case kRPVAnalysisSummaryMetricPropertySubMetricsIndexed:
            {
                if(index < m_sub_metrics.size())
                {
                    *value = (rocprofvis_handle_t*)&m_sub_metrics[index];
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryMetricPropertyMetadata:
            {
                switch(m_aggregation_level)
                {
                    case kRPVAnalysisSummaryAggregationLevelTrace:
                    {
                        *value = (rocprofvis_handle_t*)m_metadata.trace;
                        break;
                    }
                    case kRPVAnalysisSummaryAggregationLevelNode:
                    {
                        *value = (rocprofvis_handle_t*)m_metadata.node;
                        break;
                    }
                    case kRPVAnalysisSummaryAggregationLevelProcessor:
                    {
                        *value = (rocprofvis_handle_t*)m_metadata.processor;
                        break;
                    }
                    case kRPVAnalysisSummaryAggregationLevelProcess:
                    {
                        *value = (rocprofvis_handle_t*)m_metadata.process;
                        break;
                    }
                    case kRPVAnalysisSummaryAggregationLevelThread:
                    {
                        *value = (rocprofvis_handle_t*)m_metadata.thread;
                        break;
                    }
                    default:
                    {
                        *value = nullptr;
                        break;
                    }
                }
                if(*value)
                {
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyCounterIndexed:
            {
                if(m_gpu && index < m_gpu.value().counters.size())
                {
                    *value = (rocprofvis_handle_t*)m_gpu.value().counters[index].metadata;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyCounterIndexed:
            {
                if(m_cpu && index < m_cpu.value().counters.size())
                {
                    *value = (rocprofvis_handle_t*)m_cpu.value().counters[index].metadata;
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
            case kRPVAnalysisSummaryGpuPropertyTopDispatchEventNameIndexed:
            {
                if(m_gpu && index < m_gpu.value().dispatch.top.size())
                {
                    result = GetStdStringImpl(value, length, m_gpu.value().dispatch.top[index].name);
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyTopMemoryEventNameIndexed:
            {
                if(m_gpu && index < m_gpu.value().memory.top.size())
                {
                    result = GetStdStringImpl(value, length, m_gpu.value().memory.top[index].name);
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyTopMemoryEventNameIndexed:
            {
                if(m_cpu && index < m_cpu.value().memory.top.size())
                {
                    result = GetStdStringImpl(value, length, m_cpu.value().memory.top[index].name);
                }
                break;
            }
            case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventNameIndexed:
            {
                if(m_memory && index < m_memory.value().top.size())
                {
                    result = GetStdStringImpl(value, length, m_memory.value().top[index].name);
                }
                break;
            }
            case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventNameIndexed:
            {
                if(m_launch && index < m_launch.value().top.size())
                {
                    result = GetStdStringImpl(value, length, m_launch.value().top[index].name);
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
        case kRPVAnalysisSummaryMetricPropertyAggregationLevel:
        {
            m_aggregation_level = (rocprofvis_analysis_summary_aggregation_level_t)value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryMetricPropertyNumSubMetrics:
        {
            m_sub_metrics.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyNumTopDispatchEvents:
        {
            if(!m_gpu)
            {
                m_gpu = GPU{};
            }
            m_gpu.value().dispatch.top.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopDispatchEventInvocationsIndexed:
        {
            if(m_gpu && index < m_gpu.value().dispatch.top.size())
            {
                m_gpu.value().dispatch.top[index].invocations = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyDispatchEventsInvocationsTotal:
        {
            if(m_gpu)
            {
                m_gpu.value().dispatch.op_invocations = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyDispatchEventsDurationTotal:
        {
            if(m_gpu)
            {
                m_gpu.value().dispatch.op_duration = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationSumIndexed:
        {
            if(m_gpu && index < m_gpu.value().dispatch.top.size())
            {
                m_gpu.value().dispatch.top[index].duration_sum = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationMinIndexed:
        {
            if(m_gpu && index < m_gpu.value().dispatch.top.size())
            {
                m_gpu.value().dispatch.top[index].duration_min = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationMaxIndexed:
        {
            if(m_gpu && index < m_gpu.value().dispatch.top.size())
            {
                m_gpu.value().dispatch.top[index].duration_max = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyNumTopMemoryEvents:
        {
            if(!m_gpu)
            {
                m_gpu = GPU{};
            }
            m_gpu.value().memory.top.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyMemoryEventsInvocationsTotal:
        {
            if(!m_gpu)
            {
                m_gpu = GPU{};
            }
            m_gpu.value().memory.op_invocations = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyMemoryEventsDurationTotal:
        {
            if(!m_gpu)
            {
                m_gpu = GPU{};
            }
            m_gpu.value().memory.op_duration = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopMemoryEventInvocationsIndexed:
        {
            if(m_gpu && index < m_gpu.value().memory.top.size())
            {
                m_gpu.value().memory.top[index].invocations = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationSumIndexed:
        {
            if(m_gpu && index < m_gpu.value().memory.top.size())
            {
                m_gpu.value().memory.top[index].duration_sum = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationMinIndexed:
        {
            if(m_gpu && index < m_gpu.value().memory.top.size())
            {
                m_gpu.value().memory.top[index].duration_min = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationMaxIndexed:
        {
            if(m_gpu && index < m_gpu.value().memory.top.size())
            {
                m_gpu.value().memory.top[index].duration_max = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyNumCounters:
        {
            if(!m_gpu)
            {
                m_gpu = GPU{};
            }
            m_gpu.value().counters.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyCounterDurationIndexed:
        {
            if(m_gpu && index < m_gpu.value().counters.size())
            {
                m_gpu.value().counters[index].duration = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyNumTopMemoryEvents:
        {
            if(!m_cpu)
            {
                m_cpu = CPU{};
            }
            m_cpu.value().memory.top.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyMemoryEventsInvocationsTotal:
        {
            if(!m_cpu)
            {
                m_cpu = CPU{};
            }
            m_cpu.value().memory.op_invocations = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyMemoryEventsDurationTotal:
        {
            if(!m_cpu)
            {
                m_cpu = CPU{};
            }
            m_cpu.value().memory.op_duration = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyTopMemoryEventInvocationsIndexed:
        {
            if(m_cpu && index < m_cpu.value().memory.top.size())
            {
                m_cpu.value().memory.top[index].invocations = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationSumIndexed:
        {
            if(m_cpu && index < m_cpu.value().memory.top.size())
            {
                m_cpu.value().memory.top[index].duration_sum = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationMinIndexed:
        {
            if(m_cpu && index < m_cpu.value().memory.top.size())
            {
                m_cpu.value().memory.top[index].duration_min = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationMaxIndexed:
        {
            if(m_cpu && index < m_cpu.value().memory.top.size())
            {
                m_cpu.value().memory.top[index].duration_max = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyNumCounters:
        {
            if(!m_cpu)
            {
                m_cpu = CPU{};
            }
            m_cpu.value().counters.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyCounterDurationIndexed:
        {
            if(m_cpu && index < m_cpu.value().counters.size())
            {
                m_cpu.value().counters[index].duration = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryTraceNodePropertyNumTopMemoryEvents:
        {
            if(!m_memory)
            {
                m_memory = TopOpEvents{};
            }
            m_memory.value().top.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryTraceNodePropertyMemoryEventsInvocationsTotal:
        {
            if(!m_memory)
            {
                m_memory = TopOpEvents{};
            }
            m_memory.value().op_invocations = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryTraceNodePropertyMemoryEventsDurationTotal:
        {
            if(!m_memory)
            {
                m_memory = TopOpEvents{};
            }
            m_memory.value().op_duration = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventInvocationsIndexed:
        {
            if(m_memory && index < m_memory.value().top.size())
            {
                m_memory.value().top[index].invocations = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationSumIndexed:
        {
            if(m_memory && index < m_memory.value().top.size())
            {
                m_memory.value().top[index].duration_sum = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationMinIndexed:
        {
            if(m_memory && index < m_memory.value().top.size())
            {
                m_memory.value().top[index].duration_min = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationMaxIndexed:
        {
            if(m_memory && index < m_memory.value().top.size())
            {
                m_memory.value().top[index].duration_max = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryProcessThreadPropertyNumTopLaunchEvents:
        {
            if(!m_launch)
            {
                m_launch = TopOpEvents{};
            }
            m_launch.value().top.resize(value);
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryProcessThreadPropertyLaunchEventsInvocationsTotal:
        {
            if(!m_launch)
            {
                m_launch = TopOpEvents{};
            }
            m_launch.value().op_invocations = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryProcessThreadPropertyLaunchEventsDurationTotal:
        {
            if(!m_launch)
            {
                m_launch = TopOpEvents{};
            }
            m_launch.value().op_duration = value;
            result = kRocProfVisResultSuccess;
            break;
        }
        case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventInvocationsIndexed:
        {
            if(m_launch && index < m_launch.value().top.size())
            {
                m_launch.value().top[index].invocations = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationSumIndexed:
        {
            if(m_launch && index < m_launch.value().top.size())
            {
                m_launch.value().top[index].duration_sum = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationMinIndexed:
        {
            if(m_launch && index < m_launch.value().top.size())
            {
                m_launch.value().top[index].duration_min = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationMaxIndexed:
        {
            if(m_launch && index < m_launch.value().top.size())
            {
                m_launch.value().top[index].duration_max = value;
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
        case kRPVAnalysisSummaryGpuPropertyTopDispatchEventDurationPctIndexed:
        {
            if(m_gpu && index < m_gpu.value().dispatch.top.size())
            {
                m_gpu.value().dispatch.top[index].duration_pct_of_total = static_cast<float>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopMemoryEventDurationPctIndexed:
        {
            if(m_gpu && index < m_gpu.value().memory.top.size())
            {
                m_gpu.value().memory.top[index].duration_pct_of_total = static_cast<float>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyCounterMinIndexed:
        {
            if(m_gpu && index < m_gpu.value().counters.size())
            {
                m_gpu.value().counters[index].stats.min_value = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyCounterMaxIndexed:
        {
            if(m_gpu && index < m_gpu.value().counters.size())
            {
                m_gpu.value().counters[index].stats.max_value = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyCounterMeanIndexed:
        {
            if(m_gpu && index < m_gpu.value().counters.size())
            {
                m_gpu.value().counters[index].stats.mean_value = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyCounterStdDevIndexed:
        {
            if(m_gpu && index < m_gpu.value().counters.size())
            {
                m_gpu.value().counters[index].stats.std_dev = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyTopMemoryEventDurationPctIndexed:
        {
            if(m_cpu && index < m_cpu.value().memory.top.size())
            {
                m_cpu.value().memory.top[index].duration_pct_of_total = static_cast<float>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyCounterMinIndexed:
        {
            if(m_cpu && index < m_cpu.value().counters.size())
            {
                m_cpu.value().counters[index].stats.min_value = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyCounterMaxIndexed:
        {
            if(m_cpu && index < m_cpu.value().counters.size())
            {
                m_cpu.value().counters[index].stats.max_value = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyCounterMeanIndexed:
        {
            if(m_cpu && index < m_cpu.value().counters.size())
            {
                m_cpu.value().counters[index].stats.mean_value = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyCounterStdDevIndexed:
        {
            if(m_cpu && index < m_cpu.value().counters.size())
            {
                m_cpu.value().counters[index].stats.std_dev = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventDurationPctIndexed:
        {
            if(m_memory && index < m_memory.value().top.size())
            {
                m_memory.value().top[index].duration_pct_of_total = static_cast<float>(value);
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventDurationPctIndexed:
        {
            if(m_launch && index < m_launch.value().top.size())
            {
                m_launch.value().top[index].duration_pct_of_total = static_cast<float>(value);
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
            case kRPVAnalysisSummaryMetricPropertySubMetricsIndexed:
            {
                if(index < m_sub_metrics.size())
                {
                    m_sub_metrics[index] = *(SummaryMetrics*)value;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryMetricPropertyMetadata:
            {
                switch(m_aggregation_level)
                {
                    case kRPVAnalysisSummaryAggregationLevelTrace:
                    {
                        m_metadata.trace = (SystemTrace*)value;
                        result = kRocProfVisResultSuccess;
                        break;
                    }
                    case kRPVAnalysisSummaryAggregationLevelNode:
                    {
                        m_metadata.node = (Node*)value;
                        result = kRocProfVisResultSuccess;
                        break;
                    }
                    case kRPVAnalysisSummaryAggregationLevelProcessor:
                    {
                        m_metadata.processor = (Processor*)value;
                        result = kRocProfVisResultSuccess;
                        break;
                    }
                    case kRPVAnalysisSummaryAggregationLevelProcess:
                    {
                        m_metadata.process = (Process*)value;
                        result = kRocProfVisResultSuccess;
                        break;
                    }
                    case kRPVAnalysisSummaryAggregationLevelThread:
                    {
                        m_metadata.thread = (Thread*)value;
                        result = kRocProfVisResultSuccess;
                        break;
                    }
                    default:
                    {
                        break;
                    }
                }
                break;
            }
            case kRPVAnalysisSummaryGpuPropertyCounterIndexed:
            {
                if(m_gpu && index < m_gpu.value().counters.size())
                {
                    m_gpu.value().counters[index].metadata = (Controller::Counter*)value;
                    result = kRocProfVisResultSuccess;
                }
                break;
            }
            case kRPVAnalysisSummaryCpuPropertyCounterIndexed:
            {
                if(m_cpu && index < m_cpu.value().counters.size())
                {
                    m_cpu.value().counters[index].metadata = (Controller::Counter*)value;
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
        case kRPVAnalysisSummaryGpuPropertyTopDispatchEventNameIndexed:
        {
            if(m_gpu && index < m_gpu.value().dispatch.top.size())
            {
                m_gpu.value().dispatch.top[index].name = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryGpuPropertyTopMemoryEventNameIndexed:
        {
            if(m_gpu && index < m_gpu.value().memory.top.size())
            {
                m_gpu.value().memory.top[index].name = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryCpuPropertyTopMemoryEventNameIndexed:
        {
            if(m_cpu && index < m_cpu.value().memory.top.size())
            {
                m_cpu.value().memory.top[index].name = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryTraceNodePropertyTopMemoryEventNameIndexed:
        {
            if(m_memory && index < m_memory.value().top.size())
            {
                m_memory.value().top[index].name = value;
                result = kRocProfVisResultSuccess;
            }
            break;
        }
        case kRPVAnalysisSummaryProcessThreadPropertyTopLaunchEventNameIndexed:
        {
            if(m_launch && index < m_launch.value().top.size())
            {
                m_launch.value().top[index].name = value;
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
        std::vector<const TopOpEvents*> dispatch_subs;
        std::vector<const TopOpEvents*> gpu_memory_subs;
        std::vector<const TopOpEvents*> cpu_memory_subs;
        std::vector<const TopOpEvents*> memory_subs;
        std::vector<const TopOpEvents*> launch_subs;
        std::vector<const std::vector<Counter>*> gpu_counter_subs;
        std::vector<const std::vector<Counter>*> cpu_counter_subs;
        for(SummaryMetrics& sub : m_sub_metrics)
        {
            sub.m_top_events_limit = m_top_events_limit;
            result = sub.AggregateSubMetrics();
            if(result == kRocProfVisResultSuccess)
            {
                if(sub.m_gpu)
                {
                    if(sub.m_gpu.value().dispatch.top.size())
                    {
                        dispatch_subs.push_back(&sub.m_gpu.value().dispatch);
                    }
                    if(sub.m_gpu.value().memory.top.size())
                    {
                        gpu_memory_subs.push_back(&sub.m_gpu.value().memory);
                        memory_subs.push_back(&sub.m_gpu.value().memory);
                    }
                    if(sub.m_gpu.value().counters.size())
                    {
                        gpu_counter_subs.push_back(&sub.m_gpu.value().counters);
                        std::stable_sort(sub.m_gpu.value().counters.begin(), sub.m_gpu.value().counters.end(), [this](const Counter& a, const Counter& b) { return CounterSortComparator(a, b); });
                    }                    
                }
                if(sub.m_cpu)
                {
                    if(sub.m_cpu.value().memory.top.size())
                    {
                        cpu_memory_subs.push_back(&sub.m_cpu.value().memory);
                        memory_subs.push_back(&sub.m_cpu.value().memory);
                    }
                    if(sub.m_cpu.value().counters.size())
                    {
                        cpu_counter_subs.push_back(&sub.m_cpu.value().counters);
                        std::stable_sort(sub.m_cpu.value().counters.begin(), sub.m_cpu.value().counters.end(), [this](const Counter& a, const Counter& b) { return CounterSortComparator(a, b); });
                    }
                }
                if(sub.m_launch && sub.m_launch.value().top.size())
                {
                    launch_subs.push_back(&sub.m_launch.value());
                }
                if(m_gpu)
                {
                    if(sub.m_gpu)
                    {
                        m_gpu.value().dispatch.op_invocations += sub.m_gpu.value().dispatch.op_invocations;
                        m_gpu.value().dispatch.op_duration += sub.m_gpu.value().dispatch.op_duration;
                        m_gpu.value().memory.op_invocations += sub.m_gpu.value().memory.op_invocations;
                        m_gpu.value().memory.op_duration += sub.m_gpu.value().memory.op_duration;
                    }
                }
                else
                {
                    m_gpu = sub.m_gpu;
                }
                if(m_cpu)
                {
                    if(sub.m_cpu)
                    {
                        m_cpu.value().memory.op_invocations += sub.m_cpu.value().memory.op_invocations;
                        m_cpu.value().memory.op_duration += sub.m_cpu.value().memory.op_duration;
                    }
                }
                else
                {
                    m_cpu = sub.m_cpu;
                }
                if(m_launch)
                {
                    if(sub.m_launch && sub.m_launch.value().top.size())
                    {
                        m_launch.value().op_invocations += sub.m_launch.value().op_invocations;
                        m_launch.value().op_duration += sub.m_launch.value().op_duration;
                    }
                }
                else
                {
                    m_launch = sub.m_launch;
                }
            }
        }
        if(m_gpu)
        {
            m_gpu.value().counters = AggregateCounters(gpu_counter_subs);
            std::stable_sort(m_gpu.value().counters.begin(), m_gpu.value().counters.end(), [this](const Counter& a, const Counter& b) { return CounterSortComparator(a, b); });
            AggregateTopEvents(dispatch_subs, m_gpu.value().dispatch);
            AggregateTopEvents(gpu_memory_subs, m_gpu.value().memory);
            if(m_memory)
            {
                m_memory.value().op_invocations += m_gpu.value().memory.op_invocations;
                m_memory.value().op_duration += m_gpu.value().memory.op_duration;
            }
            else
            {
                m_memory = m_gpu.value().memory;
            }
        }
        if(m_cpu)
        {
            m_cpu.value().counters = AggregateCounters(cpu_counter_subs);
            std::stable_sort(m_cpu.value().counters.begin(), m_cpu.value().counters.end(), [this](const Counter& a, const Counter& b) { return CounterSortComparator(a, b); });
            AggregateTopEvents(cpu_memory_subs, m_cpu.value().memory);         
            if(m_memory)
            {
                m_memory.value().op_invocations += m_cpu.value().memory.op_invocations;
                m_memory.value().op_duration += m_cpu.value().memory.op_duration;
            }
            else
            {
                m_memory = m_cpu.value().memory;
            }
        }
        if(m_memory)
        {
            AggregateTopEvents(memory_subs, m_memory.value());
        }
        if(m_launch)
        {
            AggregateTopEvents(launch_subs, m_launch.value());          
        }
        for(SummaryMetrics& sub : m_sub_metrics)
        {
            PruneTopEvents(sub);
        }  
    }
    if(m_aggregation_level == __kRPVAnalysisSummaryAggregationLevelFirst)
    {
        PruneTopEvents(*this);
    } 
    return result;
}

void SummaryMetrics::Reset()
{
    m_aggregation_level = kRPVAnalysisSummaryAggregationLevelTrace;
    m_metadata.trace = nullptr;
    m_gpu = std::nullopt;
    m_cpu = std::nullopt;
    m_memory = std::nullopt;
    m_launch = std::nullopt;
    m_sub_metrics.clear();
}

bool SummaryMetrics::Empty() const
{
    return !(m_gpu || m_cpu || m_memory || m_launch || !m_sub_metrics.empty());
}

void SummaryMetrics::AggregateTopEvents(const std::vector<const TopOpEvents*>& subs, TopOpEvents& output)
{
    std::unordered_map<std::string_view, TopOpEvents::Event> event_map;
    for(const TopOpEvents* sub : subs)
    {
        if(sub)
        {
            for(const TopOpEvents::Event& event : sub->top)
            {
                if(event.name != TOP_EVENT_REMAINDER_NAME)
                {
                    if(event_map.count(event.name))
                    {
                        event_map[event.name].invocations += event.invocations;
                        event_map[event.name].duration_sum += event.duration_sum;
                        event_map[event.name].duration_min = std::min(event_map[event.name].duration_min, event.duration_min);
                        event_map[event.name].duration_max = std::max(event_map[event.name].duration_max, event.duration_max);
                    }
                    else
                    {
                        event_map[event.name] = event;
                    }
                }
            }
        }
    }
    output.top.clear();
    for(const std::pair<std::string_view, TopOpEvents::Event>& event : event_map)
    {
        output.top.emplace_back(event.second);
    }
    std::stable_sort(output.top.begin(), output.top.end(), [](const TopOpEvents::Event& a, const TopOpEvents::Event& b){
        return a.duration_sum > b.duration_sum;
    });
    for(TopOpEvents::Event& event : output.top)
    {
        event.duration_pct_of_total = output.op_duration ? (float)event.duration_sum / output.op_duration : 0.0f;
    }
}

void
SummaryMetrics::PruneTopEvents(SummaryMetrics& metrics)
{
    if(metrics.m_gpu)
    {
        AppendTopEventsRemainder(metrics.m_gpu.value().dispatch);
        AppendTopEventsRemainder(metrics.m_gpu.value().memory);
    }
    if(metrics.m_cpu)
    {
        AppendTopEventsRemainder(metrics.m_cpu.value().memory);
    }
    if(metrics.m_memory)
    {
        AppendTopEventsRemainder(metrics.m_memory.value());
    }
    if(metrics.m_launch)
    {
        AppendTopEventsRemainder(metrics.m_launch.value());
    }
}

void SummaryMetrics::AppendTopEventsRemainder(TopOpEvents& events)
{
    uint64_t invocations = 0;
    uint64_t duration = 0;
    const size_t limit = static_cast<size_t>(m_top_events_limit);
    for(size_t i = 0; i < std::min(limit, events.top.size()); i++)
    {
        invocations += events.top[i].invocations;
        duration += events.top[i].duration_sum;
    }
    if(events.top.size() > limit)
    {
        events.top.resize(limit);
    }
    if(invocations < events.op_invocations || duration < events.op_duration)
    {
        uint64_t duration_remainder = duration < events.op_duration ? events.op_duration - duration : 0;
        events.top.emplace_back(TopOpEvents::Event{ TOP_EVENT_REMAINDER_NAME, invocations < events.op_invocations ? events.op_invocations - invocations : 0, duration_remainder, 0, 0, events.op_duration ? (float)duration_remainder / events.op_duration : 0.0f });
    }
}

std::vector<SummaryMetrics::Counter> SummaryMetrics::AggregateCounters(const std::vector<const std::vector<Counter>*>& subs)
{
    std::vector<Counter> aggregate;
    std::vector<double> second_moment_sums;
    std::unordered_map<std::string, size_t> merge_index;
    for(const std::vector<Counter>* counters : subs)
    {
        if(counters)
        {
            for(const Counter& counter : *counters)
            {
                if(counter.metadata)
                {
                    std::string key = GetStringProperty(counter.metadata, kRPVControllerCounterName) + '\0' + GetStringProperty(counter.metadata, kRPVControllerCounterUnits) + '\0' + GetStringProperty(counter.metadata, kRPVControllerCounterValueType);
                    double second_moment = counter.stats.std_dev * counter.stats.std_dev + counter.stats.mean_value * counter.stats.mean_value;
                    if(merge_index.count(key))
                    {
                        size_t i = merge_index[key];
                        aggregate[i].stats.min_value = std::min(aggregate[i].stats.min_value, counter.stats.min_value);
                        aggregate[i].stats.max_value = std::max(aggregate[i].stats.max_value, counter.stats.max_value);
                        aggregate[i].stats.mean_value += counter.stats.mean_value * counter.duration;
                        aggregate[i].duration += counter.duration;
                        second_moment_sums[i] += second_moment * counter.duration;
                    }
                    else
                    {
                        merge_index[key] = aggregate.size();
                        aggregate.push_back(counter);
                        aggregate.back().stats.mean_value = counter.stats.mean_value * counter.duration;
                        second_moment_sums.push_back(second_moment * counter.duration);
                    }
                }
            }
        }
    }
    for(size_t i = 0; i < aggregate.size(); i++)
    {
        aggregate[i].stats.mean_value /= aggregate[i].duration;
        aggregate[i].stats.std_dev = std::sqrt(std::max(0.0, second_moment_sums[i] / aggregate[i].duration -aggregate[i].stats.mean_value * aggregate[i].stats.mean_value));
    }
    return aggregate;
}

bool SummaryMetrics::CounterSortComparator(const Counter& a, const Counter& b)
{
    
    std::string a_name = a.metadata ? GetStringProperty(a.metadata, kRPVControllerCounterName) : "";
    std::string b_name = b.metadata ? GetStringProperty(b.metadata, kRPVControllerCounterName) : "";
    return std::lexicographical_compare(a_name.begin(), a_name.end(), b_name.begin(), b_name.end(), [](char x, char y) { return std::tolower(static_cast<unsigned char>(x)) < std::tolower(static_cast<unsigned char>(y)); });
}

std::string SummaryMetrics::GetStringProperty(Handle* handle, rocprofvis_property_t property)
{
    std::string value;
    uint32_t length = 0;
    if(handle->GetString(property, 0, nullptr, &length) == kRocProfVisResultSuccess)
    {
        value.resize(length);
        handle->GetString(property, 0, value.data(), &length);
    }
    return value;
}

}
}
