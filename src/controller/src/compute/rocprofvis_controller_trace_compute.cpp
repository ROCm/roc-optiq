// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_trace_compute.h"
#include "rocprofvis_controller_metrics_container.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_workload.h"
#include "rocprofvis_controller_kernel.h"
#include "rocprofvis_controller_roofline.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_controller_table_compute_pivot.h"
#include "json.h"
#include <algorithm>
#include <cstdlib>
#include <set>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_handle_t, PcSampling, kRPVControllerObjectTypePCSampling> PcSamplingRef;

ComputeTrace::ComputeTrace(const std::string& filename)
: Trace(__kRPVControllerComputePropertiesFirst, __kRPVControllerComputePropertiesLast,
        filename)
, m_async_fetch_counter(0)
, m_kernel_metric_table(nullptr)
{}

ComputeTrace::~ComputeTrace()
{
    for(Workload* workload : m_workloads)
    {
        delete workload;
    }
    if(m_kernel_metric_table)
    {
        delete m_kernel_metric_table;
    }
}

rocprofvis_result_t ComputeTrace::Init()
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    try
    {
        m_kernel_metric_table = new ComputePivotTable(0);
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
            rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
            if(m_trace_file.find(".db", m_trace_file.size() - 3) != std::string::npos)
            {
                result = LoadRocpd(future);
            }
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
    (void) index;
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
                result = UnhandledProperty(property);             
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
            case kRPVControllerKernelMetricTable:
            {
                *value = (rocprofvis_handle_t*)m_kernel_metric_table;
                result = kRocProfVisResultSuccess;
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

rocprofvis_result_t ComputeTrace::AsyncFetch(Arguments& args, Future& future, MetricsContainer& output)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    uint64_t num_entries = 0;
    uint64_t uint64_data = 0;
    result = output.GetUInt64(kRPVControllerMetricsContainerNumMetrics, 0, &num_entries);
    if(result == kRocProfVisResultSuccess && num_entries == 0)
    {
        std::shared_ptr<QueryArgumentStore> query_args = nullptr;
        rocprofvis_db_compute_use_case_enum_t use_case;
        if(kRocProfVisResultSuccess == args.GetUInt64(kRPVControllerMetricArgsNumKernels, 0, &num_entries) && num_entries > 0)
        {
            query_args = std::make_shared<QueryArgumentStore>();            
            for(uint64_t i = 0; i < num_entries; i++)
            {
                result = args.GetUInt64(kRPVControllerMetricArgsKernelIdIndexed, i, &uint64_data);
                if(result == kRocProfVisResultSuccess)
                {
                    query_args->emplace_back(kRPVComputeParamKernelId, std::to_string(uint64_data));
                } 
                else
                {
                    break;
                }
            }
            use_case = kRPVComputeFetchMetricValues;
        }
        else if(kRocProfVisResultSuccess == args.GetUInt64(kRPVControllerMetricArgsWorkloadId, 0, &uint64_data))
        {
            query_args = std::make_shared<QueryArgumentStore>();
            query_args->emplace_back(kRPVComputeParamWorkloadId, std::to_string(uint64_data));
            use_case = kRPVComputeFetchMetricValuesByWorkload;
        }
        if(query_args)
        {
            std::shared_ptr<QueryDataStore> query_out = std::make_shared<QueryDataStore>();
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
                        query_args->emplace_back(kRPVComputeParamMetricId, metric_id.ToString());
                        result = kRocProfVisResultSuccess;
                    }
                    else
                    {
                        break;
                    }
                }
            }
            if(result == kRocProfVisResultSuccess)
            {
                future.Set(JobSystem::Get().IssueJob([this, use_case, &output, query_args, query_out](Future* future) -> rocprofvis_result_t {
                    rocprofvis_result_t result = kRocProfVisResultUnknownError;
                    *query_out = { 
                        { 
                            { kRPVComputeColumnMetricId, std::nullopt },
                            { kRPVComputeColumnMetricName, std::nullopt },
                            { kRPVComputeColumnMetricValueName, std::nullopt },
                            { kRPVComputeColumnMetricValue, std::nullopt },
                        }, 
                        {} 
                    };
                    rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(m_dm_handle, kRPVDMDatabaseHandle, 0);
                    rocprofvis_dm_result_t dm_result = ExecuteQuery(db, m_dm_handle, nullptr, future, use_case, *query_args, *query_out, [this, &output](const QueryDataStore& data_store){
                        uint64_t valid_results = 0;
                        rocprofvis_property_t property;
                        rocprofvis_controller_primitive_type_t type;
                        std::optional<int> workload_id_column_idx = data_store.columns.count(kRPVComputeColumnWorkloadId) > 0 ? data_store.columns.at(kRPVComputeColumnWorkloadId) : std::nullopt;
                        std::optional<int> kernel_id_column_idx = data_store.columns.count(kRPVComputeColumnKernelUUID) > 0 ? data_store.columns.at(kRPVComputeColumnKernelUUID) : std::nullopt;
                        for(size_t i = 0; i < data_store.rows.size(); i++)
                        {
                            const char* metric_id = data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricId).value()];
                            const char* workload_id = workload_id_column_idx ? data_store.rows[i][workload_id_column_idx.value()] : "";
                            const char* kernel_id = kernel_id_column_idx ? data_store.rows[i][kernel_id_column_idx.value()] : "";
                            const char* value = data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricValue).value()];
                            if((strlen(kernel_id) || strlen(workload_id)) && strlen(metric_id) && strlen(value))
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

rocprofvis_result_t ComputeTrace::AsyncFetch(Table& table, Arguments& args, Future& future, Array& array)
{
    rocprofvis_result_t   error     = kRocProfVisResultUnknownError;

    future.Set(JobSystem::Get().IssueJob([this, &table, &args, &array](Future* future) -> rocprofvis_result_t {
            return table.SetupAndFetch(*this, args, array, future);
        }, &future));

    if(future.IsValid())
    {
        error = kRocProfVisResultSuccess;
    }

    return error;
}

rocprofvis_result_t ComputeTrace::LoadRocpd(Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    m_dm_handle = rocprofvis_dm_create_trace();
    if(nullptr != m_dm_handle)
    {
        rocprofvis_dm_database_t db = rocprofvis_db_open_database(m_trace_file.c_str(), kComputeSqlite);
        if (nullptr != db && kRocProfVisDmResultSuccess == rocprofvis_dm_bind_trace_to_database(m_dm_handle, db))
        {
            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(&Future::ProgressCallback, future);
            if (nullptr != object2wait)
            {
                if (kRocProfVisDmResultSuccess == rocprofvis_db_read_metadata_async(db, object2wait))
                {
                    future->AddDependentFuture(object2wait);
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
                        future->ResetProgress();
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
                                uint64_t id = 0;
                                result = workload->GetUInt64(kRPVControllerWorkloadId, 0, &id);
                                if(result == kRocProfVisResultSuccess)
                                {
                                    m_query_arguments = { {kRPVComputeParamWorkloadId, std::to_string(id)} };
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
                                    std::set<std::pair<uint32_t, uint32_t>> unique_tables;
                                    future->ResetProgress();
                                    dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchWorkloadMetricsDefinition, m_query_arguments, m_query_output, [this, &workload, &unique_tables](const QueryDataStore& data_store){
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
                                            const char* cat_str = data_store.rows[i][data_store.columns.at(kRPVComputeColumnTableId).value()];
                                            const char* tbl_str = data_store.rows[i][data_store.columns.at(kRPVComputeColumnSubTableId).value()];
                                            if(strlen(cat_str) && strlen(tbl_str))
                                            {
                                                unique_tables.insert({static_cast<uint32_t>(std::stoul(cat_str)), static_cast<uint32_t>(std::stoul(tbl_str))});
                                            }
                                        }
                                    });
                                    if(dm_result == kRocProfVisDmResultSuccess)
                                    {
                                        std::string workload_id_str = std::to_string(id);
                                        uint64_t value_names_count = 0;
                                        for(const auto& [cat_id, tbl_id] : unique_tables)
                                        {
                                            std::string prefix = std::to_string(cat_id) + "." + std::to_string(tbl_id);
                                            m_query_arguments = {
                                                {kRPVComputeParamWorkloadId, workload_id_str},
                                                {kRPVComputeParamMetricId, prefix}
                                            };
                                            m_query_output = {
                                                { { kRPVComputeColumnMetricValueName, std::nullopt } },
                                                {}
                                            };
                                            future->ResetProgress();
                                            dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchWorkloadMetricValueNames, m_query_arguments, m_query_output,
                                                [&workload, &value_names_count, cat_id, tbl_id](const QueryDataStore& data_store){
                                                    for(size_t i = 0; i < data_store.rows.size(); i++)
                                                    {
                                                        workload->SetUInt64(kRPVControllerWorkloadNumMetricValueNames, 0, value_names_count + 1);
                                                        workload->SetUInt64(kRPVControllerWorkloadMetricValueNameCategoryIdIndexed, value_names_count, cat_id);
                                                        workload->SetUInt64(kRPVControllerWorkloadMetricValueNameTableIdIndexed, value_names_count, tbl_id);
                                                        workload->SetString(kRPVControllerWorkloadMetricValueNameStringIndexed, value_names_count, data_store.rows[i][data_store.columns.at(kRPVComputeColumnMetricValueName).value()]);
                                                        value_names_count++;
                                                    }
                                                });
                                            if(dm_result != kRocProfVisDmResultSuccess) break;
                                        }
                                        m_query_arguments = { {kRPVComputeParamWorkloadId, workload_id_str} };
                                    }
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
                                        future->ResetProgress();
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
                                                kernel_ids.push_back(static_cast<uint32_t>(id));
                                            }
                                        });
                                    }
                                    if(dm_result == kRocProfVisDmResultSuccess)
                                    {
                                        for(size_t ki = 0; ki < kernel_ids.size(); ki++)
                                        {
                                            rocprofvis_handle_t* kernel_handle = nullptr;
                                            if(kRocProfVisResultSuccess != workload->GetObject(kRPVControllerWorkloadKernelIndexed, ki, &kernel_handle) || !kernel_handle)
                                                continue;
                                            Kernel* kernel = (Kernel*)kernel_handle;
                                            m_query_arguments = { {kRPVComputeParamKernelId, std::to_string(kernel_ids[ki])} };
                                            m_query_output = {
                                                {
                                                    { kRPVComputeColumnPcSamplingCodeObjectId, std::nullopt },
                                                    { kRPVComputeColumnPcSamplingCodeObjectUri, std::nullopt },
                                                    { kRPVComputeColumnPcSamplingCodeObjectChecksum, std::nullopt },
                                                },
                                                {}
                                            };
                                            future->ResetProgress();
                                            ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchKernelCodeObjects, m_query_arguments, m_query_output, [this, &kernel](const QueryDataStore& data_store){
                                                rocprofvis_handle_t* pc_handle = nullptr;
                                                kernel->GetObject(kRPVControllerKernelPcSampling, 0, &pc_handle);
                                                PcSamplingRef pc_sampling(pc_handle);
                                                if(!pc_sampling.IsValid())
                                                    return;
                                                pc_sampling->SetUInt64(kRPVControllerPCSamplingNumCodeObjects, 0, data_store.rows.size());
                                                rocprofvis_property_t property;
                                                rocprofvis_controller_primitive_type_t type;
                                                for(size_t i = 0; i < data_store.rows.size(); i++)
                                                {
                                                    for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                    {
                                                        if(pc_sampling->QueryToPropertyEnum(column.first, property, type))
                                                        {
                                                            SetObjectProperty(pc_handle, property, i, data_store.rows[i][column.second.value()], type);
                                                        }
                                                    }
                                                }
                                            });
                                            {
                                                rocprofvis_handle_t* pc_handle = nullptr;
                                                kernel->GetObject(kRPVControllerKernelPcSampling, 0, &pc_handle);
                                                PcSamplingRef pc_sampling(pc_handle);
                                                if(pc_sampling.IsValid())
                                                {
                                                    uint64_t num_code_objects = 0;
                                                    pc_sampling->GetUInt64(kRPVControllerPCSamplingNumCodeObjects, 0, &num_code_objects);
                                                    uint64_t isa_offset = 0;
                                                    for(uint64_t ci = 0; ci < num_code_objects; ci++)
                                                    {
                                                        uint64_t code_object_id = 0;
                                                        pc_sampling->GetUInt64(kRPVControllerPCSamplingCodeObjectId, ci, &code_object_id);
                                                        m_query_arguments = { {kRPVComputeParamCodeObjectId, std::to_string(code_object_id)} };
                                                        m_query_output = {
                                                            {
                                                                { kRPVComputeColumnPcSamplingIsaLineId, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingIsaLineCodeObjectId, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingIsaLineCodeObjectOffset, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingIsaLineInstructionTypeId, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingIsaLineInstruction, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingIsaLineComment, std::nullopt },
                                                            },
                                                            {}
                                                        };
                                                        future->ResetProgress();
                                                        ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchCodeObjectIsaLines, m_query_arguments, m_query_output, [this, &pc_sampling, &pc_handle, &isa_offset](const QueryDataStore& data_store){
                                                            pc_sampling->SetUInt64(kRPVControllerPCSamplingNumIsaLines, 0, isa_offset + data_store.rows.size());
                                                            rocprofvis_property_t property;
                                                            rocprofvis_controller_primitive_type_t type;
                                                            for(size_t ii = 0; ii < data_store.rows.size(); ii++)
                                                            {
                                                                uint64_t flat_index = isa_offset + ii;
                                                                for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                                {
                                                                    if(pc_sampling->QueryToPropertyEnum(column.first, property, type))
                                                                    {
                                                                        SetObjectProperty(pc_handle, property, flat_index, data_store.rows[ii][column.second.value()], type);
                                                                    }
                                                                }
                                                            }
                                                            isa_offset += data_store.rows.size();
                                                        });
                                                    }
                                                }
                                            }
                                            {
                                                rocprofvis_handle_t* pc_handle2 = nullptr;
                                                kernel->GetObject(kRPVControllerKernelPcSampling, 0, &pc_handle2);
                                                PcSamplingRef pc_sampling(pc_handle2);
                                                if(pc_sampling.IsValid())
                                                {
                                                    uint64_t num_isa_lines = 0;
                                                    pc_sampling->GetUInt64(kRPVControllerPCSamplingNumIsaLines, 0, &num_isa_lines);
                                                    uint64_t isa_to_isa_offset = 0;
                                                    uint64_t isa_to_source_offset = 0;
                                                    uint64_t stall_offset = 0;
                                                    for(uint64_t ii = 0; ii < num_isa_lines; ii++)
                                                    {
                                                        uint64_t isa_line_id = 0;
                                                        pc_sampling->GetUInt64(kRPVControllerPCSamplingIsaLineId, ii, &isa_line_id);
                                                        m_query_arguments = { {kRPVComputeParamIsaLineId, std::to_string(isa_line_id)} };
                                                        m_query_output = {
                                                            {
                                                                { kRPVComputeColumnPcSamplingIsaToIsaDependentIsaLineId, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingIsaToIsaDependencyIsaLineId, std::nullopt },
                                                            },
                                                            {}
                                                        };
                                                        future->ResetProgress();
                                                        ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchIsaLineIsaLineDeps, m_query_arguments, m_query_output, [this, &pc_sampling, &pc_handle2, &isa_to_isa_offset](const QueryDataStore& data_store){
                                                            pc_sampling->SetUInt64(kRPVControllerPCSamplingNumIsaToIsaDeps, 0, isa_to_isa_offset + data_store.rows.size());
                                                            rocprofvis_property_t property;
                                                            rocprofvis_controller_primitive_type_t type;
                                                            for(size_t di = 0; di < data_store.rows.size(); di++)
                                                            {
                                                                uint64_t flat_index = isa_to_isa_offset + di;
                                                                for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                                {
                                                                    if(pc_sampling->QueryToPropertyEnum(column.first, property, type))
                                                                    {
                                                                        SetObjectProperty(pc_handle2, property, flat_index, data_store.rows[di][column.second.value()], type);
                                                                    }
                                                                }
                                                            }
                                                            isa_to_isa_offset += data_store.rows.size();
                                                        });
                                                        m_query_arguments = { {kRPVComputeParamIsaLineId, std::to_string(isa_line_id)} };
                                                        m_query_output = {
                                                            {
                                                                { kRPVComputeColumnPcSamplingIsaToSourceIsaLineId, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingIsaToSourceSourceLineId, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingIsaToSourceDepth, std::nullopt },
                                                            },
                                                            {}
                                                        };
                                                        future->ResetProgress();
                                                        ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchIsaLineSourceLineDeps, m_query_arguments, m_query_output, [this, &pc_sampling, &pc_handle2, &isa_to_source_offset](const QueryDataStore& data_store){
                                                            pc_sampling->SetUInt64(kRPVControllerPCSamplingNumIsaToSourceDeps, 0, isa_to_source_offset + data_store.rows.size());
                                                            rocprofvis_property_t property;
                                                            rocprofvis_controller_primitive_type_t type;
                                                            for(size_t si = 0; si < data_store.rows.size(); si++)
                                                            {
                                                                uint64_t flat_index = isa_to_source_offset + si;
                                                                for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                                {
                                                                    if(pc_sampling->QueryToPropertyEnum(column.first, property, type))
                                                                    {
                                                                        SetObjectProperty(pc_handle2, property, flat_index, data_store.rows[si][column.second.value()], type);
                                                                    }
                                                                }
                                                            }
                                                            isa_to_source_offset += data_store.rows.size();
                                                        });
                                                        m_query_arguments = { {kRPVComputeParamIsaLineId, std::to_string(isa_line_id)} };
                                                        m_query_output = {
                                                            {
                                                                { kRPVComputeColumnPcSamplingStallRecordId, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingStallRecordIsaLineId, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingStallRecordDispatchId, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingStallRecordAvgActiveLanes, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingStallRecordWaveIssuedCount, std::nullopt },
                                                                { kRPVComputeColumnPcSamplingStallRecordTotalSampleCount, std::nullopt },
                                                            },
                                                            {}
                                                        };
                                                        future->ResetProgress();
                                                        ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchIsaLineStallRecord, m_query_arguments, m_query_output, [this, &pc_sampling, &pc_handle2, &stall_offset](const QueryDataStore& data_store){
                                                            pc_sampling->SetUInt64(kRPVControllerPCSamplingNumStallRecords, 0, stall_offset + data_store.rows.size());
                                                            rocprofvis_property_t property;
                                                            rocprofvis_controller_primitive_type_t type;
                                                            for(size_t ri = 0; ri < data_store.rows.size(); ri++)
                                                            {
                                                                uint64_t flat_index = stall_offset + ri;
                                                                for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                                {
                                                                    if(pc_sampling->QueryToPropertyEnum(column.first, property, type))
                                                                    {
                                                                        SetObjectProperty(pc_handle2, property, flat_index, data_store.rows[ri][column.second.value()], type);
                                                                    }
                                                                }
                                                            }
                                                            stall_offset += data_store.rows.size();
                                                        });
                                                        // Fetch stall reason counts for each stall record of this ISA line
                                                        uint64_t num_stall_records = 0;
                                                        pc_sampling->GetUInt64(kRPVControllerPCSamplingNumStallRecords, 0, &num_stall_records);
                                                        uint64_t stall_reason_offset = 0;
                                                        for(uint64_t si = 0; si < num_stall_records; si++)
                                                        {
                                                            uint64_t stall_record_id = 0;
                                                            pc_sampling->GetUInt64(kRPVControllerPCSamplingStallRecordId, si, &stall_record_id);
                                                            m_query_arguments = { {kRPVComputeParamStallRecordId, std::to_string(stall_record_id)} };
                                                            m_query_output = {
                                                                {
                                                                    { kRPVComputeColumnPcSamplingStallReasonRecordId, std::nullopt },
                                                                    { kRPVComputeColumnPcSamplingStallReasonTypeId, std::nullopt },
                                                                    { kRPVComputeColumnPcSamplingStallReasonCount, std::nullopt },
                                                                },
                                                                {}
                                                            };
                                                            future->ResetProgress();
                                                            ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchStallRecordReasonCounts, m_query_arguments, m_query_output, [this, &pc_sampling, &pc_handle2, &stall_reason_offset](const QueryDataStore& data_store){
                                                                pc_sampling->SetUInt64(kRPVControllerPCSamplingNumStallReasonCounts, 0, stall_reason_offset + data_store.rows.size());
                                                                rocprofvis_property_t property;
                                                                rocprofvis_controller_primitive_type_t type;
                                                                for(size_t ri = 0; ri < data_store.rows.size(); ri++)
                                                                {
                                                                    uint64_t flat_index = stall_reason_offset + ri;
                                                                    for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                                    {
                                                                        if(pc_sampling->QueryToPropertyEnum(column.first, property, type))
                                                                        {
                                                                            SetObjectProperty(pc_handle2, property, flat_index, data_store.rows[ri][column.second.value()], type);
                                                                        }
                                                                    }
                                                                }
                                                                stall_reason_offset += data_store.rows.size();
                                                            });
                                                        }
                                                    }
                                                }
                                            }
                                            m_query_arguments = { {kRPVComputeParamKernelId, std::to_string(kernel_ids[ki])} };
                                            m_query_output = {
                                                {
                                                    { kRPVComputeColumnPcSamplingSourceFileId, std::nullopt },
                                                    { kRPVComputeColumnPcSamplingSourceFilePath, std::nullopt },
                                                    { kRPVComputeColumnPcSamplingSourceFileChecksum, std::nullopt },
                                                },
                                                {}
                                            };
                                            future->ResetProgress();
                                            ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchKernelSourceFiles, m_query_arguments, m_query_output, [this, &kernel](const QueryDataStore& data_store){
                                                rocprofvis_handle_t* pc_handle = nullptr;
                                                kernel->GetObject(kRPVControllerKernelPcSampling, 0, &pc_handle);
                                                PcSamplingRef pc_sampling(pc_handle);
                                                if(!pc_sampling.IsValid())
                                                    return;
                                                pc_sampling->SetUInt64(kRPVControllerPCSamplingNumSourceFiles, 0, data_store.rows.size());
                                                rocprofvis_property_t property;
                                                rocprofvis_controller_primitive_type_t type;
                                                for(size_t i = 0; i < data_store.rows.size(); i++)
                                                {
                                                    for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                    {
                                                        if(pc_sampling->QueryToPropertyEnum(column.first, property, type))
                                                        {
                                                            SetObjectProperty(pc_handle, property, i, data_store.rows[i][column.second.value()], type);
                                                        }
                                                    }
                                                }
                                            });
                                            rocprofvis_handle_t* pc_handle = nullptr;
                                            kernel->GetObject(kRPVControllerKernelPcSampling, 0, &pc_handle);
                                            PcSamplingRef pc_sampling(pc_handle);
                                            if(pc_sampling.IsValid())
                                            {
                                                uint64_t num_source_files = 0;
                                                pc_sampling->GetUInt64(kRPVControllerPCSamplingNumSourceFiles, 0, &num_source_files);
                                                uint64_t source_line_offset = 0;
                                                for(uint64_t fi = 0; fi < num_source_files; fi++)
                                                {
                                                    uint64_t source_file_id = 0;
                                                    pc_sampling->GetUInt64(kRPVControllerPCSamplingSourceFileId, fi, &source_file_id);
                                                    m_query_arguments = { {kRPVComputeParamSourceFileId, std::to_string(source_file_id)} };
                                                    m_query_output = {
                                                        {
                                                            { kRPVComputeColumnPcSamplingSourceLineId, std::nullopt },
                                                            { kRPVComputeColumnPcSamplingSourceLineFileId, std::nullopt },
                                                            { kRPVComputeColumnPcSamplingSourceLineNumber, std::nullopt },
                                                            { kRPVComputeColumnPcSamplingSourceLineContent, std::nullopt },
                                                        },
                                                        {}
                                                    };
                                                    future->ResetProgress();
                                                    ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchSourceFileSourceLines, m_query_arguments, m_query_output, [this, &pc_sampling, &pc_handle, &source_line_offset](const QueryDataStore& data_store){
                                                        pc_sampling->SetUInt64(kRPVControllerPCSamplingNumSourceLines, 0, source_line_offset + data_store.rows.size());
                                                        rocprofvis_property_t property;
                                                        rocprofvis_controller_primitive_type_t type;
                                                        for(size_t li = 0; li < data_store.rows.size(); li++)
                                                        {
                                                            uint64_t flat_index = source_line_offset + li;
                                                            for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                            {
                                                                if(pc_sampling->QueryToPropertyEnum(column.first, property, type))
                                                                {
                                                                    SetObjectProperty(pc_handle, property, flat_index, data_store.rows[li][column.second.value()], type);
                                                                }
                                                            }
                                                        }
                                                        source_line_offset += data_store.rows.size();
                                                    });
                                                }
                                            }
                                        }
                                    }
                                    if(dm_result == kRocProfVisDmResultSuccess)
                                    {
                                        Roofline* roofline = new Roofline();
                                        std::optional<double> max_intensity_x;
                                        std::optional<double> min_intensity_x;
                                        std::optional<double> max_intensity_y;
                                        std::optional<double> min_intensity_y;
                                        uint64_t uint_data = 0;
                                        for(const uint32_t& kernel_id : kernel_ids)
                                        {
                                            m_query_arguments = { {kRPVComputeParamKernelId, std::to_string(kernel_id)} };
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
                                            future->ResetProgress();
                                            dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchKernelRooflineIntensities, m_query_arguments, m_query_output, [&roofline, &kernel_id, &uint_data, &max_intensity_x, &min_intensity_x, &max_intensity_y, &min_intensity_y](const QueryDataStore& data_store){
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
                                                                            roofline->SetUInt64(kRPVControllerRooflineKernelIdIndexed, uint_data, kernel_id);
                                                                            roofline->SetUInt64(kRPVControllerRooflineKernelIntensityTypeIndexed, uint_data, type);
                                                                            roofline->SetDouble(kRPVControllerRooflineKernelIntensityXIndexed, uint_data, value);
                                                                            roofline->SetDouble(kRPVControllerRooflineKernelIntensityYIndexed, uint_data, flops);
                                                                            max_intensity_x = max_intensity_x ? std::max(max_intensity_x.value(), value) : value;
                                                                            min_intensity_x = min_intensity_x ? std::min(min_intensity_x.value(), value) : value;
                                                                            max_intensity_y = max_intensity_y ? std::max(max_intensity_y.value(), flops) : flops;
                                                                            min_intensity_y = min_intensity_y ? std::min(min_intensity_y.value(), flops) : flops;
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
                                        max_intensity_x = max_intensity_x ? max_intensity_x.value() * 10 : max_intensity_x;
                                        min_intensity_x = min_intensity_x ? min_intensity_x.value() / 10 : min_intensity_x;
                                        max_intensity_y = max_intensity_y ? max_intensity_y.value() * 10 : max_intensity_y;
                                        min_intensity_y = min_intensity_y ? min_intensity_y.value() / 10 : min_intensity_y;
                                        m_query_arguments = { {kRPVComputeParamWorkloadId, std::to_string(id)} };
                                        m_query_output = { {}, {} };
                                        future->ResetProgress();
                                        dm_result = ExecuteQuery(db, m_dm_handle, object2wait, nullptr, kRPVComputeFetchWorkloadRooflineCeiling, m_query_arguments, m_query_output, [&roofline, &uint_data, &max_intensity_x, &min_intensity_x, &max_intensity_y, &min_intensity_y](const QueryDataStore& data_store){
                                            if(data_store.rows.size() == 1)
                                            {
                                                std::unordered_map<rocprofvis_controller_roofline_ceiling_compute_type_t, double> compute_ceilings;
                                                std::unordered_map<rocprofvis_controller_roofline_ceiling_bandwidth_type_t, double> bandwidth_ceilings;
                                                rocprofvis_controller_roofline_ceiling_compute_type_t compute_type;
                                                rocprofvis_controller_roofline_ceiling_bandwidth_type_t bandwidth_type;
                                                for(const std::pair<const rocprofvis_db_compute_column_enum_t, std::optional<int>>& column : data_store.columns)
                                                {
                                                    if(column.second)
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
                                                                    roofline->SetDouble(kRPVControllerRooflineCeilingComputeXIndexed, compute_ceilings.size(), max_intensity_x ? std::max(max_intensity_x.value(), roofline->MaxX()) : roofline->MaxX());
                                                                    roofline->SetDouble(kRPVControllerRooflineCeilingComputeYIndexed, compute_ceilings.size(), value);
                                                                    roofline->SetDouble(kRPVControllerRooflineCeilingComputeThroughputIndexed, compute_ceilings.size(), value);
                                                                    compute_ceilings[compute_type] = value;
                                                                }
                                                                else if(roofline->QueryToPropertyEnum(column.first, bandwidth_type))
                                                                {
                                                                    roofline->SetUInt64(kRPVControllerRooflineNumCeilingsBandwidth, 0, bandwidth_ceilings.size() + 1);
                                                                    roofline->SetUInt64(kRPVControllerRooflineCeilingBandwidthTypeIndexed, bandwidth_ceilings.size(), (uint64_t)bandwidth_type);
                                                                    roofline->SetDouble(kRPVControllerRooflineCeilingBandwidthXIndexed, bandwidth_ceilings.size(), min_intensity_x ? std::min(min_intensity_x.value(), roofline->MinX()) : roofline->MinX());
                                                                    roofline->SetDouble(kRPVControllerRooflineCeilingBandwidthYIndexed, bandwidth_ceilings.size(), value * (min_intensity_x ? std::min(min_intensity_x.value(), roofline->MinX()) : roofline->MinX()));
                                                                    roofline->SetDouble(kRPVControllerRooflineCeilingBandwidthThroughputIndexed, bandwidth_ceilings.size(), value);
                                                                    bandwidth_ceilings[bandwidth_type] = value;
                                                                }  
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
                                        workload->SetObject(kRPVControllerWorkloadRoofline, 0, (rocprofvis_handle_t*)roofline);
                                    }
                                }
                            }
                        }
                        result = (dm_result == kRocProfVisDmResultSuccess) ? result : kRocProfVisResultUnknownError;
                        future->RemoveDependentFuture(object2wait);
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
        result = rocprofvis_db_build_compute_query(
            db, use_case,
            static_cast<rocprofvis_db_num_of_params_t>(query_args.size()),
            query_args.data(), &query);
        if(result == kRocProfVisDmResultSuccess)
        {
            if(allocate_db_future)
            {
                db_future = rocprofvis_db_future_alloc(nullptr);
            }
            if(db_future)
            {
                rocprofvis_dm_table_id_t table_id = 0;
                std::string async_fetch_query;
                if(controller_future)
                {
                    async_fetch_query = std::string(query) + " /* " + std::to_string(m_async_fetch_counter ++) + " */";
                }
                result = rocprofvis_db_execute_compute_query_async(db, use_case, async_fetch_query.empty() ? query : async_fetch_query.c_str(), db_future, &table_id);
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
        free(query);
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
