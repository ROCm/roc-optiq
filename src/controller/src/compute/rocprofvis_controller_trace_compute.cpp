// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_trace_compute.h"
#include "rocprofvis_controller_metrics_container.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_workload.h"
#include "rocprofvis_controller_kernel.h"
#include "rocprofvis_controller_roofline.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_core_assert.h"
#include "json.h"

#pragma region Deprecated
#include "rocprofvis_controller_compute_metrics.h"
#include "rocprofvis_controller_plot_compute.h"
#include "rocprofvis_controller_table_compute.h"
#include <filesystem>
#pragma endregion

namespace RocProfVis
{
namespace Controller
{

ComputeTrace::ComputeTrace(const std::string& filename)
: Trace(__kRPVControllerComputePropertiesFirst, __kRPVControllerComputePropertiesLast, filename)
{}

ComputeTrace::~ComputeTrace()
{
    for(Workload* workload : m_workloads)
    {
        delete workload;
    }
#pragma region Deprecated
    for (auto& it : m_tables)
    {
        ComputeTable* table = it.second;
        if (table)
        {
            delete table;
        }
    }
    for (auto& it : m_plots)
    {
        ComputePlot* plot = it.second;
        if (plot)
        {
            delete plot;
        }
    }
#pragma endregion
}

rocprofvis_result_t ComputeTrace::Init()
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    try
    {
        result = kRocProfVisResultSuccess;
    }
    catch(const std::exception&)
    {
        result = kRocProfVisResultMemoryAllocError;
    }
    return result;
}

rocprofvis_result_t ComputeTrace::Load(RocProfVis::Controller::Future& future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    future.Set(JobSystem::Get().IssueJob([this](Future* future) -> rocprofvis_result_t
        {
            (void) future;
            rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
            if(m_trace_file.find(".db", m_trace_file.size() - 3) != std::string::npos)
            {
                result = LoadRocpd();
            }
#pragma region Deprecated
            else if(m_trace_file.find(".csv", m_trace_file.size() - 4) != std::string::npos)
            {
                m_trace_file = std::filesystem::path(m_trace_file).parent_path().string();
                result = LoadCSV();
            }
#pragma endregion
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
        return result;
        },&future));
    if(future.IsValid())
    {
        result = kRocProfVisResultSuccess;
    }

    return result;
}

rocprofvis_controller_object_type_t ComputeTrace::GetType(void) 
{
    return kRPVControllerObjectTypeControllerCompute;
}

rocprofvis_result_t ComputeTrace::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                uint64_t size = 0;
                for(Workload* workload : m_workloads)
                {
                    size += workload->GetUInt64(kRPVControllerCommonMemoryUsageInclusive, 0, &size);
                }
                size += sizeof(ComputeTrace);
                *value = size;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerCommonMemoryUsageExclusive:
            {
                *value = sizeof(ComputeTrace);
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerWorkloadId:
            {
                *value = (uint64_t)m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerNumWorkloads:
            {
                *value = m_workloads.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            default:
            {
#pragma region Deprecated
                if (kRPVControllerComputeMetricTypeL2CacheRd <= property && property < kRPVControllerComputeMetricTypeCount)
                {
                    Data* metric_data = nullptr;
                    result = GetMetric(static_cast<rocprofvis_controller_compute_metric_types_t>(property), &metric_data);
                    if (result == kRocProfVisResultSuccess)
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
                                if (result == kRocProfVisResultSuccess)
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
#pragma endregion
                {
                    result = UnhandledProperty(property);
                }                
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t ComputeTrace::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerWorkloadIndexed:
            {
                if(index < m_workloads.size())
                {
                    *value = (rocprofvis_handle_t*)m_workloads[index];
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
#pragma region Deprecated                
                if(kRPVControllerComputeTableTypeKernelList <= property && property < kRPVControllerComputeTableTypeCount)
                {
                    rocprofvis_controller_compute_table_types_t table_type = static_cast<rocprofvis_controller_compute_table_types_t>(property);
                    if (m_tables.count(table_type) > 0)
                    {
                        *value = (rocprofvis_handle_t*)m_tables[static_cast<rocprofvis_controller_compute_table_types_t>(property)];
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultNotLoaded;
                    }          
                }
                else if(kRPVControllerComputePlotTypeKernelDurationPercentage <= property && property < kRPVControllerComputePlotTypeCount)
                {
                    rocprofvis_controller_compute_plot_types_t plot_type = static_cast<rocprofvis_controller_compute_plot_types_t>(property);
                    if (m_plots.count(plot_type) > 0)
                    {
                        *value = (rocprofvis_handle_t*)m_plots[static_cast<rocprofvis_controller_compute_plot_types_t>(property)];
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        result = kRocProfVisResultNotLoaded;
                    }
                }
                else
#pragma endregion
                {
                    result = UnhandledProperty(property);
                }               
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t ComputeTrace::AsyncFetch(Arguments& args, Future& future, MetricsContainer& output)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    uint64_t num_entries = 0;
    result = output.GetUInt64(kRPVControllerMetricsContainerNumMetrics, 0, &num_entries);
    if(result == kRocProfVisResultSuccess && num_entries == 0)
    {
        result = args.GetUInt64(kRPVControllerMetricArgsNumKernels, 0, &num_entries);
        if(result == kRocProfVisResultSuccess)
        {
            uint64_t uint64_data = 0;
            m_query_arguments.clear();
            for(uint64_t i = 0; i < num_entries; i++)
            {
                result = args.GetUInt64(kRPVControllerMetricArgsKernelIdIndexed, i, &uint64_data);
                if(result == kRocProfVisResultSuccess)
                {
                    m_query_arguments.emplace_back(kRPVComputeParamKernelId, std::to_string(uint64_data));
                } 
                else
                {
                    break;
                }
            }
            if(result == kRocProfVisResultSuccess)
            {
                result = args.GetUInt64(kRPVControllerMetricArgsNumMetrics, 0, &num_entries);
                if(result == kRocProfVisResultSuccess)
                {
                    for(uint64_t i = 0; i < num_entries; i++)
                    {
                        result = args.GetUInt64(kRPVControllerMetricArgsMetricCategoryIdIndexed, i, &uint64_data);
                        if(result == kRocProfVisResultSuccess)
                        {
                            MetricID metric_id((uint32_t)uint64_data);
                            result = args.GetUInt64(kRPVControllerMetricArgsMetricTableIdIndexed, i, &uint64_data);
                            if(result == kRocProfVisResultSuccess)
                            {
                                metric_id.SetTableID((uint32_t)uint64_data);
                                result = args.GetUInt64(kRPVControllerMetricArgsMetricEntryIdIndexed, i, &uint64_data);
                                if(result == kRocProfVisResultSuccess)
                                {
                                    metric_id.SetEntryID((uint32_t)uint64_data);                                   
                                }
                            }
                            m_query_arguments.emplace_back(kRPVComputeParamMetricId, metric_id.ToString());
                            result = kRocProfVisResultSuccess;
                        }
                        else
                        {
                            break;
                        }
                    }
                }
            }
            if(result == kRocProfVisResultSuccess)
            {
                future.Set(JobSystem::Get().IssueJob([this, &output](Future* future) -> rocprofvis_result_t {
                    rocprofvis_result_t result = kRocProfVisResultUnknownError;
                    m_query_output = { 
                        { 
                            { kRPVComputeColumnMetricId, std::nullopt },
                            { kRPVComputeColumnMetricName, std::nullopt },
                            { kRPVComputeColumnKernelUUID, std::nullopt },
                            { kRPVComputeColumnMetricValueName, std::nullopt },
                            { kRPVComputeColumnMetricValue, std::nullopt },
                        }, 
                        {} 
                    };
                    rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(m_dm_handle, kRPVDMDatabaseHandle, 0);
                    rocprofvis_dm_result_t dm_result = ExecuteQuery(db, m_dm_handle, nullptr, future, kRPVComputeFetchMetricValues, m_query_arguments, m_query_output, [this, &output](const QueryDataStore& data_store){
                        uint64_t valid_results = 0;
                        rocprofvis_property_t property;
                        rocprofvis_controller_primitive_type_t type;
                        for(size_t i = 0; i < data_store.rows.size(); i++)
                        {
                            const char* metric_id = data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricId).value()];
                            const char* kernel_id = data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelUUID).value()];
                            const char* value = data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricValue).value()];
                            if(strlen(metric_id) && strlen(kernel_id) && strlen(value))
                            {
                                output.SetUInt64(kRPVControllerMetricsContainerNumMetrics, 0, valid_results + 1);
                                for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                {
                                    if(output.QueryToPropertyEnum(column.first, property, type))
                                    {
                                        SetObjectProperty((rocprofvis_handle_t*)&output, property, valid_results, data_store.rows[i][column.second.value()], type);
                                    }
                                }
                                valid_results++;
                            }
                        }
                    });
                    if(future->IsCancelled())
                    {
                        result = kRocProfVisResultCancelled;
                    }
                    else
                    {
                        result = (dm_result == kRocProfVisDmResultSuccess) ? kRocProfVisResultSuccess : kRocProfVisResultUnknownError;
                    }
                    return result;
                }, &future));
                if(future.IsValid())
                {
                    result = kRocProfVisResultSuccess;
                }
            }
        }
    }
    return result;
}

rocprofvis_result_t ComputeTrace::LoadRocpd()
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    m_dm_handle = rocprofvis_dm_create_trace();
    if(nullptr != m_dm_handle)
    {
        rocprofvis_dm_database_t db = rocprofvis_db_open_database(m_trace_file.c_str(), kComputeSqlite);
        if (nullptr != db && kRocProfVisDmResultSuccess == rocprofvis_dm_bind_trace_to_database(m_dm_handle, db))
        {
            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(&Trace::ProgressCallback, this);
            if (nullptr != object2wait)
            {
                if (kRocProfVisDmResultSuccess == rocprofvis_db_read_metadata_async(db, object2wait))
                {
                    if (kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                    {
                        m_query_arguments.clear();
                        m_query_output = { 
                            { 
                                { kRPVComputeColumnWorkloadId, std::nullopt },
                                { kRPVComputeColumnWorkloadName, std::nullopt },
                                { kRPVComputeColumnWorkloadSubName, std::nullopt },
                                { kRPVComputeColumnWorkloadSysInfo, std::nullopt },
                                { kRPVComputeColumnWorkloadProfileConfig, std::nullopt }
                            }, 
                            {} 
                        };
                        rocprofvis_dm_result_t dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchListOfWorkloads, m_query_arguments, m_query_output, [this](const QueryDataStore& data_store){
                            m_workloads.resize(data_store.rows.size());
                            rocprofvis_property_t property;
                            rocprofvis_controller_primitive_type_t type;
                            for(size_t i = 0; i < data_store.rows.size(); i++)
                            {
                                m_workloads[i] = new Workload();
                                for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                {
                                    if(m_workloads[i]->QueryToPropertyEnum(column.first, property, type))
                                    {
                                        SetObjectProperty((rocprofvis_handle_t*)m_workloads[i], property, 0, data_store.rows[i][column.second.value()], type);
                                    }
                                    else if(column.first == kRPVComputeColumnWorkloadSysInfo)
                                    {
                                        std::pair<jt::Json::Status, jt::Json> json = jt::Json::parse(data_store.rows[i][column.second.value()]);
                                        if(json.first == jt::Json::Status::success && json.second.isObject())
                                        {
                                            m_workloads[i]->SetUInt64(kRPVControllerWorkloadSystemInfoNumEntries, 0, json.second.getObject().size());
                                            int j = 0;
                                            for(std::pair<std::string, jt::Json> data : json.second.getObject())
                                            {
                                                std::replace(data.first.begin(), data.first.end(), '_', ' ');
                                                m_workloads[i]->SetString(kRPVControllerWorkloadSystemInfoEntryNameIndexed, j, data.first.c_str());
                                                m_workloads[i]->SetString(kRPVControllerWorkloadSystemInfoEntryValueIndexed, j++, data.second.isString() ? data.second.getString().c_str() : data.second.toString().c_str());
                                            }
                                        }
                                    }
                                    else if(column.first == kRPVComputeColumnWorkloadProfileConfig)
                                    {
                                        std::pair<jt::Json::Status, jt::Json> json = jt::Json::parse(data_store.rows[i][column.second.value()]);
                                        if(json.first == jt::Json::Status::success && json.second.isObject())
                                        {
                                            m_workloads[i]->SetUInt64(kRPVControllerWorkloadConfigurationNumEntries, 0, json.second.getObject().size());
                                            int j = 0;
                                            for(std::pair<std::string, jt::Json> data : json.second.getObject())
                                            {
                                                m_workloads[i]->SetString(kRPVControllerWorkloadConfigurationEntryNameIndexed, j, data.first.c_str());
                                                m_workloads[i]->SetString(kRPVControllerWorkloadConfigurationEntryValueIndexed, j++, data.second.isString() ? data.second.getString().c_str() : data.second.toString().c_str());
                                            }
                                        }
                                    }
                                }
                            }
                        });
                        if(dm_result == kRocProfVisDmResultSuccess)
                        {
                            for(Workload* workload : m_workloads)
                            {                                
                                uint64_t uint_data = 0;
                                result = workload->GetUInt64(kRPVControllerWorkloadId, 0, &uint_data);
                                if(result == kRocProfVisResultSuccess)
                                {
                                    m_query_arguments = { {kRPVComputeParamWorkloadId, std::to_string(uint_data)} };
                                    m_query_output = { 
                                        { 
                                            { kRPVComputeColumnWorkloadId, std::nullopt },
                                            { kRPVComputeColumnMetricName, std::nullopt },
                                            { kRPVComputeColumnMetricDescription, std::nullopt },
                                            { kRPVComputeColumnTableId, std::nullopt },
                                            { kRPVComputeColumnSubTableId, std::nullopt },
                                            { kRPVComputeColumnMetricTableName, std::nullopt },
                                            { kRPVComputeColumnMetricSubTableName, std::nullopt },
                                            { kRPVComputeColumnMetricUnit, std::nullopt },
                                        }, 
                                        {} 
                                    };
                                    dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchWorkloadMetricsDefinition, m_query_arguments, m_query_output, [this, &workload](const QueryDataStore& data_store){
                                        workload->SetUInt64(kRPVControllerWorkloadNumAvailableMetrics, 0, data_store.rows.size());
                                        rocprofvis_property_t property;
                                        rocprofvis_controller_primitive_type_t type;
                                        for(size_t i = 0; i < data_store.rows.size(); i++)
                                        {
                                            for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                            {
                                                if(workload->QueryToPropertyEnum(column.first, property, type))
                                                {
                                                    SetObjectProperty((rocprofvis_handle_t*)workload, property, i, data_store.rows[i][column.second.value()], type);
                                                }
                                            }
                                        }
                                    });
                                    std::vector<uint32_t> kernel_ids;
                                    if(dm_result == kRocProfVisDmResultSuccess)
                                    {
                                        m_query_output = { 
                                            { 
                                                { kRPVComputeColumnKernelUUID, std::nullopt },
                                                { kRPVComputeColumnKernelName, std::nullopt },
                                                { kRPVComputeColumnKernelsCount, std::nullopt },
                                                { kRPVComputeColumnKernelDurationsSum, std::nullopt },
                                                { kRPVComputeColumnKernelDurationsAvg, std::nullopt },
                                                { kRPVComputeColumnKernelDurationsMedian, std::nullopt },
                                                { kRPVComputeColumnKernelDurationsMin, std::nullopt },
                                                { kRPVComputeColumnKernelDurationsMax, std::nullopt },
                                            }, 
                                            {} 
                                        };
                                        dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchWorkloadTopKernels, m_query_arguments, m_query_output, [this, &workload, &kernel_ids](const QueryDataStore& data_store){
                                            workload->SetUInt64(kRPVControllerWorkloadNumKernels, 0, data_store.rows.size());
                                            rocprofvis_property_t property;
                                            rocprofvis_controller_primitive_type_t type;
                                            for(size_t i = 0; i < data_store.rows.size(); i++)
                                            {
                                                Kernel* kernel = new Kernel();                                                
                                                for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                {                                                    
                                                    if(kernel->QueryToPropertyEnum(column.first, property, type))
                                                    {
                                                        SetObjectProperty((rocprofvis_handle_t*)kernel, property, 0, data_store.rows[i][column.second.value()], type);
                                                    }
                                                }
                                                workload->SetObject(kRPVControllerWorkloadKernelIndexed, i, (rocprofvis_handle_t*)kernel);
                                                uint64_t id = std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelUUID).value()]);
                                                kernel_ids.push_back(id);
                                            }
                                        });
                                    }                                   
                                    if(dm_result == kRocProfVisDmResultSuccess)
                                    {
                                        Roofline* roofline = new Roofline();
                                        m_query_output = { 
                                            { 
                                                { kRPVComputeColumnWorkloadRooflineBenchHBMBw, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchL2Bw, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchL1Bw, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchLDSBw, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchMFMAF8Flops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchFP16Flops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchMFMAF16Flops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchMFMABF16Flops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchFP32Flops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchMFMAF32Flops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchFP64Flops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchMFMAF64Flops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchI8Ops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchMFMAI8Ops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchI32Ops, std::nullopt },
                                                { kRPVComputeColumnWorkloadRooflineBenchI64Ops, std::nullopt },
                                            }, 
                                            {} 
                                        };
                                        dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchWorkloadRooflineCeiling, m_query_arguments, m_query_output, [&roofline, &uint_data](const QueryDataStore& data_store){
                                            if(data_store.rows.size() == 1)
                                            {
                                                std::unordered_map<rocprofvis_controller_roofline_ceiling_compute_type_t, double> compute_ceilings;
                                                std::unordered_map<rocprofvis_controller_roofline_ceiling_bandwidth_type_t, double> bandwidth_ceilings;
                                                rocprofvis_controller_roofline_ceiling_compute_type_t compute_type;
                                                rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth_type;
                                                for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                {
                                                    const char* data = data_store.rows[0][column.second.value()];
                                                    if(strlen(data))
                                                    {
                                                        double value = std::stod(data);
                                                        if(value > 0.0)
                                                        {                                                           
                                                            if(roofline->QueryToPropertyEnum(column.first, compute_type))
                                                            {
                                                                roofline->SetUInt64(kRPVControllerRooflineNumCeilingsCompute, 0, compute_ceilings.size() + 1);
                                                                roofline->SetUInt64(kRPVControllerRooflineCeilingComputeTypeIndexed, compute_ceilings.size(), (uint64_t)compute_type);
                                                                roofline->SetDouble(kRPVControllerRooflineCeilingComputeXIndexed, compute_ceilings.size(), roofline->MaxX());
                                                                roofline->SetDouble(kRPVControllerRooflineCeilingComputeYIndexed, compute_ceilings.size(), value);
                                                                compute_ceilings[compute_type] = value;
                                                            }
                                                            else if(roofline->QueryToPropertyEnum(column.first, bandwidth_type))
                                                            {
                                                                roofline->SetUInt64(kRPVControllerRooflineNumCeilingsBandwidth, 0, bandwidth_ceilings.size() + 1);
                                                                roofline->SetUInt64(kRPVControllerRooflineCeilingBandwidthTypeIndexed, bandwidth_ceilings.size(), (uint64_t)bandwidth_type);
                                                                roofline->SetDouble(kRPVControllerRooflineCeilingBandwidthXIndexed, bandwidth_ceilings.size(), roofline->MinX());
                                                                roofline->SetDouble(kRPVControllerRooflineCeilingBandwidthYIndexed, bandwidth_ceilings.size(), value * roofline->MinX());
                                                                bandwidth_ceilings[bandwidth_type] = value;
                                                            }  
                                                        }
                                                    } 
                                                }
                                                uint_data = 0;
                                                roofline->SetUInt64(kRPVControllerRooflineNumCeilingsRidge, 0, compute_ceilings.size() * bandwidth_ceilings.size());
                                                for(const std::pair<const rocprofvis_controller_roofline_ceiling_compute_type_t, double>& compute_ceiling : compute_ceilings)
                                                {
                                                    for(const std::pair<const rocprofvis_controller_roofline_ceiling_bandwidth_type_t, double>& bandwidth_ceiling : bandwidth_ceilings)
                                                    {
                                                        roofline->SetUInt64(kRPVControllerRooflineCeilingRidgeComputeTypeIndexed, uint_data, (uint64_t)compute_ceiling.first);
                                                        roofline->SetUInt64(kRPVControllerRooflineCeilingRidgeBandwidthTypeIndexed, uint_data, (uint64_t)bandwidth_ceiling.first);
                                                        roofline->SetDouble(kRPVControllerRooflineCeilingRidgeXIndexed, uint_data, compute_ceiling.second / bandwidth_ceiling.second);
                                                        roofline->SetDouble(kRPVControllerRooflineCeilingRidgeYIndexed, uint_data, compute_ceiling.second);
                                                        uint_data++;
                                                    }
                                                }
                                            }
                                        });
                                        uint_data = 0;
                                        double max_data = 0.0;
                                        for(const uint32_t& id : kernel_ids)
                                        {
                                            m_query_arguments = { {kRPVComputeParamKernelId, std::to_string(id)} };
                                            m_query_output = { 
                                                { 
                                                    { kRPVComputeColumnKernelUUID, std::nullopt },
                                                    { kRPVComputeColumnKernelName, std::nullopt },
                                                    { kRPVComputeColumnRooflineTotalFlops, std::nullopt },
                                                    { kRPVComputeColumnRooflineL1CacheData, std::nullopt },
                                                    { kRPVComputeColumnRooflineL2CacheData, std::nullopt },
                                                    { kRPVComputeColumnRooflineHBMCacheData, std::nullopt },
                                                }, 
                                                {} 
                                            };
                                            dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchKernelRooflineIntensities, m_query_arguments, m_query_output, [&roofline, &id, &uint_data, &max_data](const QueryDataStore& data_store){
                                                if(data_store.rows.size() == 1)
                                                {
                                                    const char* data = data_store.rows[0][data_store.columns.at(kRPVComputeColumnRooflineTotalFlops).value()];
                                                    if(strlen(data))
                                                    {
                                                        double flops = std::stod(data);
                                                        if(flops > 0.0)
                                                        {
                                                            rocprofvis_controller_roofline_kernel_intensity_type_t type;
                                                            for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                            {                                                                
                                                                if(roofline->QueryToPropertyEnum(column.first, type))
                                                                {
                                                                    data = data_store.rows[0][column.second.value()];
                                                                    if(strlen(data))
                                                                    {
                                                                        double value = std::stod(data);
                                                                        if(value > 0.0)
                                                                        {
                                                                            roofline->SetUInt64(kRPVControllerRooflineNumKernels, 0, uint_data + 1);
                                                                            roofline->SetUInt64(kRPVControllerRooflineKernelIdIndexed, uint_data, id);
                                                                            roofline->SetUInt64(kRPVControllerRooflineKernelIntensityTypeIndexed, uint_data, type);
                                                                            roofline->SetDouble(kRPVControllerRooflineKernelIntensityXIndexed, uint_data, value);
                                                                            roofline->SetDouble(kRPVControllerRooflineKernelIntensityYIndexed, uint_data, flops);
                                                                            max_data = std::max(max_data, value);
                                                                            uint_data++;
                                                                        }
                                                                    } 
                                                                }                                                               
                                                            }
                                                        }
                                                    }
                                                }
                                            });                                         
                                        }
                                        if(max_data > roofline->MaxX())
                                        {
                                            uint_data = 0;
                                            roofline->GetUInt64(kRPVControllerRooflineNumCeilingsCompute, 0, &uint_data);
                                            for(uint64_t j = 0; j < uint_data; j++)
                                            {
                                                roofline->SetDouble(kRPVControllerRooflineCeilingComputeXIndexed, j, max_data * roofline->DynamicMaxXFactor());
                                            }
                                        }
                                        workload->SetObject(kRPVControllerWorkloadRoofline, 0, (rocprofvis_handle_t*)roofline);
                                    }
                                }
                            }
                        }
                        result = (dm_result == kRocProfVisDmResultSuccess) ? result : kRocProfVisResultUnknownError;
                    }
                }
                rocprofvis_db_future_free(object2wait);
            }
        } 
    }
    return result;
}

rocprofvis_dm_result_t ComputeTrace::ExecuteQuery(rocprofvis_dm_database_t db, rocprofvis_dm_trace_t dm, rocprofvis_db_future_t db_future, Future* controller_future, rocprofvis_db_compute_use_case_enum_t use_case, QueryArgumentStore& argument_store, QueryDataStore& data_store, QueryCallback callback)
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
    bool allocate_db_future = false;
    if(db_future && !controller_future)
    {
        result = kRocProfVisDmResultSuccess;
    }
    else if(!db_future && controller_future)
    {
        allocate_db_future = true;
        result = kRocProfVisDmResultSuccess;
    }
    if(db && dm && callback && result == kRocProfVisDmResultSuccess)
    {
        char* query = nullptr;
        std::vector<rocprofvis_db_compute_param_t> query_args(argument_store.size());
        for(size_t i = 0; i < argument_store.size(); i++)
        {
            query_args[i] = {argument_store[i].first, argument_store[i].second.c_str()};
        }
        result = rocprofvis_db_build_compute_query(db, use_case, query_args.size(), query_args.data(), &query);
        if(result == kRocProfVisDmResultSuccess)
        {
            if(allocate_db_future)
            {
                db_future = rocprofvis_db_future_alloc(nullptr);
            }
            if(db_future)
            {
                rocprofvis_dm_table_id_t table_id = 0;
                result = rocprofvis_db_execute_compute_query_async(db, use_case, query, db_future, &table_id);
                if(result == kRocProfVisDmResultSuccess)
                {
                    if(controller_future)
                    {
                        controller_future->AddDependentFuture(db_future);
                    }
                    result = rocprofvis_db_future_wait(db_future, UINT64_MAX);
                    if(result == kRocProfVisDmResultSuccess)
                    {
                        uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(dm, kRPVDMNumberOfTablesUInt64, 0);
                        if(num_tables > 0)
                        {
                            rocprofvis_dm_table_t table_handle = rocprofvis_dm_get_property_as_handle(dm, kRPVDMTableHandleByID, table_id);
                            if(table_handle)
                            {
                                if(!controller_future || (controller_future && !controller_future->IsCancelled()))
                                {                               
                                    uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(table_handle, kRPVDMNumberOfTableColumnsUInt64, 0);
                                    uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(table_handle, kRPVDMNumberOfTableRowsUInt64, 0);
                                    if(num_columns == data_store.columns.size() && num_rows > 0)
                                    {
                                        for(uint64_t i = 0; i < num_columns; i++)
                                        {
                                            data_store.columns[rocprofvis_db_compute_column_enum_t(rocprofvis_dm_get_property_as_uint64(table_handle, kRPVDMExtTableColumnEnumUInt64Indexed, i))] = (int)i;
                                        }
                                        for(auto& store_column : data_store.columns)
                                        {
                                            if(!store_column.second)
                                            {
                                                result = kRocProfVisDmResultUnknownError;
                                                break;
                                            }
                                        }
                                        if(result == kRocProfVisDmResultSuccess)
                                        {
                                            data_store.rows.resize(num_rows);
                                            for(uint64_t i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t row_handle = rocprofvis_dm_get_property_as_handle(table_handle, kRPVDMExtTableRowHandleIndexed, i);
                                                if(row_handle)
                                                {
                                                    uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(row_handle, kRPVDMNumberOfTableRowCellsUInt64, 0);
                                                    if(num_cells == num_columns)
                                                    {
                                                        data_store.rows[i].resize(num_columns);
                                                        for(auto& store_column : data_store.columns)
                                                        {
                                                            const char* cell_data = rocprofvis_dm_get_property_as_charptr(row_handle, kRPVDMExtTableRowCellValueCharPtrIndexed, store_column.second.value());
                                                            if(cell_data)
                                                            {
                                                                data_store.rows[i][store_column.second.value()] = cell_data;
                                                            }
                                                            else
                                                            {
                                                                result = kRocProfVisDmResultUnknownError;
                                                                break;
                                                            }
                                                        }
                                                    }
                                                    else
                                                    {
                                                        result = kRocProfVisDmResultUnknownError;
                                                        break;
                                                    }
                                                }
                                                else
                                                {
                                                    result = kRocProfVisDmResultUnknownError;
                                                    break;
                                                }
                                            }
                                            if(result == kRocProfVisDmResultSuccess)
                                            {
                                                callback(data_store);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        result = kRocProfVisDmResultUnknownError;
                                    }
                                }
                                rocprofvis_dm_delete_table_at(dm, table_id);
                            }
                            else
                            {
                                result = kRocProfVisDmResultNotLoaded;
                            }
                        }
                        else
                        {
                            result = kRocProfVisDmResultNotLoaded;
                        }
                    }
                    if(controller_future)
                    {
                        controller_future->RemoveDependentFuture(db_future);
                    }
                }
                if(allocate_db_future)
                {
                    rocprofvis_db_future_free(db_future);
                }
            }           
        }        
    }    
    return result;
}

rocprofvis_result_t ComputeTrace::SetObjectProperty(rocprofvis_handle_t* object, rocprofvis_property_t property, uint64_t index, const char* value, rocprofvis_controller_primitive_type_t type)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(object && value)
    {
        switch(type)
        {
            case kRPVControllerPrimitiveTypeString:
            {
                result = ((Handle*)object)->SetString(property, index, value);
                break;
            }
            case kRPVControllerPrimitiveTypeUInt64:
            {
                if(strlen(value))
                {
                    uint64_t uint_value = std::stoull(value);
                    result = ((Handle*)object)->SetUInt64(property, index, uint_value);
                }                
                break;
            }
            case kRPVControllerPrimitiveTypeDouble:
            {
                if(strlen(value))
                {
                    double double_value = std::stod(value);
                    result = ((Handle*)object)->SetDouble(property, index, double_value);
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
    return result;
}

#pragma region Deprecated
rocprofvis_result_t ComputeTrace::AsyncFetch(Plot& plot, Arguments& args, Future& future, Array& array)
{
    rocprofvis_result_t error = kRocProfVisResultUnknownError;
    rocprofvis_dm_trace_t dm_handle = m_dm_handle;

    future.Set(JobSystem::Get().IssueJob([&plot, dm_handle, &args, &array](Future* future) -> rocprofvis_result_t {
            (void) future;
            rocprofvis_result_t result = kRocProfVisResultUnknownError;
            result = plot.Setup(dm_handle, args);
            if (result == kRocProfVisResultSuccess)
            {
                result = plot.Fetch(dm_handle, 0, 0, array);
            }
            return result;
        }, &future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t ComputeTrace::LoadCSV()
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    if (std::filesystem::exists(m_trace_file) && std::filesystem::is_directory(m_trace_file))
    {        
        for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator{m_trace_file})
        {
            if (entry.path().extension() == ".csv")
            {
                const std::string& file = entry.path().filename().string();
                if (COMPUTE_TABLE_DEFINITIONS.count(file) > 0)
                {
                    const ComputeTableDefinition& definition = COMPUTE_TABLE_DEFINITIONS.at(file);
                    ComputeTable* table = new ComputeTable(m_tables.size(), definition.m_type, definition.m_title);
                    ROCPROFVIS_ASSERT(table);
                    result = table->Load(entry.path().string());
                    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    m_tables[definition.m_type] = table;
                }
            }
        }
        spdlog::debug("ComputeTrace::Load - {}/{} Tables", m_tables.size(), COMPUTE_TABLE_DEFINITIONS.size());
        for (const ComputeTablePlotDefinition& definition : COMPUTE_PLOT_DEFINITIONS)
        {
            ComputePlot* plot = new ComputePlot(m_plots.size(), definition.m_title, definition.x_axis_label, definition.y_axis_label, definition.m_type);
            ROCPROFVIS_ASSERT(plot);
            for (const ComputePlotDataSeriesDefinition& series : definition.m_series)
            {
                for (const ComputePlotDataDefinition& data : series.m_values)
                {
                    if (m_tables.count(data.m_table_type) > 0)
                    {
                        result = plot->Load(m_tables[data.m_table_type], series.m_name, data.m_metric_keys);
                        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
                    }
                    else
                    {
                        delete plot;
                        result = kRocProfVisResultNotLoaded;
                        break;
                    }
                }
                if (result != kRocProfVisResultSuccess)
                {
                    break;
                }
            }
            if (result == kRocProfVisResultSuccess)
            {
                m_plots[definition.m_type] = plot;
            }
        }
        spdlog::debug("ComputeTrace::Load - {}/{} Plots", m_plots.size(), COMPUTE_PLOT_DEFINITIONS.size());
        if (m_tables.count(kRPVControllerComputeTableTypeRooflineBenchmarks) > 0 && m_tables.count(kRPVControllerComputeTableTypeRooflineCounters) > 0)
        {
            for (auto& it : ROOFLINE_DEFINITION.m_plots)
            {
                ComputePlot* plot = new ComputePlot(m_plots.size(), it.second.m_title, it.second.x_axis_label, it.second.y_axis_label, it.second.m_type);
                ROCPROFVIS_ASSERT(plot);
                result = plot->Load(m_tables[kRPVControllerComputeTableTypeRooflineCounters], m_tables[kRPVControllerComputeTableTypeRooflineBenchmarks]);
                if (result == kRocProfVisResultSuccess)
                {
                    m_plots[it.second.m_type] = plot;
                }
            }
        }
        spdlog::debug("ComputeTrace::Load - {}/{} Rooflines", m_plots.size() - COMPUTE_PLOT_DEFINITIONS.size(), ROOFLINE_DEFINITION.m_plots.size());
    }
    return result;
}

rocprofvis_result_t ComputeTrace::GetMetric(const rocprofvis_controller_compute_metric_types_t metric_type, Data** value)
{
    rocprofvis_result_t result = kRocProfVisResultNotLoaded;
    const ComputeMetricDefinition& metric = COMPUTE_METRIC_DEFINITIONS.at(metric_type);
    const rocprofvis_controller_compute_table_types_t& table_type = metric.m_table_type;
    if (m_tables.count(table_type))
    {
        const std::string& key = metric.m_metric_key;
        std::pair<std::string, Data*> metric_data;
        result = m_tables[table_type]->GetMetric(key, metric_data);
        if (result == kRocProfVisResultSuccess)
        {
            *value = metric_data.second;
        }
    }
    return result;
}
#pragma endregion

ComputeTrace::MetricID::MetricID(uint32_t cateory_id)
: m_category_id(cateory_id)
, m_table_id(std::nullopt)
, m_entry_id(std::nullopt)
{}

void ComputeTrace::MetricID::SetTableID(uint32_t table_id)
{
    m_table_id = table_id;
}

void ComputeTrace::MetricID::SetEntryID(uint32_t entry_id)
{
    m_entry_id = entry_id;
}

std::string ComputeTrace::MetricID::ToString() const
{
    std::string result = std::to_string(m_category_id);
    if(m_table_id)
    {
        result += "." + std::to_string(m_table_id.value());
    }
    if(m_entry_id)
    {
        result += "." + std::to_string(m_entry_id.value());
    }
    return result;
}

}  // namespace Controller
}
