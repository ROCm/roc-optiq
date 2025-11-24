// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_table_system.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_array.h"
#include "rocprofvis_controller_future.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;

SystemTable::SystemTable(uint64_t id)
: Table(id, __kRPVControllerTablePropertiesFirst, __kRPVControllerTablePropertiesLast)
, m_summary(false)
, m_start_ts(0)
, m_end_ts(0)
, m_track_type(kRPVControllerTrackTypeEvents)
{
}

SystemTable::~SystemTable()
{
}

void SystemTable::Reset()
{
    m_start_ts = 0;
    m_end_ts = 0;
    m_num_items = 0;
    m_sort_column = 0;
    m_sort_order = kRPVControllerSortOrderAscending;
    m_summary = false;
    m_columns.clear();
    m_rows.clear();
    m_tracks.clear();
    m_filter.clear();
    m_group.clear();
    m_group_cols.clear();
    m_string_table_filters.clear();
    m_string_table_filters_ptr.clear();
}

rocprofvis_result_t SystemTable::Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count, Array& array, Future* future)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
    if(object2wait)
    {
        rocprofvis_dm_database_t db    = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
        ROCPROFVIS_ASSERT(db);

        char const* sort_column = m_columns.size() > m_sort_column ? m_columns[m_sort_column].m_name.c_str() : nullptr;

        char* fetch_query = nullptr;
        rocprofvis_dm_result_t dm_result = rocprofvis_db_build_table_query(
            db, m_start_ts, m_end_ts, m_tracks.size(), m_tracks.data(), m_filter.c_str(), m_group.c_str(), m_group_cols.c_str(), sort_column,
            (rocprofvis_dm_sort_order_t)m_sort_order, m_string_table_filters_ptr.size(), m_string_table_filters_ptr.data(), 
            count, index, false, m_summary, &fetch_query);
        rocprofvis_dm_table_id_t table_id = 0;
        if(dm_result == kRocProfVisDmResultSuccess)
        {
            dm_result = rocprofvis_db_execute_query_async(
                db, fetch_query, "Fetch table content", object2wait, &table_id);
        }

        if(dm_result == kRocProfVisDmResultSuccess)
        {
            future->AddDependentFuture(object2wait);
            dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
        }

        uint64_t num_records = 0;

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

                                        auto& column    = m_columns[j];
                                        Data& row_value = row[j];
                                        row_value.SetType(m_columns[j].m_type);
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
                    rocprofvis_dm_delete_table_at(dm_handle, table_id);
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
                try
                {
                    Array* row_array = new Array();
                    {
                        auto& row_vec = row_array->GetVector();
                        row_vec.resize(m_rows[i].size());
                        for (uint32_t j = 0; j < m_rows[i].size(); j++)
                        {
                            row_vec[j].SetType(m_rows[i][j].GetType());
                            row_vec[j] = m_rows[i][j];
                        }
                        result = array.SetObject(kRPVControllerArrayEntryIndexed, i - index,
                                        (rocprofvis_handle_t*)row_array);
                    }
                }
                catch(const std::exception&)
                {
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
    
    QueryArguments query_args;
    result = future->IsCancelled() ? kRocProfVisResultCancelled : UnpackArguments(args, query_args);

    if (result == kRocProfVisResultSuccess)
    {
        m_sort_column = query_args.m_sort_column;
        m_sort_order  = (rocprofvis_controller_sort_order_t)query_args.m_sort_order;
        if(m_tracks.size() == query_args.m_tracks.size() && m_start_ts == query_args.m_start_ts &&
           m_end_ts == query_args.m_end_ts && m_filter == query_args.m_filter && m_group == query_args.m_group &&
           m_group_cols == query_args.m_group_cols && m_string_table_filters == query_args.m_string_table_filters &&
           m_summary == query_args.m_summary)
        {
            bool tracks_all_same = true;
            for (int i = 0; i < query_args.m_tracks.size(); i++)
            {
                if(m_tracks[i] != query_args.m_tracks[i])
                {
                    tracks_all_same = false;
                    break;
                }
            }
            if(tracks_all_same)
            {
                m_rows.clear();
                return kRocProfVisResultSuccess;
            }
        }
        Reset();

        m_tracks      = query_args.m_tracks;       
        m_start_ts    = query_args.m_start_ts;
        m_end_ts      = query_args.m_end_ts;
        m_track_type  = query_args.m_track_type;
        m_filter      = query_args.m_filter;
        m_group       = query_args.m_group;
        m_group_cols  = query_args.m_group_cols;
        m_string_table_filters = query_args.m_string_table_filters;
        for(const std::string& filter : m_string_table_filters)
        {
            m_string_table_filters_ptr.push_back(filter.c_str());
        }
        m_summary = query_args.m_summary;

        if(result == kRocProfVisResultSuccess)
        {
            rocprofvis_dm_database_t db =
                rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
            ROCPROFVIS_ASSERT(db);

            rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
            ROCPROFVIS_ASSERT(object2wait);

            char*                  count_query = nullptr;
            rocprofvis_dm_result_t dm_result   = rocprofvis_db_build_table_query(
                db, m_start_ts, m_end_ts, m_tracks.size(), m_tracks.data(), m_filter.c_str(), m_group.c_str(), m_group_cols.c_str(), nullptr,
                (rocprofvis_dm_sort_order_t) m_sort_order, m_string_table_filters_ptr.size(), m_string_table_filters_ptr.data(), 
                0, 0, true, m_summary, &count_query);
            rocprofvis_dm_table_id_t table_id = 0;
            if(dm_result == kRocProfVisDmResultSuccess)
            {
                dm_result = rocprofvis_db_execute_query_async(
                    db, count_query, "Calculate table count", object2wait, &table_id);
            }

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
                                        m_columns[i-1].m_type =
                                            kRPVControllerPrimitiveTypeString;
                                    }
                                }
                            }
                        }
                        rocprofvis_dm_delete_table_at(dm_handle, table_id);
                    }
                }
            }

            if(count_query)
            {
                free(count_query);
            }
        }
    }
    if(future->IsCancelled())
    {
        Reset();
    }
    return result;
}

rocprofvis_result_t SystemTable::ExportCSV(rocprofvis_dm_trace_t dm_handle, Arguments& args, Future* future, const char* path) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    
    QueryArguments query_args;
    result = future->IsCancelled() ? kRocProfVisResultCancelled : UnpackArguments(args, query_args);

    if (result == kRocProfVisResultSuccess)
    {
        rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
        if(object2wait)
        {
            rocprofvis_dm_database_t db    = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
            ROCPROFVIS_ASSERT(db);

            char const* sort_column = m_columns.size() > query_args.m_sort_column ? m_columns[query_args.m_sort_column].m_name.c_str() : nullptr;
            std::vector<const char*> string_table_filters_ptr;
            for(const std::string& filter : query_args.m_string_table_filters)
            {
                string_table_filters_ptr.push_back(filter.c_str());
            }

            char* query = nullptr;
            rocprofvis_dm_result_t dm_result = rocprofvis_db_build_table_query(
                db, query_args.m_start_ts, query_args.m_end_ts, (rocprofvis_db_num_of_tracks_t)query_args.m_tracks.size(), query_args.m_tracks.data(), query_args.m_filter.c_str(), query_args.m_group.c_str(), query_args.m_group_cols.c_str(), sort_column,
                (rocprofvis_dm_sort_order_t)query_args.m_sort_order, (rocprofvis_dm_num_string_table_filters_t)string_table_filters_ptr.size(), string_table_filters_ptr.data(), 
                0, 0, false, query_args.m_summary, &query);
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
        }
        else
        {
            result = kRocProfVisResultMemoryAllocError;
        }
    }
    return result;
}

rocprofvis_result_t SystemTable::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
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
                *value = m_num_items;
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

rocprofvis_result_t SystemTable::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    if(length)
    {
        switch(property)
        {
            case kRPVControllerTableColumnHeaderIndexed:
            {
                if (index < m_columns.size())
                {
                    result = GetStdStringImpl(value, length, m_columns[index].m_name);
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
SystemTable::UnpackArguments(Arguments& args, QueryArguments& out) const
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    std::vector<uint32_t> tracks;
    uint64_t sort_column = 0;
    uint64_t sort_order  = 0;
    uint64_t num_tracks = 0;
    uint64_t num_op_types = 0;
    double   end_ts     = 0;
    double   start_ts   = 0;
    std::string filter;
    std::string group;
    std::string group_cols;
    uint64_t num_string_table_filters = 0;
    std::vector<std::string> string_table_filters;
    std::vector<const char*> string_table_filters_ptr;
    bool summary = false;
    rocprofvis_controller_track_type_t track_type = kRPVControllerTrackTypeSamples;
    uint64_t table_type = kRPVControllerTableTypeEvents;

    result = args.GetUInt64(kRPVControllerTableArgsType, 0, &table_type);
    if (result == kRocProfVisResultSuccess)
    {
        switch (table_type)
        {
            case kRPVControllerTableTypeEvents:
            case kRPVControllerTableTypeSearchResults:
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
                result = kRocProfVisResultInvalidArgument;
                break;
            }
        }
    }

    if(result == kRocProfVisResultSuccess &&
       (table_type == kRPVControllerTableTypeEvents ||
        table_type == kRPVControllerTableTypeSamples ||
        table_type == kRPVControllerTableTypeSearchResults))
    {
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
            result = args.GetUInt64(kRPVControllerTableArgsNumOpTypes, 0, &num_op_types);
        }
        
        if(result == kRocProfVisResultSuccess && num_op_types == 0)
        {
            result = args.GetUInt64(kRPVControllerTableArgsNumTracks, 0, &num_tracks);

            if (result == kRocProfVisResultSuccess && num_tracks > 0)
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
        }
        else
        {
            for (uint32_t i = 0; i < num_op_types && (result == kRocProfVisResultSuccess); i++)
            {
                uint64_t op_type_uint64 = kRocProfVisDmOperationNoOp;
                result = args.GetUInt64(kRPVControllerTableArgsOpTypesIndexed, i, &op_type_uint64);
                if(result == kRocProfVisResultSuccess)
                {
                    ROCPROFVIS_ASSERT(op_type_uint64 < kRocProfVisDmNumOperation);
                    tracks.push_back(TABLE_QUERY_PACK_OP_TYPE(op_type_uint64));
                }                
            }
        }
    }

    if(result == kRocProfVisResultSuccess)
    {
        result = args.GetUInt64(kRPVControllerTableArgsSortColumn, 0, &sort_column);
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = args.GetUInt64(kRPVControllerTableArgsSortOrder, 0, &sort_order);
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
    if(num_op_types > 0 && result == kRocProfVisResultSuccess)
    {
        result = args.GetUInt64(kRPVControllerTableArgsNumStringTableFilters, 0, &num_string_table_filters);
        if(result == kRocProfVisResultSuccess)
        {
            for (uint32_t i = 0; i < num_string_table_filters && (result == kRocProfVisResultSuccess); i++)
            {
                uint32_t length = 0;
                result = args.GetString(kRPVControllerTableArgsStringTableFiltersIndexed, i, nullptr, &length);
                if(result == kRocProfVisResultSuccess && length > 0)
                {
                    std::string f;
                    f.resize(length);
                    result = args.GetString(kRPVControllerTableArgsStringTableFiltersIndexed, i, f.data(), &length);
                    if(result == kRocProfVisResultSuccess)
                    {
                        string_table_filters.push_back(f);
                    }
                }
            }
        }
    }
    if(result == kRocProfVisResultSuccess)
    {
        uint64_t summary_uint64 = 0;
        result = args.GetUInt64(kRPVControllerTableArgsSummary, 0, &summary_uint64);
        if(result == kRocProfVisResultSuccess)
        {
            summary = (bool)summary_uint64;
        }
    }

    out = {filter, group, group_cols, sort_column, (rocprofvis_controller_sort_order_t)sort_order, tracks, track_type, std::move(string_table_filters), summary, start_ts, end_ts};
    
	return result;
}

}
}
