// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db.h"
#include "rocprofvis_db_packed_storage.h"
#include <optional>

namespace RocProfVis
{
namespace DataModel
{

class ProfileDatabase;

typedef struct rocprofvis_db_sqlite_track_service_data_t
{
    rocprofvis_dm_event_operation_t op;
    uint64_t                        id;
    uint64_t                        nid;
    uint32_t                        stream_id;
    uint32_t                        process_id;
    uint32_t                        sub_process_id;
} rocprofvis_db_sqlite_track_service_data_t;

typedef struct rocprofvis_db_sqlite_track_query_format
{
    static constexpr const int NUM_PARAMS = 7; 
    std::string                 parameters[NUM_PARAMS];
    std::vector<std::string>    from;
} rocprofvis_db_sqlite_track_query_format;

typedef struct rocprofvis_db_sqlite_region_track_query_format
{
    static constexpr const int NUM_PARAMS = 7;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
    std::vector<std::string>   where;
} rocprofvis_db_sqlite_region_track_query_format;

typedef struct rocprofvis_db_sqlite_level_query_format
{
    static constexpr const int NUM_PARAMS = 9;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_level_query_format;

typedef struct rocprofvis_db_sqlite_slice_query_format
{
    static constexpr const int NUM_PARAMS = 11;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_slice_query_format;

typedef struct rocprofvis_db_sqlite_launch_table_query_format
{
    static constexpr const int NUM_PARAMS = 14;
    ProfileDatabase*            owner;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_launch_table_query_format;

typedef struct rocprofvis_db_sqlite_dispatch_table_query_format
{
    static constexpr const int NUM_PARAMS = 31;
    ProfileDatabase*            owner;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_dispatch_table_query_format;

typedef struct rocprofvis_db_sqlite_memory_alloc_table_query_format
{
    static constexpr const int NUM_PARAMS = 24;
    ProfileDatabase*            owner;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_memory_alloc_table_query_format;

typedef struct rocprofvis_db_sqlite_memory_alloc_activity_query_format
{
    static constexpr const int NUM_PARAMS = 11;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_memory_alloc_activity_query_format;

typedef struct rocprofvis_db_sqlite_memory_copy_table_query_format
{
    static constexpr const int NUM_PARAMS = 28;
    ProfileDatabase*            owner;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_memory_copy_table_query_format;

typedef struct rocprofvis_db_sqlite_rocpd_table_query_format
{
    static constexpr const int NUM_PARAMS = 10;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_rocpd_table_query_format;

typedef struct rocprofvis_db_sqlite_sample_table_query_format
{
    static constexpr const int NUM_PARAMS = 7;
    ProfileDatabase*           owner;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_sample_table_query_format;

typedef struct rocprofvis_db_sqlite_rocpd_sample_table_query_format
{
    static constexpr const int NUM_PARAMS = 9;
    ProfileDatabase*           owner;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_sample_rocpd_table_query_format;

typedef struct rocprofvis_db_sqlite_dataflow_query_format
{
    static constexpr const int NUM_PARAMS = 11;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
    std::vector<std::string>   where;
} rocprofvis_db_sqlite_dataflow_query_format;

typedef struct rocprofvis_db_sqlite_essential_data_query_format
{
    static constexpr const int NUM_PARAMS = 7;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
    std::vector<std::string>   where;
} rocprofvis_db_sqlite_essential_data_query_format;

typedef struct rocprofvis_db_sqlite_argument_data_query_format
{
    static constexpr const int NUM_PARAMS = 5;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
    std::vector<std::string>   where;
} rocprofvis_db_sqlite_argument_data_query_format;

typedef struct rocprofvis_db_sqlite_stream_to_hw_format
{
    static constexpr const int NUM_PARAMS = 5;
    std::string                parameters[NUM_PARAMS];
    std::vector<std::string>   from;
} rocprofvis_db_sqlite_stream_to_hw_format;

enum class MultiNode : bool { No = false, Yes = true };

class Builder
{
    public:
        static constexpr const char* AGENT_ID_SERVICE_NAME = "agentId";
        static constexpr const char* QUEUE_ID_SERVICE_NAME = "queueId";
        static constexpr const char* STREAM_ID_SERVICE_NAME = "streamId";
        static constexpr const char* NODE_ID_SERVICE_NAME = "nodeId";
        static constexpr const char* NODE_ID_PUBLIC_NAME = "nodeId";
        static constexpr const char* OPERATION_SERVICE_NAME = "op";
        static constexpr const char* TRACK_CATEGORY_SERVICE_NAME = "trackCategory";
        static constexpr const char* CATEGORY_PUBLIC_NAME = "category";
        static constexpr const char* PROCESS_ID_PUBLIC_NAME = "PID";
        static constexpr const char* THREAD_ID_PUBLIC_NAME = "TID";
        static constexpr const char* PROCESS_ID_SERVICE_NAME = "processId";
        static constexpr const char* THREAD_ID_SERVICE_NAME  = "threadId";
        static constexpr const char* COUNTER_ID_SERVICE_NAME  = "counterId";
        static constexpr const char* COUNTER_ID_PUBLIC_NAME  = "counter";
        static constexpr const char* COUNTER_VALUE_SERVICE_NAME = "counterValue";
        static constexpr const char* COUNTER_VALUE_PUBLIC_NAME = "value";
        static constexpr const char* TRACK_ID_PUBLIC_NAME = "__trackId";
        static constexpr const char* STREAM_TRACK_ID_PUBLIC_NAME = "__streamTrackId";
        static constexpr const char* SPACESAVER_SERVICE_NAME     = "const";
        static constexpr const char* COUNTER_NAME_SERVICE_NAME   = "monitorType";
        static constexpr const char* BLANK_COLUMN_STR            = "0";
        static constexpr const char* START_SERVICE_NAME     = "startTs";
        static constexpr const char* END_SERVICE_NAME       = "endTs";
        static constexpr const char* START_PUBLIC_NAME     = "start";
        static constexpr const char* END_PUBLIC_NAME       = "end";
        static constexpr const char* ID_PUBLIC_NAME = "__uuid";
        static constexpr const char* DB_ID_PUBLIC_NAME = "id";
        static constexpr const char* DURATION_PUBLIC_NAME = "duration";
        static constexpr const char* SIZE_PUBLIC_NAME = "size";
        static constexpr const char* ADDRESS_PUBLIC_NAME = "address";
        static constexpr const char* STREAM_PUBLIC_NAME = "stream";
        static constexpr const char* QUEUE_PUBLIC_NAME = "queue";
        static constexpr const char* NAME_PUBLIC_NAME = "name";
        static constexpr const char* NODE_PUBLIC_NAME = "node";
        static constexpr const char* SRC_ADDRESS_PUBLIC_NAME = "SrcAddr";

        static constexpr const char* GRID_SIZEX_PUBLIC_NAME = "GridSizeX";
        static constexpr const char* GRID_SIZEY_PUBLIC_NAME = "GridSizeY";
        static constexpr const char* GRID_SIZEZ_PUBLIC_NAME = "GridSizeZ";
        static constexpr const char* WORKGROUP_SIZEX_PUBLIC_NAME = "WGSizeX";
        static constexpr const char* WORKGROUP_SIZEY_PUBLIC_NAME = "WGSizeY";
        static constexpr const char* WORKGROUP_SIZEZ_PUBLIC_NAME = "WGSizeZ";

        static constexpr const char* LDS_SIZE_PUBLIC_NAME = "LDSSize";
        static constexpr const char* SCRATCH_SIZE_PUBLIC_NAME = "ScratchSize";

        static constexpr const char* STATIC_LDS_SIZE_PUBLIC_NAME = "StaticLDSSize";
        static constexpr const char* STATIC_SCRATCH_SIZE_PUBLIC_NAME = "StaticScratchSize";

        static constexpr const char* AGENT_ABS_INDEX_PUBLIC_NAME = "AgentAbsoluteIndex";
        static constexpr const char* AGENT_TYPE_PUBLIC_NAME = "AgentType";
        static constexpr const char* AGENT_TYPE_INDEX_PUBLIC_NAME = "AgentTypeIndex";
        static constexpr const char* AGENT_NAME_PUBLIC_NAME = "AgentName";
        static constexpr const char* AGENT_SRC_ABS_INDEX_PUBLIC_NAME = "SrcAgentAbsoluteIndex";
        static constexpr const char* AGENT_SRC_TYPE_PUBLIC_NAME = "SrcAgentType";
        static constexpr const char* AGENT_SRC_TYPE_INDEX_PUBLIC_NAME = "SrcAgentTypeIndex";
        static constexpr const char* AGENT_SRC_NAME_PUBLIC_NAME = "SrcAgentName";

        static constexpr const char* NODE_REFERENCE = "_nid";
        static constexpr const char* CATEGORY_REFERENCE = "_s_cat";
        static constexpr const char* CATEGORY_REFERENCE_RPD = "_s_cat_rpd";
        static constexpr const char* EVENT_NAME_REFERENCE = "_s_name";
        static constexpr const char* EVENT_NAME_REFERENCE_RPD = "_s_name_rpd";
        static constexpr const char* STREAM_NAME_REFERENCE = "_st_name";
        static constexpr const char* QUEUE_NAME_REFERENCE = "_q_name";
        static constexpr const char* SYMBOL_NAME_REFERENCE = "_sy_name";
        static constexpr const char* AGENT_ABS_INDEX_REFERENCE = "_ag_abs_idx";
        static constexpr const char* AGENT_TYPE_REFERENCE = "_ag_type";
        static constexpr const char* AGENT_TYPE_INDEX_REFERENCE = "_ag_type_idx";
        static constexpr const char* AGENT_NAME_REFERENCE = "_ag_name";
        static constexpr const char* AGENT_SRC_ABS_INDEX_REFERENCE = "_ag_src_abs_idx";
        static constexpr const char* AGENT_SRC_TYPE_REFERENCE = "_ag_src_type";
        static constexpr const char* AGENT_SRC_TYPE_INDEX_REFERENCE = "_ag_src_type_idx";
        static constexpr const char* AGENT_SRC_NAME_REFERENCE = "_ag_src_name";
        static constexpr const char* M_TYPE_REFERENCE = "_m_type";
        static constexpr const char* LEVEL_REFERENCE = "_level";
        static constexpr const char* EVENT_LEVEL_SERVICE_NAME = "event_level";
        static constexpr const char* COUNTER_NAME_REFERENCE_RPD = "_c_name";

        static constexpr const char* SQL_AS_STATEMENT = " as ";
        static constexpr const char* SQL_ON_STATEMENT = " ON ";

        static constexpr const char* ARG_POS_PUBLIC_NAME = "position";
        static constexpr const char* ARG_TYPE_PUBLIC_NAME = "type";
        static constexpr const char* ARG_NAME_PUBLIC_NAME = "name";
        static constexpr const char* ARG_VALUE_PUBLIC_NAME = "value";

        static constexpr const int  LAST_SINGLE_NODE_LEVEL_CALCULATION_VERSION  = 3;
        static constexpr const int  FIRST_MULTINODE_NODE_LEVEL_CALCULATION_VERSION  = 4;
        static constexpr const int  LEVEL_CALCULATION_VERSION  = 4;
        static constexpr const int  TRACKS_CONFIG_VERSION  = 2;
        static constexpr ColumnType TRACK_ID_TYPE = ColumnType::Word;

        static constexpr const char* NODE_HOSTNAME_SERVICE_NAME = "hostname";
        static constexpr const char* NODE_SYSTEM_SERVICE_NAME = "system_name";
        static constexpr const char* NODE_RELEASE_SERVICE_NAME = "release";
        static constexpr const char* NODE_VERSION_SERVICE_NAME = "version";


        static constexpr const char* PROCESS_COMMAND_SERVICE_NAME = "command";
        static constexpr const char* PROCESS_ENVIRONMENT_SERVICE_NAME = "environment";
        static constexpr const char* TID_SERVICE_NAME = "tid";
        static constexpr const char* PID_SERVICE_NAME = "pid";
        static constexpr const char* AGENT_PRODUCT_NAME_SERVICE_NAME = "product_name";
        static constexpr const char* AGENT_TYPE_SERVICE_NAME = "type";
        static constexpr const char* AGENT_TYPE_INDEX_SERVICE_NAME = "type_index";

        static constexpr const char* COUNTER_DESCRIPTION_SERVICE_NAME = "description";
        static constexpr const char* COUNTER_UNITS_SERVICE_NAME = "units";
        static constexpr const char* COUNTER_VALUE_TYPE_SERVICE_NAME = "value_type";

        static constexpr const char* NOT_APLICABLE = "N/A";

        inline static std::vector<std::string> mem_alloc_types = {
            "ALLOC", "FREE", "REALLOC", "RECLAIM"
        };

        inline static std::vector<std::string> mem_alloc_levels = {
            "REAL", "VIRTUAL", "SCRATCH"
        };

        typedef struct ColumnData
        {
            std::string public_name;
            ColumnType type;
            uint8_t index;
        } ColumnData;


        typedef enum table_view_schema_index_t
        {
            SCHEMA_INDEX_NULL,
            SCHEMA_INDEX_OPERATION,
            SCHEMA_INDEX_EVENT_DB_ID,
            SCHEMA_INDEX_EVENT_ID,
            SCHEMA_INDEX_CATEGORY,
            SCHEMA_INDEX_CATEGORY_RPD,
            SCHEMA_INDEX_EVENT_NAME,
            SCHEMA_INDEX_EVENT_SYMBOL,
            SCHEMA_INDEX_EVENT_NAME_RPD,
            SCHEMA_INDEX_MEM_TYPE,
            SCHEMA_INDEX_STREAM_NAME,
            SCHEMA_INDEX_QUEUE_NAME,
            SCHEMA_INDEX_NODE_ID,
            SCHEMA_INDEX_PROCESS_ID,
            SCHEMA_INDEX_THREAD_ID,
            SCHEMA_INDEX_AGENT_ABS_INDEX,
            SCHEMA_INDEX_AGENT_TYPE,
            SCHEMA_INDEX_AGENT_TYPE_INDEX,
            SCHEMA_INDEX_AGENT_NAME,
            SCHEMA_INDEX_START,
            SCHEMA_INDEX_END,
            SCHEMA_INDEX_DURATION,
            SCHEMA_INDEX_GRID_SIZEX,
            SCHEMA_INDEX_GRID_SIZEY,
            SCHEMA_INDEX_GRID_SIZEZ,
            SCHEMA_INDEX_WORKGROUP_SIZEX,
            SCHEMA_INDEX_WORKGROUP_SIZEY,
            SCHEMA_INDEX_WORKGROUP_SIZEZ,
            SCHEMA_INDEX_LDS_SIZE,
            SCHEMA_INDEX_SCRATCH_SIZE,
            SCHEMA_INDEX_STATIC_LDS_SIZE,
            SCHEMA_INDEX_STATIC_SCRATCH_SIZE,
            SCHEMA_INDEX_SIZE,
            SCHEMA_INDEX_ADDRESS,
            SCHEMA_INDEX_LEVEL,
            SCHEMA_INDEX_AGENT_SRC_ABS_INDEX,
            SCHEMA_INDEX_AGENT_SRC_TYPE,
            SCHEMA_INDEX_AGENT_SRC_TYPE_INDEX,
            SCHEMA_INDEX_AGENT_SRC_NAME,
            SCHEMA_INDEX_SRC_ADDRESS,
            SCHEMA_INDEX_COUNTER_ID,
            SCHEMA_INDEX_COUNTER_ID_RPD,
            SCHEMA_INDEX_COUNTER_VALUE,
            SCHEMA_INDEX_TRACK_ID,
            SCHEMA_INDEX_STREAM_TRACK_ID,
        } table_view_schema_index_t;

        inline static std::unordered_map<std::string, ColumnData> table_view_schema = {
            {OPERATION_SERVICE_NAME, {OPERATION_SERVICE_NAME, ColumnType::Byte, SCHEMA_INDEX_OPERATION}},
            {DB_ID_PUBLIC_NAME, {DB_ID_PUBLIC_NAME, ColumnType::Qword, SCHEMA_INDEX_EVENT_DB_ID}},
            {ID_PUBLIC_NAME, {ID_PUBLIC_NAME, ColumnType::Qword, SCHEMA_INDEX_EVENT_ID}},
            {CATEGORY_REFERENCE, {CATEGORY_PUBLIC_NAME, ColumnType::Word, SCHEMA_INDEX_CATEGORY}},
            {CATEGORY_REFERENCE_RPD, {CATEGORY_PUBLIC_NAME, ColumnType::Word, SCHEMA_INDEX_CATEGORY_RPD}},
            {EVENT_NAME_REFERENCE, {NAME_PUBLIC_NAME, ColumnType::Dword, SCHEMA_INDEX_EVENT_NAME}},
            {SYMBOL_NAME_REFERENCE, {NAME_PUBLIC_NAME, ColumnType::Dword, SCHEMA_INDEX_EVENT_SYMBOL}},
            {EVENT_NAME_REFERENCE_RPD, {NAME_PUBLIC_NAME, ColumnType::Dword, SCHEMA_INDEX_EVENT_NAME_RPD}},
            {M_TYPE_REFERENCE, {NAME_PUBLIC_NAME, ColumnType::Byte, SCHEMA_INDEX_MEM_TYPE}},
            {STREAM_NAME_REFERENCE, {STREAM_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_STREAM_NAME}},
            {QUEUE_NAME_REFERENCE, {QUEUE_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_QUEUE_NAME}},
            {NODE_ID_SERVICE_NAME, {NODE_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_NODE_ID}},
            {PROCESS_ID_PUBLIC_NAME, {PROCESS_ID_PUBLIC_NAME, ColumnType::Dword,SCHEMA_INDEX_PROCESS_ID}},
            {THREAD_ID_PUBLIC_NAME, {THREAD_ID_PUBLIC_NAME, ColumnType::Dword,SCHEMA_INDEX_THREAD_ID}},
            {AGENT_ABS_INDEX_REFERENCE, {AGENT_ABS_INDEX_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_AGENT_ABS_INDEX}},
            {AGENT_TYPE_REFERENCE, {AGENT_TYPE_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_AGENT_TYPE}},
            {AGENT_TYPE_INDEX_REFERENCE, {AGENT_TYPE_INDEX_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_AGENT_TYPE_INDEX}},
            {AGENT_NAME_REFERENCE, {AGENT_NAME_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_AGENT_NAME}},
            {START_SERVICE_NAME, {START_PUBLIC_NAME, ColumnType::Qword,SCHEMA_INDEX_START}},
            {END_SERVICE_NAME, {END_PUBLIC_NAME, ColumnType::Qword,SCHEMA_INDEX_END}},
            {DURATION_PUBLIC_NAME, {DURATION_PUBLIC_NAME, ColumnType::Qword,SCHEMA_INDEX_DURATION}},
            {GRID_SIZEX_PUBLIC_NAME, {GRID_SIZEX_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_GRID_SIZEX}},
            {GRID_SIZEY_PUBLIC_NAME, {GRID_SIZEY_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_GRID_SIZEY}},
            {GRID_SIZEZ_PUBLIC_NAME, {GRID_SIZEZ_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_GRID_SIZEZ}},
            {WORKGROUP_SIZEX_PUBLIC_NAME, {WORKGROUP_SIZEX_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_WORKGROUP_SIZEX}},
            {WORKGROUP_SIZEY_PUBLIC_NAME, {WORKGROUP_SIZEY_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_WORKGROUP_SIZEY}},
            {WORKGROUP_SIZEZ_PUBLIC_NAME, {WORKGROUP_SIZEZ_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_WORKGROUP_SIZEZ}},
            {LDS_SIZE_PUBLIC_NAME, {LDS_SIZE_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_LDS_SIZE}},
            {SCRATCH_SIZE_PUBLIC_NAME, {SCRATCH_SIZE_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_SCRATCH_SIZE}},
            {STATIC_LDS_SIZE_PUBLIC_NAME, {STATIC_LDS_SIZE_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_STATIC_LDS_SIZE}},
            {STATIC_SCRATCH_SIZE_PUBLIC_NAME, {STATIC_SCRATCH_SIZE_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_STATIC_SCRATCH_SIZE}},
            {SIZE_PUBLIC_NAME, {SIZE_PUBLIC_NAME, ColumnType::Dword,SCHEMA_INDEX_SIZE}},
            {ADDRESS_PUBLIC_NAME, {ADDRESS_PUBLIC_NAME, ColumnType::Qword,SCHEMA_INDEX_ADDRESS}},
            {LEVEL_REFERENCE, {LEVEL_REFERENCE, ColumnType::Byte,SCHEMA_INDEX_LEVEL}},
            {AGENT_SRC_ABS_INDEX_REFERENCE, {AGENT_SRC_ABS_INDEX_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_AGENT_SRC_ABS_INDEX}},
            {AGENT_SRC_TYPE_REFERENCE, {AGENT_SRC_TYPE_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_AGENT_SRC_TYPE}},
            {AGENT_SRC_TYPE_INDEX_REFERENCE, {AGENT_SRC_TYPE_INDEX_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_AGENT_SRC_TYPE_INDEX}},
            {AGENT_SRC_NAME_REFERENCE, {AGENT_SRC_NAME_PUBLIC_NAME, ColumnType::Byte,SCHEMA_INDEX_AGENT_SRC_NAME}},
            {SRC_ADDRESS_PUBLIC_NAME, {SRC_ADDRESS_PUBLIC_NAME, ColumnType::Qword,SCHEMA_INDEX_SRC_ADDRESS}},
            {COUNTER_ID_SERVICE_NAME,{COUNTER_ID_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_COUNTER_ID}},
            {COUNTER_NAME_REFERENCE_RPD,{COUNTER_ID_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_COUNTER_ID_RPD}},
            {COUNTER_VALUE_SERVICE_NAME,{COUNTER_VALUE_PUBLIC_NAME, ColumnType::Double,SCHEMA_INDEX_COUNTER_VALUE}},
            {TRACK_ID_PUBLIC_NAME,{TRACK_ID_PUBLIC_NAME, TRACK_ID_TYPE,SCHEMA_INDEX_TRACK_ID}},
            {STREAM_TRACK_ID_PUBLIC_NAME,{STREAM_TRACK_ID_PUBLIC_NAME, TRACK_ID_TYPE,SCHEMA_INDEX_STREAM_TRACK_ID}},
        };



        static std::optional<std::string> FindColumnNameByIndex(const std::unordered_map<std::string, ColumnData>& m, const uint8_t& index) {
            for (const auto& [key, val] : m) {
                if (val.index == index)
                    return key;
            }
            return std::nullopt;
        }

    public:
        static const char* IntToTypeEnum(int val, std::vector<std::string>& lookup);
        static const uint8_t TypeEnumToInt(const char* type, std::vector<std::string>& lookup);

    public:
        static std::string Select(rocprofvis_db_sqlite_track_query_format params);
        static std::string Select(rocprofvis_db_sqlite_region_track_query_format params);
        static std::string Select(rocprofvis_db_sqlite_level_query_format params);
        static std::string Select(rocprofvis_db_sqlite_slice_query_format params);
        static std::string Select(rocprofvis_db_sqlite_launch_table_query_format params);
        static std::string Select(rocprofvis_db_sqlite_dispatch_table_query_format params);
        static std::string Select(rocprofvis_db_sqlite_memory_alloc_table_query_format params);
        static std::string Select(rocprofvis_db_sqlite_memory_copy_table_query_format params);
        static std::string Select(rocprofvis_db_sqlite_rocpd_sample_table_query_format params);
        static std::string Select(rocprofvis_db_sqlite_sample_table_query_format params);
        static std::string Select(rocprofvis_db_sqlite_rocpd_table_query_format params);
        static std::string Select(rocprofvis_db_sqlite_dataflow_query_format params);
        static std::string Select(rocprofvis_db_sqlite_essential_data_query_format params);
        static std::string Select(rocprofvis_db_sqlite_argument_data_query_format params);
        static std::string Select(rocprofvis_db_sqlite_stream_to_hw_format params);
        static std::string Select(rocprofvis_db_sqlite_memory_alloc_activity_query_format);
        static std::string SelectAll(std::string query);
        static std::string QParam(std::string name, std::string public_name);
        static std::string Blank();
        static std::string QParamBlank(std::string public_name);
        static std::string QParam(std::string name);
        static std::string QParamOperation(const rocprofvis_dm_event_operation_t op);
        static std::string QParamCategory(const rocprofvis_dm_track_category_t category);
        static std::string From(std::string table, MultiNode multinode = MultiNode::Yes);
        static std::string From(std::string table, std::string nick_name, MultiNode multinode = MultiNode::Yes);
        static std::string InnerJoin(std::string table, std::string nick_name, std::string on, MultiNode multinode = MultiNode::Yes);
        static std::string LeftJoin(std::string table, std::string nick_name, std::string on, MultiNode multinode = MultiNode::Yes);
        static std::string RightJoin(std::string table, std::string nick_name, std::string on, MultiNode multinode = MultiNode::Yes);
        static std::string SpaceSaver(int val);
        static std::string THeader(std::string header);
        static std::string TVar(std::string tag, std::string var);
        static std::string TVar(std::string tag, std::string var1, std::string var2);
        static std::string Concat(std::vector<std::string> strings);
        static std::string Where(std::string name, std::string condition, std::string value);
        static std::string Union();
        static std::string LevelTable(std::string operation, std::string guid="");
        static void OldLevelTables(std::string operation, std::vector<std::string> & table_list, std::string guid="");
        static std::string StoreConfigVersion();
    private:
        static std::string BuildQuery(std::string start_with, int num_params,
                                      std::string* params, std::vector<std::string> from,
                                      std::string finalize_with);
        static std::string BuildQuery(std::string start_with, int num_params,
                                      std::string* params, std::vector<std::string> from,
                                      std::vector<std::string> where,
                                      std::string              finalize_with);
        static std::string BuildTrackQuery(int num_params,
                                      std::string* params, std::vector<std::string> from,
                                      std::string finalize_with);
        static std::string BuildTrackQuery(int num_params,
                                      std::string* params, std::vector<std::string> from,
                                      std::vector<std::string> where,
                                      std::string              finalize_with);
        static std::string GroupBy(int num_params, std::string* params);

       
};

}  // namespace DataModel
}  // namespace RocProfVis