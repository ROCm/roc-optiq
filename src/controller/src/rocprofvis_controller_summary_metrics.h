// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"
#include "rocprofvis_controller_handle.h"
#include "rocprofvis_c_interface.h"
#include <optional>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class SummaryMetrics : public Handle
{
public:
    SummaryMetrics();
    virtual ~SummaryMetrics();

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
    void PadTopKernels();
    void Reset();
    bool Empty() const;

private:
    template<typename T>
    std::optional<T> AddOptionals(const std::optional<T>& a, const std::optional<T>& b)
    {
        if(a && b)
        {
            return a.value() + b.value();
        }
        else if(a)
        {
            return a;
        }
        else if(b)
        {
            return b;
        }
        else
        {
            return std::nullopt;
        }
    }

    struct KernelMetrics {
        std::string name;
        uint64_t invocations;
        double exec_time_sum;
        double exec_time_min;
        double exec_time_max;
        float exec_time_pct;
    };
    struct GPUMetrics {
        std::optional<float> gfx_util;
        std::optional<float> mem_util;
        std::vector<KernelMetrics> top_kernels;
        double kernel_exec_time_total;
    };
    struct CPUMetrics {
    };

    rocprofvis_controller_summary_aggregation_level_t m_aggregation_level;
    std::optional<uint64_t> m_id;
    std::optional<std::string> m_name;
    std::optional<rocprofvis_controller_processor_type_t> m_processor_type;
    std::optional<uint64_t> m_processor_type_idx;
    std::optional<GPUMetrics> m_gpu;
    std::optional<CPUMetrics> m_cpu;
    std::vector<SummaryMetrics> m_sub_metrics;
};

}
}
