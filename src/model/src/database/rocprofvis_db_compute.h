// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_sqlite.h"

namespace RocProfVis
{
namespace DataModel
{

    enum class MetricIdFormat {
        XY,
        XYZ,
        Other
    };

    class ComputeQueryFactory : public DatabaseVersion
    {
    public:
        std::string GetComputeListOfWorkloads(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params);
        std::string GetComputeWorkloadRooflineCeiling(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params);
        std::string GetComputeWorkloadTopKernels(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params);
        std::string GetComputeWorkloadKernelsList(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params);
        std::string GetComputeKernelRooflineIntensities(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params);
        std::string GetComputeKernelMetricCategoriesList(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params);
        std::string GetComputeMetricCategoryTablesList(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params);
        std::string GetComputeMetricValues(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params);
    private:
        MetricIdFormat ClassifyMetricIdFormat(const std::string& s);
    };


    class ComputeDatabase : public SqliteDatabase
    {
    public:
        ComputeDatabase(rocprofvis_db_filename_t path) :
            SqliteDatabase(path)
        {
            CreateDbNode(path);
        };

        // class destructor, not really required, unless declared as virtual
        ~ComputeDatabase()override {};
        // worker method to read trace metadata
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadTraceMetadata(
            Future* object) override;

        rocprofvis_dm_result_t BuildComputeQuery(
            rocprofvis_db_compute_use_case_enum_t use_case, rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params,
            rocprofvis_dm_string_t& query) override;

        rocprofvis_dm_result_t  ExecuteComputeQuery(
            rocprofvis_db_compute_use_case_enum_t use_case,
            rocprofvis_dm_charptr_t query,
            Future* future) override;

    protected:

        // worker method to read flow trace info
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadFlowTraceInfo(
            rocprofvis_dm_event_id_t event_id,
            Future* object) override {
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not support flow trace", kRocProfVisDmResultNotSupported);
        };
        // worker method to read stack trace info
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadStackTraceInfo(
            rocprofvis_dm_event_id_t event_id,
            Future* object) override {
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not support stack trace", kRocProfVisDmResultNotSupported);
        };
        // worker method to read extended info
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadExtEventInfo(
            rocprofvis_dm_event_id_t event_id,
            Future* object) override {
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not support extended data", kRocProfVisDmResultNotSupported);
        };
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
            Future* object) override {
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not support extended data", kRocProfVisDmResultNotSupported);
        };
        // worker method to execute database query
        // @param query - database query 
        // @param description - database description
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ExecuteQuery(
            rocprofvis_dm_charptr_t query,
            rocprofvis_dm_charptr_t description,
            Future* future) override;

        rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t start,
            rocprofvis_dm_timestamp_t end,
            rocprofvis_dm_charptr_t new_db_path,
            Future* future) override {
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not support trimming", kRocProfVisDmResultNotSupported);
        };

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

        // method to build a query to read time slice of records for single track 
        // @param index - track index 
        // @param tyte - query type
        // @param query - reference to output query string  
        // @return status of operation
        rocprofvis_dm_result_t BuildTrackQuery(
            rocprofvis_dm_index_t index,
            rocprofvis_dm_index_t   type,
            rocprofvis_dm_string_t& query,
            uint32_t split_count,
            uint32_t split_index) override{
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not build track query", kRocProfVisDmResultNotSupported);
        }

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
            slice_array_t& slices) override{
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not build slice query", kRocProfVisDmResultNotSupported);
        }

        rocprofvis_dm_result_t BuildTableQuery(
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
            rocprofvis_dm_num_string_table_filters_t num_string_table_filters,
            rocprofvis_dm_string_table_filters_t string_table_filters,
            uint64_t max_count,
            uint64_t offset,
            bool count_only,
            bool summary,
            rocprofvis_dm_string_t& query) override {
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not build table query", kRocProfVisDmResultNotSupported);
        }

    private:

        ComputeQueryFactory m_query_factory;
        std::string m_db_version;

        static int CallbackGetComputeGeneric(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackGetComputeRooflineCeiling(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackParseMetadata(void* data, int argc, sqlite3_stmt* stmt, char** azColName);

        inline static const rocprofvis_null_data_exceptions_skip
            s_null_data_exceptions_skip = {
                { 

                }
        };

        inline static const rocprofvis_null_data_exceptions_int 
            s_null_data_exceptions_int = {
                { 

                }
        };
        inline static const rocprofvis_null_data_exceptions_string
            s_null_data_exceptions_string = {
                { 

                }
        };

    };

}  // namespace DataModel
}  // namespace RocProfVis