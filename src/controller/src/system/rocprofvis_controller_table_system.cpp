// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_table_system.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_future.h"
#include "rocprofvis_controller_trace_system.h"
#include <cstdlib>

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;

SystemTable::SystemTable(uint64_t id)
: Table(id, __kRPVControllerTablePropertiesFirst, __kRPVControllerTablePropertiesLast)
, m_use_case(kRPVDMTableUseCaseEventTrackTable)
, m_start_ts(0)
, m_end_ts(0)
{
}

SystemTable::~SystemTable()
{
}

void SystemTable::Reset()
{
    Table::Reset();
    m_start_ts = 0;
    m_end_ts = 0;
    m_use_case = kRPVDMTableUseCaseEventTrackTable;
    m_tracks.clear();
    m_where.clear();
    m_filter.clear();
    m_group.clear();
    m_group_cols.clear();
}

rocprofvis_result_t SystemTable::SetupAndFetch(Trace& controller, Arguments& args, Array& array, Future* future)
{
    SystemTrace* system_controller = dynamic_cast<SystemTrace*>(&controller);
    ROCPROFVIS_ASSERT(system_controller);
    rocprofvis_result_t result = UnpackUseCase(args, m_use_case);
    ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
    std::lock_guard<std::mutex> lock(system_controller->GetTableMutex(m_use_case));
    return Table::SetupAndFetch(controller, args, array, future);
}

rocprofvis_result_t SystemTable::Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count, Array& array, Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
    if(object2wait)
    {
        rocprofvis_dm_database_t db    = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
        ROCPROFVIS_ASSERT(db);

        TableArguments* fetch_args = nullptr;
        GetCurrentArguments(fetch_args);
        uint64_t num_records = 0;

        rocprofvis_dm_result_t dm_result = kRocProfVisDmResultUnknownError;
        if(fetch_args)
        {
            char* fetch_query = nullptr;
            dm_result = BuildQuery(db, *fetch_args, index, count, false, &fetch_query);
            rocprofvis_dm_table_id_t table_id = 0;
            
            if(dm_result == kRocProfVisDmResultSuccess && fetch_query)
            {
                if(strlen(fetch_query) > 0)
                {
                    dm_result = rocprofvis_db_execute_query_async(
                        db, fetch_query, "Fetch table content", object2wait, &table_id);

                    if(dm_result == kRocProfVisDmResultSuccess)
                    {
                        future->AddDependentFuture(object2wait);
                        dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
                    }

                    if(dm_result == kRocProfVisDmResultSuccess)
                    {
                        uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(
                            dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                        if(num_tables > 0)
                        {
                            rocprofvis_dm_table_t table = rocprofvis_dm_get_property_as_handle(
                                dm_handle, kRPVDMTableHandleByID, table_id);
                            if(nullptr != table)
                            {
                                if(!future->IsCancelled())
                                {                    
                                    char* table_query = rocprofvis_dm_get_property_as_charptr(
                                        table, kRPVDMExtTableQueryCharPtr, 0);
                                    uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(
                                        table, kRPVDMNumberOfTableColumnsUInt64, 0);
                                    uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(
                                        table, kRPVDMNumberOfTableRowsUInt64, 0);
                                    num_records = num_rows;
                                    if(strcmp(table_query, fetch_query) == 0)
                                    {
                                        ROCPROFVIS_ASSERT(m_columns.size() == num_columns);

                                        std::vector<Data> row;
                                        row.resize(m_columns.size());
                                        for (uint32_t i = 0; i < num_rows; i++)
                                        {
                                            rocprofvis_dm_table_row_t table_row =
                                                rocprofvis_dm_get_property_as_handle(
                                                    table, kRPVDMExtTableRowHandleIndexed, i);
                                            if(table_row != nullptr)
                                            {
                                                uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(
                                                    table_row, kRPVDMNumberOfTableRowCellsUInt64, 0);
                                                ROCPROFVIS_ASSERT(num_cells == num_columns);
                                                for(uint32_t j = 0; j < num_cells; j++)
                                                {
                                                    char const* value =
                                                        rocprofvis_dm_get_property_as_charptr(
                                                            table_row,
                                                            kRPVDMExtTableRowCellValueCharPtrIndexed, j);
                                                    ROCPROFVIS_ASSERT(value);

                                                    Data& row_value = row[j];
                                                    row_value.SetType(kRPVControllerPrimitiveTypeString);
                                                    row_value.SetString(value);
                                                }

                                                m_rows[index + i] = row;
                                            }
                                            else
                                            {
                                                dm_result = kRocProfVisDmResultUnknownError;
                                            }
                                        }
                                    }
                                    else
                                    {
                                        dm_result = kRocProfVisDmResultUnknownError;
                                    }
                                }
                            }
                            else
                            {
                                dm_result = kRocProfVisDmResultUnknownError;
                            }
                        }
                        else
                        {
                            dm_result = kRocProfVisDmResultUnknownError;
                        }
                    }
                    rocprofvis_dm_delete_table_at(dm_handle, table_id);
                }
            }
            free(fetch_query);
        }
        delete fetch_args;
        
        if(!future->IsCancelled())
        {
            switch (dm_result)
            {
                case kRocProfVisDmResultSuccess:
                {
                    result = array.SetUInt64(kRPVControllerArrayNumEntries, 0, num_records);
                    break;
                }
                default:
                {
                    result = kRocProfVisResultUnknownError;
                    break;
                }
            }

            for(uint32_t i = index;
                (result == kRocProfVisResultSuccess) && i < index + num_records; i++)
            {
                Array* row_array = nullptr;
                try
                {
                    row_array = new Array();
                    {
                        auto& row_vec = row_array->GetVector();
                        row_vec.resize(m_rows[i].size());
                        for (uint32_t j = 0; j < m_rows[i].size(); j++)
                        {
                            row_vec[j].SetType(m_rows[i][j].GetType());
                            row_vec[j] = m_rows[i][j];
                        }
                        result = array.SetOwnedObject(kRPVControllerArrayEntryIndexed, i - index,
                                        (rocprofvis_handle_t*)row_array);
                        if(result != kRocProfVisResultSuccess)
                        {
                            delete row_array;
                        }
                    }
                }
                catch(const std::exception&)
                {
                    delete row_array;
                    result = kRocProfVisResultMemoryAllocError;
                }
            }
        }
        future->RemoveDependentFuture(object2wait);
        rocprofvis_db_future_free(object2wait);
    }
    else
    {
        result = kRocProfVisResultMemoryAllocError;
    }
    return future->IsCancelled() ? kRocProfVisResultCancelled : result;
}

rocprofvis_result_t SystemTable::Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    
    TableArguments* query_args = nullptr;
    result = future->IsCancelled() ? kRocProfVisResultCancelled : UnpackArguments(args, query_args);

    if(result == kRocProfVisResultSuccess && query_args)
    {
        Table::SetCurrentArguments(*query_args);

        if(!ArgumentsChanged((SystemTableArguments&)*query_args))
        {
            m_rows.clear();
        }
        else
        {
            Reset();

            SetCurrentArguments(*query_args);

            rocprofvis_dm_database_t db =
                rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
            ROCPROFVIS_ASSERT(db);

            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
            ROCPROFVIS_ASSERT(object2wait);

            char*                  count_query = nullptr;
            rocprofvis_dm_result_t dm_result = BuildQuery(db, *query_args, 0, 0, true, &count_query);
            if(dm_result == kRocProfVisDmResultSuccess)
            {
                if(strlen(count_query) > 0)
                {
                    rocprofvis_dm_table_id_t table_id = 0;
                    dm_result = rocprofvis_db_execute_query_async(
                        db, count_query, "Calculate table count", object2wait, &table_id);

                    if(dm_result == kRocProfVisDmResultSuccess)
                    {
                        future->AddDependentFuture(object2wait);
                        dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
                    }

                    future->RemoveDependentFuture(object2wait);
                    rocprofvis_db_future_free(object2wait);

                    if(dm_result == kRocProfVisDmResultSuccess)
                    {
                        dm_result           = kRocProfVisDmResultUnknownError;
                        uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(
                            dm_handle, kRPVDMNumberOfTablesUInt64, 0);
                        if(num_tables > 0)
                        {
                            rocprofvis_dm_table_t table = rocprofvis_dm_get_property_as_handle(
                                dm_handle, kRPVDMTableHandleByID, table_id);
                            if(nullptr != table)
                            {
                                char* table_description = rocprofvis_dm_get_property_as_charptr(
                                    table, kRPVDMExtTableQueryCharPtr, 0);
                                uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(
                                    table, kRPVDMNumberOfTableColumnsUInt64, 0);
                                uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(
                                    table, kRPVDMNumberOfTableRowsUInt64, 0);
                                if(!future->IsCancelled() && strcmp(table_description, count_query) == 0 &&
                                   num_columns > 1 && num_rows == 1)
                                {
                                    char const* column = rocprofvis_dm_get_property_as_charptr(
                                        table, kRPVDMExtTableColumnNameCharPtrIndexed, 0);
                                    if(strcmp(column, "NumRecords") == 0)
                                    {
                                        rocprofvis_dm_table_row_t table_row =
                                            rocprofvis_dm_get_property_as_handle(
                                                table, kRPVDMExtTableRowHandleIndexed, 0);
                                        if(table_row != nullptr)
                                        {

                                            std::string value =
                                                rocprofvis_dm_get_property_as_charptr(
                                                    table_row,
                                                    kRPVDMExtTableRowCellValueCharPtrIndexed,
                                                    0);
                                            m_num_items = std::stoull(value);
                                            dm_result   = kRocProfVisDmResultSuccess;


                                            m_columns.clear();
                                            m_columns.resize(num_columns-1);
                                            for(int i = 1; i < num_columns; i++)
                                            {
                                                m_columns[i-1].m_name =
                                                    rocprofvis_dm_get_property_as_charptr(
                                                        table,
                                                        kRPVDMExtTableColumnNameCharPtrIndexed,
                                                        i);
                                                m_columns[i-1].m_type = PrimitiveType(
                                                    (rocprofvis_db_data_type_t)
                                                        rocprofvis_dm_get_property_as_uint64(
                                                            table,
                                                            kRPVDMExtTableColumnTypeUInt64Indexed,
                                                            i));
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                    rocprofvis_dm_delete_table_at(dm_handle, table_id);
                }
                else
                {
                    m_columns.clear();
                }
            }
            free(count_query);
        }
    }
    if(future->IsCancelled())
    {
        Reset();
    }
    delete query_args;
    return result;
}

rocprofvis_controller_primitive_type_t
SystemTable::PrimitiveType(rocprofvis_db_data_type_t db_data_type) const
{
    rocprofvis_controller_primitive_type_t result = kRPVControllerPrimitiveTypeString;
    switch(db_data_type)
    {
        case kRPVDataTypeInt:
        {
            result = kRPVControllerPrimitiveTypeUInt64;
            break;
        }
        case kRPVDataTypeDouble:
        {
            result = kRPVControllerPrimitiveTypeDouble;
            break;
        }
        default:
        {
            break;
        }
    }
    return result;
}

rocprofvis_result_t SystemTable::ExportCSV(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future, const char* path) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    
    TableArguments* query_args = nullptr;
    result = future->IsCancelled() ? kRocProfVisResultCancelled : UnpackArguments(args, query_args);

    if(result == kRocProfVisResultSuccess && query_args)
    {
        rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(Future::ProgressCallback, future);
        if(object2wait)
        {
            rocprofvis_dm_database_t db    = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
            ROCPROFVIS_ASSERT(db);

            char* query = nullptr;
            rocprofvis_dm_result_t dm_result = BuildQuery(db, *query_args, 0, 0, false, &query);
            if(dm_result == kRocProfVisDmResultSuccess)
            {
                dm_result = rocprofvis_db_export_table_csv_async(db, query, path, object2wait);
            }

            if(dm_result == kRocProfVisDmResultSuccess)
            {
                future->AddDependentFuture(object2wait);
                dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
            }
            
            future->RemoveDependentFuture(object2wait);
            rocprofvis_db_future_free(object2wait);
            free(query);
        }
        else
        {
            result = kRocProfVisResultMemoryAllocError;
        }
    }
    delete query_args;
    return result;
}

rocprofvis_result_t
SystemTable::UnpackArguments(Arguments& args, TableArguments*& out) const
{
    if(!out)
    {
        out = new SystemTableArguments();
    }
    SystemTableArguments* sys_out = (SystemTableArguments*)out;
    rocprofvis_result_t result = Table::UnpackArguments(args, out);
    if(result == kRocProfVisResultSuccess)
    {
        std::vector<uint32_t> tracks;
        uint64_t num_tracks = 0;
        uint64_t num_op_types = 0;
        double   end_ts     = 0;
        double   start_ts   = 0;
        std::string where;
        std::string filter;
        std::string group;
        std::string group_cols;
        rocprofvis_dm_table_use_case_enum_t use_case = kRPVDMTableUseCaseEventTrackTable;
        rocprofvis_controller_track_type_t track_type = kRPVControllerTrackTypeEvents;
        uint64_t table_type = static_cast<uint64_t>(kRPVControllerTableTypeEvents);

        result = UnpackUseCase(args, use_case);
        if (result == kRocProfVisResultSuccess)
        {
            result = args.GetUInt64(kRPVControllerTableArgsType, 0, &table_type);
        }
        if (result == kRocProfVisResultSuccess)
        {
            switch (table_type)
            {
                case kRPVControllerTableTypeEvents:
                {
                    track_type = kRPVControllerTrackTypeEvents;
                    break;
                }
                case kRPVControllerTableTypeSamples:
                {
                    track_type = kRPVControllerTrackTypeSamples;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetDouble(kRPVControllerTableArgsStartTime, 0, &start_ts);
        }

        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetDouble(kRPVControllerTableArgsEndTime, 0, &end_ts);
        }
        
        if(result == kRocProfVisResultSuccess)
        {
            args.GetUInt64(kRPVControllerTableArgsNumTracks, 0, &num_tracks);
            args.GetUInt64(kRPVControllerTableArgsNumOpTypes, 0, &num_op_types);
        }
        
        if(result == kRocProfVisResultSuccess)
        {
            if(num_tracks > 0 && num_op_types == 0)
            {
                for (uint32_t i = 0; i < num_tracks && (result == kRocProfVisResultSuccess); i++)
                {
                    TrackRef track_ref;
                    result = args.GetObject(kRPVControllerTableArgsTracksIndexed, i, track_ref.GetHandleAddress());
                    if (track_ref.IsValid())
                    {
                        uint64_t test_type = 0;
                        result = track_ref->GetUInt64(kRPVControllerTrackType, 0, &test_type);
                        if (test_type == track_type)
                        {
                            uint64_t track_id = 0;
                            result = track_ref->GetUInt64(kRPVControllerTrackId, 0, &track_id);
                            if(result == kRocProfVisResultSuccess)
                            {
                                ROCPROFVIS_ASSERT(track_id <= UINT32_MAX);
                                tracks.push_back((uint32_t)track_id);
                            }
                        }
                        else
                        {
                            result = kRocProfVisResultInvalidType;
                        }
                    }
                    else
                    {
                        result = kRocProfVisResultInvalidArgument;
                    }
                }
            }
            else if(num_tracks == 0 && num_op_types > 0)
            {
                for (uint32_t i = 0; i < num_op_types && (result == kRocProfVisResultSuccess); i++)
                {
                    uint64_t op_type_uint64 = kRocProfVisDmOperationNoOp;
                    result = args.GetUInt64(kRPVControllerTableArgsOpTypesIndexed, i, &op_type_uint64);
                    if(result == kRocProfVisResultSuccess)
                    {
                        ROCPROFVIS_ASSERT(op_type_uint64 < kRocProfVisDmNumOperation);
                        tracks.push_back(static_cast<uint32_t>(TABLE_QUERY_PACK_OP_TYPE(op_type_uint64)));
                    }                
                }
            }
            else
            {
                result = kRocProfVisResultInvalidArgument;
            }
        }

        if(result == kRocProfVisResultSuccess)
        {
            uint32_t length = 0;
            result = args.GetString(kRPVControllerTableArgsWhere, 0, nullptr, &length);
            if(result == kRocProfVisResultSuccess)
            {
                where.resize(length);
                result = args.GetString(kRPVControllerTableArgsWhere, 0, where.data(), &length);
            }
        }
        if(result == kRocProfVisResultSuccess)
        {
            uint32_t length = 0;
            result = args.GetString(kRPVControllerTableArgsFilter, 0, nullptr, &length);
            if(result == kRocProfVisResultSuccess)
            {
                filter.resize(length);
                result = args.GetString(kRPVControllerTableArgsFilter, 0, filter.data(), &length);
            }
        }
        if(result == kRocProfVisResultSuccess)
        {
            uint32_t length = 0;
            result = args.GetString(kRPVControllerTableArgsGroup, 0, nullptr, &length);
            if(result == kRocProfVisResultSuccess)
            {
                group.resize(length);
                result = args.GetString(kRPVControllerTableArgsGroup, 0, group.data(), &length);
            }
        }
        if(result == kRocProfVisResultSuccess)
        {
            uint32_t length = 0;
            result = args.GetString(kRPVControllerTableArgsGroupColumns, 0, nullptr, &length);
            if(result == kRocProfVisResultSuccess)
            {
                group_cols.resize(length);
                result = args.GetString(kRPVControllerTableArgsGroupColumns, 0, group_cols.data(), &length);
            }
        }
        if(result == kRocProfVisResultSuccess)
        {
            sys_out->m_where = std::move(where);
            sys_out->m_filter = std::move(filter);
            sys_out->m_group = std::move(group);
            sys_out->m_group_cols = std::move(group_cols);
            sys_out->m_tracks = std::move(tracks);
            sys_out->m_use_case = use_case;
            sys_out->m_start_ts = start_ts;
            sys_out->m_end_ts = end_ts;
        }   
    }
	return result;
}

void
SystemTable::GetCurrentArguments(TableArguments*& out) const
{
    if(!out)
    {
        out = new SystemTableArguments();
    }
    SystemTableArguments* sys_out = (SystemTableArguments*)out;
    Table::GetCurrentArguments(out);
    sys_out->m_where = m_where;
    sys_out->m_filter = m_filter;
    sys_out->m_group = m_group;
    sys_out->m_group_cols = m_group_cols;
    sys_out->m_tracks = m_tracks;
    sys_out->m_use_case = m_use_case;
    sys_out->m_start_ts = m_start_ts;
    sys_out->m_end_ts = m_end_ts;
}

void
SystemTable::SetCurrentArguments(TableArguments& in)
{
    SystemTableArguments& sys_in = (SystemTableArguments&)in;
    Table::SetCurrentArguments(sys_in);
    m_where = sys_in.m_where;
    m_filter = sys_in.m_filter;
    m_group = sys_in.m_group;
    m_group_cols = sys_in.m_group_cols;
    m_tracks = sys_in.m_tracks;
    m_use_case = sys_in.m_use_case;
    m_start_ts = sys_in.m_start_ts;
    m_end_ts = sys_in.m_end_ts;
}

bool
SystemTable::ArgumentsChanged(SystemTableArguments& in) const
{
    bool result = true;
    if(m_tracks.size() == in.m_tracks.size() && m_start_ts == in.m_start_ts &&
        m_end_ts == in.m_end_ts && m_where == in.m_where && m_filter == in.m_filter && 
        m_group == in.m_group && m_group_cols == in.m_group_cols && 
        m_use_case == in.m_use_case)
    {
        result = false;
        for (size_t i = 0; i < in.m_tracks.size(); i++)
        {
            if(m_tracks[i] != in.m_tracks[i])
            {
                result = true;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t
SystemTable::UnpackUseCase(Arguments& args, rocprofvis_dm_table_use_case_enum_t& out) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    uint64_t num_search_terms = 0;
    result = args.GetUInt64(kRPVControllerTableArgsNumStringTableFilters, 0, &num_search_terms);
    if(result == kRocProfVisResultSuccess && num_search_terms > 0)
    {
        out = kRPVDMTableUseCaseEventSearch;
    }
    else
    {
        uint64_t table_type = kRPVControllerTableTypeEvents;
        result = args.GetUInt64(kRPVControllerTableArgsType, 0, &table_type);
        ROCPROFVIS_ASSERT(result == kRocProfVisResultSuccess);
        switch (table_type)
        {
            case kRPVControllerTableTypeEvents:
            case kRPVControllerTableTypeSummaryKernelInstances:
            {
                out = kRPVDMTableUseCaseEventTrackTable;
                break;
            }
            case kRPVControllerTableTypeSamples:
            {
                out = kRPVDMTableUseCaseSampleTrackTable;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidArgument;
                break;
            }
        }
    }    
    return result;
}

rocprofvis_dm_result_t
SystemTable::BuildQuery(rocprofvis_dm_database_t db, TableArguments& args, uint64_t index, uint64_t count, bool count_only, char** out) const
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultUnknownError;
    SystemTableArguments& arguments = (SystemTableArguments&)args;
    char const* sort_column = count_only ? nullptr : (m_columns.size() > arguments.m_sort_column ? m_columns[arguments.m_sort_column].m_name.c_str() : nullptr);
    result = rocprofvis_db_build_table_query(db, arguments.m_use_case, 
                                             static_cast<rocprofvis_dm_timestamp_t>(arguments.m_start_ts), static_cast<rocprofvis_dm_timestamp_t>(arguments.m_end_ts),
                                             static_cast<rocprofvis_db_num_of_tracks_t>(arguments.m_tracks.size()), arguments.m_tracks.data(), 
                                             arguments.m_where.c_str(), arguments.m_filter.c_str(), 
                                             arguments.m_group.c_str(), arguments.m_group_cols.c_str(), 
                                             sort_column, (rocprofvis_dm_sort_order_t)arguments.m_sort_order,
                                             count_only ? 0 : count, count_only ? 0 : index, count_only, out);
    return result;
}

}
}
