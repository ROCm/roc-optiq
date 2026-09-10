// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_sqlite.h"
#include "sqlite3.h" 
#include <set>
#include <mutex>
#include <condition_variable>
#include "rocprofvis_db_query_builder.h"
#include "rocprofvis_db_table_processor.h"

namespace RocProfVis
{
namespace DataModel
{

#define MAX_CONNECTIONS 100
#define SINGLE_THREAD_RECORDS_COUNT_LIMIT 50000
#define NO_THREAD_RECORDS_COUNT_LIMIT 1000

typedef enum rocprofvis_db_query_type_t
{
    kRPVRocpdQuerySliceByQueue=0,
    kRPVPerfettoQuerySlice=0,
    kRPVRocpdQuerySliceByStream=1,
    kRPVRocpdQueryTable=2,
    kRPVPerfettoQueryTable=2,
    kRPVRocpdQueryLevel=3,
    kRPVRocpdQuerySliceByTrackSliceQuery=5,
} rocprofvis_db_query_type_t;

// type of sqlite3_exec callback function
typedef int (*RpvSqliteExecuteQueryCallback)(void*, int, sqlite3_stmt*, char**);


typedef enum rocprofvis_db_sqlite_query_type_t
{
    kRPVSourceQueryGeneric = 0,
    kRPVSourceQueryTrackByQueue = 0,
    kRPVSourceQueryTrackByStream = 1,
    kRPVCacheTableName = 1,
    kRPVSourceQueryLevel = 2,
    kRPVSourceQuerySliceByQueue = 3,
    kRPVSourceQuerySliceByStream = 4,
    kRPVSourceQueryTable = 5,
    kRPVNumSourceQueryTypes = 6
} rocprofvis_db_sqlite_query_type_t;

typedef enum rocprofvis_dm_track_search_id_t
{
    kRPVTrackSearchIdThreads,
    kRPVTrackSearchIdThreadSamples,
    kRPVTrackSearchIdDispatches,
    kRPVTrackSearchIdMemAllocs,
    kRPVTrackSearchIdMemCopies,
    kRPVTrackSearchIdCounters,
    kRPVTrackSearchIdStreams,
    kRPVTrackSearchIdUnknown,
} rocprofvis_dm_track_search_id_t;

typedef enum rocprofvis_db_compound_table_type {
    kRPVTableDataTypeEvent,
    kRPVTableDataTypeSample,
    kRPVTableDataTypeSearch,
    kRPVTableDataTypesNum
} rocprofvis_db_compound_table_type;

typedef std::map<uint64_t, std::map<std::string, rocprofvis_event_data_category_enum_t>> rocprofvis_event_data_category_map_t;

// class for any Sqlite database methods and properties 
class QueryManager : public SqliteDatabase
{
    friend class TableProcessor;
    friend class PackedTable;
    friend class TrackLookup;
    public:
        // Database constructor
        // @param path - full path to database file
        QueryManager( rocprofvis_db_filename_t path, RpvSqliteExecuteQueryCallback callback_add_any_record) : 
                        SqliteDatabase(path), m_callback_add_any_record(callback_add_any_record),
            m_table_processor{TableProcessor(this),TableProcessor(this),TableProcessor(this)} {};
        // SqliteDatabase destructor, must be defined as virtual to free resources of derived classes 
        virtual ~QueryManager() {};

        // worker method to execute database query
        // @param query - database query 
        // @param description - database description
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ExecuteQuery(
            rocprofvis_dm_charptr_t query,
            rocprofvis_dm_charptr_t description,
            Future* future) override; 

        // method to execute table database query with appropriate .CSV writer callback based on existence of GROUP BY clause
        // @param query - database query 
        // @param file_path output path to write .CSV
        // @param future - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t ExportTableCSV(
            rocprofvis_dm_charptr_t query,
            rocprofvis_dm_charptr_t file_path,
            Future* future) override;

    protected:
        // ----------------------------------Query builders------------------------------------------
        // 
        // method to build a query to read time slice of records for single track 
        // @param index - track index 
        // @param type - query type
        // @param query - reference to output query string  
        // @return status of operation
        virtual rocprofvis_dm_result_t  BuildTrackQuery(           
            rocprofvis_dm_index_t index, 
            rocprofvis_dm_index_t type,
            rocprofvis_dm_string_t & query,
            uint32_t split_count,
            uint32_t split_index) = 0;

        // method to build a query to read time slice of records for timeline view 
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param num - number of tracks
        // @param tracks - uint32_t array with track IDs 
        // @param query - reference to query string 
        // @param slices - reference map array for storing slice handlers for multi-track request   
        // @return status of operation                                                      
        virtual rocprofvis_dm_result_t  BuildSliceQuery(      
            rocprofvis_dm_timestamp_t start, 
            rocprofvis_dm_timestamp_t end, 
            rocprofvis_db_num_of_tracks_t num, 
            rocprofvis_db_track_selection_t tracks, 
            rocprofvis_dm_string_t& query, 
            slice_array_t& slices);


        rocprofvis_dm_result_t BuildCompoundQuery(rocprofvis_dm_table_use_case_enum_t use_case,
            rocprofvis_dm_timestamp_t start, 
            rocprofvis_dm_timestamp_t end,
            rocprofvis_db_num_of_tracks_t num,
            rocprofvis_db_track_selection_t tracks,
            std::vector<slice_query_map_t>& slice_query_map_array,
            rocprofvis_dm_charptr_t where,
            rocprofvis_dm_charptr_t filter,
            rocprofvis_dm_charptr_t group,
            rocprofvis_dm_charptr_t group_cols, 
            rocprofvis_dm_charptr_t sort_column, 
            rocprofvis_dm_sort_order_t sort_order,
            uint64_t max_count, 
            uint64_t offset,
            bool count_only,
            rocprofvis_dm_string_t& query);

        // method to build a query to read time slice of records for table view 
        // @param use_case - the method is multi-use, this is enumeration of use cases
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param num - number of tracks
        // @param tracks - uint32_t array with track IDs 
        // @param where - where clause 
        // @param filter - filter clause 
        // @param group - aggregation clause 
        // @param group_cols - group by columns
        // @param sort_column - sort by column
        // @param sort_order - sort order
        // @param max_count - rows limit
        // @param offset - start row
        // @param count only - retrieve only rows count
        // @return status of operation 

        rocprofvis_dm_result_t BuildTableQuery(
            rocprofvis_dm_table_use_case_enum_t use_case,
            rocprofvis_dm_timestamp_t start, 
            rocprofvis_dm_timestamp_t end,
            rocprofvis_db_num_of_tracks_t num,
            rocprofvis_db_track_selection_t tracks,
            rocprofvis_dm_charptr_t where,
            rocprofvis_dm_charptr_t filter,
            rocprofvis_dm_charptr_t group,
            rocprofvis_dm_charptr_t group_cols, 
            rocprofvis_dm_charptr_t sort_column, 
            rocprofvis_dm_sort_order_t sort_order,
            uint64_t max_count, 
            uint64_t offset,
            bool count_only,
            rocprofvis_dm_string_t& query) override;

        // method to build a query to read time slice of records for event search 
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param num - number of tracks
        // @param ops - uint32_t array with track IDs 
        // @param where - where clause 
        // @param sort_column - sort by column
        // @param sort_order - sort order
        // @param num_string_table_filters - number of search string parameters   
        // @param string_table_filters - search string parameters 
        // @param max_count - rows limit
        // @param offset - start row
        // @param count only - retrieve only rows count
        // @return status of operation 

        rocprofvis_dm_result_t BuildEventSearchQuery(
            rocprofvis_dm_timestamp_t start, 
            rocprofvis_dm_timestamp_t end,
            rocprofvis_db_num_of_tracks_t num,
            rocprofvis_db_track_selection_t ops,
            rocprofvis_dm_charptr_t where,
            rocprofvis_dm_num_string_table_filters_t num_string_table_filters, 
            rocprofvis_dm_string_table_filters_t string_table_filters,
            bool include_substring,
            bool include_category,
            bool partial_matching,
            rocprofvis_dm_charptr_t sort_column,
            rocprofvis_dm_sort_order_t sort_order,
            uint64_t max_count, 
            uint64_t offset,
            bool count_only,
            rocprofvis_dm_string_t& query) override;

        // Counter sample singe timestamp has to be converted to start/end timestamps using neighbour samples
        // Builds query for retrieving first counter sample start timstamp
        rocprofvis_dm_result_t BuildCounterSliceLeftNeighbourQuery(
            rocprofvis_dm_timestamp_t start, 
            rocprofvis_dm_timestamp_t end, 
            rocprofvis_dm_index_t track_index, 
            rocprofvis_dm_string_t& query);
        // Counter sample singe timestamp has to be converted to start/end timestamps using neighbour samples
        // Builds query for retrieving last counter sample end timstamp
        rocprofvis_dm_result_t BuildCounterSliceRightNeighbourQuery(
            rocprofvis_dm_timestamp_t start, 
            rocprofvis_dm_timestamp_t end, 
            rocprofvis_dm_index_t track_index, 
            rocprofvis_dm_string_t& query);

        // builds query map based on track identifiers for slice query
        virtual void BuildSliceQueryMap(
            slice_query_map_t& slice_query_map, 
            rocprofvis_dm_track_params_t* props,
            rocprofvis_db_query_type_t query_type) = 0;

        // Searches for strings matching the passed in list of filter strings and builds a WHERE IN clause for the table query.
        // @param num_string_table_filters - number of filter strings
        // @param string_table_filters - array of filter strings
        // @param include_substring - when true a string matches if it contains the filter, when false it has to equal the filter.
        // @param include_category - when true the filters are matched against the event category as well as the event name, when false only against the event name.
        // @param partial_matching - when true a string matches if it matches any of the filters, when false it has to match all of them.
        // @param filter - output string containing WHERE clause
        // @return status of operation
        virtual rocprofvis_dm_result_t BuildTableStringIdFilter(
            rocprofvis_dm_num_string_table_filters_t num_string_table_filters, 
            rocprofvis_dm_string_table_filters_t string_table_filters,
            bool include_substring,
            bool include_category,
            bool partial_matching,
            table_string_id_filter_map_t& filter) = 0;

        // needs to be overriden in all adapters. Used by public interface method. 
        rocprofvis_dm_result_t BuildComputeQuery(
            rocprofvis_db_compute_use_case_enum_t use_case, rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params,
            rocprofvis_dm_string_t& query) override {
            (void) use_case;
            (void) num;
            (void) params;
            (void) query;
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Systems database does not build compute query", kRocProfVisDmResultNotSupported);
        }

        // Get prefix an suffix part of histogram calculation query
        std::string GetHistogramQueryPrefix(uint64_t bucket_size);
        std::string GetHistogramQuerySuffix();

        // Get table view query per operation
        virtual rocprofvis_dm_string_t GetEventOperationQuery(
            const rocprofvis_dm_event_operation_t operation) = 0;

        // ---------------------------------- Slice readers------------------------------------------
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
            rocprofvis_dm_hashed_timestamp_tag_t tag,
            rocprofvis_db_num_of_tracks_t num,
            rocprofvis_db_track_selection_t tracks,
            Future* object) override;
        // worker method to read PMC time slice
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param track - track ID
        // @param left_neighbor - include the left neighbor of the time range
        // @param right_neighbor - include the right neighbor of the time range 
        // @param object - future object providing asynchronous execution mechanism   
        // @return status of operation 
        rocprofvis_dm_result_t  ReadTracePMCSlice(
            rocprofvis_dm_timestamp_t start,
            rocprofvis_dm_timestamp_t end,
            rocprofvis_dm_hashed_timestamp_tag_t tag,
            rocprofvis_db_track_selection_t track,
            bool left_neighbor,
            bool right_neighbor,
            Future* object) override;

        // ------------------------------SQL query callbacks-----------------------------------
        // @param data - pointer to callback caller argument
        // @param argc - number of columns in the query
        // @param argv - pointer to row values
        // @param azColName - pointer to column names  
        // @return SQLITE_OK if successful
        static int CallbackGetValue(void* data, int argc, sqlite3_stmt* stmt, char** azColName);  
        static int CallbackRunQuery(void *data, int argc, sqlite3_stmt* stmt, char **azColName); 
        static int CallbackMakeHistogramPerTrack(void* data, int argc, sqlite3_stmt* stmt, char** azColName);

        // ---------------------------------- Helpers ----------------------------------------
        
        // converts column name to rocprofvis_event_data_category_enum_t enumeration
        static rocprofvis_event_data_category_enum_t GetColumnDataCategory(
            const rocprofvis_event_data_category_map_t category_map,
            rocprofvis_dm_event_operation_t op, std::string column);

        // execute set of queries to fill in information tables cache
        rocprofvis_dm_result_t RunCacheQueries(
            Future* future, 
            std::vector<std::pair<std::string, std::string>>& info_table_lis, 
            RpvSqliteExecuteQueryCallback   callback, 
            bool async = true);

        // calculate number of CPU threads for processing track data 
        uint32_t CalculateParallelProcessSplitCount(uint32_t track_index);

        // executes query for all tracks asynchronously
        rocprofvis_dm_result_t ExecuteQueryForAllTracksAsync(
            uint32_t flags, 
            rocprofvis_dm_index_t query_type,
            rocprofvis_dm_charptr_t prefix, 
            rocprofvis_dm_charptr_t suffix,
            RpvSqliteExecuteQueryCallback callback, 
            std::function<std::string(rocprofvis_dm_track_params_t*, rocprofvis_dm_charptr_t)> func_prepare,
            std::function<void(rocprofvis_dm_track_params_t*)> func_clear,
            guid_list_t run_for_db_instances);
      
        // executes set of queries asynchronously
        rocprofvis_dm_result_t ExecuteQueriesAsync(
            std::vector<std::pair<DbInstance*, std::string>>& queries,
            Future* parent,
            rocprofvis_dm_handle_t handle,
            RpvSqliteExecuteQueryCallback callback);

        // needs to be defined as override. Used by public interface method
        rocprofvis_dm_result_t  ExecuteComputeQuery(
            rocprofvis_db_compute_use_case_enum_t use_case,
            rocprofvis_dm_charptr_t query,
            Future* future) override {
            (void) use_case;
            (void) query;
            (void) future;
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Systems database does not support compute query", kRocProfVisDmResultNotSupported);
        }

        StringTable& StringTableReference() { return m_string_table; };

        virtual rocprofvis_dm_result_t RemapStringId(uint64_t id, rocprofvis_db_string_type_t type, uint32_t node, uint64_t & result) = 0;
        virtual void GetTrackIdentifierIndices(int column_index, char** azColName, rocprofvis_db_sqlite_track_identifier_index_t& track_ids_indices) = 0;
        virtual bool FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance, uint32_t& out_track) = 0;
        virtual rocprofvis_dm_track_category_t GetRegionTrackCategory()    = 0;
        virtual const rocprofvis_event_data_category_map_t* GetCategoryEnumMap() = 0;

    private:

        static rocprofvis_dm_event_operation_t GetTableQueryOperation(std::string query);
        bool IsEmptyRange(uint32_t track, uint64_t start, uint64_t end);


        RpvSqliteExecuteQueryCallback m_callback_add_any_record;

    protected:
        TableProcessor m_table_processor[kRPVTableDataTypesNum];
        StringTable m_string_table;
       
};

}  // namespace DataModel
}  // namespace RocProfVis