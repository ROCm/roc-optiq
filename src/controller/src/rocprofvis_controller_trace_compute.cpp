// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_trace_compute.h"
#include "rocprofvis_controller_compute_metrics.h"
#include "rocprofvis_controller_plot_compute.h"
#include "rocprofvis_controller_table_compute.h"
#include "rocprofvis_core_assert.h"
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <pybind11/embed.h>
#include <string>

namespace RocProfVis
{
namespace Controller
{

ComputeTrace::ComputeTrace()
: Handle(0, 0)
{}

ComputeTrace::~ComputeTrace()
{
    for(auto& it : m_tables)
    {
        ComputeTable* table = it.second;
        if(table)
        {
            delete table;
        }
    }
}

rocprofvis_result_t
ComputeTrace::Load(char const* const directory)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if(std::filesystem::exists(directory) && std::filesystem::is_directory(directory))
    {
        for(const std::filesystem::directory_entry& entry :
            std::filesystem::directory_iterator{ directory })
        {
            if(entry.path().extension() == ".csv")
            {
                const std::string& file = entry.path().filename().string();
                if(COMPUTE_TABLE_DEFINITIONS.count(file) > 0)
                {
                    const ComputeTableDefinition& definition =
                        COMPUTE_TABLE_DEFINITIONS.at(file);
                    ComputeTable* table = new ComputeTable(
                        m_tables.size(), definition.m_type, definition.m_title);
                    ROCPROFVIS_ASSERT(table);
                    result = table->Load(entry.path().string());
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    m_tables[definition.m_type] = table;
                }
            }
        }
        spdlog::info("ComputeTrace::Load - {}/{} Tables", m_tables.size(),
                     COMPUTE_TABLE_DEFINITIONS.size());
        for(const ComputeTablePlotDefinition& definition : COMPUTE_PLOT_DEFINITIONS)
        {
            ComputePlot* plot = new ComputePlot(
                m_plots.size(), definition.m_title, definition.x_axis_label,
                definition.y_axis_label, definition.m_type);
            ROCPROFVIS_ASSERT(plot);
            for(const ComputePlotDataSeriesDefinition& series : definition.m_series)
            {
                for(const ComputePlotDataDefinition& data : series.m_values)
                {
                    if(m_tables.count(data.m_table_type) > 0)
                    {
                        result = plot->Load(m_tables[data.m_table_type], series.m_name,
                                            data.m_metric_keys);
                        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    }
                    else
                    {
                        delete plot;
                        result = kRocProfVisResultNotLoaded;
                        break;
                    }
                }
                if(result != kRocProfVisResultSuccess)
                {
                    break;
                }
            }
            if(result == kRocProfVisResultSuccess)
            {
                m_plots[definition.m_type] = plot;
            }
        }
        spdlog::info("ComputeTrace::Load - {}/{} Plots", m_plots.size(),
                     COMPUTE_PLOT_DEFINITIONS.size());
        if(m_tables.count(kRPVControllerComputeTableTypeRooflineBenchmarks) > 0 &&
           m_tables.count(kRPVControllerComputeTableTypeRooflineCounters) > 0)
        {
            for(auto& it : ROOFLINE_DEFINITION.m_plots)
            {
                ComputePlot* plot = new ComputePlot(
                    m_plots.size(), it.second.m_title, it.second.x_axis_label,
                    it.second.y_axis_label, it.second.m_type);
                ROCPROFVIS_ASSERT(plot);
                result = plot->Load(
                    m_tables[kRPVControllerComputeTableTypeRooflineCounters],
                    m_tables[kRPVControllerComputeTableTypeRooflineBenchmarks]);
                if(result == kRocProfVisResultSuccess)
                {
                    m_plots[it.second.m_type] = plot;
                }
            }
        }
        spdlog::info("ComputeTrace::Load - {}/{} Rooflines",
                     m_plots.size() - COMPUTE_PLOT_DEFINITIONS.size(),
                     ROOFLINE_DEFINITION.m_plots.size());
    }

    pybind11::scoped_interpreter guard{};
    pybind11::exec(R"(
        import sys
        sys.path.insert(0, '/home/rocm/Dev/rocm-systems/projects/rocprofiler-compute/src/')
        from rocprof_compute_analyze.analysis_optiq import Analyze
        workload = '/home/rocm/Desktop/workloads/7.1.0/prefix_sum/MI350'
        output = Analyze(workload)

    )");
    pybind11::dict output = pybind11::globals()["output"];
    for(const auto& workload : output)
    {
        spdlog::info("========================================================");
        spdlog::info("Workload:{}", workload.first.cast<std::string>());
        spdlog::info("========================================================");
        spdlog::info("System Info:");
        spdlog::info("========================================================");
        pybind11::dict system_info = workload.second["system_info"];
        for(const auto& field : system_info)
        {
            for(const auto& value : field.second)
            {
                std::string str = field.first.cast<std::string>();
                if(pybind11::isinstance<pybind11::int_>(value))
                {
                    str += ": " + std::to_string(value.cast<uint64_t>());
                }
                else if(pybind11::isinstance<pybind11::float_>(value))
                {
                    str += ": " + std::to_string(value.cast<float>());
                }
                else 
                {
                    str += ": " + value.cast<std::string>();
                }
                spdlog::info("{}", str);
            }
        }
        spdlog::info("========================================================");
        spdlog::info("Metrics:");
        spdlog::info("========================================================");
        pybind11::dict metrics = workload.second["metrics"];
        for(const auto& table : metrics)
        {
            for(const auto& field : table.second.cast<pybind11::dict>())
            {
                for(const auto& value : field.second)
                {                    
                    std::string str = table.first.cast<std::string>() + " " + field.first.cast<std::string>();
                    if(pybind11::isinstance<pybind11::int_>(value))
                    {
                        str += ": " + std::to_string(value.cast<uint64_t>());
                    }
                    else if(pybind11::isinstance<pybind11::float_>(value))
                    {
                        str += ": " + std::to_string(value.cast<float>());
                    }
                    else 
                    {
                        str += ": " + value.cast<std::string>();
                    }
                    spdlog::info("{}", str);
                }
            }            
        }
    }
    return result;
}

rocprofvis_controller_object_type_t
ComputeTrace::GetType(void)
{
    return kRPVControllerObjectTypeComputeTrace;
}

rocprofvis_result_t
ComputeTrace::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        if(kRPVControllerComputeMetricTypeL2CacheRd <= property &&
           property < kRPVControllerComputeMetricTypeCount)
        {
            Data* metric_data = nullptr;
            result            = GetMetric(
                static_cast<rocprofvis_controller_compute_metric_types_t>(property),
                &metric_data);
            if(result == kRocProfVisResultSuccess)
            {
                ROCPROFVIS_ASSERT(metric_data);
                switch(metric_data->GetType())
                {
                    case kRPVControllerPrimitiveTypeUInt64:
                    {
                        result = metric_data->GetUInt64(value);
                        break;
                    }
                    case kRPVControllerPrimitiveTypeDouble:
                    {
                        double data;
                        result = metric_data->GetDouble(&data);
                        if(result == kRocProfVisResultSuccess)
                        {
                            *value = static_cast<uint64_t>(data);
                        }
                        break;
                    }
                    default:
                    {
                        result = kRocProfVisResultInvalidType;
                        break;
                    }
                }
            }
        }
        else
        {
            result = kRocProfVisResultInvalidEnum;
        }
    }
    return result;
}

rocprofvis_result_t
ComputeTrace::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t
ComputeTrace::GetObject(rocprofvis_property_t property, uint64_t index,
                        rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        if(kRPVControllerComputeTableTypeKernelList <= property &&
           property < kRPVControllerComputeTableTypeCount)
        {
            rocprofvis_controller_compute_table_types_t table_type =
                static_cast<rocprofvis_controller_compute_table_types_t>(property);
            if(m_tables.count(table_type) > 0)
            {
                *value = (rocprofvis_handle_t*)
                    m_tables[static_cast<rocprofvis_controller_compute_table_types_t>(
                        property)];
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultNotLoaded;
            }
        }
        else if(kRPVControllerComputePlotTypeKernelDurationPercentage <= property &&
                property < kRPVControllerComputePlotTypeCount)
        {
            rocprofvis_controller_compute_plot_types_t plot_type =
                static_cast<rocprofvis_controller_compute_plot_types_t>(property);
            if(m_plots.count(plot_type) > 0)
            {
                *value = (rocprofvis_handle_t*)
                    m_plots[static_cast<rocprofvis_controller_compute_plot_types_t>(
                        property)];
                result = kRocProfVisResultSuccess;
            }
            else
            {
                result = kRocProfVisResultNotLoaded;
            }
        }
        else
        {
            result = kRocProfVisResultInvalidEnum;
        }
    }
    return result;
}

rocprofvis_result_t
ComputeTrace::GetMetric(const rocprofvis_controller_compute_metric_types_t metric_type,
                        Data**                                             value)
{
    rocprofvis_result_t            result = kRocProfVisResultNotLoaded;
    const ComputeMetricDefinition& metric = COMPUTE_METRIC_DEFINITIONS.at(metric_type);
    const rocprofvis_controller_compute_table_types_t& table_type = metric.m_table_type;
    if(m_tables.count(table_type))
    {
        const std::string&            key = metric.m_metric_key;
        std::pair<std::string, Data*> metric_data;
        result = m_tables[table_type]->GetMetric(key, metric_data);
        if(result == kRocProfVisResultSuccess)
        {
            *value = metric_data.second;
        }
    }
    return result;
}

}  // namespace Controller
}  // namespace RocProfVis
