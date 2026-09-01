// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_sqlite.h"
#include "rocprofvis_shared_types.h"
#include "json.h"

namespace RocProfVis
{
namespace DataModel
{

    enum class MetricIdFormat {
        XY,
        XYZ,
        Other
    };

    class ComputeDatabase;

    class ComputeQueryFactory : public DatabaseVersion
    {
    public:
        ComputeQueryFactory(ComputeDatabase* db) : m_db(db) {}
        rocprofvis_dm_result_t GetComputeListOfWorkloads(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeWorkloadRooflineCeiling(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeWorkloadTopKernels(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeWorkloadKernelsList(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelRooflineIntensities(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelMetricCategoriesList(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeWorkloadMetricsDefinition(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeWorkloadMetricValueNames(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeMetricCategoryTablesList(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeMetricValues(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeMetricValuesByWorkload(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelMetricsMatrix(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelSourceFiles(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeSourceFileSourceLines(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelCodeObjects(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelIsaToIsaDeps(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelIsaLines(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelIsaToSourceDeps(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelSamplingStates(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
        rocprofvis_dm_result_t GetComputeKernelSamplingStateReasonCounts(rocprofvis_db_num_of_params_t num, rocprofvis_db_compute_params_t params, rocprofvis_dm_string_t& query_out);
    private:
        MetricIdFormat ClassifyMetricIdFormat(const std::string& s);
        std::string SanitizeMetricValueName(const std::string& name);
        void ParseMetricParam(std::string metric_str, uint32_t workload_id, std::set<uint32_t>& metric_ids);
        ComputeDatabase * m_db;
    };

    struct KernelStats {
        uint64_t count = 0;
        uint64_t sum = 0;
        uint64_t min = std::numeric_limits<uint64_t>::max();
        uint64_t max = 0;
        double mean = 0;
        double median = 0;
        std::string name;
        std::vector<uint64_t> durations;
    };

    struct MetricSelector {
        std::string metric_id;
        std::string value_name;
    };

    struct MetricRow {
        uint32_t kernel_uuid;
        uint32_t metric_uuid;
        std::string value_name;
        double value;
    };

    struct KernelMetricsRow {
        uint32_t kernel_uuid;
        KernelStats stats;
        std::vector<double> metrics; // pivoted metric columns
    };

    class ComputeDatabase : public SqliteDatabase
    {
    public:
        ComputeDatabase(rocprofvis_db_filename_t path) :
            SqliteDatabase(path),
            m_query_factory(this),
		m_last_matrix_workload_id(INVALID_INDEX)
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
            rocprofvis_dm_event_id_t /*event_id*/,
            Future* /*object*/) override {
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not support flow trace", kRocProfVisDmResultNotSupported);
        };
        // worker method to read stack trace info
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadStackTraceInfo(
            rocprofvis_dm_event_id_t /*event_id*/,
            Future* /*object*/) override {
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not support stack trace", kRocProfVisDmResultNotSupported);
        };
        // worker method to read extended info
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t  ReadExtEventInfo(
            rocprofvis_dm_event_id_t /*event_id*/,
            Future* /*object*/) override {
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
            rocprofvis_dm_timestamp_t /*start*/,
            rocprofvis_dm_timestamp_t /*end*/,
            rocprofvis_dm_hashed_timestamp_tag_t /*tag*/,
            rocprofvis_db_num_of_tracks_t /*num*/,
            rocprofvis_db_track_selection_t /*tracks*/,
            Future* /*object*/) override {
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

        rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t /*start*/,
            rocprofvis_dm_timestamp_t /*end*/,
            rocprofvis_dm_charptr_t /*new_db_path*/,
            Future* /*future*/) override {
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

        rocprofvis_dm_result_t BuildTableQuery(
            rocprofvis_dm_table_use_case_enum_t /*use_case*/,
            rocprofvis_dm_timestamp_t /*start*/,
            rocprofvis_dm_timestamp_t /*end*/,
            rocprofvis_db_num_of_tracks_t /*num*/,
            rocprofvis_db_track_selection_t /*tracks*/,
            rocprofvis_dm_charptr_t /*where*/,
            rocprofvis_dm_charptr_t /*filter*/,
            rocprofvis_dm_charptr_t /*group*/,
            rocprofvis_dm_charptr_t /*group_cols*/,
            rocprofvis_dm_charptr_t /*sort_column*/,
            rocprofvis_dm_sort_order_t /*sort_order*/,
            uint64_t /*max_count*/,
            uint64_t /*offset*/,
            bool /*count_only*/,
            rocprofvis_dm_string_t& /*query*/) override {
            ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Compute database does not build table query", kRocProfVisDmResultNotSupported);
        }

    private:

        ComputeQueryFactory m_query_factory;
        std::string m_db_version;
        // True when the compute_workload table has the optional
        // memory_chart_extdata column (detected once at metadata load).
        bool m_has_memory_chart_extdata = false;
        std::unordered_map<uint32_t, KernelStats> m_kernel_stats;
        std::vector<MetricRow> m_metric_rows;
        std::mutex m_mutex;
        std::map<uint32_t, std::map<uint32_t, std::string>> m_metric_id_lookup;
        std::map<uint32_t, std::vector<std::pair<std::string, uint32_t>>> m_metric_uuid_lookup;
        std::map<uint32_t, uint32_t> m_kernel_workload_lookup;
        uint32_t m_last_matrix_workload_id;
        std::string m_last_top_kernels_query;

        static int CallbackGetComputeGeneric(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackGetComputeKernelWorkloadLookupTable(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackGetComputeRooflineCeiling(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackGetComputeKernelMetricsMatrix(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackParseMetadata(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackDetectMemoryChartColumn(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackGetComputeWorkloadTopKernels(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackGetComputeMetricsData(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static int CallbackStoreMetricsLookupTable(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        rocprofvis_dm_result_t ComputeWorkloadTopKernelsMeanAndMedian(rocprofvis_dm_table_t table);
        rocprofvis_dm_result_t BuildKernelMetricsMatrix(rocprofvis_dm_table_t table, jt::Json & plan);
        rocprofvis_dm_result_t CreateIndexes();

        inline static const rocprofvis_null_data_exceptions_skip
            s_null_data_exceptions_skip = {
                { (void*)&CallbackGetComputeMetricsData,
                    {
                       "value"
                    }
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

        friend class ComputeQueryFactory;
    };

}  // namespace DataModel
}  // namespace RocProfVis
