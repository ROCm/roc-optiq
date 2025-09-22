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

#include "rocprofvis_db_query_builder.h"
#include "rocprofvis_db_sqlite.h"

namespace RocProfVis
{
namespace DataModel
{

    std::string Builder::Select(rocprofvis_db_sqlite_track_query_format params)
    {
        return BuildQuery("SELECT DISTINCT", params.NUM_PARAMS, params.parameters,
                          params.from, ";");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_region_track_query_format params)
    {
        return BuildQuery("SELECT DISTINCT", params.NUM_PARAMS, params.parameters,
                          params.from, params.where, ";");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_level_query_format params)
    {
        return BuildQuery("SELECT", params.NUM_PARAMS, params.parameters, params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_slice_query_format params)
    {
        return BuildQuery("SELECT", params.NUM_PARAMS, params.parameters, params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_table_query_format params)
    {
        BuildBlanksMask(params.owner, params.NUM_PARAMS, params.parameters);
        return BuildQuery("SELECT", params.NUM_PARAMS, params.parameters, params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_sample_table_query_format params)
    {
        return BuildQuery("SELECT", params.NUM_PARAMS, params.parameters,
                                   params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_rocpd_table_query_format params)
    {
        return BuildQuery("SELECT", params.NUM_PARAMS, params.parameters, params.from,
                          "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_dataflow_query_format params)
    {
        return BuildQuery("SELECT", params.NUM_PARAMS, params.parameters, params.from,
                          params.where, "");
    }
    std::string Builder::Select(rocprofvis_db_sqlite_essential_data_query_format params) 
    {
        return BuildQuery("SELECT", params.NUM_PARAMS, params.parameters, params.from,
                          params.where, "");
    }
    std::string Builder::SelectAll(std::string query)
    {
        return "SELECT * FROM(" + query + ")";
    }
    std::string Builder::QParam(std::string name, std::string public_name)
    {
        return name + " as " + public_name;
    };
    std::string Builder::QParamBlank(std::string public_name)
    {
        return std::string(BLANK_COLUMN_STR) + " as " + public_name;
    };
    std::string Builder::QParam(std::string name) { return name; };
    std::string Builder::QParamOperation(const rocprofvis_dm_event_operation_t op)
    {
        return std::to_string((uint32_t) op) + " as " + OPERATION_SERVICE_NAME;
    }
    std::string Builder::QParamCategory(const rocprofvis_dm_track_category_t category)
    {
        return std::to_string((uint32_t) category) + " as " + OPERATION_SERVICE_NAME;
    }
    std::string Builder::From(std::string table) { return std::string(" FROM ") + table; }
    std::string Builder::From(std::string table, std::string nick_name)
    {
        return std::string(" FROM ") + table + " " + nick_name;
    }
    std::string Builder::InnerJoin(std::string table, std::string nick_name, std::string on)
    {
        return std::string(" INNER JOIN ") + table + " " + nick_name + " ON " + on;
    }
    std::string Builder::LeftJoin(std::string table, std::string nick_name, std::string on)
    {
        return std::string(" LEFT JOIN ") + table + " " + nick_name + " ON " + on;
    }
    std::string Builder::RightJoin(std::string table, std::string nick_name, std::string on)
    {
        return std::string(" RIGHT JOIN ") + table + " " + nick_name + " ON " + on;
    }
    std::string Builder::SpaceSaver(int val) { return std::to_string(val) + " as const"; }
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

    void Builder::BuildBlanksMask(SqliteDatabase* owner, int num_params, std::string* params)
    {
        for(int i = 0; i < num_params; i++)
        {
            if(params[i].find(BLANK_COLUMN_STR) == 0)
            {
                owner->SetBlankMask(params[0], (uint64_t) 1 << i);
            }
        }
    }
    std::string Builder::BuildQuery(std::string select, int num_params, std::string* params,
                                  std::vector<std::string> from,
                                  std::string              finalize_with)
    {
        std::string query = select + " ";
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

    std::string
    Builder::BuildQuery(std::string select, int num_params, std::string* params,
                        std::vector<std::string> from, std::vector<std::string> where, std::string finalize_with)
    {
        std::string query = select + " ";
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

    std::string Builder::LevelTable(std::string operation)
    {
        return std::string("event_levels_")+ operation + "_v" + std::to_string(LEVEL_CALCULATION_VERSION);
    }

    std::vector<std::string>
    Builder::OldLevelTables(std::string operation)
    {
        std::vector<std::string> v;
        std::string base = std::string("event_levels_");
        v.push_back(base+operation);
        for(int i = 1; i < LEVEL_CALCULATION_VERSION; i++)
        {
            v.push_back(base + operation + "_v" + std::to_string(i)); 
        }
        return v;
    }

}  // namespace DataModel
}  // namespace RocProfVis