// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_trace_compute.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_compute_metrics.h"
#include "rocprofvis_controller_plot_compute.h"
#include "rocprofvis_controller_table_compute.h"
#include "rocprofvis_core_assert.h"
#include "spdlog/spdlog.h"
#include <filesystem>

namespace RocProfVis
{
namespace Controller
{

ComputeTrace::ComputeTrace(char const* const filename)
: Trace(__kRPVControllerComputePropertiesFirst, __kRPVControllerComputePropertiesLast, filename)
{}

ComputeTrace::~ComputeTrace()
{
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
            else if(m_trace_file.find(".csv", m_trace_file.size() - 4) != std::string::npos)
            {
                m_trace_file = std::filesystem::path(m_trace_file).parent_path().string();
                result = LoadCSV();
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
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
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
        {
            result = kRocProfVisResultInvalidEnum;
        }
    }
    return result;
}

rocprofvis_result_t ComputeTrace::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value) 
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if (value)
    {
        if (kRPVControllerComputeTableTypeKernelList <= property && property < kRPVControllerComputeTableTypeCount)
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
        else if (kRPVControllerComputePlotTypeKernelDurationPercentage <= property && property < kRPVControllerComputePlotTypeCount)
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
        {
            result = kRocProfVisResultInvalidEnum;
        }
    }
    return result;
}

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

rocprofvis_result_t ComputeTrace::LoadRocpd()
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    m_dm_handle = rocprofvis_dm_create_trace();
    if(nullptr != m_dm_handle)
    {
        /*The code below is for compute support debugging purposes only. Feel free to refactor it or remove it */
        rocprofvis_dm_database_t db =
            rocprofvis_db_open_database(m_trace_file.c_str(), kComputeSqlite);
        if (nullptr != db && kRocProfVisDmResultSuccess ==
            rocprofvis_dm_bind_trace_to_database(m_dm_handle, db))
        {
            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(&Trace::ProgressCallback, this);
            if (nullptr != object2wait)
            {
                if (kRocProfVisDmResultSuccess ==
                    rocprofvis_db_read_metadata_async(db, object2wait))
                {
                    if (kRocProfVisDmResultSuccess ==
                        rocprofvis_db_future_wait(object2wait, UINT64_MAX))
                    {
                        char* fetch_query = nullptr;
                        rocprofvis_dm_result_t dm_result = rocprofvis_db_build_compute_query(db, kRPVComputeFetchListOfWorkloads, 0, nullptr, &fetch_query);
                        if (dm_result == kRocProfVisDmResultSuccess)
                        {
                            rocprofvis_dm_table_id_t table_id = 0;
                            dm_result = rocprofvis_db_execute_compute_query_async(
                                db, kRPVComputeFetchListOfWorkloads, fetch_query, object2wait, &table_id);

                            if (dm_result == kRocProfVisDmResultSuccess)
                            {
                                if (kRocProfVisDmResultSuccess ==
                                    rocprofvis_db_future_wait(object2wait, UINT64_MAX))
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
                    }
                }
                rocprofvis_db_future_free(object2wait);
            }
        }
        /*The code above is for compute support debugging purposes only. Feel free to refactor it or remove it */   
    }
    return result;
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

}
}
