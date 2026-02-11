// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_table_compute_pivot.h"
#include "rocprofvis_c_interface.h"
#include "rocprofvis_c_interface_types.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_enums.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_core_assert.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace Controller
{

ComputePivotTable::ComputePivotTable(const uint64_t id)
: Table(id, __kRPVControllerTablePropertiesFirst, __kRPVControllerTablePropertiesLast)
, m_workload_id(0)
{
    m_sort_column = 1; // Default to duration_ns_sum column
    m_sort_order = kRPVControllerSortOrderDescending; // Default to descending order
}

ComputePivotTable::~ComputePivotTable() {}

rocprofvis_result_t
ComputePivotTable::Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future)
{
    (void) dm_handle;
    (void) future;

    rocprofvis_result_t result = kRocProfVisResultSuccess;

    // Check if this is a dynamic metrics matrix query by detecting pivot table arguments
    if(args.GetUInt64(kRPVControllerCPTArgsWorkloadId, 0, &m_workload_id) ==
       kRocProfVisResultSuccess)
    {
        // Extract number of metric selectors
        uint64_t num_selectors = 0;
        args.GetUInt64(kRPVControllerCPTArgsNumMetricSelectors, 0,
                       &num_selectors);

        // Extract metric selectors (indexed string property)
        m_metric_selectors.clear();
        for(uint64_t i = 0; i < num_selectors; i++)
        {
            char     buffer[256];
            uint32_t length = sizeof(buffer);
            if(args.GetString(kRPVControllerCPTArgsMetricSelectorIndexed, i,
                              buffer, &length) == kRocProfVisResultSuccess)
            {
                m_metric_selectors.push_back(std::string(buffer));
            }
        }

        // Set sort column index
        args.GetUInt64(kRPVControllerCPTArgsSortColumnIndex, 0,
                       &m_sort_column);

        // Set sort order
        uint64_t sort_order_val = 0;
        if(args.GetUInt64(kRPVControllerCPTArgsSortOrder, 0, &sort_order_val) !=
           kRocProfVisResultSuccess)
        {
            m_sort_order = kRPVControllerSortOrderDescending;  // Default to descending
        }
        else
        {
            m_sort_order = (sort_order_val == kRPVControllerSortOrderAscending)
                               ? kRPVControllerSortOrderAscending
                               : kRPVControllerSortOrderDescending;
        }
    }
    else
    {
        result = kRocProfVisResultInvalidArgument;
    }

    return result;
}

rocprofvis_result_t
ComputePivotTable::Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count,
                         Array& array, Future* future)
{
    (void) index;
    (void) count;

    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;

    if(m_workload_id == 0)
    {
        return kRocProfVisResultInvalidArgument;
    }

    // Build parameter array for model layer
    std::vector<rocprofvis_db_compute_param_t> params;
    std::vector<std::string>                   param_storage;  // Storage for string data

    // Reserve capacity to prevent reallocations that would invalidate pointers
    // Fixed params: workload_id, sort_column_index, sort_order
    const size_t fixed_params_count = 3;
    param_storage.reserve(fixed_params_count + m_metric_selectors.size());

    // Add workload ID
    param_storage.push_back(std::to_string(m_workload_id));
    params.push_back({ kRPVComputeParamWorkloadId, param_storage.back().c_str() });

    // Add metric selectors
    for(const auto& selector : m_metric_selectors)
    {
        param_storage.push_back(selector);
        params.push_back({ kRPVComputeParamMetricSelector, param_storage.back().c_str() });
    }

    // Add sort column index
    param_storage.push_back(std::to_string(m_sort_column));
    params.push_back({ kRPVComputeParamSortColumnIndex, param_storage.back().c_str() });

    // Add sort order
    param_storage.push_back((m_sort_order == kRPVControllerSortOrderAscending) ? "ASC" : "DESC");
    params.push_back({ kRPVComputeParamSortColumnOrder, param_storage.back().c_str() });

    rocprofvis_dm_database_t db =
        rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
    ROCPROFVIS_ASSERT(db);

    // Build query using model layer
    char*                  query     = nullptr;
    rocprofvis_dm_result_t dm_result = rocprofvis_db_build_compute_query(
        db, kRPVComputeFetchKernelMetricsMatrix, params.size(), params.data(), &query);

    if(dm_result == kRocProfVisDmResultSuccess && query)
    {
        // Create or get database future
        rocprofvis_db_future_t db_future = nullptr;
        if(future)
        {
            db_future = rocprofvis_db_future_alloc(nullptr);
        }

        if(db_future)
        {
            // Execute query
            rocprofvis_dm_table_id_t table_id = 0;
            dm_result                         = rocprofvis_db_execute_compute_query_async(
                db, kRPVComputeFetchKernelMetricsMatrix, query, db_future, &table_id);

            if(dm_result == kRocProfVisDmResultSuccess)
            {
                // Wait for query completion
                future->AddDependentFuture(db_future);
                dm_result = rocprofvis_db_future_wait(db_future, UINT64_MAX);

                uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(
                    dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                if(num_tables > 0)
                {
                    rocprofvis_dm_table_t table_handle =
                        rocprofvis_dm_get_property_as_handle(
                            dm_handle, kRPVDMTableHandleByID, table_id);
                    if(table_handle)
                    {
                        uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(
                            table_handle, kRPVDMNumberOfTableColumnsUInt64, 0);
                        uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(
                            table_handle, kRPVDMNumberOfTableRowsUInt64, 0);
                        m_columns.clear();
                        m_columns.resize(num_columns);

                        for(int i = 0; i < num_columns; i++)
                        {
                            m_columns[i].m_name = rocprofvis_dm_get_property_as_charptr(
                                table_handle, kRPVDMExtTableColumnNameCharPtrIndexed, i);
                            m_columns[i].m_type = kRPVControllerPrimitiveTypeString;
                        }

                        m_rows.clear();

                        for(int k = 0; k < num_rows; k++)
                        {
                            rocprofvis_dm_table_row_t row_handle =
                                rocprofvis_dm_get_property_as_handle(
                                    table_handle, kRPVDMExtTableRowHandleIndexed, k);

                            uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(
                                row_handle, kRPVDMNumberOfTableRowCellsUInt64, 0);

                            if(num_cells != m_columns.size())
                            {
                                spdlog::error("Column mismatch!");
                            }

                            std::vector<Data> row;
                            row.resize(m_columns.size());
                            for(int i = 0; i < num_columns; i++)
                            {
                                const char* cell_data =
                                    rocprofvis_dm_get_property_as_charptr(
                                        row_handle,
                                        kRPVDMExtTableRowCellValueCharPtrIndexed, i);

                                row[i].SetType(kRPVControllerPrimitiveTypeString);

                                if(cell_data)
                                {
                                    row[i].SetString(cell_data);
                                }
                                else
                                {
                                    row[i].SetString(cell_data);
                                }
                            }
                            m_rows[k] = std::move(row);
                        }
                    }
                }
                rocprofvis_db_future_free(db_future);
                future->RemoveDependentFuture(db_future);
            }
        }
    }

    result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, m_rows.size());
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);

    for(size_t i = 0; i < m_rows.size(); i++)
    {
        try
        {
            Array* row_array = new Array();
        
            auto& row_vec = row_array->GetVector();
            row_vec.resize(m_rows[i].size());
            for(uint32_t j = 0; j < m_rows[i].size(); j++)
            {
                row_vec[j].SetType(m_rows[i][j].GetType());
                row_vec[j] = m_rows[i][j];
            }
            result = array.SetObject(kRPVControllerArrayEntryIndexed, i,
                                        (rocprofvis_handle_t*) row_array);
        
            ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        } catch(const std::exception&)
        {
            result = kRocProfVisResultMemoryAllocError;
        }
    }

    return result;
}

rocprofvis_result_t
ComputePivotTable::ExportCSV(rocprofvis_dm_trace_t dm_handle, Arguments& args,
                             Future* future, const char* path) const
{
    (void) dm_handle;
    (void) args;
    (void) future;
    (void) path;

    return kRocProfVisResultNotSupported;
}

rocprofvis_result_t
ComputePivotTable::GetUInt64(rocprofvis_property_t property, uint64_t index,
                             uint64_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(value)
    {
        switch(property)
        {
            case kRPVControllerTableId:
            {
                *value = m_id;
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTableNumColumns:
            {
                *value = m_columns.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTableNumRows:
            {
                *value = m_rows.size();
                result = kRocProfVisResultSuccess;
                break;
            }
            case kRPVControllerTableColumnTypeIndexed:
            {
                if(index < m_columns.size())
                {
                    *value = m_columns[index].m_type;
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

rocprofvis_result_t
ComputePivotTable::GetString(rocprofvis_property_t property, uint64_t index, char* value,
                             uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerTableColumnHeaderIndexed:
            {
                if(index < m_columns.size())
                {
                    if(!value && length)
                    {
                        *length = m_columns[index].m_name.size();
                        result  = kRocProfVisResultSuccess;
                    }
                    else if(value && length)
                    {
                        strncpy(value, m_columns[index].m_name.c_str(), *length);
                        result = kRocProfVisResultSuccess;
                    }
                }
                break;
            }
            case kRPVControllerTableTitle:
            {
                std::string title = "Kernel Metrics Matrix";
                if(!value && length)
                {
                    *length = title.size();
                    result  = kRocProfVisResultSuccess;
                }
                else if(value && length)
                {
                    strncpy(value, title.c_str(), *length);
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

}  // namespace Controller
}  // namespace RocProfVis
