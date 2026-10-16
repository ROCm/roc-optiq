// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_controller_analysis.h"
#include <optional>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class Counter;
class SystemTrace;
class Node;
class Processor;
class Process;
class Thread;

class SummaryMetrics : public Handle
{
public:
    SummaryMetrics();

    rocprofvis_controller_object_type_t GetType(void) final;

    rocprofvis_result_t GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) final;
    rocprofvis_result_t GetDouble(rocprofvis_property_t property, uint64_t index, double* value) final;
    rocprofvis_result_t GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) final;
    rocprofvis_result_t GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length) final;

    rocprofvis_result_t SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value) final;
    rocprofvis_result_t SetDouble(rocprofvis_property_t property, uint64_t index, double value) final;
    rocprofvis_result_t SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value) final;
    rocprofvis_result_t SetString(rocprofvis_property_t property, uint64_t index, char const* value) final;

    rocprofvis_result_t AggregateSubMetrics();
    void Reset();
    bool Empty() const;

private:
    struct TopOpEvents
    {        
        struct Event
        {
            std::string name;
            uint64_t invocations;
            uint64_t duration_sum;
            uint64_t duration_min;
            uint64_t duration_max;
            float duration_pct_of_total;
        };
        std::vector<Event> top;
        uint64_t op_invocations;
        uint64_t op_duration;
    };
    struct Counter
    {
        Controller::Counter* metadata;
        rocprofvis_analysis_counter_statistics_t stats;
        uint64_t weight;
    };
    struct GPU 
    {
        TopOpEvents dispatch;
        TopOpEvents memory;
        std::vector<Counter> counters;        
    };
    struct CPU
    {
        TopOpEvents memory;
        std::vector<Counter> counters;
    };

    void AggregateTopEvents(const std::vector<const TopOpEvents*>& subs, TopOpEvents& output);
    void PruneTopEvents(SummaryMetrics& metrics);
    void AppendTopEventsRemainder(TopOpEvents& events);
    std::vector<Counter> AggregateCounters(const std::vector<const std::vector<Counter>*>& subs);
    bool CounterSortComparator(const Counter& a, const Counter& b);
    std::string GetStringProperty(Handle* handle, rocprofvis_property_t property);

    rocprofvis_analysis_summary_aggregation_level_t m_aggregation_level;
    union
    {
        SystemTrace* trace;
        Node* node;
        Processor* processor;
        Process* process;
        Thread* thread;
    } m_metadata;
    std::optional<GPU> m_gpu;
    std::optional<CPU> m_cpu;
    std::optional<TopOpEvents> m_memory;
    std::optional<TopOpEvents> m_launch;
    std::vector<SummaryMetrics> m_sub_metrics;
};

}
}
