// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db.h"
#include "json.h"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <mutex>
#include <string>
#include <type_traits>
#include <variant>
#include <functional>
#include <vector>

namespace RocProfVis
{
namespace DataModel
{

    class SystemDatabase;

    struct rocprofvis_db_name_duration_t
    {
        std::string name;
        uint64_t count;
        uint64_t min_duration;
        uint64_t max_duration;
        uint64_t total_duration;
    };

    struct rocprofvis_db_duration_summary_query_t
    {
        DbInstance* db_instance;
        uint32_t track_id;
        std::string query;
    };

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
        // One statement is emitted once per guid. Every guid stays with it.
        std::vector<uint32_t> guid_ids;
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

        // An action already in flight runs with m_mutex released, so pause()
        // cannot stop it. Actions that take a lock of their own call this
        // after acquiring it to drop work a pause() or restart() cancelled.
        bool paused() {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_paused || m_pending;
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

    // jsoncpp on MSVC parses an integer token above 2^31 as a double. A 64-bit
    // query value is sent as {"lo","hi"} uint32 halves. Every uint32 is exact in
    // a double, so a half that arrives as a double converts back unchanged.
    inline bool ReadJsonUint32(jt::Json& json, uint32_t& val) {
        if (json.isLong()) {
            const long long number = json.getLong();
            if (number < 0 || number > static_cast<long long>(UINT32_MAX)) return false;
            val = static_cast<uint32_t>(number);
            return true;
        }
        if (!json.isNumber()) return false;
        const double number = json.getNumber();
        if (!std::isfinite(number) || number < 0.0 || std::trunc(number) != number ||
            number > static_cast<double>(UINT32_MAX))
            return false;
        val = static_cast<uint32_t>(number);
        return static_cast<double>(val) == number;
    }

    template<typename T>
    jt::Json JsonQueryValue(const T& val) {
        if constexpr (std::is_integral_v<T> && sizeof(T) == sizeof(uint64_t)) {
            uint64_t bits = 0;
            if constexpr (std::is_signed_v<T>) {
                const int64_t signed_val = static_cast<int64_t>(val);
                std::memcpy(&bits, &signed_val, sizeof(bits));
            } else {
                bits = static_cast<uint64_t>(val);
            }
            jt::Json pair;
            pair.setObject();
            pair.getObject()["lo"] = jt::Json(static_cast<uint32_t>(bits));
            pair.getObject()["hi"] = jt::Json(static_cast<uint32_t>(bits >> 32));
            return pair;
        } else {
            return jt::Json(val);
        }
    }

    template<typename T>
    bool ReadJsonQueryValue(jt::Json& json, T& val) {
        if constexpr (std::is_same_v<T, bool>) {
            if (!json.isBool()) return false;
            val = json.getBool();
        } else if constexpr (std::is_same_v<T, float>) {
            if (!json.isNumber()) return false;
            val = static_cast<float>(json.getNumber());
        } else if constexpr (std::is_same_v<T, double>) {
            if (!json.isNumber()) return false;
            val = json.getNumber();
        } else if constexpr (std::is_integral_v<T> && sizeof(T) == sizeof(uint64_t)) {
            // A 64-bit reader also receives 32-bit values written as plain numbers,
            // e.g. uint32 track ids read into size_t.
            if (!json.isObject()) {
                uint32_t number = 0;
                if (!ReadJsonUint32(json, number)) return false;
                val = static_cast<T>(number);
                return true;
            }
            if (!json.contains("lo") || !json.contains("hi")) return false;
            uint32_t lo = 0;
            uint32_t hi = 0;
            if (!ReadJsonUint32(json["lo"], lo) || !ReadJsonUint32(json["hi"], hi)) return false;
            const uint64_t bits = (static_cast<uint64_t>(hi) << 32) | lo;
            if constexpr (std::is_signed_v<T>) {
                int64_t signed_bits = 0;
                std::memcpy(&signed_bits, &bits, sizeof(bits));
                val = static_cast<T>(signed_bits);
            } else {
                val = static_cast<T>(bits);
            }
        } else if constexpr (std::is_integral_v<T>) {
            // A uint32 above 2^31 also arrives as a double. It is exact, unlike a
            // full 64-bit value. Packed track ids use the top bits and can land here.
            if constexpr (std::is_unsigned_v<T> && sizeof(T) <= sizeof(uint32_t)) {
                uint32_t number = 0;
                if (!ReadJsonUint32(json, number) ||
                    number > static_cast<uint32_t>(std::numeric_limits<T>::max()))
                    return false;
                val = static_cast<T>(number);
            } else {
                if (!json.isLong()) return false;
                val = static_cast<T>(json.getLong());
            }
        } else if constexpr (std::is_same_v<T, std::string>) {
            if (!json.isString()) return false;
            val = json.getString();
        } else {
            return false;
        }
        return true;
    }

    class ParamSerializer {
        jt::Json obj_;
    public:
        ParamSerializer() { obj_.setObject(); }

        explicit ParamSerializer(const std::string& text)
        {
            auto [status, json] = jt::Json::parse(text);
            if(status != jt::Json::success)
            {
                throw std::runtime_error(std::string("JSON parse error: ") +
                    jt::Json::StatusToString(status));
            }
            if(!json.isObject())
            {
                throw std::runtime_error("Expected JSON object");
            }
            obj_ = std::move(json);
        }

        template<typename T>
        void set(const std::string& tag, const T& val) {
            obj_.getObject()[tag] = JsonQueryValue(val);
        }

        // Set an array of values
        template<typename T>
        void setArray(const std::string& tag, const std::vector<T>& values) {
            jt::Json arr;
            arr.setArray();
            for (const auto& v : values)
                arr.getArray().push_back(JsonQueryValue(v));
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
            return ReadJsonQueryValue(json_[tag], val);
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
                if (!ReadJsonQueryValue(elem, val)) return false;
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
                if (m_timer.paused()) return;
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
        uint32_t AddPackedTable();
        static int CallbackRunCompoundQuery(void* data, int argc, void* stmt, char** azColName);

    private:
        rocprofvis_dm_result_t ProcessCompoundQuery(Future* future, rocprofvis_dm_table_t table, std::vector<rocprofvis_db_compound_query_command>& commands, bool updated);
        static std::string Trim(const std::string& str);
        rocprofvis_dm_result_t AddTableCells(bool to_file, rocprofvis_dm_handle_t handle, uint32_t row_index);
        rocprofvis_dm_result_t AddTableColumns(bool to_file, rocprofvis_dm_handle_t handle);
        rocprofvis_dm_result_t AddAggregatedColumns(bool to_file, rocprofvis_dm_handle_t handle);
        rocprofvis_dm_result_t AddAggregatedCells(bool to_file, rocprofvis_dm_handle_t handle, uint32_t row_index);
        rocprofvis_dm_result_t ExecuteNameDurationSummary(
            Future* future,
            std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>>& queries,
            std::vector<rocprofvis_db_compound_query_command>& commands,
            rocprofvis_dm_handle_t handle);

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