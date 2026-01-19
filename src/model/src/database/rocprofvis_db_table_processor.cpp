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
#include <fstream>

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
        std::vector<rocprofvis_db_compound_query>& queries, 
        std::set<uint32_t>& tracks,
        std::vector<rocprofvis_db_compound_query_command> commands, 
        rocprofvis_dm_handle_t handle, 
        rocprofvis_db_compound_table_type type,
        bool query_updated) 
    {
        m_timer.pause();
        rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
        if (queries.size() > 0)
        {
            std::lock_guard<std::mutex> lock(m_lock);

            std::set<uint32_t> removed_tracks;
            std::set<uint32_t> added_tracks;


            std::set_difference(
                m_tracks.begin(), m_tracks.end(),
                tracks.begin(), tracks.end(),
                std::inserter(removed_tracks, removed_tracks.end())
            );

            std::set_difference(
                tracks.begin(), tracks.end(),
                m_tracks.begin(), m_tracks.end(),
                std::inserter(added_tracks, added_tracks.end())
            );
            bool sametracks = (removed_tracks.size() == 0) && (added_tracks.size() == 0);
            if (sametracks && query_updated)
            {
                removed_tracks = m_tracks;
                added_tracks = tracks;
            }

            result = kRocProfVisDmResultSuccess;
            bool same_queries = (removed_tracks.size() == 0) && (added_tracks.size() == 0);
            if (!same_queries)
            {
                m_tables.clear();
                std::vector<std::pair<DbInstance*, std::string>> new_queries;
                for (int i = 0; i < queries.size(); i++)
                {
                    if (added_tracks.find(queries[i].track) != added_tracks.end())
                    {
                        m_tables.push_back(std::make_unique<PackedTable>());
                        DbInstance* db_instance = m_db->DbInstancePtrAt(queries[i].guid_id);
                        new_queries.push_back({ db_instance, queries[i].query });
                    }
                    
                }

                m_merged_table.RemoveRowsForSetOfTracks(removed_tracks, sametracks && query_updated);

                if (new_queries.size())
                {
                    result = m_db->ExecuteQueriesAsync(new_queries, future->SubFeatures(), (rocprofvis_dm_handle_t)this, &CallbackRunCompoundQuery);
                    if (kRocProfVisDmResultSuccess == result)
                    {
                        try {

                            m_merged_table.Merge(m_tables);
                        }
                        catch (const std::runtime_error& e) {
                            spdlog::error("Error: {} ", e.what());
                            result = kRocProfVisDmResultUnknownError;
                        }
                        catch (const std::out_of_range& e) {
                            spdlog::error("Error: {} ", e.what());
                        } 
                        catch (const std::bad_variant_access& e) {
                            spdlog::error("Bad_variant_access: {} ", e.what());
                            result = kRocProfVisDmResultUnknownError;
                        } 
                        catch (...) {
                            spdlog::error("Unknown error!");
                            result = kRocProfVisDmResultUnknownError;
                        }
                    }                      
                }
                else
                {
                    m_merged_table.ManageColumns(m_tables);
                }
                m_merged_table.CreateSortOrderArray();
                if (m_merged_table.RowCount() == 0)
                {
                    result = kRocProfVisDmResultNotLoaded;
                }
            }


            if (kRocProfVisDmResultSuccess == result)
            {
                result = ProcessCompoundQuery(handle, commands, !same_queries);
                m_tracks = tracks;
            }
            else
            {
                m_tables.clear();
            }
        }
        m_timer.restart(std::chrono::seconds(60));
        return result;
    }

    std::string TableProcessor::QueryWithoutCommands(const char* query) {
        std::string query_without_commands = query;
        size_t pos = query_without_commands.find(QUERY_COMMAND_TAG);
        if (pos != std::string::npos)
        {
            query_without_commands.erase(pos);
        }
        return query_without_commands;
    }

    bool TableProcessor::IsCompoundQuery(const char* query, std::vector<rocprofvis_db_compound_query>& queries, std::set<uint32_t>& tracks, std::vector<rocprofvis_db_compound_query_command>& commands)
    {
        std::istringstream stream(query);
        std::string line;
        std::string currentSql;

        while (std::getline(stream, line)) {
            line = Trim(line);
            if (line.empty()) continue;

            if (line.rfind(QUERY_COMMAND_TAG, 0) == 0) {
                std::string cmdline = Trim(line.substr(strlen(QUERY_COMMAND_TAG)));
                std::istringstream cmdStream(cmdline);
                rocprofvis_db_compound_query_command cmd;
                cmdStream >> cmd.name;
                std::getline(cmdStream, cmd.parameter);
                cmd.parameter = Trim(cmd.parameter);
                commands.push_back(cmd);
            } else {
                currentSql = line;

                size_t pos;
                if ((pos = currentSql.find(';')) != std::string::npos) {
                    std::string stmt = currentSql.substr(0, pos);
                    if (!stmt.empty())
                    {
                        currentSql = Trim(currentSql.substr(pos + 1));
                        if ((pos = currentSql.find(';')) != std::string::npos) {
                            auto s_track  = currentSql.substr(0, pos);
                            auto s_guid_id = currentSql.substr(pos + 1);
                            if (Database::IsNumber(s_track) && Database::IsNumber(s_guid_id))
                            {
                                uint32_t track = std::atol(s_track.c_str());
                                uint32_t guid_id = std::atoll(s_guid_id.c_str());
                                tracks.insert(track);
                                queries.push_back({ stmt,track,guid_id });
                            }
                           
                        }
                    }

                }
            }
        }

        if (queries.size() > 1 || commands.size() > 0)
        {
            return true;
        } 
        return false;
    }


    rocprofvis_dm_result_t TableProcessor::AddTableCells(bool to_file, rocprofvis_dm_handle_t handle, uint32_t row_index)
    {
        rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
        rocprofvis_dm_table_row_t* row = (rocprofvis_dm_table_row_t*)handle;
        std::ofstream* file = (std::ofstream*)handle;

        if (row_index < m_merged_table.RowCount())
        { 
            auto columns = m_merged_table.GetMergedColumns();
            uint8_t op = m_merged_table.GetOperationValue(row_index);
            int column_counter = 0;
            DbInstance * db_instance = m_merged_table.GetDbInstanceForRow(m_db, row_index);
            ROCPROFVIS_ASSERT_MSG_RETURN(db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
            for (int column_index = 0; column_index < m_merged_table.MergedColumnCount(); column_index++)
            {
                if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_OPERATION)
                    continue;
                if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_STREAM_TRACK_ID)
                {
                    if (to_file == false)
                    {
                        if (op == kRocProfVisDmOperationDispatch || op == kRocProfVisDmOperationMemoryAllocate || op == kRocProfVisDmOperationMemoryCopy)
                        {
                            Numeric val = m_merged_table.GetMergeTableValue(op, row_index, column_index, m_db);
                            result = m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(val.data.u64).c_str());
                            if (result != kRocProfVisDmResultSuccess)
                                break;
                        }
                        else
                        {
                            result = m_db->BindObject()->FuncAddTableRowCell(row, "-1");
                            if (result != kRocProfVisDmResultSuccess)
                                break;
                        }
                    }
                }
                else
                if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_NULL)
                {
                    if (to_file)
                    {
                        if (column_counter > 0)
                        {
                            *file << ", ";
                        }
                        column_counter++;
                    }
                    else
                    {
                        result = m_db->BindObject()->FuncAddTableRowCell(row, "");
                        if (result != kRocProfVisDmResultSuccess)
                            break;
                    }
                }
                else
                if (columns[column_index].m_schema_index[op] == Builder::SCHEMA_INDEX_COUNTER_VALUE)
                {
                    Numeric val = m_merged_table.GetMergeTableValue(op, row_index, column_index, m_db);
                    std::string output = std::to_string(val.data.d);
                    if (to_file)
                    {
                        if (column_counter > 0)
                        {
                            *file << ", ";
                        }
                        *file << output;
                        column_counter++;
                    }
                    else
                    {
                        result = m_db->BindObject()->FuncAddTableRowCell(row, output.c_str());
                        if (result != kRocProfVisDmResultSuccess)
                            break;
                    }
                }
                else
                {
                    Numeric val = m_merged_table.GetMergeTableValue(op, row_index, column_index, m_db);
                    bool numeric_string = false;
                    const char* str = columns[column_index].m_type[op] == ColumnType::Null ? "" :
                        PackedTable::ConvertSqlStringReference(m_db, columns[column_index].m_schema_index[op], val.data.u64, db_instance->GuidIndex(), numeric_string);
                    std::string output;
                    if (str == nullptr)
                    {
                        output += std::to_string(val.data.u64);
                    }
                    else
                    {
                        output = str;
                    }
                    if (to_file)
                    {
                        if (columns[column_index].m_schema_index[op] != Builder::SCHEMA_INDEX_TRACK_ID)
                        {
                            if (column_counter > 0)
                            {
                                *file << ", ";
                            }
                            if (str != nullptr && !numeric_string)  *file << '"';
                            *file << output;
                            if (str != nullptr && !numeric_string)  *file << '"';
                            column_counter++;
                        }
                    }
                    else
                    {
                        result = m_db->BindObject()->FuncAddTableRowCell(row, output.c_str());
                        if (result != kRocProfVisDmResultSuccess)
                            break;
                    }
                }
            }
        }
        return result;
    }

    rocprofvis_dm_result_t TableProcessor::AddTableColumns(bool to_file, rocprofvis_dm_handle_t handle)
    {
        rocprofvis_dm_table_t* table = (rocprofvis_dm_table_t*)handle;
        std::ofstream* file = (std::ofstream*)handle;
        rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
        int column_index = 0;
        for (auto column : m_merged_table.GetMergedColumns())
        {
            if (column.m_max_schema_index != Builder::SCHEMA_INDEX_OPERATION)
            {
                if (to_file)
                {
                    if (column.m_max_schema_index != Builder::SCHEMA_INDEX_TRACK_ID && column.m_max_schema_index != Builder::SCHEMA_INDEX_STREAM_TRACK_ID)
                    {
                        if (column_index > 0)
                        {
                            *file << ", ";
                        }
                        *file << column.m_name;
                        column_index++;
                    }
                }
                else
                {
                    result = m_db->BindObject()->FuncAddTableColumn(table, column.m_name.c_str());
                    column_index++;
                    if (result != kRocProfVisDmResultSuccess)
                        break;
                }               
            }
        }
        return result;
    };

    rocprofvis_dm_result_t TableProcessor::AddAggregatedColumns(bool to_file, rocprofvis_dm_handle_t handle)
    {
        rocprofvis_dm_table_t* table = (rocprofvis_dm_table_t*)handle;
        std::ofstream* file = (std::ofstream*)handle;
        rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
        int column_index = 0;
        for (auto column : m_merged_table.GetAggregationSpec())
        {
            if (to_file)
            {
                    if (column_index > 0)
                    {
                        *file << ", ";
                    }
                    *file << column.public_name;
                    column_index++;
            }
            else
            {
                result = m_db->BindObject()->FuncAddTableColumn(table, column.public_name.c_str());
                column_index++;
                if (result != kRocProfVisDmResultSuccess)
                    break;
            }               
        }

        return result;
    };

    rocprofvis_dm_result_t TableProcessor::AddAggregatedCells(bool to_file, rocprofvis_dm_handle_t handle, uint32_t row_index)
    {
        rocprofvis_dm_table_row_t* row = (rocprofvis_dm_table_row_t*)handle;
        std::ofstream* file = (std::ofstream*)handle;
        rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
        auto r = m_merged_table.GetAggreagationRow(row_index);
        if (to_file)
        {
            bool numeric = false;
            try {
                size_t pos;
                double d = std::stod(r.first, &pos);
                numeric = pos == r.first.size(); 
            } catch (...) {
                numeric = false;
            }
            if (!numeric) *file << '"';
            *file << r.first;
            if (!numeric) *file << '"';
        }
        else
        {
            m_db->BindObject()->FuncAddTableRowCell(row, r.first.c_str());
        }
        auto aggr_spec = m_merged_table.GetAggregationSpec();
        for (auto param : aggr_spec)
        {  
            auto it = r.second.result.find(param.public_name);
            if (it == r.second.result.end())
                continue;
            auto data = it->second;
            std::string cell;
            if (data.type == NotNumeric)
            {
                cell = m_merged_table.GetAggregationStringByIndex(data.numeric.data.u64);
            } else
            if (data.type == NumericUInt64)
            {
                cell = std::to_string(data.numeric.data.u64);
            }
            else
            {
                cell = std::to_string(data.numeric.data.d);
            }
            if (to_file)
            {
                *file << ", " << cell;
            }
            else
            {
                result = m_db->BindObject()->FuncAddTableRowCell(row, cell.c_str());
                if (result != kRocProfVisDmResultSuccess)
                    break;
            }
        }
        return result;
    };

    rocprofvis_dm_result_t TableProcessor::ProcessCompoundQuery(rocprofvis_dm_table_t table, std::vector<rocprofvis_db_compound_query_command>& commands, bool updated)
    {
        auto GetRowMap = [&](int row_index, std::unordered_map<std::string, FilterExpression::Value> & map)
            {
                DbInstance * db_instance = m_merged_table.GetDbInstanceForRow(m_db, row_index);
                ROCPROFVIS_ASSERT_MSG_RETURN(db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, );
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
                                bool numeric_string = false;
                                const char* str = columns[column_index].m_type[op] == ColumnType::Null ? "" :
                                    PackedTable::ConvertSqlStringReference(m_db, columns[column_index].m_schema_index[op], val.data.u64, db_instance->GuidIndex(), numeric_string);
                                if (str != nullptr)
                                {
                                    if (numeric_string)
                                    {
                                        map[columns[column_index].m_name] = std::stod(str);
                                    }
                                    else
                                    {
                                        map[columns[column_index].m_name] = str;
                                    }
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
            rocprofvis_dm_result_t result = m_db->BindObject()->FuncAddTableColumn(table, "NumRecords");
            if (kRocProfVisDmResultSuccess == result)
            {
                result = m_db->BindObject()->FuncAddTableRowCell(row, std::to_string(num_rows).c_str());
            }
            return result;
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
        rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
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

        if (updated)
        {
            InvalidateFiltering();
            InvalidateGrouping();
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
                        int use_threads = (m_merged_table.RowCount()+10000) / 10000;
                        size_t thread_count = std::thread::hardware_concurrency() - 1;
                        if (use_threads < thread_count)
                            thread_count = use_threads;

                        std::mutex mtx;

                        auto task = [&](size_t start_row, size_t end_row, std::exception_ptr& eptr) {
                            std::unordered_map<std::string, FilterExpression::Value> row_map;
                            auto lfilter = filter;
                            for (size_t row_index = start_row; row_index < end_row; row_index++)
                            {
                                GetRowMap(row_index, row_map);
                                bool valid = true;
                                try {
                                    valid = lfilter.Evaluate(row_map);
                                }
                                catch (std::runtime_error err)
                                {
                                    valid = false;
                                    eptr = std::current_exception();
                                    break;
                                }
                                if (valid)
                                {
                                    std::lock_guard<std::mutex> lock(mtx);
                                    m_filter_lookup.insert(row_index);
                                }

                            }
                            };

                        std::vector<std::thread> threads;
                        std::exception_ptr eptr = nullptr;
                        size_t rows_per_task = thread_count == 0 ? 0 : m_merged_table.RowCount() / thread_count;
                        size_t leftover_rows_count = m_merged_table.RowCount() - (rows_per_task * thread_count);
                        for (int i = 0; i < thread_count; ++i)
                            threads.emplace_back(task, rows_per_task * i, rows_per_task * (i + 1), std::ref(eptr));
                        if (leftover_rows_count > 0)
                            threads.emplace_back(task, rows_per_task * thread_count, leftover_rows_count, std::ref(eptr));

                        for (auto& t : threads)
                            t.join();
                        if (eptr) {
                                std::rethrow_exception(eptr);
                            }
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
                    static int max_events_per_thread = 10000;
                    int use_threads = (m_merged_table.RowCount()+max_events_per_thread) / max_events_per_thread;
                    size_t thread_count = std::thread::hardware_concurrency() - 1;
                    if (use_threads < thread_count)
                        thread_count = use_threads;

                    std::vector<std::thread> threads;
                    size_t rows_per_task = thread_count == 0 ? 0 : m_merged_table.RowCount() / thread_count;
                    size_t leftover_rows_count = m_merged_table.RowCount() - (rows_per_task * thread_count);

                    if (m_merged_table.SetupAggregation(m_last_group_str, thread_count + 1))
                    {
                        auto task = [&](size_t thread_index, size_t start_row, size_t end_row) {
                            for (size_t row_index = start_row; row_index < end_row; row_index++)
                            {
                                if (!filtered || m_filter_lookup.count(row_index))
                                    m_merged_table.AggregateRow(m_db, row_index, thread_index);
                            }
                            };

                        for (int i = 0; i < thread_count-1; ++i)
                            threads.emplace_back(task, i, rows_per_task * i, rows_per_task * (i + 1));
                        threads.emplace_back(task, thread_count, rows_per_task * (thread_count-1), m_merged_table.RowCount());

                        for (auto& t : threads)
                            t.join();
                        m_merged_table.FinalizeAggregation();
                    }
                    else
                    {
                        m_merged_table.ClearAggregation();
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
                result = AddNumRecordsColumn(row, m_merged_table.AggregationRowCount());
                if (kRocProfVisDmResultSuccess == result)
                    result = AddAggregatedColumns(false, table);
                if (kRocProfVisDmResultSuccess == result)
                {
                    result = AddAggregatedCells(false, row, 0);
                }
            }
            else
            {
                result = AddAggregatedColumns(false, table);
                if (kRocProfVisDmResultSuccess == result)
                {
                    for (uint32_t row_index = offset; row_index < offset + limit; row_index++)
                    {
                        if (row_index >= m_merged_table.AggregationRowCount())
                            break;
                        rocprofvis_dm_table_row_t row =
                            m_db->BindObject()->FuncAddTableRow(table);
                        result = AddAggregatedCells(false, row, row_index);
                        if (result != kRocProfVisDmResultSuccess)
                            break;
                    }
                }
            }

        }
        else
        {
            InvalidateGrouping();

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

                result = AddNumRecordsColumn(row, table_size);
                if (kRocProfVisDmResultSuccess == result)
                    result = AddTableColumns(false, table);
                if (kRocProfVisDmResultSuccess == result)
                {
                    result = AddTableCells(false, row, 0);
                }
            }
            else
            {
                result = AddTableColumns(false, table);
                if (kRocProfVisDmResultSuccess == result)
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
                                    result = AddTableCells(false, row, sorted_index);
                                    if (result != kRocProfVisDmResultSuccess)
                                        break;
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
                            result = AddTableCells(false, row, sorted_index);
                            if (result != kRocProfVisDmResultSuccess)
                                break;
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
        ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
        ProfileDatabase* db = (ProfileDatabase*)callback_params->db;
        TableProcessor* table_processor = (TableProcessor*)callback_params->handle;
        void* func = (void*)&CallbackRunCompoundQuery;
        if (callback_params->future->Interrupted())
        {
            return 1;
        }

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
                    table_processor->m_tables[callback_params->track_id]->AddColumn(it->second.public_name, it->second.type, column_index, it->second.index);
                }
            }

            auto it = Builder::table_view_schema.find(Builder::TRACK_ID_PUBLIC_NAME);
            table_processor->m_tables[callback_params->track_id]->AddColumn(it->first, it->second.type, column_index, it->second.index); 
            if (service_data.op != kRocProfVisDmOperationNoOp)
            {
                it = Builder::table_view_schema.find(Builder::STREAM_TRACK_ID_PUBLIC_NAME);
                table_processor->m_tables[callback_params->track_id]->AddColumn(it->first, it->second.type, column_index, Builder::table_view_schema.size());
            }

            ProfileDatabase::FindTrackIDs(db, service_data, callback_params->db_instance,
                table_processor->m_tables[callback_params->track_id]->track_id, 
                table_processor->m_tables[callback_params->track_id]->stream_track_id);

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
                rocprofvis_dm_event_id_t value;
                value.bitfield.event_id = id;
                value.bitfield.event_node = callback_params->db_instance->GuidIndex();
                value.bitfield.event_op = op;
                table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, value.value);
            } else if (columns[column_index].m_schema_index == Builder::SCHEMA_INDEX_COUNTER_ID_RPD)
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
            (uint64_t)table_processor->m_tables[callback_params->track_id]->track_id);
        if (op != kRocProfVisDmOperationNoOp)
        {
            table_processor->m_tables[callback_params->track_id]->PlaceValue(column_index, 
                (uint64_t)table_processor->m_tables[callback_params->track_id]->stream_track_id);
        }

        callback_params->future->CountThisRow();

        return 0;
    }

    rocprofvis_dm_result_t TableProcessor::ExportToCSV(rocprofvis_dm_charptr_t file_path) {
        rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
        std::ofstream file(file_path);
        if (file.is_open())
        {
            bool delim = false;
            bool aggregated = !m_last_group_str.empty();
            
            if (aggregated)
            {
                result = AddAggregatedColumns(true, (rocprofvis_dm_handle_t)&file);
                if (kRocProfVisDmResultSuccess == result)
                {
                    file << "\n";
                    for (uint32_t row_index = 0; row_index < m_merged_table.AggregationRowCount(); row_index++)
                    {
                        result = AddAggregatedCells(true, (rocprofvis_dm_handle_t)&file, row_index);
                        if (kRocProfVisDmResultSuccess != result)
                        {
                            break;
                        }
                        file << "\n";
                    }
                }

            }
            else
            {
                bool filtered = !m_last_filter_str.empty();
                result = AddTableColumns(true, (rocprofvis_dm_handle_t)&file);
                if (kRocProfVisDmResultSuccess == result)
                {
                    file << "\n";
                    for (uint32_t row_index = 0; row_index < m_merged_table.RowCount(); row_index++)
                    {
                        uint32_t sorted_index = m_merged_table.SortedIndex(row_index);
                        if (!filtered || m_filter_lookup.count(sorted_index))
                        {
                            result = AddTableCells(true, (rocprofvis_dm_handle_t)&file, sorted_index);
                            if (kRocProfVisDmResultSuccess != result)
                            {
                                break;
                            }
                            file << "\n";
                        }
                    }
                }
            }
            file.close();
            return result;
        }
        else
        {
            return kRocProfVisDmResultDbAccessFailed;
        }
    }

}  // namespace DataModel
}  // namespace RocProfVis
