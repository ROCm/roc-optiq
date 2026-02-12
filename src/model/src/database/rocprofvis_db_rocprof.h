// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_profile.h"
#include "rocprofvis_db_query_factory.h"

namespace RocProfVis
{
namespace DataModel
{

typedef struct rocprofvis_db_string_id_hash_t
{
    size_t operator()(const rocprofvis_db_string_id_t& s) const noexcept
    {
        size_t h1 = std::hash<uint32_t>{}(s.m_string_id);
        size_t h2 = std::hash<uint32_t>{}(s.m_guid_id);
        size_t h3 = std::hash<rocprofvis_db_string_type_t>{}(s.m_string_type);

        size_t seed = h1;
        seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
} rocprofvis_db_string_id_hash_t;

typedef enum rocprofvis_db_memalloc_type_t : uint8_t
{
    kRPVMemActivityAlloc,
    kRPVMemActivityFree,
    kRPVMemActivityRealloc,
    kRPVMemActivityReclaim
}rocprofvis_db_memalloc_type_t;

typedef enum rocprofvis_db_memalloc_level_t : uint8_t
{
    kRPVMemLevelReal,
    kRPVMemLevelVirtual,
    kRPVMemLevelScratch
}rocprofvis_db_memalloc_level_t;

typedef struct rocprofvis_db_memalloc_activity_t
{
    uint64_t start;
    uint64_t end;
    uint64_t address;
    uint64_t size;
    uint32_t id;
    uint32_t pid;
    uint16_t stream_id;
    uint8_t agent_id;
    uint8_t queue_id;
    rocprofvis_db_memalloc_type_t type;
    rocprofvis_db_memalloc_level_t level;
}rocprofvis_db_memalloc_activity_t;

class RocprofDatabase : public ProfileDatabase
{

    // type of map array for string indexes remapping
    typedef std::unordered_map<rocprofvis_db_string_id_t, rocprofvis_dm_index_t, rocprofvis_db_string_id_hash_t> string_index_map_t;
    typedef std::unordered_map<rocprofvis_dm_index_t, std::vector<rocprofvis_db_string_id_t>> string_id_map_t;
    typedef std::unordered_map<std::string, uint32_t> string_map_t;
    typedef std::map<uint32_t, std::vector<rocprofvis_db_memalloc_activity_t>> memalloc_activity_t;
    typedef std::map<uint32_t, std::unordered_map<uint32_t, uint32_t>> mem_free_stream_to_agent_t;

public:
    RocprofDatabase(rocprofvis_db_filename_t path) :
        ProfileDatabase(path),
        m_query_factory(this)
    {
        CreateDbNode(path);
    };
    RocprofDatabase(rocprofvis_db_filename_t path, std::vector<std::string> & multinode_files) :
        ProfileDatabase(path),
        m_query_factory(this)
    {
        CreateDbNodes(multinode_files);
    };

    // class destructor, not really required, unless declared as virtual
    ~RocprofDatabase()override{};
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
                                        Future* object) override;
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
                                        Future* object) override;

    rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t start, 
                                           rocprofvis_dm_timestamp_t end,
                                           rocprofvis_dm_charptr_t new_db_path,
                                           Future* future) override;

    rocprofvis_dm_result_t BuildTableStringIdFilter( 
                                        rocprofvis_dm_num_string_table_filters_t num_string_table_filters, 
                                        rocprofvis_dm_string_table_filters_t string_table_filters,
                                        table_string_id_filter_map_t& filters) override;

    rocprofvis_dm_string_t GetEventOperationQuery(
                                        const rocprofvis_dm_event_operation_t operation) override;

    rocprofvis_dm_result_t StringIndexToId(
                                        rocprofvis_dm_index_t index, std::vector<rocprofvis_db_string_id_t>& id) override;

    rocprofvis_dm_result_t RemapStringId(uint64_t id, rocprofvis_db_string_type_t type, uint32_t node, uint64_t & result) override;

    rocprofvis_dm_result_t BuildTableSummaryClause(
                                        bool sample_query,
                                        rocprofvis_dm_string_t& select,
                                        rocprofvis_dm_string_t& group_by) override;

private:
    // sqlite3_exec callback to process string list query and add string object to Trace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallbackCaptureMemoryActivity(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
    // sqlite3_exec callback to process string list query and add string object to Trace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallBackAddString(void *data, int argc, sqlite3_stmt* stmt, char **azColName);
    // sqlite3_exec callback to cache specified tables data
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallbackCacheTable(void *data, int argc, sqlite3_stmt* stmt, char **azColName);
    // sqlite3_exec callback to process stack trace information query and add stack trace
    // object to StackTrace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallbackAddStackTrace(void* data, int argc, sqlite3_stmt* stmt,
                                     char** azColName);
    // sqlite3_exec callback to detect nodes and table names in the database
    // object to StackTrace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallbackNodeEnumeration(void* data, int argc, sqlite3_stmt* stmt,
                                       char** azColName);
    // sqlite3_exec callback to parse metadata table of new schema rocprof database
    // object to StackTrace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallbackParseMetadata(void* data, int argc, sqlite3_stmt* stmt,
        char** azColName);
    // sqlite3_exec callback to collect memory allocation activity
    // object to StackTrace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallBackAddMemoryAllocationActivity(void* data, int argc, sqlite3_stmt* stmt,
        char** azColName);
    // method to remap string IDs. Main reason for remapping is having strings and kernel symbol names in one array 
    // @param record - event record structure
    // @return status of operation
    rocprofvis_dm_result_t RemapStringIds(rocprofvis_db_record_data_t & record) override;
    rocprofvis_dm_result_t RemapStringIds(rocprofvis_db_flow_data_t& record) override;

    int ProcessTrack(rocprofvis_dm_track_params_t& track_params, rocprofvis_dm_charptr_t*  newqueries) override;

    protected:
    const rocprofvis_event_data_category_map_t* GetCategoryEnumMap() override {
        return &s_rocprof_categorized_data;
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
    const rocprofvis_dm_track_category_t GetRegionTrackCategory() override
    {
        return kRocProfVisDmRegionMainTrack;
    }


    private:
        rocprofvis_dm_result_t CreateIndexes();
        rocprofvis_dm_result_t LoadInformationTables(Future* future);
        rocprofvis_dm_result_t PopulateStreamToHardwareFlowProperties(uint32_t stream_track_index, uint32_t db_instance);
        rocprofvis_dm_result_t PopulateUnusedAgents(uint32_t db_instance);
        
    private:
        QueryFactory m_query_factory;
        std::string m_db_version;
        // map array for string indexes remapping. Main reason for remapping is older rocpd schema keeps duplicated symbols, one per GPU 
        string_index_map_t m_string_index_map; // id to index
        string_id_map_t m_string_id_map; // index to id
        string_map_t m_string_map; //temporary map to reuse string
        memalloc_activity_t m_memalloc_activity;
        mem_free_stream_to_agent_t m_memfree_stream_to_agent;
        std::mutex m_lock;


        inline static const rocprofvis_event_data_category_map_t
            s_rocprof_categorized_data = {
                {
                    kRocProfVisDmOperationNoOp,
                    {
                        { "id", kRocProfVisEventEssentialDataId },
                        { "category", kRocProfVisEventEssentialDataCategory },
                        { "name", kRocProfVisEventEssentialDataName },
                        { "start", kRocProfVisEventEssentialDataStart },
                        { "end", kRocProfVisEventEssentialDataEnd },
                        { "duration", kRocProfVisEventEssentialDataDuration },
                        { "nid", kRocProfVisEventEssentialDataNode },
                        { "pid", kRocProfVisEventEssentialDataProcess },
                        { "tid", kRocProfVisEventEssentialDataThread },
                        { "queue_name", kRocProfVisEventEssentialDataQueue },
                        { "stream_name", kRocProfVisEventEssentialDataStream },
                        { "stack_id", kRocProfVisEventEssentialDataInternal },
                        { "parent_stack_id", kRocProfVisEventEssentialDataInternal },
                        { "corr_id", kRocProfVisEventEssentialDataInternal },
                        { "stream_id", kRocProfVisEventEssentialDataInternal },
                        { "queue_id", kRocProfVisEventEssentialDataInternal },
                    },
                },
                {
                    kRocProfVisDmOperationDispatch,
                    {
                        { "agent_type", kRocProfVisEventEssentialDataAgentType },
                        { "agent_type_index", kRocProfVisEventEssentialDataAgentIndex },
                    },
                },
                {
                    kRocProfVisDmOperationMemoryAllocate,
                    {
                        { "agent_type", kRocProfVisEventEssentialDataAgentType },
                        { "agent_type_index", kRocProfVisEventEssentialDataAgentIndex },
                        { "type", kRocProfVisEventEssentialDataName },
                    },
                },
                {
                    kRocProfVisDmOperationMemoryCopy,
                    {
                        { "dst_agent_type", kRocProfVisEventEssentialDataAgentType },
                        { "dst_agent_type_index", kRocProfVisEventEssentialDataAgentIndex },
                    } 
                }
            };

        inline static const rocprofvis_null_data_exceptions_skip
            s_null_data_exceptions_skip = { 
                { (void*)&CallBackAddTrack,
                  { 
                        Builder::AGENT_ID_SERVICE_NAME, 
                        Builder::QUEUE_ID_SERVICE_NAME,
                  }
                },
                { (void*)&CallbackGetTrackProperties,
                  { 
                        "MIN(startTs)",
                        "MAX(endTs)"
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
                (void*)&CallbackCacheTable, 
                { 
                    { "name", "N/A" }, 
                    { "start", "0" }, 
                    { "end", "0" } 
                },
            },
            {
                (void*)&CallbackRunQuery,
                { 
                    { "name", "N/A" },  
                    { "start", "0" }, 
                    { "end", "0" } 
                },
            }

       };
};

}  // namespace DataModel
}  // namespace RocProfVis