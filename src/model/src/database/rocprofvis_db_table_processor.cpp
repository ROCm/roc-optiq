// Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of m_db software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and m_db permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "rocprofvis_db_table_processor.h"
#include "rocprofvis_db_expression_filter.h"
#include "rocprofvis_db_profile.h"
#include <sstream>
#include <unordered_set>

namespace RocProfVis
{
namespace DataModel
{

    std::string TableProcessor::Trim(const std::string& str) {
        auto start = std::find_if_not(str.begin(), str.end(), ::isspace);
        auto end = std::find_if_not(str.rbegin(), str.rend(), ::isspace).base();
        return (start < end) ? std::string(start, end) : "";
    }

    std::string TableProcessor::ParseSortCommand(std::string param, bool & order) {
        std::string column = "";
        if (!param.empty())
        {
            std::string order_str = "ASC ";
            size_t pos = param.find(order_str);
            if (pos != std::string::npos)
            {
                order = true;
            }
            else
            {
                order_str = "DESC ";
                pos = param.find(order_str);
                if (pos != std::string::npos)
                {
                    order = false;
                }
            }
            column = param.substr(pos + order_str.size());
        }
        return column;
    }

    rocprofvis_dm_result_t TableProcessor::ExecuteCompoundQuery(Future* future, 
        std::vector<std::string>& queries, 
        std::vector<rocprofvis_db_compound_query_command> commands, 
        rocprofvis_dm_handle_t handle, 
        rocprofvis_db_compound_table_type type) 
    {
        m_timer.pause();
        rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
        if (queries.size() > 0)
        {
            std::lock_guard<std::mutex> lock(m_lock);

            bool same_queries = false;
            if (queries.size() == m_query_data.size())
            {
                same_queries = true;
                for (int i = 0; i < queries.size(); i++)
                {
                    if (m_query_data[i].query != queries[i])
                    {
                        same_queries = false;
                    }
                }
            }
            if (!same_queries)
            {
                m_query_data.clear();
                m_tables.clear();
                m_tables.reserve(queries.size());
                m_query_data.resize(queries.size());
                for (int i = 0; i < queries.size(); i++)
                {
                    m_tables.push_back(std::make_unique<PackedTable>());
                    m_query_data[i].query = queries[i];
                }
                spdlog::info("table query 0: {} ", queries[0].c_str());
                result = m_db->ExecuteQueriesAsync(queries, future->SubFeatures(), (rocprofvis_dm_handle_t)this, &CallbackRunCompoundQuery);
                if (kRocProfVisDmResultSuccess == result)
                {
                    MergeQueriesData();
                }
                else
                {
                    m_tables.clear();
                }
            }
            else
            {
                result = kRocProfVisDmResultSuccess;
            }


            if (kRocProfVisDmResultSuccess == result)
            {
                result = ProcessCompoundQuery(handle, commands);
                
            }
            else
            {
                m_query_data.clear();
                m_merged_table.Clear();
                m_tables.clear();
            }
        }
        m_timer.restart(std::chrono::seconds(60));
        return result;
    }

    bool TableProcessor::IsCompoundQuery(const char* query, std::vector<std::string>& queries, std::vector<rocprofvis_db_compound_query_command>& commands)
    {
        const std::string command_tag = "-- CMD:";
        std::istringstream stream(query);
        std::string line;
        std::string currentSql;

        while (std::getline(stream, line)) {
            line = Trim(line);
            if (line.empty()) continue;

            if (line.rfind(command_tag.c_str(), 0) == 0) {
                std::string cmdline = Trim(line.substr(command_tag.length()));
                std::istringstream cmdStream(cmdline);
                rocprofvis_db_compound_query_command cmd;
                cmdStream >> cmd.name;
                std::getline(cmdStream, cmd.parameter);
                cmd.parameter = Trim(cmd.parameter);
                commands.push_back(cmd);
            } else {
                currentSql += line + " ";

                size_t pos;
                while ((pos = currentSql.find(';')) != std::string::npos) {
                    std::string stmt = Trim(currentSql.substr(0, pos));
                    if (!stmt.empty()) queries.push_back(stmt);
                    currentSql = currentSql.substr(pos + 1);
                }
            }
        }

        std::string leftover = Trim(currentSql);
        if (!leftover.empty()) queries.push_back(leftover);
        if (queries.size() > 1 || commands.size() > 0)
        {
            return true;
        } 
        return false;
    }


    rocprofvis_dm_result_t TableProcessor::ProcessCompoundQuery(rocprofvis_dm_table_t table, std::vector<rocprofvis_db_compound_query_command>& commands)
    {

        auto AddRows = [&](rocprofvis_dm_table_row_t & row, uint32_t row_index)
            {
                if (row_index < m_merged_table.RowCount())
                { 
                    auto columns = m_merged_table.GetMergedColumns();
                    uint8_t op = m_merged_table.GetOperationValue(row_index);
                    for (int column_index = 0; column_index < m_merged_table.MergedColumnCount(); column_index++)
                    {
                        if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_OPERATION)
                            continue;
                        if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_STREAM_TRACK_ID)
                        {
                            if (op == kRocProfVisDmOperationDispatch || op == kRocProfVisDmOperationMemoryAllocate || op == kRocProfVisDmOperationMemoryCopy)
                            {
                                Numeric val = m_merged_table.GetMergeTableValue(op, row_index, column_index, m_db);
                                m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(val.data.u64).c_str());
                            }
                            else
                            {
                                m_db->BindObject()->FuncAddTableRowCell(row, "-1");
                            }
                        }
                        else
                            if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_NULL)
                            {
                                m_db->BindObject()->FuncAddTableRowCell(row, "");
                            }
                            else
                                if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_COUNTER_VALUE)
                                {
                                    Numeric val = m_merged_table.GetMergeTableValue(op, row_index, column_index, m_db);
                                    m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(val.data.d).c_str());
                                }
                                else
                                {
                                    Numeric val = m_merged_table.GetMergeTableValue(op, row_index, column_index, m_db);
                                    const char* str = columns[column_index].m_type[op] == ColumnType::Null ? "" :
                                        PackedTable::ConvertTableIndexToString(m_db, columns[column_index].m_schema_index[op], val.data.u64);
                                    if (str == nullptr)
                                    {
                                        m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(val.data.u64).c_str());
                                    }
                                    else
                                    {
                                        m_db->BindObject()->FuncAddTableRowCell(row, str);
                                    }

                                }
                    }
                }
            };

        auto GetRow = [&](int row_index, std::unordered_map<std::string, FilterExpression::Value> & map)
            {
                if (row_index < m_merged_table.RowCount())
                {
                    auto columns = m_merged_table.GetMergedColumns();
                    uint8_t op = m_merged_table.GetOperationValue(row_index);
                    for (int column_index = 0; column_index < m_merged_table.MergedColumnCount(); column_index++)
                    {
                        if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_OPERATION)
                            continue;
                        if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_NULL)
                        {
                            map[columns[column_index].m_name] = "";
                        }
                        else
                            if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_COUNTER_VALUE)
                            {
                                Numeric val = m_merged_table.GetMergeTableValue(op, row_index, column_index, m_db);
                                map[columns[column_index].m_name] = val.data.d;
                            }
                            else
                            {
                                Numeric val = m_merged_table.GetMergeTableValue(op, row_index, column_index, m_db);
                                const char* str = columns[column_index].m_type[op] == ColumnType::Null ? "" :
                                    PackedTable::ConvertTableIndexToString(m_db, columns[column_index].m_schema_index[op], val.data.u64);
                                if (str != nullptr)
                                {
                                    map[columns[column_index].m_name] = str;
                                }
                                else
                                {
                                    map[columns[column_index].m_name] = (double)val.data.u64;
                                }

                            }
                    }
                }
            };

        auto AddNumRecordsColumn = [&](rocprofvis_dm_table_row_t row, int num_rows)
            {
                return kRocProfVisDmResultSuccess ==
                    m_db->BindObject()->FuncAddTableColumn(table, "NumRecords") &&
                    kRocProfVisDmResultSuccess ==
                    m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(num_rows).c_str());
            };

        auto AddTableColumns = [&]()
            {
                rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
                for (auto column : m_merged_table.GetMergedColumns())
                {
                    if (column.m_name != Builder::OPERATION_SERVICE_NAME)
                    {
                        result = m_db->BindObject()->FuncAddTableColumn(table, column.m_name.c_str());
                        if (result != kRocProfVisDmResultSuccess)
                            break;
                    }
                }
                return result == kRocProfVisDmResultSuccess;
            };

        auto AddAggregationColumns = [&](std::string column_name)
            {
                rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
                std::vector<std::string> aggr_columns = { column_name, PackedTable::COLUMN_NAME_COUNT, PackedTable::COLUMN_NAME_MIN, PackedTable::COLUMN_NAME_MAX, PackedTable::COLUMN_NAME_AVG };
                for (auto column : aggr_columns)
                {
                    result = m_db->BindObject()->FuncAddTableColumn(table, column.c_str());
                    if (result != kRocProfVisDmResultSuccess)
                        break;
                }
                return result == kRocProfVisDmResultSuccess;
            };

        auto AddAggregationRows = [&](rocprofvis_dm_table_row_t& row, uint32_t row_index)
            {
                auto r = m_merged_table.GetAggreagationRow(row_index);
                m_db->BindObject()->FuncAddTableRowCell(row, r.second.name.c_str());
                m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(r.second.count).c_str());
                m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(r.second.min_duration).c_str());
                m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(r.second.max_duration).c_str());
                m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(r.second.avg_duration).c_str());
            };

        auto InvalidateFiltering = [&]()
        {
            m_last_filter_str = "";
        };

        auto InvalidateGrouping = [&]()
        {
            m_last_group_str = "";
        };

        auto InvalidateSorting = [&]()
        {
            m_sort_order = true;
            m_sort_column = "id";
        };
        


        uint64_t offset = 0;
        uint64_t limit = 100;
        auto it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "OFFSET"; });
        if (it != commands.end())
        {
            offset = std::atoll(it->parameter.c_str());
        }

        it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "LIMIT"; });
        if (it != commands.end())
        {
            limit = std::atoll(it->parameter.c_str());
        }

        if (m_merged_table.RowCount() != m_row_count)
        {
            InvalidateFiltering();
            InvalidateGrouping();
            m_row_count = m_merged_table.RowCount();
        }

        bool filtered = !m_last_filter_str.empty();
        it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "FILTER"; });
        if (it != commands.end())
        {
            if (m_last_filter_str != it->parameter)
            {
                m_last_filter_str = Trim(it->parameter);
                m_filter_lookup.clear();
                InvalidateGrouping();
                if (!m_last_filter_str.empty())
                {
                    try {
                        filtered = true;
                        auto filter = FilterExpression::Parse(m_last_filter_str);
                        int use_threads = m_merged_table.RowCount() / 10000;
                        size_t thread_count = std::thread::hardware_concurrency() - 1;
                        if (use_threads < thread_count)
                            thread_count = use_threads;

                        std::mutex mtx;

                        auto task = [&](size_t start_row, size_t end_row) {
                            std::unordered_map<std::string, FilterExpression::Value> row_map;
                            auto lfilter = filter;
                            for (size_t row_index = start_row; row_index < end_row; row_index++)
                            {
                                GetRow(row_index, row_map);
                                bool valid = true;
                                try {
                                    valid = lfilter.Evaluate(row_map);
                                }
                                catch (std::runtime_error err)
                                {
                                    valid = false;
                                }
                                if (valid)
                                {
                                    std::lock_guard<std::mutex> lock(mtx);
                                    m_filter_lookup.insert(row_index);
                                }
                            }
                            };

                        std::vector<std::thread> threads;
                        size_t rows_per_task = thread_count == 0 ? 0 : m_merged_table.RowCount() / thread_count;
                        size_t leftover_rows_count = m_merged_table.RowCount() - (rows_per_task * thread_count);
                        for (int i = 0; i < thread_count; ++i)
                            threads.emplace_back(task, rows_per_task * i, rows_per_task * (i + 1));
                        if (leftover_rows_count > 0)
                            threads.emplace_back(task, rows_per_task * thread_count, leftover_rows_count);

                        for (auto& t : threads)
                            t.join();
                    }
                    catch (std::runtime_error e)
                    {
                        spdlog::error("Error: {} ", e.what());
                        m_filter_lookup.clear();
                        filtered = false;
                    }

                }
            }
        }
        else
        {
            m_filter_lookup.clear();
            filtered = false;
            if (!m_last_filter_str.empty())
            {
                InvalidateGrouping();
            }
            InvalidateFiltering();
        }

        size_t table_size = filtered ? m_filter_lookup.size() : m_merged_table.RowCount();

        it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "GROUP"; });
        if (it != commands.end())
        {
            if (m_last_group_str != it->parameter)
            {
                m_last_group_str = Trim(it->parameter);
                if (!m_last_group_str.empty())
                {
                    int use_threads = m_merged_table.RowCount() / 10000;
                    size_t thread_count = std::thread::hardware_concurrency() - 1;
                    if (use_threads < thread_count)
                        thread_count = use_threads;

                    std::vector<std::thread> threads;
                    size_t rows_per_task = thread_count == 0 ? 0 : m_merged_table.RowCount() / thread_count;
                    size_t leftover_rows_count = m_merged_table.RowCount() - (rows_per_task * thread_count);
                    if (m_merged_table.SetupAggregation(m_last_group_str, thread_count+1))
                    {

                        if (filtered)
                        {
                            auto task = [&](size_t thread_index, size_t start_row, size_t end_row) {
                                for (size_t row_index = start_row; row_index < end_row; row_index++)
                                {
                                    if (m_filter_lookup.count(row_index))
                                        m_merged_table.AggregateRow(m_db, row_index, thread_index);
                                }
                                };
                            for (int i = 0; i < thread_count; ++i)
                                threads.emplace_back(task, i, rows_per_task * i, rows_per_task * (i + 1));
                            threads.emplace_back(task, thread_count, rows_per_task * thread_count, leftover_rows_count);
                        }
                        else
                        {
                            auto task = [&](size_t thread_index, size_t start_row, size_t end_row) {
                                for (size_t row_index = start_row; row_index < end_row; row_index++)
                                {
                                    m_merged_table.AggregateRow(m_db, row_index, thread_index);
                                }
                                };
                            for (int i = 0; i < thread_count; ++i)
                                threads.emplace_back(task, i, rows_per_task * i, rows_per_task * (i + 1));
                            if (leftover_rows_count > 0)
                                threads.emplace_back(task, thread_count, rows_per_task * thread_count, leftover_rows_count);
                        }

                        for (auto& t : threads)
                            t.join();
                        m_merged_table.FinalizeAggregation();
                    }
                }
            }

            auto it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "SORT"; });
            if (it != commands.end())
            {
                bool sort_order = m_sort_order;
                std::string sort_column = ParseSortCommand(it->parameter, sort_order);
                if (sort_order != m_sort_order || sort_column != m_sort_column)
                {
                    m_merged_table.SortAggregationByColumn(m_db, sort_column, sort_order);
                    m_sort_order = sort_order;
                    m_sort_column = sort_column;
                }                
            }

            it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "COUNT"; });
            if (it != commands.end())
            {
                rocprofvis_dm_table_row_t row =
                    m_db->BindObject()->FuncAddTableRow(table);
                if (AddNumRecordsColumn(row, m_merged_table.AggregationRowCount()) && AddAggregationColumns(m_last_group_str))
                {
                    AddAggregationRows(row, 0);
                }
            }
            else
            {
                if (AddAggregationColumns(m_last_group_str))
                {
                    for (uint32_t row_index = offset; row_index < offset + limit; row_index++)
                    {
                        if (row_index >= m_merged_table.AggregationRowCount())
                            break;
                        rocprofvis_dm_table_row_t row =
                            m_db->BindObject()->FuncAddTableRow(table);
                        AddAggregationRows(row, row_index);
                    }
                }
            }

        }
        else
        {

            auto it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "SORT"; });
            if (it != commands.end())
            {
                bool sort_order = m_sort_order;
                std::string sort_column = ParseSortCommand(it->parameter, sort_order);
                if (sort_order != m_sort_order || sort_column != m_sort_column)
                {
                    m_merged_table.SortByColumn(m_db, sort_column, sort_order);
                    m_sort_order = sort_order;
                    m_sort_column = sort_column;
                }
            }

            it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "COUNT"; });
            if (it != commands.end())
            {
                rocprofvis_dm_table_row_t row =
                    m_db->BindObject()->FuncAddTableRow(table);

                if (AddNumRecordsColumn(row, table_size) && AddTableColumns())
                {
                    AddRows(row, 0);
                }
            }
            else
            {

                if (AddTableColumns())
                {

                    if (filtered)
                    {
                        size_t count = 0;
                        for (uint32_t row_index = 0; row_index < m_merged_table.RowCount(); row_index++)

                        {
                            uint32_t sorted_index = m_merged_table.SortedIndex(row_index);
                            if (m_filter_lookup.count(sorted_index))
                            {
                                if (count >= offset && count < offset + limit) {
                                    rocprofvis_dm_table_row_t row =
                                        m_db->BindObject()->FuncAddTableRow(table);
                                    AddRows(row, sorted_index);
                                }
                                ++count;
                                if (count >= offset + limit)
                                    break;

                            }
                        }
                    }
                    else
                    {
                        for (uint32_t row_index = offset; row_index < offset + limit; row_index++)
                        {
                            if (row_index >= m_merged_table.RowCount())
                                break;
                            uint32_t sorted_index = m_merged_table.SortedIndex(row_index);
                            rocprofvis_dm_table_row_t row =
                                m_db->BindObject()->FuncAddTableRow(table);
                            AddRows(row, sorted_index);
                        }
                    }
                }
            }
        }

        return kRocProfVisDmResultSuccess;
    }

    int TableProcessor::CallbackRunCompoundQuery(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        rocprofvis_db_sqlite_track_service_data_t service_data{};
        rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
        ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
        TableProcessor* table_processor = (TableProcessor*)callback_params->handle;
        void* func = (void*)&CallbackRunCompoundQuery;
        if (callback_params->future->Interrupted()) return 1;

        int column_index = 0;

        if (callback_params->future->GetProcessedRowsCount() == 0)
        {
            for (; column_index < argc; column_index++)
            {
                uint8_t size = 0;
                std::string column = azColName[column_index];

                db->CollectTrackServiceData(db, stmt, column_index, azColName, service_data);

                auto it = Builder::table_view_schema.find(azColName[column_index]);

                if (it != Builder::table_view_schema.end())
                {
                    table_processor->m_tables[callback_params->track_id]->AddColumn(azColName[column_index], it->second.type, column_index, it->second.index);
                }
            }

            auto it = Builder::table_view_schema.find(Builder::TRACK_ID_PUBLIC_NAME);
            table_processor->m_tables[callback_params->track_id]->AddColumn(it->first, it->second.type, column_index, it->second.index); 
            if (service_data.op != kRocProfVisDmOperationNoOp)
            {
                it = Builder::table_view_schema.find(Builder::STREAM_TRACK_ID_PUBLIC_NAME);
                table_processor->m_tables[callback_params->track_id]->AddColumn(it->first, ColumnType::Word, column_index, Builder::table_view_schema.size());
            }

            ProfileDatabase::FindTrackIDs(db, service_data, 
                table_processor->m_query_data[callback_params->track_id].track_id, 
                table_processor->m_query_data[callback_params->track_id].stream_track_id);

        }

        auto columns = table_processor->m_tables[callback_params->track_id]->GetColumns();
        table_processor->m_tables[callback_params->track_id]->AddRow();
        column_index = 0;
        uint64_t op = 0;
        for (; column_index < table_processor->m_tables[callback_params->track_id]->ColumnCount(); column_index++)
        {  
            if (columns[column_index].m_schema_index == Builder::SCHEMA_INDEX_TRACK_ID)
            {
                break;
            }

            if (columns[column_index].m_schema_index == Builder::SCHEMA_INDEX_MEM_TYPE)
            {
                uint64_t value = Builder::TypeEnumToInt(db->Sqlite3ColumnText(func, stmt, azColName, 
                    columns[column_index].m_orig_index), Builder::mem_alloc_types);
                table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, value);
            } else if (columns[column_index].m_schema_index == Builder::SCHEMA_INDEX_LEVEL)
            {
                uint64_t value = Builder::TypeEnumToInt(db->Sqlite3ColumnText(func, stmt, azColName, 
                    columns[column_index].m_orig_index), Builder::mem_alloc_levels);
                table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, value);
            } else if (columns[column_index].m_schema_index == Builder::SCHEMA_INDEX_COUNTER_VALUE)
            {
                double value = db->Sqlite3ColumnDouble(func, stmt, azColName, columns[column_index].m_orig_index);
                table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, value);
            } else if (columns[column_index].m_schema_index == Builder::SCHEMA_INDEX_OPERATION)
            {
                op = db->Sqlite3ColumnInt(func, stmt, azColName, columns[column_index].m_orig_index);
                table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, op);
            } else if (columns[column_index].m_schema_index == Builder::SCHEMA_INDEX_EVENT_ID)
            {
                uint64_t id = db->Sqlite3ColumnInt64(func, stmt, azColName, columns[column_index].m_orig_index);
                uint64_t value = id | (op << 60);
                table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, value);
            }
            else if (columns[column_index].m_schema_index == Builder::SCHEMA_INDEX_COUNTER_ID_RPD)
            {
                uint64_t value = db->StringTableReference().ToInt(db->Sqlite3ColumnText(func, stmt, azColName,
                    columns[column_index].m_orig_index));
                table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, value);
            }
            else if (columns[column_index].m_schema_index == Builder::SCHEMA_INDEX_NODE_ID)
            {
                uint64_t value = 0;
                table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, value);
            }
            else 
            {
                uint64_t value = db->Sqlite3ColumnInt64(func, stmt, azColName, columns[column_index].m_orig_index);
                table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, value);
            }
            
        }


        table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index++, 
            (uint64_t)table_processor->m_query_data[callback_params->track_id].track_id);
        if (service_data.op != kRocProfVisDmOperationNoOp)
        {
            table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, 
                (uint64_t)table_processor->m_query_data[callback_params->track_id].stream_track_id);
        }

        callback_params->future->CountThisRow();

        return 0;
    }

    rocprofvis_dm_result_t  TableProcessor::MergeQueriesData()
    {

        try {

            PackedTable::CreateMergedTable(m_tables, m_merged_table);
        }
        catch (const std::runtime_error& e) {
            spdlog::error("Error: {} ", e.what());
            return kRocProfVisDmResultUnknownError;
        } 

        return kRocProfVisDmResultSuccess;

    }
}  // namespace DataModel
}  // namespace RocProfVis
