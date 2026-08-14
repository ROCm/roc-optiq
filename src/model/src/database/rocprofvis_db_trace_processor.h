// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef ROCPROFVIS_PERFETTO_ENABLED

#include "rocprofvis_db_query_manager.h"
#include "perfetto/trace_processor/trace_processor.h"          // main class
#include "perfetto/trace_processor/trace_processor_storage.h"  // base storage class
#include "perfetto/trace_processor/read_trace.h"               // file reading helper
#include "perfetto/trace_processor/basic_types.h"              // SqlValue, Iterator
#include "rocprofvis_db_query_factory.h"

namespace RocProfVis
{
namespace DataModel
{

    using perfetto::trace_processor::Config;
    using perfetto::trace_processor::TraceProcessor;
    using perfetto::trace_processor::TraceBlob;
    using perfetto::trace_processor::TraceBlobView;
    using perfetto::base::Status;

    class TraceConverter {
    public:
        // Converts a source trace file to a SQLite cache file.
        // TraceProcessor is created and destroyed within this call —
        // all conversion memory is released when this returns.
        //
        // progress_callback receives values from 0.0 to 1.0
        // Returns true on success, false on any error
        static bool Convert(
            const std::string& source_path,
            const std::string& output_path,
            std::function<void(float)> progress_callback = nullptr);

    private:

    };

// class for methods and members common for all RocPd-based schemas
class GoogleTraceProcessor : public QueryManager
{

    typedef std::unordered_map<std::string, uint32_t> string_map_t;
    typedef std::unordered_map<uint32_t, uint32_t> track_map_t;
    
    public:
        // Database constructor
        // @param path - full path to database file
        GoogleTraceProcessor( rocprofvis_db_filename_t path) : QueryManager(path, CallbackAddAnyRecord), m_query_factory(this) {};
        // ProfileDatabase destructor, must be defined as virtual to free resources of derived classes 
        virtual ~GoogleTraceProcessor() {}
        // worker method to read trace metadata
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadTraceMetadata(
            Future* object) override;
        // worker method to read flow trace info
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadFlowTraceInfo(
            rocprofvis_dm_event_id_t event_id,
            Future* object) override ;
        // worker method to read stack trace info
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadStackTraceInfo(
            rocprofvis_dm_event_id_t event_id,
            Future* object) override;
        // worker method to read extended info
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadExtEventInfo(
            rocprofvis_dm_event_id_t event_id,
            Future* object) override ;

        rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t start,
            rocprofvis_dm_timestamp_t end,
            rocprofvis_dm_charptr_t new_db_path,
            Future* future) override {
            ShowProgress(0, "Trimming is not supported for Perfetto traces!", kRPVDbError, future);
            return future->SetPromise( kRocProfVisDmResultNotSupported);
        };

    static rocprofvis_db_type_t Detect(rocprofvis_db_filename_t filename);

    private:
        // ------------------------------SQL query callbacks-----------------------------------
        // @param data - pointer to callback caller argument
        // @param argc - number of columns in the query
        // @param argv - pointer to row values
        // @param azColName - pointer to column names  
        // @return SQLITE_OK if successful

        static int CallbackCacheTable(void *data, int argc, sqlite3_stmt* stmt, char **azColName);
        static int CallbackAddTrack(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackAddAnyRecord(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallBackAddString(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackGetTrackProperties(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackAddFlowTrace(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackAddExtInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackAddArgumentsInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackAddEssentialInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackAddStackTrace(void* data, int argc, sqlite3_stmt* stmt, char** azColName);

        rocprofvis_dm_result_t LoadInformationTables(Future* future);
        rocprofvis_dm_result_t BuildHistogram(Future* future, uint32_t desired_bins);
        rocprofvis_dm_result_t CreateIndexes();


    protected:

        // ----------------------------------Query builders------------------------------------------

        rocprofvis_dm_result_t BuildTrackQuery(
            rocprofvis_dm_index_t index,
            rocprofvis_dm_index_t   type,
            rocprofvis_dm_string_t& query,
            uint32_t split_count,
            uint32_t split_index) override;

        rocprofvis_dm_result_t BuildTableStringIdFilter( 
            rocprofvis_dm_num_string_table_filters_t num_string_table_filters, 
            rocprofvis_dm_string_table_filters_t string_table_filters,
            bool include_substring,
            bool include_category,
            bool partial_matching,
            table_string_id_filter_map_t& filters) override;

        rocprofvis_dm_string_t GetEventOperationQuery(
            const rocprofvis_dm_event_operation_t operation) override;

        // builds query map based on track identifiers for slice query
        void BuildSliceQueryMap(
            slice_query_map_t& slice_query_map, 
            rocprofvis_dm_track_params_t* props,
            rocprofvis_db_query_type_t query_type) override;

        // ---------------------------------- Helpers ----------------------------------------
        rocprofvis_dm_result_t RemapStringId(uint64_t id, rocprofvis_db_string_type_t type, uint32_t node, uint64_t& result) override { result = id; return kRocProfVisDmResultSuccess; };
        rocprofvis_dm_track_category_t GetRegionTrackCategory() override { return kRocProfVisDmRegionTrack; }
        void GetTrackIdentifierIndices(int column_index, char** azColName, rocprofvis_db_sqlite_track_identifier_index_t& track_ids_indices) override;
        bool FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance, uint32_t& out_track) override;

     private:

            std::mutex    m_lock;
            QueryFactory  m_query_factory;
            string_map_t m_string_map; //temporary map to reuse string
            track_map_t m_track_map; //quick track remapping

            const rocprofvis_null_data_exceptions_int* GetNullDataExceptionsInt() override
            {
                return &s_null_data_exceptions_int;
            }
            const rocprofvis_null_data_exceptions_string* GetNullDataExceptionsString() override
            {
                return &s_null_data_exceptions_string;
            }
            const rocprofvis_null_data_exceptions_skip* GetNullDataExceptionsSkip() override
            {
                return &s_null_data_exceptions_skip;
            }
            const rocprofvis_event_data_category_map_t* GetCategoryEnumMap() override {
                return &s_perfetto_categorized_data;
            };

            inline static const rocprofvis_null_data_exceptions_skip
                s_null_data_exceptions_skip = {

            };

            inline static const rocprofvis_null_data_exceptions_int
                s_null_data_exceptions_int = {
                    { 
                        (void*)&CallbackCacheTable, 
                        { 
                            { "start_ts", (uint64_t)0 }, 
                            { "end_ts", (uint64_t)0 }, 
                        },
                    },
            };      

            inline static const rocprofvis_null_data_exceptions_string
                s_null_data_exceptions_string = { 
                    { 
                        (void*)&CallBackAddString, 
                        { 
                            { "name", "N/A" }, 
                            { "category", "N/A" }, 
                        },
                    },
                    { 
                        (void*)&CallbackCacheTable, 
                        { 
                            { "system_name", "N/A" }, 
                            { "system_version", "N/A" }, 
                            { "system_release", "N/A" },
                            { "system_machine", "N/A" },
                            { "start_ts", "0" }, 
                            { "end_ts",  "0" }, 
                        },
                    },
                    { 
                        (void*)&CallbackAddAnyRecord,
                        { 
                            { "name", "N/A" }, 
                            { "category", "N/A" },
                        },
                    },
                };

            inline static const rocprofvis_event_data_category_map_t
                s_perfetto_categorized_data = {
                    {
                        kRocProfVisDmOperationLaunch,
                        {
                        { "id", kRocProfVisEventEssentialDataId },
                        { "category", kRocProfVisEventEssentialDataCategory },
                        { "name", kRocProfVisEventEssentialDataName },
                        { "start", kRocProfVisEventEssentialDataStart },
                        { "end", kRocProfVisEventEssentialDataEnd },
                        { "duration", kRocProfVisEventEssentialDataDuration },
                        { "stack_id", kRocProfVisEventEssentialDataInternal },
                        { "parent_stack_id", kRocProfVisEventEssentialDataInternal },
                    },
                }
            };

};

}  // namespace DataModel
}  // namespace RocProfVis
#endif
