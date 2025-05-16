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


typedef struct
{
    uint64_t process_id[NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS];
    uint64_t start_time;
    uint64_t end_time;
    uint32_t level;
} rocprofvis_event_timing_params_t;

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

        rocprofvis_dm_result_t  ReadTableSlice(
                        rocprofvis_dm_timestamp_t start,
                        rocprofvis_dm_timestamp_t end,
                        rocprofvis_db_num_of_tracks_t num,
                        rocprofvis_db_track_selection_t tracks,
                        rocprofvis_dm_sort_columns_t sort_column,
                        uint64_t max_count,
                        uint64_t offset,
                        Future* object, rocprofvis_dm_slice_t* output_slice) override;

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

    // sqlite3_exec callback to add event record to time slice container. Used in per-track time slice query
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names  
    // @return SQLITE_OK if successful
        static int CallbackAddEventRecord(void *data, int argc, char **argv, char **azColName);
    // sqlite3_exec callback to add PMC record to time slice container. Used in per-track time slice query
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names 
    // @return SQLITE_OK if successful 
        static int CallbackAddPmcRecord(void *data, int argc, char **argv, char **azColName);
    // sqlite3_exec callback to add any record (Event or PMC) to time slice container. Used in all-selected-tracks time slice query
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names  
    // @return SQLITE_OK if successful
        static int CallbackAddAnyRecord(void* data, int argc, char** argv, char** azColName);

        static int CallbackAddAnyTableRecord(void* data, int argc, char** argv, char** azColName);

    // method to build a query to read time slice of records for single track 
    // @param index - track index 
    // @param query - reference to query string  
    // @return status of operation
        rocprofvis_dm_result_t BuildTrackQuery(
                            rocprofvis_dm_index_t index,
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
            rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
            rocprofvis_db_num_of_tracks_t num, rocprofvis_db_track_selection_t tracks,
            rocprofvis_dm_sort_columns_t sort_column, uint64_t max_count, uint64_t offset,
            bool count_only, rocprofvis_dm_string_t& query) override;

    protected:
    // sqlite3_exec callback to add flowtrace record to FlowTrace container.
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names  
    // @return SQLITE_OK if successful
        static int CallbackAddFlowTrace(void *data, int argc, char **argv, char **azColName);
    // sqlite3_exec callback to add extended info record ExtData container.
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names 
    // @return SQLITE_OK if successful
        static int CallbackAddExtInfo(void* data, int argc, char** argv, char** azColName);

    // sqlite3_exec callback to calculate graph level for an event and store it into trace object map array
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names  
    // @return SQLITE_OK if successful
       static int CalculateEventLevels(void* data, int argc, char** argv, char** azColName);
    protected:
    // offset of kernel symbols in string table
        uint32_t m_symbols_offset;

    private:
    // vector array to keep current events stack
        std::vector<rocprofvis_event_timing_params_t> m_event_timing_params;
};

}  // namespace DataModel
}  // namespace RocProfVis