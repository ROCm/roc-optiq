// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include "rocprofvis_db_sqlite.h"

namespace RocProfVis
{
namespace DataModel
{

typedef enum rocprofvis_db_async_tracks_flags_t
{
    kRocProfVisDmIncludePmcTracks = 1,
    kRocProfVisDmIncludeStreamTracks = 2
} rocprofvis_db_async_tracks_flags_t;

typedef struct rocprofvis_db_event_level_t
{
    uint64_t id;
    uint8_t  level_for_queue;
    uint8_t  level_for_stream;
} rocprofvis_db_event_level_t;

typedef struct rocprofvis_db_sqlite_trim_parameters
{
    // Table names as we can't issue recursively
    std::map<std::string, std::string> tables;
} rocprofvis_db_sqlite_trim_parameters;

// class for methods and members common for all RocPd-based schemas
class ProfileDatabase : public SqliteDatabase
{
    public:
        // Database constructor
        // @param path - full path to database file
        ProfileDatabase( rocprofvis_db_filename_t path) : 
                        SqliteDatabase(path), 
                        m_symbols_offset(0){};
        // ProfileDatabase destructor, must be defined as virtual to free resources of derived classes 
        virtual ~ProfileDatabase() {}
        // worker method to read time slice
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param num - number of tracks
        // @param tracks - uint32_t array with track IDs  
        // @param object - future object providing asynchronous execution mechanism   
        // @return status of operation        
        rocprofvis_dm_result_t  ReadTraceSlice(
                        rocprofvis_dm_timestamp_t start,
                        rocprofvis_dm_timestamp_t end,
                        rocprofvis_db_num_of_tracks_t num,
                        rocprofvis_db_track_selection_t tracks,
                        Future* object) override;
        // worker method to execute database query
        // @param query - database query 
        // @param description - database description
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ExecuteQuery(
                        rocprofvis_dm_charptr_t query,
                        rocprofvis_dm_charptr_t description,
                        Future* future) override; 
        // method to detect rocpd-based database type (rocpd vs rocprof)
        // @param filename - full path to database file
        static rocprofvis_db_type_t Detect(rocprofvis_db_filename_t filename);

    private:

    // sqlite3_exec callback to add any record (Event or PMC) to time slice container. Used in all-selected-tracks time slice query
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names  
    // @return SQLITE_OK if successful
        static int CallbackAddAnyRecord(void* data, int argc, sqlite3_stmt* stmt, char** azColName);

    protected:

    // method to build a query to read time slice of records for single track 
    // @param index - track index 
    // @param tyte - query type
    // @param query - reference to output query string  
    // @return status of operation
        rocprofvis_dm_result_t BuildTrackQuery(
                            rocprofvis_dm_index_t index,
                            rocprofvis_dm_index_t   type,
                            rocprofvis_dm_string_t& query) override;
    // method to build a query to read time slice of records for all tracks in one shot 
    // @param start - start timestamp of time slice 
    // @param end - end timestamp of time slice 
    // @param num - number of tracks
    // @param tracks - uint32_t array with track IDs 
    // @param query - reference to query string 
    // @param slices - reference map array for storing slice handlers for multi-track request   
    // @return status of operation  
        rocprofvis_dm_result_t BuildSliceQuery(
                            rocprofvis_dm_timestamp_t start,
                            rocprofvis_dm_timestamp_t end,
                            rocprofvis_db_num_of_tracks_t num,
                            rocprofvis_db_track_selection_t tracks,
                            rocprofvis_dm_string_t& query,
                            slice_array_t& slices) override;

        rocprofvis_dm_result_t BuildTableQuery(
                            rocprofvis_dm_timestamp_t start, 
                            rocprofvis_dm_timestamp_t end,
                            rocprofvis_db_num_of_tracks_t num,
                            rocprofvis_db_track_selection_t tracks,
                            rocprofvis_dm_charptr_t filter,
                            rocprofvis_dm_charptr_t group,
                            rocprofvis_dm_charptr_t group_cols, 
                            rocprofvis_dm_charptr_t sort_column, 
                            rocprofvis_dm_sort_order_t sort_order,
                            uint64_t max_count, 
                            uint64_t offset,
                            bool count_only, 
                            rocprofvis_dm_string_t& query) override;

        rocprofvis_dm_result_t ExecuteQueryForAllTracksAsync(
                            uint32_t flags, 
                            rocprofvis_dm_index_t query_type,
                            rocprofvis_dm_charptr_t prefix, 
                            rocprofvis_dm_charptr_t suffix,
                            RpvSqliteExecuteQueryCallback callback, 
                            std::function<void(rocprofvis_dm_track_params_t*)> func_clear);

    protected:
    // sqlite3_exec callback to add flowtrace record to FlowTrace container.
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names  
    // @return SQLITE_OK if successful
        static int CallbackAddFlowTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName);
    // sqlite3_exec callback to add extended info record ExtData container.
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names 
    // @return SQLITE_OK if successful
        static int CallbackAddExtInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName);

    // sqlite3_exec callback to calculate graph level for an event and store it into trace object map array
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names  
    // @return SQLITE_OK if successful
       static int CalculateEventLevels(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
    // sqlite3_exec callback to collect minimum/maximum timestamps and minimu/maximum value/level
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
        static int CallbackGetTrackProperties(void* data, int argc, sqlite3_stmt* stmt,
                                              char** azColName);
       // sqlite3_exec callback to collect number of records in te track
       // @param data - pointer to callback caller argument
       // @param argc - number of columns in the query
       // @param argv - pointer to row values
       // @param azColName - pointer to column names
       // @return SQLITE_OK if successful
        static int CallbackGetTrackRecordsCount(void* data, int argc, sqlite3_stmt* stmt,
                                                char** azColName);
        static int CallbackTrimTableQuery(void* data, int argc, sqlite3_stmt* stmt,
                                          char** azColName);
    protected:
    // offset of kernel symbols in string table
        uint32_t m_symbols_offset;
        std::vector<rocprofvis_db_event_level_t> m_event_levels[kRocProfVisDmNumOperation];
        std::unordered_map<uint64_t, size_t> m_event_levels_id_to_index[kRocProfVisDmNumOperation];
        std::mutex   m_level_lock;
};

}  // namespace DataModel
}  // namespace RocProfVis