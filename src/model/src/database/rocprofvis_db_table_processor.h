// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_systems.h"
#include "json.h"

namespace RocProfVis
{
namespace DataModel
{

    class Database;

    typedef enum rocprofvis_dm_track_search_id_t
    {
        kRPVTrackSearchIdThreads,
        kRPVTrackSearchIdThreadSamples,
        kRPVTrackSearchIdDispatches,
        kRPVTrackSearchIdMemAllocs,
        kRPVTrackSearchIdMemCopies,
        kRPVTrackSearchIdCounters,
        kRPVTrackSearchIdStreams,
        kRPVTrackSearchIdUnknown,
    } rocprofvis_dm_track_search_id_t;

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

    typedef struct rocprofvis_db_compound_query_info {
        uint32_t track;
        uint32_t guid_id;
    } rocprofvis_db_compound_query_info;


    typedef std::variant<std::monostate, std::string, uint64_t, double> rocprofvis_table_cell_t;

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

    class ParamSerializer {
        jt::Json obj_;
    public:
        ParamSerializer() { obj_.setObject(); }

        template<typename T>
        void set(const std::string& tag, const T& val) {
            obj_.getObject()[tag] = jt::Json(val);
        }

        // Set an array of values
        template<typename T>
        void setArray(const std::string& tag, const std::vector<T>& values) {
            jt::Json arr;
            arr.setArray();
            for (const auto& v : values)
                arr.getArray().push_back(jt::Json(v));
            obj_.getObject()[tag] = std::move(arr);
        }

        std::string toString() const { return obj_.toString(); }
    };


    class ParamDeserializer {
        jt::Json json_;
    public:
        explicit ParamDeserializer(const std::string& text) {
            auto [status, json] = jt::Json::parse(text);
            if (status != jt::Json::success)
                throw std::runtime_error(
                    std::string("JSON parse error: ") + jt::Json::StatusToString(status));
            if (!json.isObject())
                throw std::runtime_error("Expected JSON object");
            json_ = std::move(json);
        }

        template<typename T>
        bool get(const std::string& tag, T& val) {
            if (!json_.contains(tag)) return false;
            jt::Json& j = json_[tag];
            if constexpr (std::is_same_v<T, bool>) {
                if (!j.isBool()) return false;
                val = j.getBool();
            } else if constexpr (std::is_same_v<T, float>) {
                if (!j.isNumber()) return false;
                val = static_cast<float>(j.getNumber());
            } else if constexpr (std::is_same_v<T, double>) {
                if (!j.isNumber()) return false;
                val = j.getNumber();
            } else if constexpr (std::is_integral_v<T>) {
                if (!j.isLong()) return false;
                val = static_cast<T>(j.getLong());
            } else if constexpr (std::is_same_v<T, std::string>) {
                if (!j.isString()) return false;
                val = j.getString();
            } else {
                return false;
            }
            return true;
        }

        // Get an array of values
        template<typename T>
        bool getArray(const std::string& tag, std::vector<T>& values) {
            if (!json_.contains(tag)) return false;
            jt::Json& j = json_[tag];
            if (!j.isArray()) return false;

            values.clear();
            for (auto& elem : j.getArray()) {
                T val;
                if constexpr (std::is_same_v<T, bool>) {
                    if (!elem.isBool()) return false;
                    val = elem.getBool();
                } else if constexpr (std::is_same_v<T, float>) {
                    if (!elem.isNumber()) return false;
                    val = static_cast<float>(elem.getNumber());
                } else if constexpr (std::is_same_v<T, double>) {
                    if (!elem.isNumber()) return false;
                    val = elem.getNumber();
                } else if constexpr (std::is_integral_v<T>) {
                    if (!elem.isLong()) return false;
                    val = static_cast<T>(elem.getLong());
                } else if constexpr (std::is_same_v<T, std::string>) {
                    if (!elem.isString()) return false;
                    val = elem.getString();
                } else {
                    return false;
                }
                values.push_back(val);
            }
            return true;
        }
    };


    class TableProcessor
    {

    public:
        TableProcessor(SystemDatabase* db) : m_db(db) {

            m_timer.setAction([this] {
                std::lock_guard<std::mutex> lock(m_lock);
                this->m_merged_table.Clear();
                this->m_tracks.clear();
                });

        };

        static rocprofvis_dm_result_t
            BuildTableSemanticSubQuery(
                rocprofvis_dm_table_use_case_enum_t use_case,
                rocprofvis_dm_charptr_t filter,
                rocprofvis_dm_charptr_t group,
                rocprofvis_dm_charptr_t group_cols,
                rocprofvis_dm_charptr_t sort_column,
                rocprofvis_dm_sort_order_t sort_order,
                uint64_t max_count,
                uint64_t offset,
                bool count_only,
                bool sample_query,
                rocprofvis_dm_string_t& query);
        static bool IsCompoundQuery(const char* query, std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>>& queries, std::set<uint32_t>& tracks,
            std::vector<rocprofvis_db_compound_query_command>& commands);
        static std::string QueryWithoutCommands(const char* query); // Unused
        rocprofvis_dm_result_t ExecuteCompoundQuery(Future* future,
            std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>>& queries,
            std::set<uint32_t>& tracks,
            std::vector<rocprofvis_db_compound_query_command> commands,
            rocprofvis_dm_handle_t handle,
            bool query_updated);
        std::string ParseSortCommand(std::string param, bool& order);
        void SaveCurrentQuery(std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>>& queries) { m_current_queries = queries; };
        bool IsCurrentQuery(std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>>& queries);
        rocprofvis_dm_result_t ExportToCSV(rocprofvis_dm_charptr_t file_path);
        static int CallbackRunCompoundQuery(void* data, int argc, void* stmt, char** azColName);

    private:
        rocprofvis_dm_result_t ProcessCompoundQuery(rocprofvis_dm_table_t table, std::vector<rocprofvis_db_compound_query_command>& commands, bool updated);
        static std::string Trim(const std::string& str);
        rocprofvis_dm_result_t AddTableCells(bool to_file, rocprofvis_dm_handle_t handle, uint32_t row_index);
        rocprofvis_dm_result_t AddTableColumns(bool to_file, rocprofvis_dm_handle_t handle);
        rocprofvis_dm_result_t AddAggregatedColumns(bool to_file, rocprofvis_dm_handle_t handle);
        rocprofvis_dm_result_t AddAggregatedCells(bool to_file, rocprofvis_dm_handle_t handle, uint32_t row_index);

    private:
        SystemDatabase* m_db;
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
        // track id, query string, query info.
        std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>> m_current_queries;

        static constexpr const char* QUERY_COMMAND_TAG = "-- CMD:";
    };


}  // namespace DataModel
}  // namespace RocProfVis