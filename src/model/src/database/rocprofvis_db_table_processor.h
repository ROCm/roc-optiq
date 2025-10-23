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

#pragma once

#include "rocprofvis_db_sqlite.h"

namespace RocProfVis
{
namespace DataModel
{

    class ProfileDatabase;

    typedef enum rocprofvis_db_compound_table_type {
        kRPVTableDataTypeEvent,
        kRPVTableDataTypeSample,
        kRPVTableDataTypesNum
    } rocprofvis_db_compound_table_type;

    typedef struct rocprofvis_db_compound_query_command {
        std::string name;
        std::string parameter;
    } rocprofvis_db_compound_query_command;



    typedef struct rocprofvis_db_compound_query_data {
        std::string query;
        int track_id;
        int stream_track_id;
    } rocprofvis_db_compound_query_data;


    class TableProcessor
    {

    public:
        TableProcessor(ProfileDatabase* db) : m_db(db), m_data_type(kRPVTableDataTypeEvent) {};

        static bool IsCompoundQuery(const char* query, std::vector<std::string>& queries, std::vector<rocprofvis_db_compound_query_command>& commands);
        rocprofvis_dm_result_t ExecuteCompoundQuery(Future* future, 
            std::vector<std::string>& queries, 
            std::vector<rocprofvis_db_compound_query_command> commands, 
            rocprofvis_dm_handle_t handle, 
            rocprofvis_db_compound_table_type type);
        std::string ParseSortCommand(std::string param, bool& order);

    private:
        rocprofvis_dm_result_t ProcessCompoundQuery(rocprofvis_dm_table_t table, std::vector<rocprofvis_db_compound_query_command>& commands);
        rocprofvis_dm_result_t  MergeQueriesData();
        static int CallbackRunCompoundQuery(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static std::string Trim(const std::string& str);

    private:
        ProfileDatabase* m_db;
        std::vector<std::unique_ptr<PackedTable>> tables;
        std::vector<rocprofvis_db_compound_query_data> query_data;
        PackedTable merged_table[kRPVTableDataTypesNum];
        std::string m_last_filter_str[kRPVTableDataTypesNum];
        std::string m_last_group_str[kRPVTableDataTypesNum];
        std::unordered_set<uint32_t> m_filter_lookup[kRPVTableDataTypesNum];
        bool m_sort_order[kRPVTableDataTypesNum];
        std::string m_sort_column[kRPVTableDataTypesNum];
        size_t m_row_count[kRPVTableDataTypesNum];
        rocprofvis_db_compound_table_type m_data_type;
        std::mutex  m_lock;


    };


}  // namespace DataModel
}  // namespace RocProfVis