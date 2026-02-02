// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_trace_compute.h"
#include "rocprofvis_controller_metrics_container.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_workload.h"
#include "rocprofvis_controller_kernel.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_core_assert.h"
#include "json.h"
#include "spdlog/spdlog.h"

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
                    rocprofvis_dm_result_t dm_result = ExecuteQuery(db, m_dm_handle, nullptr, future, kRPVComputeFetchMetricValues, m_query_arguments, m_query_output, [&output](const QueryDataStore& data_store){
                        uint64_t valid_results = 0;
                        for(size_t i = 0; i < data_store.rows.size(); i++)
                        {
                            const char* metric_id = data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricId).value()];
                            const char* kernel_id = data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelUUID).value()];
                            const char* value = data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricValue).value()];
                            if(strlen(metric_id) && strlen(kernel_id) && strlen(value))
                            {
                                output.SetUInt64(kRPVControllerMetricsContainerNumMetrics, 0, valid_results + 1);
                                output.SetString(kRPVControllerMetricsContainerMetricIdIndexed, valid_results, metric_id);
                                output.SetString(kRPVControllerMetricsContainerMetricNameIndexed, valid_results, data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricName).value()]);
                                output.SetUInt64(kRPVControllerMetricsContainerKernelIdIndexed, valid_results, std::stoull(kernel_id));
                                output.SetString(kRPVControllerMetricsContainerMetricValueNameIndexed, valid_results, data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricValueName).value()]);
                                output.SetDouble(kRPVControllerMetricsContainerMetricValueValueIndexed, valid_results++, std::stod(value));
                            }
                        }
                    });   
                    return (dm_result == kRocProfVisDmResultSuccess) ? kRocProfVisResultSuccess : kRocProfVisResultUnknownError;
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
        /*The code below is for compute support debugging purposes only. Feel free to refactor it or remove it */
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
                            for(size_t i = 0; i < data_store.rows.size(); i++)
                            {
                                m_workloads[i] = new Workload();
                                m_workloads[i]->SetUInt64(kRPVControllerWorkloadId, 0, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnWorkloadId).value()]));
                                m_workloads[i]->SetString(kRPVControllerWorkloadName, 0, data_store.rows[i][data_store.columns.at(kRPVComputeColumnWorkloadName).value()]);
                                std::pair<jt::Json::Status, jt::Json> json = jt::Json::parse(data_store.rows[i][data_store.columns.at(kRPVComputeColumnWorkloadSysInfo).value()]);
                                if (json.first == jt::Json::Status::success && json.second.isObject())
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
                                json = jt::Json::parse(data_store.rows[i][data_store.columns.at(kRPVComputeColumnWorkloadProfileConfig).value()]);
                                if (json.first == jt::Json::Status::success && json.second.isObject())
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
                                    dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchWorkloadMetricsDefinition, m_query_arguments, m_query_output, [&workload](const QueryDataStore& data_store){
                                        workload->SetUInt64(kRPVControllerWorkloadNumAvailableMetrics, 0, data_store.rows.size());
                                        for(size_t i = 0; i < data_store.rows.size(); i++)
                                        {
                                            workload->SetUInt64(kRPVControllerWorkloadAvailableMetricCategoryIdIndexed, i, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnTableId).value()]));
                                            workload->SetUInt64(kRPVControllerWorkloadAvailableMetricTableIdIndexed, i, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnSubTableId).value()]));
                                            workload->SetString(kRPVControllerWorkloadAvailableMetricCategoryNameIndexed, i, data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricTableName).value()]);
                                            workload->SetString(kRPVControllerWorkloadAvailableMetricTableNameIndexed, i, data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricSubTableName).value()]);
                                            workload->SetString(kRPVControllerWorkloadAvailableMetricNameIndexed, i, data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricName).value()]);
                                            workload->SetString(kRPVControllerWorkloadAvailableMetricDescriptionIndexed, i, data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricDescription).value()]);
                                            workload->SetString(kRPVControllerWorkloadAvailableMetricUnitIndexed, i, data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricUnit).value()]);
                                        }
                                    });
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
                                        dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchWorkloadTopKernels, m_query_arguments, m_query_output, [&workload](const QueryDataStore& data_store){
                                            workload->SetUInt64(kRPVControllerWorkloadNumKernels, 0, data_store.rows.size());
                                            for(size_t i = 0; i < data_store.rows.size(); i++)
                                            {
                                                Kernel* kernel = new Kernel();
                                                kernel->SetUInt64(kRPVControllerKernelId, 0, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelUUID).value()]));
                                                kernel->SetString(kRPVControllerKernelName, 0, data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelName).value()]);
                                                kernel->SetUInt64(kRPVControllerKernelInvocationCount, 0, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelsCount).value()]));
                                                kernel->SetUInt64(kRPVControllerKernelDurationTotal, 0, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelDurationsSum).value()]));
                                                kernel->SetUInt64(kRPVControllerKernelDurationMedian, 0, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelDurationsMedian).value()]));
                                                kernel->SetUInt64(kRPVControllerKernelDurationMean, 0, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelDurationsMedian).value()]));
                                                kernel->SetUInt64(kRPVControllerKernelDurationMin, 0, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelDurationsMin).value()]));
                                                kernel->SetUInt64(kRPVControllerKernelDurationMax, 0, std::stoull(data_store.rows[i][data_store.columns.at(kRPVComputeColumnKernelDurationsMax).value()]));
                                                workload->SetObject(kRPVControllerWorkloadKernelIndexed, i, (rocprofvis_handle_t*)kernel);
                                            }
                                        });
                                    }
                                }
                            }
                        }
                        result = (dm_result == kRocProfVisDmResultSuccess) ? result : kRocProfVisResultUnknownError;
                        /*
                        char* fetch_query = nullptr;
                        dm_result = rocprofvis_db_build_compute_query(db, kRPVComputeFetchListOfWorkloads, 0, nullptr, &fetch_query);
                        if (dm_result == kRocProfVisDmResultSuccess)
                        {
                            rocprofvis_dm_table_id_t table_id = 0;
                            dm_result = rocprofvis_db_execute_compute_query_async(db, kRPVComputeFetchListOfWorkloads, fetch_query, object2wait, &table_id);

                            if (dm_result == kRocProfVisDmResultSuccess)
                            {
                                if (kRocProfVisDmResultSuccess == rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                                {
                                    DebugComputeTable(table_id, fetch_query, "Fetch list of workloads");
                                }
                            }
                        }
                        std::vector<rocprofvis_db_compute_param_t> params = { {kRPVComputeParamWorkloadId,"1"} };
                        dm_result = rocprofvis_db_build_compute_query(db, kRPVComputeFetchWorkloadRooflineCeiling, params.size(), (rocprofvis_db_compute_params_t)params.data(), &fetch_query);
                        if (dm_result == kRocProfVisDmResultSuccess)
                        {
                            rocprofvis_dm_table_id_t table_id = 0;
                            dm_result = rocprofvis_db_execute_compute_query_async(
                                db, kRPVComputeFetchWorkloadRooflineCeiling, fetch_query, object2wait, &table_id);

                            if (dm_result == kRocProfVisDmResultSuccess)
                            {
                                if (kRocProfVisDmResultSuccess ==
                                    rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                                {
                                    DebugComputeTable(table_id, fetch_query, "Get workload roofline ceiling");
                                }
                            }
                        }

                        dm_result = rocprofvis_db_build_compute_query(db, kRPVComputeFetchWorkloadTopKernels, params.size(), (rocprofvis_db_compute_params_t)params.data(), &fetch_query);
                        if (dm_result == kRocProfVisDmResultSuccess)
                        {
                            rocprofvis_dm_table_id_t table_id = 0;
                            dm_result = rocprofvis_db_execute_compute_query_async(
                                db, kRPVComputeFetchWorkloadTopKernels, fetch_query, object2wait, &table_id);

                            if (dm_result == kRocProfVisDmResultSuccess)
                            {
                                if (kRocProfVisDmResultSuccess ==
                                    rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                                {
                                    DebugComputeTable(table_id, fetch_query, "Get workload top kernels");
                                }
                            }
                        }

                        dm_result = rocprofvis_db_build_compute_query(db, kRPVComputeFetchWorkloadKernelsList, params.size(), (rocprofvis_db_compute_params_t)params.data(), &fetch_query);
                        if (dm_result == kRocProfVisDmResultSuccess)
                        {
                            rocprofvis_dm_table_id_t table_id = 0;
                            dm_result = rocprofvis_db_execute_compute_query_async(
                                db, kRPVComputeFetchWorkloadKernelsList, fetch_query, object2wait, &table_id);

                            if (dm_result == kRocProfVisDmResultSuccess)
                            {
                                if (kRocProfVisDmResultSuccess ==
                                    rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                                {
                                    DebugComputeTable(table_id, fetch_query, "Get workload kernels list");
                                }
                            }
                        }

                        params = { {kRPVComputeParamKernelId,"1"} };
                        dm_result = rocprofvis_db_build_compute_query(db, kRPVComputeFetchKernelRooflineIntensities, params.size(), (rocprofvis_db_compute_params_t)params.data(), &fetch_query);
                        if (dm_result == kRocProfVisDmResultSuccess)
                        {
                            rocprofvis_dm_table_id_t table_id = 0;
                            dm_result = rocprofvis_db_execute_compute_query_async(
                                db, kRPVComputeFetchKernelRooflineIntensities, fetch_query, object2wait, &table_id);

                            if (dm_result == kRocProfVisDmResultSuccess)
                            {
                                if (kRocProfVisDmResultSuccess ==
                                    rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                                {
                                    DebugComputeTable(table_id, fetch_query, "Get kernel roofline intensities");
                                }
                            }
                        }


                        dm_result = rocprofvis_db_build_compute_query(db, kRPVComputeFetchKernelMetricCategoriesList, params.size(), (rocprofvis_db_compute_params_t)params.data(), &fetch_query);
                        if (dm_result == kRocProfVisDmResultSuccess)
                        {
                            rocprofvis_dm_table_id_t table_id = 0;
                            dm_result = rocprofvis_db_execute_compute_query_async(
                                db, kRPVComputeFetchKernelMetricCategoriesList, fetch_query, object2wait, &table_id);

                            if (dm_result == kRocProfVisDmResultSuccess)
                            {
                                if (kRocProfVisDmResultSuccess ==
                                    rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                                {
                                    DebugComputeTable(table_id, fetch_query, "Get kernel metric categories list");
                                }
                            }
                        }

                        params = { {kRPVComputeParamKernelId,"1"}, {kRPVComputeParamMetricId,"3"} };
                        dm_result = rocprofvis_db_build_compute_query(db, kRPVComputeFetchMetricCategoryTablesList, params.size(), (rocprofvis_db_compute_params_t)params.data(), &fetch_query);
                        if (dm_result == kRocProfVisDmResultSuccess)
                        {
                            rocprofvis_dm_table_id_t table_id = 0;
                            dm_result = rocprofvis_db_execute_compute_query_async(
                                db, kRPVComputeFetchKernelMetricCategoriesList, fetch_query, object2wait, &table_id);

                            if (dm_result == kRocProfVisDmResultSuccess)
                            {
                                if (kRocProfVisDmResultSuccess ==
                                    rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                                {
                                    DebugComputeTable(table_id, fetch_query, "Get metric category tables list");
                                }
                            }
                        }
                        */
                        /*The code above is for compute support debugging purposes only. Feel free to refactor it or remove it */  
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
        bool cancellable = false;
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

/*DebugComputeTable function is for debugging purposes only. Feel free to refactor it or remove it */
rocprofvis_result_t ComputeTrace::DebugComputeTable(rocprofvis_dm_table_id_t table_id, std::string query, std::string description)
{
    rocprofvis_result_t result = kRocProfVisResultNotLoaded;
    uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(
        m_dm_handle, kRPVDMNumberOfTablesUInt64, 0);
    if (num_tables > 0)
    {
        rocprofvis_dm_table_t table = rocprofvis_dm_get_property_as_handle(
            m_dm_handle, kRPVDMTableHandleByID, table_id);
        if (nullptr != table)
        {
            uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(
                table, kRPVDMNumberOfTableColumnsUInt64, 0);
            uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(
                table, kRPVDMNumberOfTableRowsUInt64, 0);

            std::string column_names;
            std::string column_enums;

            for (uint32_t i = 0; i < num_columns; i++)
            {
                char const* column_name =
                    rocprofvis_dm_get_property_as_charptr(table, kRPVDMExtTableColumnNameCharPtrIndexed, i);
                column_names += column_name;
                column_names += ", ";
                uint64_t column_enum = 
                    rocprofvis_dm_get_property_as_uint64(table, kRPVDMExtTableColumnEnumUInt64Indexed, i);  
                column_enums += std::to_string(column_enum);
                column_enums += ", ";
            }

            spdlog::info("SQL Query : {}",query);
            spdlog::info("Description : {}", description);
            spdlog::info("Column names : {}", column_names);
            spdlog::info("Column enumerations : {}", column_enums);

            for (uint32_t i = 0; i < num_rows; i++)
            {
                std::string data_cells;
                rocprofvis_dm_table_row_t table_row =
                    rocprofvis_dm_get_property_as_handle(
                        table, kRPVDMExtTableRowHandleIndexed, i);
                if (table_row != nullptr)
                {
                    uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(
                        table_row, kRPVDMNumberOfTableRowCellsUInt64, 0);
                    ROCPROFVIS_ASSERT(num_cells == num_columns);
                    for (uint32_t j = 0; j < num_cells; j++)
                    {
                        char const* value =
                            rocprofvis_dm_get_property_as_charptr(
                                table_row,
                                kRPVDMExtTableRowCellValueCharPtrIndexed, j);
                        ROCPROFVIS_ASSERT(value);
                        data_cells += value;
                        data_cells += ",";
                    }
                    spdlog::info("Data row {} : {}", i, data_cells);
                }
            }

            rocprofvis_dm_delete_table_at(m_dm_handle, table_id);
            result = kRocProfVisResultSuccess;
        }
    }
    return result;
}
/*DebugComputeTable function is for debugging purposes only. Feel free to refactor it or remove it */

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
