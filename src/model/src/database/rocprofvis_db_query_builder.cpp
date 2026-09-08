// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT


#include "rocprofvis_db_query_builder.h"
#include "rocprofvis_db_profile.h"

namespace RocProfVis
{
namespace DataModel
{
    const char* g_select_str = "SELECT ";
    std::string Builder::Select(rocprofvis_db_sqlite_mem_act_subquery_format params)
    {
        return BuildQuery("(SELECT ", params.NUM_PARAMS, params.parameters, params.from,
            ")");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_track_query_format params)
    {
        return BuildTrackQuery(params.NUM_PARAMS, params.parameters,
                          params.from, ";");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_region_track_query_format params)
    {
        return BuildTrackQuery(params.NUM_PARAMS, params.parameters,
            params.from, params.where, ";");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_level_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_slice_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_perfetto_slice_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
            "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_launch_table_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_dispatch_table_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
            "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_memory_alloc_table_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
            "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_memory_alloc_activity_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
            "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_memory_copy_table_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
            "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_sample_table_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters,
                                   params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_rocpd_sample_table_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters,
            params.from,
            "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_rocpd_table_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_perfetto_launch_table_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
            "");
    }
    std::string Builder::Select(rocprofvis_db_perfetto_sample_table_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
            "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_dataflow_query_format params)
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
                          params.where, "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_essential_data_query_format params) 
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
                          params.where, "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_argument_data_query_format params) 
    {
        return BuildQuery(g_select_str, params.NUM_PARAMS, params.parameters, params.from,
            params.where, "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_stream_to_hw_format params) 
    {
        return BuildQuery("SELECT DISTINCT ", params.NUM_PARAMS, params.parameters, params.from,
             "");
    }
    

    std::string Builder::SelectAll(std::string query)
    {
        return "SELECT * FROM(" + query + ")";
    }
    std::string Builder::QParam(std::string name, std::string public_name)
    {
        return name + SQL_AS_STATEMENT + public_name;
    };
    std::string Builder::Blank()
    {
        return std::string();
    };
    std::string Builder::QParamBlank(std::string public_name)
    {
        return std::string(BLANK_COLUMN_STR) + SQL_AS_STATEMENT + public_name;
    };
    std::string Builder::StoreConfigVersion()
    {
        return std::to_string(TRACKS_CONFIG_VERSION) + SQL_AS_STATEMENT + "config";
    }
    std::string Builder::QParam(std::string name) { return name; };
    std::string Builder::QParamOperation(const rocprofvis_dm_event_operation_t op)
    {
        return std::to_string((uint32_t) op) + SQL_AS_STATEMENT + OPERATION_SERVICE_NAME;
    }
    std::string Builder::QParamCategory(const rocprofvis_dm_track_category_t category)
    {
        return std::to_string((uint32_t) category) + SQL_AS_STATEMENT + TRACK_CATEGORY_SERVICE_NAME;
    }
    std::string Builder::From(std::string table, MultiNode multinode) 
    { 
        return std::string(" FROM ") + table + (multinode == MultiNode::Yes ? "_%GUID% " : " ");
    }
    std::string Builder::From(std::string table, std::string nick_name, MultiNode multinode)
    {
        return std::string(" FROM ") + table + (multinode == MultiNode::Yes ? "_%GUID% " : " ") + nick_name;
    }
    std::string Builder::InnerJoin(std::string table, std::string nick_name, std::string on, MultiNode multinode)
    {
        return std::string(" INNER JOIN ") + table + (multinode == MultiNode::Yes ? "_%GUID% " : " ") + nick_name + SQL_ON_STATEMENT + on;
    }
    std::string Builder::LeftJoin(std::string table, std::string nick_name, std::string on, MultiNode multinode)
    {
        return std::string(" LEFT JOIN ") + table + (multinode == MultiNode::Yes ? "_%GUID% " : " ") + nick_name + SQL_ON_STATEMENT + on;
    }
    std::string Builder::RightJoin(std::string table, std::string nick_name, std::string on, MultiNode multinode)
    {
        return std::string(" RIGHT JOIN ") + table + (multinode == MultiNode::Yes ? "_%GUID% " : " ") + nick_name + SQL_ON_STATEMENT + on;
    }
    std::string Builder::SpaceSaver(int val) { return std::to_string(val) + SQL_AS_STATEMENT + "const"; }
    std::string Builder::THeader(std::string header)
    {
        return std::string("'") + header + " '";
    };
    std::string Builder::TVar(std::string tag, std::string var)
    {
        return std::string("'") + tag + ":'," + var;
    };
    std::string Builder::TVar(std::string tag, std::string var1, std::string var2)
    {
        return std::string("'") + tag + ":'," + var1 + "," + var2;
    };
    std::string Builder::Where(std::string name, std::string condition, std::string value)
    {
        return std::string(" WHERE ") + name + condition + value;
    };
    std::string Builder::Union()
    {
        return std::string(" UNION ALL ");
    }
    std::string Builder::Concat(std::vector<std::string> strings)
    {
        std::string result;
        for(const auto& s : strings)
        {
            if(result.size() > 0)
            {
                result += ",";
            }
            result += s;
            result += ",' '";
        }
        return std::string("concat(") + result + ")";
    }

    std::string Builder::BuildQuery(std::string start_with, int num_params, std::string* params,
                                  std::vector<std::string> from,
                                  std::string              finalize_with)
    {
        std::string query = start_with;
        for(int i = 0; i < num_params; i++)
        {
            if(i > 0)
            {
                query += ", ";
            }
            query += params[i];
        }
        for(int i = 0; i < from.size(); i++)
        {
            query += from[i];
            query += " ";
        }
        return query + finalize_with;
    }

    std::string Builder::GroupBy(int num_params, std::string* params)
    {
        std::string query = " GROUP BY ";
        int count = 0;
        for(int i = 0; i < num_params; i++)
        {
            size_t pos = params[i].find(SQL_AS_STATEMENT);
            if (pos != std::string::npos)
            {
                std:: string column = params[i].substr(pos+strlen(SQL_AS_STATEMENT));
                if (column != TRACK_CATEGORY_SERVICE_NAME && column != SPACESAVER_SERVICE_NAME)
                {
                    if(count++ > 0)
                    {
                        query += ", ";
                    }
                    query += column;
                }
            }
            else
            {
                if(count++ > 0)
                {
                    query += ", ";
                }
                query += params[i];
            }
        }
        return query;
    }

    std::string Builder::BuildTrackQuery(int num_params, std::string* params,
        std::vector<std::string> from,
        std::string              finalize_with)
    {
        std::string query = "SELECT ";
        for(int i = 0; i < num_params; i++)
        {
            if(i > 0)
            {
                query += ", ";
            }
            query += params[i];
        }
        query += ", COUNT(*) ";
        for(int i = 0; i < from.size(); i++)
        {
            query += from[i];
            query += " ";
        }
        query += GroupBy(num_params,params);
        return query + finalize_with;
    }

    std::string
    Builder::BuildTrackQuery(int num_params, std::string* params,
                        std::vector<std::string> from, std::vector<std::string> where, std::string finalize_with)
    {
        std::string query = "SELECT ";
        for(int i = 0; i < num_params; i++)
        {
            if(i > 0)
            {
                query += ", ";
            }
            query += params[i];
        }
        query += ", COUNT(*) ";
        for(int i = 0; i < from.size(); i++)
        {
            query += from[i];
            query += " ";
        }
        for(int i = 0; i < where.size(); i++)
        {
            query += where[i];
            query += " ";
        }
        query += GroupBy(num_params,params);
        return query + finalize_with;
    }

    std::string
        Builder::BuildQuery(std::string start_with, int num_params, std::string* params,
            std::vector<std::string> from, std::vector<std::string> where, std::string finalize_with)
    {
        std::string query = start_with;
        for(int i = 0; i < num_params; i++)
        {
            if(i > 0)
            {
                query += ", ";
            }
            query += params[i];
        }
        for(int i = 0; i < from.size(); i++)
        {
            query += from[i];
            query += " ";
        }
        for(int i = 0; i < where.size(); i++)
        {
            query += where[i];
            query += " ";
        }
        return query + finalize_with;
    }

    std::string Builder::LevelTable(std::string operation, std::string guid)
    {
        return std::string("roc_optiq_event_levels_") + operation + (guid.empty() ? "" : "_" + guid);
    }


    const char* Builder::IntToTypeEnum(int val, std::vector<std::string>& lookup) {
        return lookup[val].c_str();
    }

    const uint8_t Builder::TypeEnumToInt(const char* type, std::vector<std::string>& lookup) {
        for (size_t i = 0; i < lookup.size(); i++)
        {
            if (lookup[i] == type)
            {
                return static_cast<uint8_t>(i);
            }
        }
        return 0;
    }

    std::unordered_map<std::string, Builder::ColumnData> RocProfVis::DataModel::Builder::table_view_schema = {
            {OPERATION_SERVICE_NAME, {OPERATION_SERVICE_NAME, ColumnType::Byte, SCHEMA_INDEX_OPERATION}},
            {DB_ID_PUBLIC_NAME, {DB_ID_PUBLIC_NAME, ColumnType::Qword, SCHEMA_INDEX_EVENT_DB_ID}},
            {ID_PUBLIC_NAME, {ID_PUBLIC_NAME, ColumnType::Qword, SCHEMA_INDEX_EVENT_ID}},
            {CATEGORY_REFERENCE, {CATEGORY_PUBLIC_NAME, ColumnType::Word, SCHEMA_INDEX_CATEGORY}},
            {CATEGORY_REFERENCE_RPD, {CATEGORY_PUBLIC_NAME, ColumnType::Word, SCHEMA_INDEX_CATEGORY_RPD}},
            {CATEGORY_REFERENCE_PERFETTO, {CATEGORY_PUBLIC_NAME, ColumnType::Word, SCHEMA_INDEX_CATEGORY_PERFETTO}},
            {EVENT_NAME_REFERENCE, {NAME_PUBLIC_NAME, ColumnType::Dword, SCHEMA_INDEX_EVENT_NAME}},
            {SYMBOL_NAME_REFERENCE, {NAME_PUBLIC_NAME, ColumnType::Dword, SCHEMA_INDEX_EVENT_SYMBOL}},
            {EVENT_NAME_REFERENCE_RPD, {NAME_PUBLIC_NAME, ColumnType::Qword, SCHEMA_INDEX_EVENT_NAME_RPD}},
            {EVENT_NAME_REFERENCE_PERFETTO, {NAME_PUBLIC_NAME, ColumnType::Qword, SCHEMA_INDEX_EVENT_NAME_PERFETTO}},
            {EVENT_ARGS_RPD, {ARGS_PUBLIC_NAME, ColumnType::Qword, SCHEMA_INDEX_EVENT_ARGS_RPD}},
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
            {COUNTER_ID_SERVICE_NAME,{COUNTER_ID_PUBLIC_NAME, ColumnType::Qword,SCHEMA_INDEX_COUNTER_ID}},
            {COUNTER_NAME_REFERENCE_RPD,{COUNTER_ID_PUBLIC_NAME, ColumnType::Word,SCHEMA_INDEX_COUNTER_ID_RPD}},
            {COUNTER_VALUE_SERVICE_NAME,{COUNTER_VALUE_PUBLIC_NAME, ColumnType::Double,SCHEMA_INDEX_COUNTER_VALUE}},
            {TRACK_ID_PUBLIC_NAME,{TRACK_ID_PUBLIC_NAME, TRACK_ID_TYPE,SCHEMA_INDEX_TRACK_ID}},
            {STREAM_TRACK_ID_PUBLIC_NAME,{STREAM_TRACK_ID_PUBLIC_NAME, TRACK_ID_TYPE,SCHEMA_INDEX_STREAM_TRACK_ID}},
    };


    std::vector<std::string> RocProfVis::DataModel::Builder::mem_alloc_types = {
        "ALLOC", "FREE", "REALLOC", "RECLAIM"
    };

    std::vector<std::string> RocProfVis::DataModel::Builder::mem_alloc_levels = {
        "REAL", "VIRTUAL", "SCRATCH"
    };

}  // namespace DataModel
}  // namespace RocProfVis