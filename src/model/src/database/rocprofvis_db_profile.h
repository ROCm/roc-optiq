// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_query_manager.h"
#include "rocprofvis_db_version.h"
#include "rocprofvis_db_table_processor.h"

namespace RocProfVis
{
namespace DataModel
{

typedef enum rocprofvis_track_load_params
{
    kRpvDbTrackLoadId=1,
    kRpvDbTrackLoadTrackId,
    kRpvDbTrackLoadCategory,
    kRpvDbTrackLoadOp,
    kRpvDbTrackLoadRecordCount,
    kRpvDbTrackLoadMinTs,
    kRpvDbTrackLoadMaxTs,
    kRpvDbTrackLoadMinValue,
    kRpvDbTrackLoadMaxValue,
    kRpvDbTrackLoadNodeId,
    kRpvDbTrackLoadProcessId,
    kRpvDbTrackLoadSubprocessId,
    kRpvDbTrackLoadNodeTag,
    kRpvDbTrackLoadProcessTag,
    kRpvDbTrackLoadSubprocessTag,
    kRpvDbTrackLoadGuid,
    kRpvDbTrackLoadPID,
    kRpvDbTrackLoadNumItems,
} rocprofvis_track_load_params;

typedef struct rocprofvis_db_event_level_t
{
    uint64_t id;
    uint64_t parent_id;
    uint8_t  level_for_queue;
    uint8_t  level_for_stream;
} rocprofvis_db_event_level_t;

typedef struct rocprofvis_db_sqlite_trim_parameters
{
    // Table names as we can't issue recursively
    std::map<std::string, std::string> tables;
} rocprofvis_db_sqlite_trim_parameters;


// class for methods and members common for all RocPd-based schemas
class ProfileDatabase : public QueryManager
{
    friend class TableProcessor;
    
    public:
        // Database constructor
        // @param path - full path to database file
        ProfileDatabase( rocprofvis_db_filename_t path) : 
            QueryManager(path, CallbackAddAnyRecord) {};

        // ProfileDatabase destructor, must be defined as virtual to free resources of derived classes 
        virtual ~ProfileDatabase() {}
        // method to detect rocpd-based database type (rocpd vs rocprof)
        // @param filename - full path to database file
        // @param multinode_files - detected list of files from multi-node package
        static rocprofvis_db_type_t Detect(rocprofvis_db_filename_t filename, std::vector<std::string> & multinode_files);
        static rocprofvis_dm_result_t  DetectMultiNode(rocprofvis_db_filename_t filename, std::vector<std::string> & db_files);

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
        // builds query map based on track identifiers for slice query
         void BuildSliceQueryMap(
             slice_query_map_t& slice_query_map, 
             rocprofvis_dm_track_params_t* props,
             rocprofvis_db_query_type_t query_type) override;
    

        // method to build a query to read time slice of records for single track 
        // @param index - track index 
        // @param type - query type
        // @param query - reference to output query string  
        // @return status of operation
        rocprofvis_dm_result_t BuildTrackQuery(
                                rocprofvis_dm_index_t index,
                                rocprofvis_dm_index_t   type,
                                rocprofvis_dm_string_t& query,
                                uint32_t split_count,
                                uint32_t split_index) override;

        // adds a new query to the track queries collection 
        // multiple queries for single track are required to support data from multiple database tables on single track,
        // like Kernel Dispatch, Memory Copy and Memory Allocation
        // @param it - track properties array iterator
        // @param newprops - new track properties structure
        // @param newquery - new track records query. One track can have multiple queries.
        void  UpdateQueryForTrack(rocprofvis_dm_track_params_it it, 
            rocprofvis_dm_track_params_t& newprops,
            std::vector<rocprofvis_dm_string_t> & newqueries);

    protected:
    // ------------------------------SQL query callbacks-----------------------------------
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names  
    // @return SQLITE_OK if successful

        // sqlite3_exec callback to add any record (Event or PMC) to time slice container. 
        static int CallbackAddAnyRecord(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to process stack trace information query and add stack trace object to StackTrace container
        static int CallbackAddStackTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName);
        // sqlite3_exec callback to cache specified tables data
        static int CallbackCacheTable(void *data, int argc, sqlite3_stmt* stmt, char **azColName);
        // sqlite3_exec callback to process track information query and add track object to Trace container
        static int CallBackAddTrack(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to load saved track information and add track object to Trace container
        static int CallBackLoadTrack(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to add flowtrace record to FlowTrace container.
        static int CallbackAddFlowTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName);
        // sqlite3_exec callback to add extended info record ExtData container.
        static int CallbackAddExtInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to add essential info into ExtData container.
        static int CallbackAddEssentialInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to add arguments info into ExtData container.
        static int CallbackAddArgumentsInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to calculate graph level for an event and store it into trace object map array
        static int CalculateEventLevels(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to collect minimum/maximum timestamps and minimu/maximum value/level
        static int CallbackGetTrackProperties(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to collect number of records in te track
        static int CallbackGetTrackRecordsCount(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to collect existing tables in database
        static int CallbackTrimTableQuery(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        // sqlite3_exec callback to collect calculate histogram buckets
        static int CallBackLoadHistogram(void* data, int argc, sqlite3_stmt* stmt, char** azColName);

    // ---------------------------------- Helpers ----------------------------------------

        // collect service/identification parameters from table row 
        static void CollectTrackServiceData(ProfileDatabase* db,
            sqlite3_stmt* stmt, int column_index, char** azColName,
            rocprofvis_db_sqlite_track_service_data_t& service_data);

        // save track properties back into database for future use
        rocprofvis_dm_result_t SaveTrackProperties(Future* future);

        // build histogram
        rocprofvis_dm_result_t BuildHistogram(Future* future, uint32_t desired_bins);

        // hash histogram query and schema for version control 
        uint64_t GetHistogramQueryAndSchemaHash();

        // get indeces of colums representing track identifiers
        void GetTrackIdentifierIndices(int column_index, char** azColName, rocprofvis_db_sqlite_track_identifier_index_t& track_ids_indices) override;

        // process track discovery data and populate track parameters
        virtual int ProcessTrack(rocprofvis_dm_track_params_t& track_params, std::vector<rocprofvis_dm_string_t> & newqueries) = 0;

        // Find track essential identifiers
        bool FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance, uint32_t& out_track) override;

    protected:
    // offset of kernel symbols in string table
        std::unordered_map<uint32_t, std::vector<rocprofvis_db_event_level_t>> m_event_levels[kRocProfVisDmNumOperation];
        std::unordered_map<uint32_t, std::unordered_map<uint64_t, size_t>> m_event_levels_id_to_index[kRocProfVisDmNumOperation];
        std::mutex   m_level_lock;
        OrderedMutex m_add_track_mutex;
        std::mutex m_lock;

    private:
        inline static SQLInsertParams s_histogram_schema_params = { 
            { "id", "INTEGER PRIMARY KEY" },
            { "track_number", "INTEGER" },
            { "bucket_number", "INTEGER" },
            { "events_count", "INTEGER" },
            { "bucket_value", "REAL" }
        };


        friend class MetadataVersionControl;

};

}  // namespace DataModel
}  // namespace RocProfVis
