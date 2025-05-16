// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_table.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"
#include "rocprofvis_controller_array.h"

namespace RocProfVis
{
namespace Controller
{

typedef Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack> TrackRef;

Table::Table(uint64_t id)
: m_num_items(0)
, m_id(id)
, m_sort_column(0)
, m_sort_order(kRPVControllerSortOrderAscending)
, m_track_type(kRPVControllerTrackTypeEvents)
{
}

Table::~Table()
{
}

rocprofvis_controller_object_type_t Table::GetType(void)
{
	return kRPVControllerObjectTypeTable;
}

void Table::Reset()
{
    m_start_ts = 0;
    m_end_ts = 0;
    m_num_items = 0;
    m_sort_column = 0;
    m_sort_order = kRPVControllerSortOrderAscending;
    m_columns.clear();
    m_rows.clear();
    m_lru.clear();
    m_tracks.clear();
}

rocprofvis_result_t Table::Fetch(rocprofvis_dm_trace_t dm_handle, uint64_t index, uint64_t count, Array& array)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
    if(object2wait)
    {
        rocprofvis_dm_database_t db    = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
        ROCPROFVIS_ASSERT(db);

        Columns sort_column = m_columns[m_sort_column].m_sort_enum;

        rocprofvis_dm_sort_columns_t sort_enum = (rocprofvis_dm_sort_columns_t)(sort_column <= Columns::LastSortColumn ? sort_column : Columns::Timestamp);

        char* fetch_query = nullptr;
        rocprofvis_dm_result_t dm_result = rocprofvis_db_build_table_query(
            db, m_start_ts, m_end_ts, m_tracks.size(), m_tracks.data(), sort_enum, count,
            index, false, &fetch_query);
        if(dm_result == kRocProfVisDmResultSuccess)
        {
            dm_result = rocprofvis_db_execute_query_async(
                db, fetch_query, "Fetch table content", object2wait);
        }

        if(dm_result == kRocProfVisDmResultSuccess)
        {
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
                    dm_handle, kRPVDMTableHandleIndexed, 0);
                if(nullptr != table)
                {
                    char* table_description = rocprofvis_dm_get_property_as_charptr(
                        table, kRPVDMExtTableDescriptionCharPtr, 0);
                    uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(
                        table, kRPVDMNumberOfTableColumnsUInt64, 0);
                    uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(
                        table, kRPVDMNumberOfTableRowsUInt64, 0);
                    num_records = num_rows;
                    if(strcmp(table_description, fetch_query) == 0)
                    {
                        m_columns.clear();
                        m_columns.resize(num_columns);
                        for(int i = 0; i < num_columns; i++)
                        {
                            m_columns[i].m_name = rocprofvis_dm_get_property_as_charptr(
                                table, kRPVDMExtTableColumnNameCharPtrIndexed, i);
                            m_columns[i].m_type = kRPVControllerPrimitiveTypeString;
                            m_columns[i].m_sort_enum = Columns::Timestamp;
                        }
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
                    rocprofvis_dm_delete_all_tables(dm_handle);
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
            Array* row_array = new Array();
            if(row_array)
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
            else
            {
                result = kRocProfVisResultMemoryAllocError;
            }
        }

        rocprofvis_db_future_free(object2wait);
    }
    else
    {
        result = kRocProfVisResultMemoryAllocError;
    }
    return result;
}

rocprofvis_result_t Table::Setup(rocprofvis_dm_trace_t dm_handle, Arguments& args)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    std::vector<ColumnDefintion> columns;
    std::vector<uint32_t> tracks;
    uint64_t sort_column = 0;
    uint64_t sort_order  = 0;
    uint64_t start_index = 0;
    uint64_t start_count = 0;
    uint64_t num_tracks = 0;
    double   end_ts     = 0;
    double   start_ts   = 0;
    rocprofvis_controller_track_type_t track_type = kRPVControllerTrackTypeSamples;
    uint64_t table_type = kRPVControllerTableTypeEvents;
    result = args.GetUInt64(kRPVControllerTableArgsType, 0, &table_type);
    if (result == kRocProfVisResultSuccess)
    {
        switch (table_type)
        {
            case kRPVControllerTableTypeEvents:
            {
                track_type = kRPVControllerTrackTypeEvents;

                ColumnDefintion column;

                column.m_name = "id";
                column.m_type = kRPVControllerPrimitiveTypeUInt64;
                column.m_sort_enum = Columns::EventId;
                columns.push_back(column);
                
                column.m_name = "start_ts";
                column.m_type = kRPVControllerPrimitiveTypeDouble;
                column.m_sort_enum = Columns::Timestamp;
                columns.push_back(column);
                
                column.m_name = "end_ts";
                column.m_type = kRPVControllerPrimitiveTypeDouble;
                column.m_sort_enum = Columns::EventEnd;
                columns.push_back(column);
                
                column.m_name = "duration";
                column.m_type = kRPVControllerPrimitiveTypeDouble;
                column.m_sort_enum = Columns::EventDuration;
                columns.push_back(column);

                column.m_name = "name";
                column.m_type = kRPVControllerPrimitiveTypeString;
                column.m_sort_enum = Columns::EventSymbol;
                columns.push_back(column);
                
                column.m_name = "category";
                column.m_type = kRPVControllerPrimitiveTypeString;
                column.m_sort_enum = Columns::EventType;
                columns.push_back(column);
                
                column.m_name = "track_id";
                column.m_type = kRPVControllerPrimitiveTypeUInt64;
                column.m_sort_enum = Columns::TrackId;
                columns.push_back(column);
                
                column.m_name = "track_name";
                column.m_type = kRPVControllerPrimitiveTypeString;
                column.m_sort_enum = Columns::TrackName;
                columns.push_back(column);

                break;
            }
            case kRPVControllerTableTypeSamples:
            {
                track_type = kRPVControllerTrackTypeSamples;

                ColumnDefintion column;
                
                column.m_name = "timestamp";
                column.m_type = kRPVControllerPrimitiveTypeDouble;
                column.m_sort_enum = Columns::Timestamp;
                columns.push_back(column);
                
                column.m_name = "value";
                column.m_type = kRPVControllerPrimitiveTypeDouble;
                column.m_sort_enum = Columns::PmcValue;
                columns.push_back(column);
                
                column.m_name = "track_id";
                column.m_type = kRPVControllerPrimitiveTypeUInt64;
                column.m_sort_enum = Columns::TrackId;
                columns.push_back(column);
                
                column.m_name = "track_name";
                column.m_type = kRPVControllerPrimitiveTypeString;
                column.m_sort_enum = Columns::TrackName;
                columns.push_back(column);

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
        table_type == kRPVControllerTableTypeSamples))
    {
        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetUInt64(kRPVControllerTableArgsNumTracks, 0, &num_tracks);
        }
        
        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetDouble(kRPVControllerTableArgsStartTime, 0, &start_ts);
        }

        if(result == kRocProfVisResultSuccess)
        {
            result = args.GetDouble(kRPVControllerTableArgsEndTime, 0, &end_ts);
        }

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

    if(result == kRocProfVisResultSuccess)
    {
        result = args.GetUInt64(kRPVControllerTableArgsSortColumn, 0, &sort_column);
    }
    if(result == kRocProfVisResultSuccess)
    {
        result = args.GetUInt64(kRPVControllerTableArgsSortOrder, 0, &sort_order);
    }

    if (result == kRocProfVisResultSuccess)
    {
        rocprofvis_dm_database_t db =
            rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
        ROCPROFVIS_ASSERT(db);

        rocprofvis_db_future_t object2wait = rocprofvis_db_future_alloc(nullptr);
        ROCPROFVIS_ASSERT(object2wait);

        Columns column = columns[sort_column].m_sort_enum;

        rocprofvis_dm_sort_columns_t sort_enum =
            (rocprofvis_dm_sort_columns_t) (column <= Columns::LastSortColumn
                                                ? column
                                                : Columns::Timestamp);

        char*                  count_query = nullptr;
        rocprofvis_dm_result_t dm_result   = rocprofvis_db_build_table_query(
            db, start_ts, end_ts, tracks.size(), tracks.data(), sort_enum, 0,
            0, true, &count_query);
        if(dm_result == kRocProfVisDmResultSuccess)
        {
            dm_result = rocprofvis_db_execute_query_async(
                db, count_query, "Calculate table count", object2wait);
        }

        if(dm_result == kRocProfVisDmResultSuccess)
        {
            dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
            rocprofvis_db_future_free(object2wait);
            object2wait = rocprofvis_db_future_alloc(nullptr);
        }

        if(dm_result == kRocProfVisDmResultSuccess)
        {
            dm_result           = kRocProfVisDmResultUnknownError;
            uint64_t num_tables = rocprofvis_dm_get_property_as_uint64(
                dm_handle, kRPVDMNumberOfTablesUInt64, 0);
            if(num_tables > 0)
            {
                rocprofvis_dm_table_t table = rocprofvis_dm_get_property_as_handle(
                    dm_handle, kRPVDMTableHandleIndexed, 0);
                if(nullptr != table)
                {
                    char* table_description = rocprofvis_dm_get_property_as_charptr(
                        table, kRPVDMExtTableDescriptionCharPtr, 0);
                    uint64_t num_columns = rocprofvis_dm_get_property_as_uint64(
                        table, kRPVDMNumberOfTableColumnsUInt64, 0);
                    uint64_t num_rows = rocprofvis_dm_get_property_as_uint64(
                        table, kRPVDMNumberOfTableRowsUInt64, 0);
                    if(strcmp(table_description, count_query) == 0 && num_columns == 1 &&
                       num_rows == 1)
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
                                uint64_t num_cells = rocprofvis_dm_get_property_as_uint64(
                                    table_row, kRPVDMNumberOfTableRowCellsUInt64, 0);
                                if(num_cells == 1)
                                {
                                    std::string value =
                                        rocprofvis_dm_get_property_as_charptr(
                                            table_row,
                                            kRPVDMExtTableRowCellValueCharPtrIndexed, 0);
                                    m_num_items = std::stoull(value);
                                    dm_result   = kRocProfVisDmResultSuccess;
                                }
                            }
                        }
                    }
                    rocprofvis_dm_delete_all_tables(dm_handle);
                }
            }
        }

        if(count_query)
        {
            free(count_query);
        }
    }

    if (result == kRocProfVisResultSuccess)
    {
        Reset();
        m_columns = columns;
        m_tracks  = tracks;
        m_sort_column = sort_column;
        m_sort_order  = (rocprofvis_controller_sort_order_t)sort_order;
        m_start_ts = start_ts;
        m_end_ts = end_ts;
        m_track_type = track_type;
    }

    return result;
}

rocprofvis_result_t Table::GetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t* value)
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
            case kRPVControllerTableColumnHeaderIndexed:
            case kRPVControllerTableRowHeaderIndexed:
            case kRPVControllerTableRowIndexed:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Table::GetDouble(rocprofvis_property_t property, uint64_t index, double* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t Table::GetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t** value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t Table::GetString(rocprofvis_property_t property, uint64_t index, char* value, uint32_t* length)
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
                    if(!value && length)
                    {
                        *length = m_columns[index].m_name.size();
                        result  = kRocProfVisResultSuccess;
                    }
                    else if (value && length)
                    {
                        strncpy(value, m_columns[index].m_name.c_str(), *length);
                        result = kRocProfVisResultSuccess;
                    }
                }
                break;
            }
            case kRPVControllerTableId:
            case kRPVControllerTableNumColumns:
            case kRPVControllerTableNumRows:
            case kRPVControllerTableColumnTypeIndexed:
            case kRPVControllerTableRowHeaderIndexed:
            case kRPVControllerTableRowIndexed:
            {
                result = kRocProfVisResultInvalidType;
                break;
            }
            default:
            {
                result = kRocProfVisResultInvalidEnum;
                break;
            }
        }
    }
    return result;
}

rocprofvis_result_t Table::SetUInt64(rocprofvis_property_t property, uint64_t index, uint64_t value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t Table::SetDouble(rocprofvis_property_t property, uint64_t index, double value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t Table::SetObject(rocprofvis_property_t property, uint64_t index, rocprofvis_handle_t* value)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

rocprofvis_result_t Table::SetString(rocprofvis_property_t property, uint64_t index, char const* value, uint32_t length)
{
    rocprofvis_result_t result = kRocProfVisResultInvalidArgument;
    return result;
}

}
}
