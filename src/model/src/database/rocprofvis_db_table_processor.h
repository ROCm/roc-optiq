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
        kRPVTableDataTypeSearch,
        kRPVTableDataTypesNum
    } rocprofvis_db_compound_table_type;

    typedef struct rocprofvis_db_compound_query_command {
        std::string name;
        std::string parameter;
    } rocprofvis_db_compound_query_command;

    typedef struct rocprofvis_db_compound_query {
        std::string query;
        uint32_t track;
        uint32_t guid_id;
    } rocprofvis_db_compound_query;

    class RestartableTimer {
    public:
        RestartableTimer() : m_stop(false), m_paused(false) {}

        ~RestartableTimer() {
            stop();
        }

        void setAction(std::function<void()> action) {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_action = std::move(action);
        }

        void restart(std::chrono::milliseconds delay) {
            std::lock_guard<std::mutex> lock(m_mutex);

            if (!m_action) return;

            m_delay = delay;
            m_paused = false;
            m_pending = true;

            if (!m_worker.joinable()) {
                m_worker = std::thread([this] { run(); });
            }

            m_cv.notify_all();
        }

        void pause() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_paused = true;
            m_pending = false;
            m_cv.notify_all();
        }

        void stop() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_stop = true;
                m_cv.notify_all();
            }
            if (m_worker.joinable()) {
                m_worker.join();
            }
        }

    private:
        void run() {
            std::unique_lock<std::mutex> lock(m_mutex);
            while (!m_stop) {
                if (!m_pending) {
                    m_cv.wait(lock, [this] { return m_pending || m_stop; });
                    if (m_stop) break;
                }

                m_pending = false;
                auto wake_time = std::chrono::steady_clock::now() + m_delay;

                while (!m_stop) {
                    if (m_paused) {
                        m_cv.wait(lock, [this] { return !m_paused || m_stop; });
                        break; 
                    }

                    if (m_cv.wait_until(lock, wake_time) == std::cv_status::timeout)
                        break; 
                    else
                        wake_time = std::chrono::steady_clock::now() + m_delay; 
                }

                if (!m_stop && !m_paused && !m_pending && m_action) {
                    lock.unlock();
                    m_action();
                    lock.lock();
                }
            }
        }

        std::function<void()> m_action;
        std::chrono::milliseconds m_delay{1000};
        std::atomic<bool> m_stop;
        bool m_paused = false;
        std::condition_variable m_cv;
        std::mutex m_mutex;
        std::thread m_worker;
        bool m_pending = false;
    };


    class TableProcessor
    {

    public:
        TableProcessor(ProfileDatabase* db) : m_db(db) { 

            m_timer.setAction([this] {
                std::lock_guard<std::mutex> lock(m_lock);
                this->m_merged_table.Clear(); 
                this->m_tracks.clear();
                });

        };

        static bool IsCompoundQuery(const char* query, std::vector<rocprofvis_db_compound_query>& queries, std::set<uint32_t>& tracks, 
            std::vector<rocprofvis_db_compound_query_command>& commands);
        static std::string QueryWithoutCommands(const char* query);
        rocprofvis_dm_result_t ExecuteCompoundQuery(Future* future, 
            std::vector<rocprofvis_db_compound_query>& queries, 
            std::set<uint32_t>& tracks,
            std::vector<rocprofvis_db_compound_query_command> commands, 
            rocprofvis_dm_handle_t handle, 
            rocprofvis_db_compound_table_type type,
            bool query_updated);
        std::string ParseSortCommand(std::string param, bool& order);
        void SaveCurrentQuery(rocprofvis_dm_charptr_t query) { m_current_query = query; };
        bool IsCurrentQuery(rocprofvis_dm_charptr_t query) { return m_current_query == query; };
        rocprofvis_dm_result_t ExportToCSV(rocprofvis_dm_charptr_t file_path);

    private:
        rocprofvis_dm_result_t ProcessCompoundQuery(rocprofvis_dm_table_t table, std::vector<rocprofvis_db_compound_query_command>& commands, bool updated);
        static int CallbackRunCompoundQuery(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
        static std::string Trim(const std::string& str);
        rocprofvis_dm_result_t AddTableCells(bool to_file, rocprofvis_dm_handle_t handle, uint32_t row_index);
        rocprofvis_dm_result_t AddTableColumns(bool to_file, rocprofvis_dm_handle_t handle);
        rocprofvis_dm_result_t AddAggregatedColumns(bool to_file, rocprofvis_dm_handle_t handle);
        rocprofvis_dm_result_t AddAggregatedCells(bool to_file, rocprofvis_dm_handle_t handle, uint32_t row_index);

    private:
        ProfileDatabase* m_db;
        std::vector<std::unique_ptr<PackedTable>> m_tables;
        std::set<uint32_t> m_tracks;
        PackedTable m_merged_table;
        RestartableTimer m_timer;
        std::string m_last_filter_str;
        std::string m_last_group_str;
        std::unordered_set<uint32_t> m_filter_lookup;
        bool m_sort_order = true;
        std::string m_sort_column;
        std::mutex m_lock;
        std::string m_current_query;

        static constexpr const char* QUERY_COMMAND_TAG = "-- CMD:";
    };


}  // namespace DataModel
}  // namespace RocProfVis