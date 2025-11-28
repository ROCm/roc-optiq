// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_summary.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_counter.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_node.h"
#include "rocprofvis_controller_processor.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_table_system.h"
#include "rocprofvis_controller_trace.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

Summary::Summary(Trace* ctx)
: Handle(__kRPVControllerSummaryPropertiesFirst, __kRPVControllerSummaryPropertiesLast)
, m_ctx(ctx)
, m_start_ts(-1.0)
, m_end_ts(-1.0)
, m_kernel_instance_table(nullptr)
{
    m_kernel_instance_table = new SystemTable(0);
}

Summary::~Summary() 
{
    if(m_kernel_instance_table)
    {
        delete m_kernel_instance_table;
    }
}

rocprofvis_result_t Summary::Fetch(rocprofvis_dm_trace_t dm_handle, Arguments& args, SummaryMetrics& output, Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    result = args.GetDouble(kRPVControllerSummaryArgsStartTimestamp, 0, &m_start_ts);
    if(result == kRocProfVisResultSuccess)
    {
        result = args.GetDouble(kRPVControllerSummaryArgsEndTimestamp, 0, &m_end_ts);
        if(result == kRocProfVisResultSuccess)
        {
            result = FetchMetrics(dm_handle, future);
            if(result == kRocProfVisResultSuccess && !future->IsCancelled())
            {
                output = m_metrics;
            }
        }
    }
    return future->IsCancelled() ? kRocProfVisResultCancelled : result;
}

rocprofvis_controller_object_type_t Summary::GetType(void) 
{
    return kRPVControllerObjectTypeSummary;
}

rocprofvis_result_t Summary::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerCommonMemoryUsageInclusive:
            {
                *value = sizeof(Summary) + sizeof(m_metrics);
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

rocprofvis_result_t Summary::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    (void) index;
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerSummaryPropertyKernelInstanceTable:
            {
                *value = (rocprofvis_handle_t*)m_kernel_instance_table;
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

rocprofvis_result_t Summary::FetchMetrics(rocprofvis_dm_trace_t dm_handle, Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    m_metrics.Reset();
    if(m_ctx)
    {
        uint64_t num_nodes = 0;
        result = m_ctx->GetUInt64(kRPVControllerNumNodes, 0, &num_nodes);
        if(result == kRocProfVisResultSuccess)
        {
            for(int i = 0; i < num_nodes; i ++)
            {
                rocprofvis_handle_t* node_handle = nullptr;
                result = m_ctx->GetObject(kRPVControllerNodeIndexed, i, &node_handle);
                if(result == kRocProfVisResultSuccess)
                {
                    Node* node = (Node*)node_handle;
                    if(node)
                    {                        
                        uint32_t uint32_data = 0;
                        uint64_t uint64_data = 0;
                        std::string str_data;
                        SummaryMetrics node_metrics;
                        node_metrics.SetUInt64(kRPVControllerSummaryMetricPropertyAggregationLevel, 0, kRPVControllerSummaryAggregationLevelNode);
                        result = node->GetString(kRPVControllerNodeHostName, 0, nullptr, &uint32_data);
                        if(result == kRocProfVisResultSuccess)
                        {
                            str_data.resize(uint32_data);
                            result = node->GetString(kRPVControllerNodeHostName, 0, str_data.data(), &uint32_data);
                            if(result == kRocProfVisResultSuccess)
                            {
                                node_metrics.SetString(kRPVControllerSummaryMetricPropertyName, 0, str_data.c_str());
                            }
                        }
                        uint64_t node_id = 0;
                        result = node->GetUInt64(kRPVControllerNodeId, 0, &node_id);
                        if(result == kRocProfVisResultSuccess)
                        {                            
                            node_metrics.SetUInt64(kRPVControllerSummaryMetricPropertyId, 0, node_id);
                            uint64_t num_processors = 0;
                            result = node->GetUInt64(kRPVControllerNodeNumProcessors, 0, &num_processors);
                            if(result == kRocProfVisResultSuccess)
                            {
                                rocprofvis_handle_t* processor_handle = nullptr;
                                for(int j = 0; j < num_processors; j ++)
                                {
                                    result = node->GetObject(kRPVControllerNodeProcessorIndexed, j, &processor_handle);
                                    if(result == kRocProfVisResultSuccess)
                                    {
                                        Processor* processor = (Processor*)processor_handle;
                                        if(processor)
                                        {                                            
                                            SummaryMetrics processor_metrics;
                                            processor_metrics.SetUInt64(kRPVControllerSummaryMetricPropertyAggregationLevel, 0, kRPVControllerSummaryAggregationLevelProcessor);
                                            result = processor->GetString(kRPVControllerProcessorProductName, 0, nullptr, &uint32_data);
                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                str_data.resize(uint32_data);
                                                result = processor->GetString(kRPVControllerProcessorProductName, 0, str_data.data(), &uint32_data);
                                                if(result == kRocProfVisResultSuccess)
                                                {
                                                    processor_metrics.SetString(kRPVControllerSummaryMetricPropertyName, 0, str_data.c_str());
                                                }
                                            }
                                            uint64_t processor_id = 0;
                                            result = processor->GetUInt64(kRPVControllerProcessorId, 0, &processor_id);
                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                processor_metrics.SetUInt64(kRPVControllerSummaryMetricPropertyId, 0, processor_id);
                                                uint64_t processor_type_uint = 0;
                                                result = processor->GetUInt64(kRPVControllerProcessorType, 0, &processor_type_uint);
                                                if(result == kRocProfVisResultSuccess)
                                                {
                                                    processor_metrics.SetUInt64(kRPVControllerSummaryMetricPropertyProcessorType, 0, processor_type_uint);
                                                    rocprofvis_controller_processor_type_t processor_type = static_cast<rocprofvis_controller_processor_type_t>(processor_type_uint);
                                                    result = processor->GetUInt64(kRPVControllerProcessorTypeIndex, 0, &uint64_data);
                                                    if(result == kRocProfVisResultSuccess)
                                                    {
                                                        processor_metrics.SetUInt64(kRPVControllerSummaryMetricPropertyProcessorTypeIndex, 0, uint64_data);
                                                    }                                                    
                                                    if(processor_type == kRPVControllerProcessorTypeGPU)
                                                    {
                                                        FetchTopKernels(dm_handle, node, processor, processor_metrics, future);
                                                    }
                                                    uint64_t num_counters = 0;
                                                    result = processor->GetUInt64(kRPVControllerProcessorNumCounters, 0, &num_counters);
                                                    if(result == kRocProfVisResultSuccess)
                                                    {                                                    
                                                        for(int k = 0; k < num_counters; k ++)
                                                        {
                                                            rocprofvis_handle_t* counter_handle = nullptr;
                                                            result = processor->GetObject(kRPVControllerProcessorCounterIndexed, k, &counter_handle);
                                                            if(result == kRocProfVisResultSuccess)
                                                            {
                                                                Counter* counter = (Counter*)counter_handle;
                                                                if(counter)
                                                                {
                                                                    uint32_t counter_name_length = 0;
                                                                    result = counter->GetString(kRPVControllerCounterName, 0, nullptr, &counter_name_length);
                                                                    if(result == kRocProfVisResultSuccess)
                                                                    {
                                                                        std::string counter_name;
                                                                        counter_name.resize(counter_name_length);
                                                                        result = counter->GetString(kRPVControllerCounterName, 0, counter_name.data(), &counter_name_length);
                                                                        if(result == kRocProfVisResultSuccess)
                                                                        {                 
                                                                            float data = 0.0f;
                                                                            switch(processor_type)
                                                                            {
                                                                                case kRPVControllerProcessorTypeGPU:
                                                                                {
                                                                                    if(counter_name.find("device_busy_gfx") != std::string::npos && 
                                                                                       kRocProfVisResultSuccess == FetchCounterAverage(dm_handle, counter, data, future))
                                                                                    {
                                                                                        processor_metrics.SetDouble(kRPVControllerSummaryMetricPropertyGpuGfxUtil, 0, (double)data);
                                                                                    }
                                                                                    else if(counter_name.find("device_memory_usage") != std::string::npos && 
                                                                                            kRocProfVisResultSuccess == FetchCounterAverage(dm_handle, counter, data, future))
                                                                                    {
                                                                                        processor_metrics.SetDouble(kRPVControllerSummaryMetricPropertyGpuMemUtil, 0, (double)data);
                                                                                    }
                                                                                    break;
                                                                                }
                                                                                case kRPVControllerProcessorTypeCPU:
                                                                                {
                                                                                    //CPU
                                                                                    break;
                                                                                }
                                                                            }
                                                                        }
                                                                        else
                                                                        {
                                                                            result = kRocProfVisResultUnknownError;
                                                                            break;
                                                                        }
                                                                    }
                                                                    else
                                                                    {
                                                                        result = kRocProfVisResultUnknownError;
                                                                        break;
                                                                    }
                                                                }
                                                                else
                                                                {
                                                                    result = kRocProfVisResultUnknownError;
                                                                    break;
                                                                }
                                                            }
                                                            else
                                                            {
                                                                result = kRocProfVisResultUnknownError;
                                                                break;
                                                            }
                                                        }
                                                    }
                                                    else
                                                    {
                                                        result = kRocProfVisResultUnknownError;
                                                        break;
                                                    }
                                                    if(result == kRocProfVisResultSuccess && !processor_metrics.Empty())
                                                    {
                                                        result = node_metrics.GetUInt64(kRPVControllerSummaryMetricPropertyNumSubMetrics, 0, &uint64_data);
                                                        if(result == kRocProfVisResultSuccess)
                                                        {
                                                            node_metrics.SetUInt64(kRPVControllerSummaryMetricPropertyNumSubMetrics, 0, uint64_data + 1);
                                                            node_metrics.SetObject(kRPVControllerSummaryMetricPropertySubMetricsIndexed, uint64_data, 
                                                                                   (rocprofvis_handle_t*)&processor_metrics);
                                                        }
                                                    }
                                                }
                                            }
                                            else
                                            {
                                                result = kRocProfVisResultUnknownError;
                                                break;
                                            }
                                        }
                                        else
                                        {
                                            result = kRocProfVisResultUnknownError;
                                            break;
                                        }
                                    }
                                    else
                                    {
                                        result = kRocProfVisResultUnknownError;
                                        break;
                                    }
                                }
                            }
                            else
                            {
                                result = kRocProfVisResultUnknownError;
                                break;
                            }
                        }
                        else
                        {
                            result = kRocProfVisResultUnknownError;
                            break;
                        }
                        if(result == kRocProfVisResultSuccess)
                        {
                            if(node_metrics.Empty())
                            {
                                FetchTopKernels(dm_handle, node, nullptr, node_metrics, future);
                            }
                            if(!node_metrics.Empty())
                            {
                                result = m_metrics.GetUInt64(kRPVControllerSummaryMetricPropertyNumSubMetrics, 0, &uint64_data);
                                if(result == kRocProfVisResultSuccess)
                                {
                                    m_metrics.SetUInt64(kRPVControllerSummaryMetricPropertyNumSubMetrics, 0, uint64_data + 1);
                                    m_metrics.SetObject(kRPVControllerSummaryMetricPropertySubMetricsIndexed, uint64_data, 
                                                       (rocprofvis_handle_t*)&node_metrics);
                                }
                                else
                                {
                                    result = kRocProfVisResultUnknownError;
                                    break;
                                }
                            }
                        }                       
                    }
                    else
                    {
                        result = kRocProfVisResultUnknownError;
                        break;
                    }
                }
                else
                {
                    result = kRocProfVisResultUnknownError;
                    break;
                }
            }
        }
        if(result == kRocProfVisResultSuccess)
        {
            if(m_metrics.Empty())
            {
                FetchTopKernels(dm_handle, nullptr, nullptr, m_metrics, future);
            }
            else
            {
                m_metrics.AggregateSubMetrics();
            }            
        }
    }
    return result;
}

rocprofvis_result_t Summary::FetchCounterAverage(rocprofvis_dm_trace_t dm_handle, Counter* counter, float& average, Future* future) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(counter)
    {
        rocprofvis_handle_t* track_handle = nullptr;
        result = counter->GetObject(kRPVControllerCounterTrack, 0, &track_handle);
        if(result == kRocProfVisResultSuccess)
        {
            Track* track = (Track*)track_handle;
            if(track_handle)
            {
                uint64_t track_id = 0;
                result = track->GetUInt64(kRPVControllerTrackId, 0, &track_id);
                if(result == kRocProfVisResultSuccess)
                {
                    rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
                    if(object2wait)
                    {
                        rocprofvis_dm_database_t db = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
                        if(db)
                        {
                            rocprofvis_dm_result_t dm_result = kRocProfVisDmResultUnknownError;
                            rocprofvis_dm_table_id_t table_id = 0;
                            char* query = nullptr;
                            dm_result = rocprofvis_db_build_table_query(db, m_start_ts, m_end_ts, 1, (rocprofvis_db_track_selection_t)&track_id, nullptr, nullptr, 
                                                                        nullptr, nullptr, nullptr, kRPVDMSortOrderAsc, 0, nullptr,  0, 0, false, true, &query);
                            if(dm_result == kRocProfVisDmResultSuccess)
                            {
                                dm_result = rocprofvis_db_execute_query_async(db, query, "Fetch counter summary", object2wait, &table_id);
                                if(dm_result == kRocProfVisDmResultSuccess)
                                {
                                    future->AddDependentFuture(object2wait);
                                    dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
                                    if(dm_result == kRocProfVisDmResultSuccess)
                                    {
                                        uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                                        if(num_tables > 0)
                                        {
                                            rocprofvis_dm_table_t table = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMTableHandleByID, table_id);
                                            if(table)
                                            {
                                                if(!future->IsCancelled())
                                                {                    
                                                    const char* table_query = rocprofvis_dm_get_property_as_charptr(table, kRPVDMExtTableQueryCharPtr, 0);
                                                    uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(table, kRPVDMNumberOfTableRowsUInt64, 0);
                                                    uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(table, kRPVDMNumberOfTableColumnsUInt64, 0);
                                                    if(strcmp(table_query, query) == 0 && num_rows == 1)
                                                    {
                                                        rocprofvis_dm_table_row_t table_row = rocprofvis_dm_get_property_as_handle(table, kRPVDMExtTableRowHandleIndexed, 0);
                                                        if(table_row != nullptr)
                                                        {
                                                            uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(table_row, kRPVDMNumberOfTableRowCellsUInt64, 0);
                                                            if(num_cells == num_columns)
                                                            {
                                                                for(int i = 0; i < num_cells; i++)
                                                                {
                                                                    char const* col_name = rocprofvis_dm_get_property_as_charptr(table, kRPVDMExtTableColumnNameCharPtrIndexed, i);
                                                                    char const* value = rocprofvis_dm_get_property_as_charptr(table_row, kRPVDMExtTableRowCellValueCharPtrIndexed, i);
                                                                    if(strcmp(col_name, "avg_value") == 0)
                                                                    {
                                                                        if(value && strlen(value) > 0)
                                                                        {
                                                                            average = strtof(value, NULL);
                                                                            result = kRocProfVisResultSuccess;
                                                                        }
                                                                        else
                                                                        {
                                                                            result = kRocProfVisResultUnknownError;
                                                                        }
                                                                        break;
                                                                    }
                                                                }
                                                            }
                                                            else
                                                            {
                                                                result = kRocProfVisResultUnknownError;
                                                            }
                                                        }
                                                        else
                                                        {
                                                            result = kRocProfVisResultUnknownError;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        result = kRocProfVisResultUnknownError;
                                                    }
                                                }
                                                rocprofvis_dm_delete_table_at(dm_handle, table_id);
                                            }
                                            else
                                            {
                                                result = kRocProfVisResultUnknownError;
                                            }
                                        }
                                        else
                                        {
                                            result = kRocProfVisResultUnknownError;
                                        }
                                    }
                                    else
                                    {
                                        result = kRocProfVisResultUnknownError;
                                    }                                    
                                }
                                else
                                {
                                    result = kRocProfVisResultUnknownError;
                                }
                            }
                            else
                            {
                                result = kRocProfVisResultUnknownError;
                            }
                        }
                        else
                        {
                            result = kRocProfVisResultUnknownError;
                        }
                        future->RemoveDependentFuture(object2wait);
                        rocprofvis_db_future_free(object2wait);
                    }
                }
            }
            else
            {
                result = kRocProfVisResultUnknownError;
            }
        }
    }
    return result;
}

rocprofvis_result_t Summary::FetchTopKernels(rocprofvis_dm_trace_t dm_handle, Node* node, Processor* processor, SummaryMetrics& output, Future* future) const
{
    rocprofvis_result_t result = kRocProfVisResultUnknownError;
    rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
    if(object2wait)
    {
        rocprofvis_dm_database_t db  = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
        if(db)
        {
            rocprofvis_dm_result_t dm_result = kRocProfVisDmResultUnknownError;
            uint32_t op[1] = { TABLE_QUERY_PACK_OP_TYPE(kRocProfVisDmOperationDispatch) };
            std::string where_str;
            if(node)
            {
                uint64_t node_id = 0;
                result = node->GetUInt64(kRPVControllerNodeId, 0, &node_id);
                if(result == kRocProfVisResultSuccess)
                {
                    where_str = "nodeId = " + std::to_string(node_id);
                    if(processor)
                    {
                        uint64_t agent_id = 0;
                        result = processor->GetUInt64(kRPVControllerProcessorId, 0, &agent_id);
                        if(result == kRocProfVisResultSuccess)
                        {
                            where_str += " AND agentId = " + std::to_string(agent_id);
                        }
                    }
                }
            }
            rocprofvis_dm_table_id_t table_id = 0;
            rocprofvis_dm_charptr_t where = where_str.empty() ? nullptr : where_str.c_str();
            std::string sort_column = "total_duration";
            char* query = nullptr;
            dm_result = rocprofvis_db_build_table_query(db, m_start_ts, m_end_ts, 1, (rocprofvis_db_track_selection_t)op, where, nullptr, nullptr, 
                                                        nullptr, sort_column.c_str(), kRPVDMSortOrderDesc, 0, nullptr, 0, 0, false, true, &query);
            if(dm_result == kRocProfVisDmResultSuccess)
            {
                dm_result = rocprofvis_db_execute_query_async(db, query, "Fetch kernel summary", object2wait, &table_id);
                if(dm_result == kRocProfVisDmResultSuccess)
                {
                    future->AddDependentFuture(object2wait);
                    dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
                    if(dm_result == kRocProfVisDmResultSuccess)
                    {
                        uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                        if(num_tables > 0)
                        {
                            rocprofvis_dm_table_t table = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMTableHandleByID, table_id);
                            if(table)
                            {
                                if(!future->IsCancelled())
                                {                    
                                    const char* table_query = rocprofvis_dm_get_property_as_charptr(table, kRPVDMExtTableQueryCharPtr, 0);
                                    uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(table, kRPVDMNumberOfTableRowsUInt64, 0);
                                    uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(table, kRPVDMNumberOfTableColumnsUInt64, 0);
                                    if(strcmp(table_query, query) == 0)
                                    {
                                        result = kRocProfVisResultSuccess;
                                        if(num_rows > 0)
                                        { 
                                            double exec_time_total = 0.0;
                                            uint64_t top_kernels_count = std::min(uint64_t(10), num_rows);
                                            output.SetUInt64(kRPVControllerSummaryMetricPropertyNumKernels, 0, top_kernels_count);                                        
                                            for(int i = 0; i < num_rows; i++)
                                            {
                                                rocprofvis_dm_table_row_t table_row = rocprofvis_dm_get_property_as_handle(table, kRPVDMExtTableRowHandleIndexed, i);
                                                if(table_row != nullptr)
                                                {
                                                    uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(table_row, kRPVDMNumberOfTableRowCellsUInt64, 0);                                        
                                                    if(num_cells == num_columns)
                                                    {
                                                        for(int j = 0; j < num_cells; j++)
                                                        {
                                                            char const* col_name = rocprofvis_dm_get_property_as_charptr(table, kRPVDMExtTableColumnNameCharPtrIndexed, j);
                                                            char const* value = rocprofvis_dm_get_property_as_charptr(table_row, kRPVDMExtTableRowCellValueCharPtrIndexed, j);
                                                            if(i < top_kernels_count)
                                                            {
                                                                if(strcmp(col_name, "name") == 0)
                                                                {
                                                                    output.SetString(kRPVControllerSummaryMetricPropertyKernelNameIndexed, i, value);                                                            
                                                                }
                                                                else if(strcmp(col_name, "num_invocations") == 0)
                                                                {
                                                                    output.SetUInt64(kRPVControllerSummaryMetricPropertyKernelInvocationsIndexed, i, strtoull(value, NULL, 10));
                                                                }
                                                                else if(strcmp(col_name, "total_duration") == 0)
                                                                {
                                                                    double duration = strtod(value, NULL);
                                                                    output.SetDouble(kRPVControllerSummaryMetricPropertyKernelExecTimeSumIndexed, i, duration);
                                                                    exec_time_total += duration;                                                                
                                                                }
                                                                else if(strcmp(col_name, "min_duration") == 0)
                                                                {
                                                                    output.SetDouble(kRPVControllerSummaryMetricPropertyKernelExecTimeMinIndexed, i, strtod(value, NULL));
                                                                }
                                                                else if(strcmp(col_name, "max_duration") == 0)
                                                                {
                                                                    output.SetDouble(kRPVControllerSummaryMetricPropertyKernelExecTimeMaxIndexed, i, strtod(value, NULL));
                                                                }
                                                            }
                                                            else if(strcmp(col_name, "total_duration") == 0)
                                                            {
                                                                exec_time_total += strtod(value, NULL);
                                                            }
                                                        }
                                                    }
                                                    else
                                                    {
                                                        result = kRocProfVisResultUnknownError;
                                                    }                                        
                                                }
                                                else
                                                {
                                                    result = kRocProfVisResultUnknownError;
                                                }
                                            }
                                            if(result == kRocProfVisResultSuccess)
                                            {
                                                for(int i = 0; i < top_kernels_count; i ++)
                                                {
                                                    double exec_time_sum = 0.0;
                                                    result = output.GetDouble(kRPVControllerSummaryMetricPropertyKernelExecTimeSumIndexed, i, &exec_time_sum);
                                                    if(result == kRocProfVisResultSuccess)
                                                    {
                                                        output.SetDouble(kRPVControllerSummaryMetricPropertyKernelExecTimePctIndexed, i, exec_time_sum / exec_time_total);
                                                    }
                                                    else
                                                    {
                                                        break;
                                                    }
                                                }
                                                output.SetDouble(kRPVControllerSummaryMetricPropertyKernelsExecTimeTotal, 0, exec_time_total);
                                                output.PadTopKernels();
                                            }
                                        }
                                    }
                                    else
                                    {
                                        result = kRocProfVisResultUnknownError;
                                    }
                                }
                                rocprofvis_dm_delete_table_at(dm_handle, table_id);
                            }
                            else
                            {
                                result = kRocProfVisResultUnknownError;
                            }
                        }
                        else
                        {
                            result = kRocProfVisResultUnknownError;
                        }
                    }
                    else
                    {
                        result = kRocProfVisResultUnknownError;
                    }                                    
                }
                else
                {
                    result = kRocProfVisResultUnknownError;
                }
            }
            else
            {
                result = kRocProfVisResultUnknownError;
            }
        }
        else
        {
            result = kRocProfVisResultUnknownError;
        }
        future->RemoveDependentFuture(object2wait);
        rocprofvis_db_future_free(object2wait);
    }
    return result;
}

}
}
