// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_table.h"
#include "rocprofvis_controller_arguments.h"
#include "rocprofvis_controller_track.h"
#include "rocprofvis_controller_reference.h"

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
        rocprofvis_dm_slice_t  slice     = nullptr;
        rocprofvis_dm_database_t db    = rocprofvis_dm_get_property_as_handle(dm_handle, kRPVDMDatabaseHandle, 0);
        ROCPROFVIS_ASSERT(db);

        Columns sort_column = m_columns[m_sort_column].m_sort_enum;

        rocprofvis_dm_sort_columns_t sort_enum = (rocprofvis_dm_sort_columns_t)(sort_column <= Columns::LastSortColumn ? sort_column : Columns::Timestamp);

        rocprofvis_dm_result_t dm_result = rocprofvis_db_read_table_slice_async(db, m_start_ts, m_end_ts,
                                             m_tracks.size(), m_tracks.data(),
                                             sort_enum, count, index, object2wait, &slice);

        if(dm_result == kRocProfVisDmResultSuccess)
        {
            dm_result = rocprofvis_db_future_wait(object2wait, UINT64_MAX);
        }
        
        if (dm_result == kRocProfVisDmResultSuccess)
        {
            uint64_t num_records = rocprofvis_dm_get_property_as_uint64(
                slice, kRPVDMNumberOfRecordsUInt64, 0);
            for (uint32_t i = 0; i < num_records; i++)
            {
                double timestamp = (double) rocprofvis_dm_get_property_as_uint64(
                    slice, kRPVDMTimestampUInt64Indexed, i);
                double duration = (double) rocprofvis_dm_get_property_as_int64(
                    slice, kRPVDMEventDurationInt64Indexed, i);

                uint64_t track_id = rocprofvis_dm_get_property_as_uint64(slice, kRPVDMTableTrackIdIndexed, i);

                char* track_cat = rocprofvis_dm_get_property_as_charptr(slice, kRPVDMTableTrackCategoryNameIndexed, i);
                char* track_main = rocprofvis_dm_get_property_as_charptr(slice, kRPVDMTableTrackMainProcessNameIndexed, i);
                char* track_sub = rocprofvis_dm_get_property_as_charptr(slice, kRPVDMTableTrackSubProcessNameIndexed, i);

                uint64_t event_id = 0;
                char* cat = "";
                char* name = "";
                double value = 0;

                if(m_track_type == kRPVControllerTrackTypeEvents)
                {
                    event_id = rocprofvis_dm_get_property_as_uint64(
                        slice, kRPVDMEventIdUInt64Indexed, i);
                    cat = rocprofvis_dm_get_property_as_charptr(
                        slice, kRPVDMEventTypeStringCharPtrIndexed, i);
                    name = rocprofvis_dm_get_property_as_charptr(
                        slice, kRPVDMEventSymbolStringCharPtrIndexed, i);

                    cat = cat == nullptr ? "" : cat;
                    name = name == nullptr ? "" : name;
                }
                else
                {
                    value = rocprofvis_dm_get_property_as_double(slice,
                                                         kRPVDMPmcValueDoubleIndexed, i);
                }

                std::vector<Data> row;
                row.resize(m_columns.size());
                for(uint32_t c = 0; c < m_columns.size(); c++)
                {
                    auto& column = m_columns[c];
                    Data& row_value = row[c];
                    row_value.SetType(m_columns[c].m_type);
                    switch(column.m_sort_enum)
                    {
                        case Columns::Timestamp:
                        {
                            row_value.SetDouble(timestamp);
                            break;
                        }
                        case Columns::EventId:
                        {
                            row_value.SetUInt64(event_id);
                            break;
                        }
                        case Columns::EventOperation:
                        {
                            break;
                        }
                        case Columns::EventDuration:
                        {
                            row_value.SetDouble(duration);
                            break;
                        }
                        case Columns::EventEnd:
                        {
                            row_value.SetDouble(timestamp + duration);
                            break;
                        }
                        case Columns::EventType:
                        {
                            row_value.SetString(cat);
                            break;
                        }
                        case Columns::EventSymbol:
                        {
                            row_value.SetString(name);
                            break;
                        }
                        case Columns::PmcValue:
                        {
                            row_value.SetDouble(value);
                            break;
                        }
                        case Columns::TrackId:
                        {
                            row_value.SetUInt64(track_id);
                            break;
                        }
                        case Columns::TrackName:
                        {
                            std::string track_name = track_cat;
                            track_name += ":";
                            track_name += track_main;
                            track_name += ":";
                            track_name += track_sub;

                            row_value.SetString(track_name.c_str());
                            break;
                        }
                        default:
                        {
                            break;
                        }
                    }
                }

                m_rows[index + i] = row;
            }
        }

        switch (dm_result)
        {
            case kRocProfVisDmResultSuccess:
            {
                result = kRocProfVisResultSuccess;
                break;
            }
            default:
            {
                result = kRocProfVisResultUnknownError;
                break;
            }
        }

        rocprofvis_db_table_slice_free(slice);
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
