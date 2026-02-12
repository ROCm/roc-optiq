// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT


#include "rocprofvis_db_query_builder.h"
#include "rocprofvis_db_profile.h"

namespace RocProfVis
{
namespace DataModel
{
    const char* g_select_str = "SELECT ";
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
                if (column != TRACK_CATEGORY_SERVICE_NAME && column != OPERATION_SERVICE_NAME && column != SPACESAVER_SERVICE_NAME)
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
        return std::string("event_levels_")+ operation + "_v" + std::to_string(LEVEL_CALCULATION_VERSION) + (guid.empty() ? "" : "_" + guid);
    }


    void Builder::OldLevelTables(std::string operation, std::vector<std::string> & table_list, std::string guid)
    {
        std::string base = std::string("event_levels_");
        if (guid.empty())
        {
            table_list.push_back(base + operation);
            for (int i = 1; i <= LAST_SINGLE_NODE_LEVEL_CALCULATION_VERSION; i++)
            {
                table_list.push_back(base + operation + "_v" + std::to_string(i));
            }
        }
        else
        {
            for (int i = FIRST_MULTINODE_NODE_LEVEL_CALCULATION_VERSION; i < LEVEL_CALCULATION_VERSION; i++)
            {
                table_list.push_back(base + operation + "_v" + std::to_string(i) + "_" + guid);
            }
        }
    }

    const char* Builder::IntToTypeEnum(int val, std::vector<std::string>& lookup) {
        return lookup[val].c_str();
    }

    const uint8_t Builder::TypeEnumToInt(const char* type, std::vector<std::string>& lookup) {
        for (int i=0; i < lookup.size(); i++)
        {
            if (lookup[i] == type)
            {
                return i;
            }
        }
        return 0;
    }

}  // namespace DataModel
}  // namespace RocProfVis