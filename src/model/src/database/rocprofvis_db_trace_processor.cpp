// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
#ifdef ROCPROFVIS_PERFETTO_ENABLED

#include "rocprofvis_db_trace_processor.h"
#include "perfetto/ext/base/file_utils.h"
#include "rocprofvis_c_interface.h"
#include <fstream>
#include <sstream>
#include <filesystem>
#include "json.h"

namespace RocProfVis
{
    namespace DataModel
    {
        struct PerfettoHandle;
        struct PerfettoQuery;

#ifdef _WIN32
#  define PERFETTO_API __declspec(dllimport)
#else
#  define PERFETTO_API
#endif

        extern "C" {
            PERFETTO_API PerfettoHandle* Perfetto_Create();
            PERFETTO_API void            Perfetto_Destroy(PerfettoHandle*);
            PERFETTO_API bool            Perfetto_LoadModules(PerfettoHandle*);
            PERFETTO_API bool            Perfetto_ReadTrace(PerfettoHandle*, const char*);
            PERFETTO_API bool            Perfetto_Flush(PerfettoHandle*);
            PERFETTO_API bool            Perfetto_NotifyEndOfFile(PerfettoHandle*);
            PERFETTO_API PerfettoQuery*  Perfetto_Query(PerfettoHandle*, const char*);
            PERFETTO_API bool            Perfetto_QueryNext(PerfettoQuery*);
            PERFETTO_API bool            Perfetto_QueryOk(PerfettoQuery*);
            PERFETTO_API uint32_t        Perfetto_QueryColCount(PerfettoQuery*);
            PERFETTO_API const char*     Perfetto_QueryColName(PerfettoQuery*, uint32_t);
            PERFETTO_API int             Perfetto_QueryValueType(PerfettoQuery*, uint32_t);
            PERFETTO_API int64_t         Perfetto_QueryGetLong(PerfettoQuery*, uint32_t);
            PERFETTO_API double          Perfetto_QueryGetDouble(PerfettoQuery*, uint32_t);
            PERFETTO_API const char*     Perfetto_QueryGetString(PerfettoQuery*, uint32_t);
            PERFETTO_API void            Perfetto_QueryDestroy(PerfettoQuery*);
            PERFETTO_API const char*     Perfetto_TableList(PerfettoHandle*);
        }


        namespace {

            bool CreateTable(sqlite3* db,
                PerfettoHandle* h,
                const std::string& table_name,
                uint32_t& col_count_out) {

                // Get column names via schema query
                std::string schema_sql = "SELECT * FROM " + table_name + " LIMIT 0";
                PerfettoQuery* schema = Perfetto_Query(h, schema_sql.c_str());
                if (!schema) return false;

                col_count_out = Perfetto_QueryColCount(schema);
                if (col_count_out == 0) {
                    Perfetto_QueryDestroy(schema);
                    return false;
                }

                std::string create = "CREATE TABLE IF NOT EXISTS " + table_name + " (";
                for (uint32_t i = 0; i < col_count_out; i++) {
                    if (i > 0) create += ", ";
                    create += Perfetto_QueryColName(schema, i);
                }
                create += ");";
                Perfetto_QueryDestroy(schema);

                char* err = nullptr;
                if (sqlite3_exec(db, create.c_str(), nullptr, nullptr, &err) != SQLITE_OK) {
                    spdlog::warn("TraceConverter: CREATE TABLE {} failed: {}",
                        table_name, err ? err : "unknown");
                    sqlite3_free(err);
                    return false;
                }
                return true;
            }

            void ExportTable(sqlite3* db,
                PerfettoHandle* h,
                const std::string& table_name) {

                uint32_t col_count = 0;
                if (!CreateTable(db, h, table_name, col_count)) return;

                // Build INSERT statement
                std::string insert = "INSERT OR IGNORE INTO " + table_name + " VALUES (";
                for (uint32_t i = 0; i < col_count; i++)
                    insert += (i > 0 ? ",?" : "?");
                insert += ");";

                sqlite3_stmt* stmt = nullptr;
                if (sqlite3_prepare_v2(db, insert.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                    spdlog::warn("TraceConverter: prepare INSERT for {} failed: {}",
                        table_name, sqlite3_errmsg(db));
                    return;
                }

                std::string select_sql = "SELECT * FROM " + table_name;
                PerfettoQuery* rows = Perfetto_Query(h, select_sql.c_str());
                if (!rows) {
                    sqlite3_finalize(stmt);
                    return;
                }

                sqlite3_exec(db, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr);

                uint32_t row_count = 0;
                while (Perfetto_QueryNext(rows)) {
                    for (uint32_t i = 0; i < col_count; i++) {
                        switch (Perfetto_QueryValueType(rows, i)) {
                        case 1:  // long
                            sqlite3_bind_int64(stmt, i + 1,
                                Perfetto_QueryGetLong(rows, i));
                            break;
                        case 2:  // double
                            sqlite3_bind_double(stmt, i + 1,
                                Perfetto_QueryGetDouble(rows, i));
                            break;
                        case 3:  // string
                            sqlite3_bind_text(stmt, i + 1,
                                Perfetto_QueryGetString(rows, i),
                                -1, SQLITE_TRANSIENT);
                            break;
                        case 0:  // null
                        case 4:  // bytes — fall back to null for now
                        default:
                            sqlite3_bind_null(stmt, i + 1);
                            break;
                        }
                    }

                    if (sqlite3_step(stmt) != SQLITE_DONE)
                        spdlog::warn("TraceConverter: insert row in {} failed: {}",
                            table_name, sqlite3_errmsg(db));

                    sqlite3_reset(stmt);
                    row_count++;
                }

                if (!Perfetto_QueryOk(rows))
                    spdlog::warn("TraceConverter: query error in table {}", table_name);

                sqlite3_exec(db, "COMMIT;", nullptr, nullptr, nullptr);
                sqlite3_finalize(stmt);
                Perfetto_QueryDestroy(rows);

                spdlog::debug("TraceConverter: {} — {} rows", table_name, row_count);
            }

            bool ExportToSQLite(PerfettoHandle* h, const std::string& output_path) {
                namespace fs = std::filesystem;
                fs::create_directories(fs::path(output_path).parent_path());

                sqlite3* db = nullptr;
                if (sqlite3_open(output_path.c_str(), &db) != SQLITE_OK) {
                    spdlog::error("TraceConverter: cannot open output db: {}",
                        sqlite3_errmsg(db));
                    sqlite3_close(db);
                    return false;
                }

                // Performance pragmas for bulk insert
                sqlite3_exec(db, "PRAGMA journal_mode=WAL;",   nullptr, nullptr, nullptr);
                sqlite3_exec(db, "PRAGMA synchronous=NORMAL;", nullptr, nullptr, nullptr);
                sqlite3_exec(db, "PRAGMA cache_size=-65536;",  nullptr, nullptr, nullptr);
                sqlite3_exec(db, "PRAGMA temp_store=MEMORY;",  nullptr, nullptr, nullptr);

                // Get table list
                const char* table_list = Perfetto_TableList(h);
                if (!table_list || table_list[0] == '\0') {
                    spdlog::warn("TraceConverter: no tables found");
                    sqlite3_close(db);
                    return true;
                }

                // Parse comma-separated table list
                std::istringstream ss(table_list);
                std::string table_name;
                while (std::getline(ss, table_name, ',')) {
                    if (!table_name.empty()) {
                        spdlog::debug("TraceConverter: exporting {}", table_name);
                        ExportTable(db, h, table_name);
                    }
                }



                sqlite3_close(db);
                return true;
            }

            std::string HashFileIdentity(const std::string& path) {
                namespace fs = std::filesystem;

                // Get file metadata
                fs::path   fspath(path);
                std::string name = fspath.filename().string();
                uintmax_t   size = fs::file_size(fspath);
                auto        ftime = fs::last_write_time(fspath);
                int64_t     time_val = ftime.time_since_epoch().count();

                // Combine into a single hash using std::hash
                size_t hash = 0;

                auto combine = [&hash](size_t val) {
                    // Standard hash_combine pattern
                    hash ^= val + 0x9e3779b9 + (hash << 6) + (hash >> 2);
                    };

                combine(std::hash<std::string>{}(name));
                combine(std::hash<uintmax_t>{}(size));
                combine(std::hash<int64_t>{}(time_val));

                // Convert to hex string
                std::ostringstream oss;
                oss << std::hex << std::setfill('0')
                    << std::setw(sizeof(size_t) * 2) << hash;

                return oss.str();
            }

            // Check for Perfetto protobuf format
            // Must satisfy all conditions:
            // 1. Starts with 0x0a (field 1, wire type 2 = TracePacket)
            // 2. Followed by varint bytes (high bit set = continuation byte)
            //    OR a small length followed by binary protobuf data
            // 3. Contains non-ASCII bytes (>= 0x80) within the first 16 bytes
            //    — guaranteed in any real protobuf, impossible in valid JSON
            static bool IsPerfettoProtobuf(const uint8_t* magic, size_t len) {
                if (len < 4) return false;
                if (magic[0] != 0x0a) return false;

                // Check that byte 1 is a varint — for any real trace packet
                // the length will be > 127 so the continuation bit will be set,
                // OR it's a tiny packet but then byte 2 will be a protobuf tag (binary)
                // JSON after \n would be whitespace, '[', '{', or printable ASCII (< 0x80)

                // Scan first 16 bytes for non-ASCII content — protobuf always has it,
                // JSON/text never will in the header bytes
                bool has_binary = false;
                for (size_t i = 1; i < std::min(len, size_t(16)); i++) {
                    if (magic[i] >= 0x80) {
                        has_binary = true;
                        break;
                    }
                }

                if (!has_binary) return false;

                // Additional check: byte 1 should be a valid varint start
                // (either continuation byte >= 0x80, or small value < 0x80 
                //  followed by binary protobuf field tag)
                // This rules out \n followed by printable text
                return true;
            }

        } // anonymous namespace


        bool TraceConverter::Convert(
            const std::string& source_path,
            const std::string& output_path,
            std::function<void(float)> progress_callback) {

            namespace fs = std::filesystem;
            namespace chr = std::chrono;

            if (!fs::exists(source_path)) {
                spdlog::error("TraceConverter: source not found: {}", source_path);
                return false;
            }

            spdlog::info("TraceConverter: converting {} ({:.1f} MB)",
                source_path,
                fs::file_size(source_path) / 1e6);

            if (progress_callback) progress_callback(0.0f);

            PerfettoHandle* h = Perfetto_Create();
            if (!h) {
                spdlog::error("TraceConverter: failed to create TraceProcessor");
                return false;
            }

            if (!Perfetto_LoadModules(h))
                spdlog::warn("TraceConverter: SQL module load failed — continuing");

            if (progress_callback) progress_callback(0.05f);

            // ReadTrace calls NotifyEndOfFile internally — do NOT call it again
            spdlog::info("TraceConverter: reading trace...");
            auto t0 = chr::steady_clock::now();

            if (!Perfetto_ReadTrace(h, source_path.c_str())) {
                spdlog::error("TraceConverter: read failed");
                Perfetto_Destroy(h);
                return false;
            }

            auto read_ms = chr::duration_cast<chr::milliseconds>(
                chr::steady_clock::now() - t0).count();
            spdlog::info("TraceConverter: read+finalize done in {}ms", read_ms);

            if (progress_callback) progress_callback(0.7f);

            spdlog::info("TraceConverter: exporting to {}", output_path);
            auto t1 = chr::steady_clock::now();

            bool ok = ExportToSQLite(h, output_path);

            auto export_ms = chr::duration_cast<chr::milliseconds>(
                chr::steady_clock::now() - t1).count();
            spdlog::info("TraceConverter: export done in {}ms", export_ms);

            Perfetto_Destroy(h);

            if (progress_callback) progress_callback(1.0f);

            spdlog::info("TraceConverter: {} total {}ms",
                ok ? "complete" : "failed",
                chr::duration_cast<chr::milliseconds>(
                    chr::steady_clock::now() - t0).count());

            return ok;
        }



    rocprofvis_db_type_t GoogleTraceProcessor::Detect(
        rocprofvis_db_filename_t filename) {

        std::ifstream file(filename, std::ios::binary);
        if (!file) {
            spdlog::debug("Cannot open file {}!", filename);
            return kAutodetect;
        }

        // Read enough bytes to skip leading whitespace and still check magic
        uint8_t magic[256] = {};
        file.read(reinterpret_cast<char*>(magic), sizeof(magic));
        std::streamsize bytes_read = file.gcount();
        if (bytes_read < 4) {
            return kAutodetect;
        }

        // Find first non-whitespace byte
        size_t start = 0;
        while (start < static_cast<size_t>(bytes_read) &&
            (magic[start] == ' '  ||
                magic[start] == '\t' ||
                magic[start] == '\n' ||
                magic[start] == '\r')) {
            start++;
        }

        if (start >= static_cast<size_t>(bytes_read)) {
            return kAutodetect;  // file is all whitespace
        }

        uint8_t first = magic[start];

        // SQLite database — check before JSON since it won't have whitespace
        if (start == 0 && memcmp(magic, "SQLite format 3", 15) == 0) {
            sqlite3 *db;
            if (sqlite3_open_v2(filename, &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK)
            {
                sqlite3_close(db);
                return rocprofvis_db_type_t::kAutodetect;
            }

            if (DetectTable(db, "__intrinsic_trace_file") == SQLITE_OK) {
                sqlite3_close(db);
                return rocprofvis_db_type_t::kGoogleSqlite;
            }
            sqlite3_close(db);
        }

        // Gzipped trace
        if (start == 0 && magic[0] == 0x1f && magic[1] == 0x8b) {
            return kPerfettoTrace;
        }

        // Perfetto protobuf
        if (IsPerfettoProtobuf(magic, bytes_read)) {
            return kPerfettoTrace;
        }

        // Chrome JSON trace
        if (first == '[' || first == '{') {
            return kChromeTrace;
        }

        return kAutodetect;
    }


    int GoogleTraceProcessor::CallbackCacheTable(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        void* func = (void*)&CallbackCacheTable;
        rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
        GoogleTraceProcessor* db = (GoogleTraceProcessor*)callback_params->db;
        DatabaseCache * ref_tables = (DatabaseCache *)callback_params->handle;
        std::lock_guard<std::mutex> lock(db->m_lock);
        if (callback_params->future->GetProcessedRowsCount() == 0)
        {
            for (int i = 0; i < argc; i++)
            {
                ref_tables->AddTableColumn(callback_params->query[kRPVCacheTableName].c_str(), azColName[i], (rocprofvis_db_data_type_t)sqlite3_column_type(stmt, i));
            }
        }

        uint64_t id = db->Sqlite3ColumnInt64(func, stmt, azColName, 0);
        ref_tables->AddTableRow(callback_params->query[kRPVCacheTableName].c_str(), id);
        for (int i = 0; i < argc; i++)
        {
            rocprofvis_db_data_type_t col_type = (rocprofvis_db_data_type_t)sqlite3_column_type(stmt, i);
            ref_tables->AddTableCell(callback_params->query[kRPVCacheTableName].c_str(), id, i,
                (char*)db->Sqlite3ColumnText(func, stmt, azColName, i));
        }

        callback_params->future->CountThisRow();
        return 0;
    }

    int GoogleTraceProcessor::CallbackAddTrack(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        void* func = (void*)&CallbackAddTrack;
        rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
        GoogleTraceProcessor* db = (GoogleTraceProcessor*)callback_params->db;
        uint32_t db_index = callback_params->db_instance->GuidIndex();

        bool leaf = db->Sqlite3ColumnInt64(func, stmt, azColName, 7);
        if (leaf)
        {
            rocprofvis_dm_track_params_t track_params = { 0 };
            track_params.track_indentifiers.db_instance = callback_params->db_instance;
            track_params.track_indentifiers.track_id = (rocprofvis_dm_track_id_t)db->NumTracks();
            uint64_t track_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 0);
            std::string type = db->Sqlite3ColumnText(func, stmt, azColName, 1);
            if (type != "process_track" && type != "thread_track" && type != "process_counter_track")
                return 0;
            std::string name = db->Sqlite3ColumnText(func, stmt, azColName, 2);
            uint64_t parent_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 3);
            uint64_t source_arg_set_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 4);
            uint64_t machine_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 5);
            uint64_t child_count = db->Sqlite3ColumnInt64(func, stmt, azColName, 6);
            track_params.track_indentifiers.id[TRACK_ID_PID] = parent_id;
            track_params.track_indentifiers.id[TRACK_ID_TID] = track_id;
            track_params.track_indentifiers.is_numeric[TRACK_ID_NODE] = true;
            track_params.track_indentifiers.is_numeric[TRACK_ID_PID] = true;
            track_params.track_indentifiers.is_numeric[TRACK_ID_TID] = true;
            track_params.track_indentifiers.tag[TRACK_ID_NODE] = Builder::NODE_ID_SERVICE_NAME;
            track_params.min_ts = ULLONG_MAX;
            track_params.max_ts = 0;
            track_params.record_count = 0;
            track_params.track_indentifiers.source_type = kRPVSystemSourcePerfetto;

            if (type == "process_track" || type == "thread_track")
            {
                std::string items = db->CachedTables(db_index)->GetTableCell("event_track_items", track_id, "items_count");
                if (items.empty())
                {
                    return 0;
                }
                track_params.record_count = std::atol(items.c_str());
                track_params.track_indentifiers.category = kRocProfVisDmRegionTrack;
                track_params.track_indentifiers.tag[TRACK_ID_PID] = Builder::PROCESS_ID_SERVICE_NAME;
                track_params.track_indentifiers.tag[TRACK_ID_TID] = Builder::THREAD_ID_SERVICE_NAME;
                track_params.query[kRPVPerfettoQuerySlice].push_back(db->m_query_factory.GetPerfettoEventSliceQuery());
                track_params.query[kRPVPerfettoQueryTable].push_back(db->m_query_factory.GetPerfettoRegionTableQuery());
            }
            else
            {
                std::string items = db->CachedTables(db_index)->GetTableCell("counter_track_items", track_id, "items_count");
                if (items.empty())
                {
                    return 0;
                }
                track_params.track_indentifiers.category = kRocProfVisDmPmcTrack;
                track_params.record_count = std::atol(items.c_str());
                track_params.track_indentifiers.tag[TRACK_ID_PID] = Builder::AGENT_ID_SERVICE_NAME;
                track_params.track_indentifiers.tag[TRACK_ID_TID] = Builder::QUEUE_ID_SERVICE_NAME;
                track_params.query[kRPVPerfettoQuerySlice].push_back(db->m_query_factory.GetPerfettoCounterSliceQuery());
                track_params.query[kRPVPerfettoQueryTable].push_back(db->m_query_factory.GetPerfettoPerformanceCountersTableQuery());
            }
            
            if (!track_params.record_count)
                return 0;
            track_params.track_indentifiers.category = type == "process_counter_track" ? kRocProfVisDmPmcTrack : kRocProfVisDmRegionTrack;

            if (type == "process_track"  || type == "process_counter_track")
            {
                std::string spid = db->CachedTables(db_index)->GetTableCell(type.c_str(), track_params.track_indentifiers.track_id, "upid");
                if (!spid.empty())
                {
                    uint64_t pid = std::atol(spid.c_str());
                    track_params.track_indentifiers.name[TRACK_ID_PID] = 
                        db->CachedTables(db_index)->GetTableCell("Process", pid, "name");
                    track_params.track_indentifiers.id[TRACK_ID_PID] = pid;
                    track_params.track_indentifiers.process_id = pid;
                }
                track_params.track_indentifiers.name[TRACK_ID_TID] = name;
            }
            else
            if (type == "thread_track")
            {
     
                std::string spid = db->CachedTables(db_index)->GetTableCell("Thread", track_id, "upid");
                if (!spid.empty())
                {
                    uint64_t pid = std::atol(spid.c_str());
                    track_params.track_indentifiers.name[TRACK_ID_PID] = 
                        db->CachedTables(db_index)->GetTableCell("Process", pid, "name");
                    track_params.track_indentifiers.id[TRACK_ID_PID] = pid;
                    track_params.track_indentifiers.process_id = pid;
                }

                if (name.empty())
                {
                    track_params.track_indentifiers.name[TRACK_ID_TID] =
                        db->CachedTables(db_index)->GetTableCell("Thread", track_id, "name");
                }
                else
                {
                    track_params.track_indentifiers.name[TRACK_ID_TID] = name;
                }
            }

            if (kRocProfVisDmResultSuccess != db->AddTrackProperties(track_params)) 
                return 1;

            db->m_track_map[track_id] = track_params.track_indentifiers.track_id;

            if (db->BindObject()->FuncAddTrack(db->BindObject()->trace_object, db->TrackPropertiesLast()) != kRocProfVisDmResultSuccess) 
                return 1; 

            if (db->BindObject()->FuncAddTopologyNode(db->BindObject()->trace_object, &track_params.track_indentifiers) != kRocProfVisDmResultSuccess) 
                return 1; 

            if (db->CachedTables(db_index)->PopulateTrackExtendedDataTemplate(db, db_index,  "Node", track_params.track_indentifiers.id[TRACK_ID_NODE]) != kRocProfVisDmResultSuccess) 
                return 1;

            if (type == "process_track" || type == "thread_track")
            {
                if (db->CachedTables(db_index)->PopulateTrackExtendedDataTemplate(db, db_index, "Process", track_params.track_indentifiers.id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess)
                    return 1;
                if (db->CachedTables(db_index)->PopulateTrackExtendedDataTemplate(db, db_index, "Thread", track_params.track_indentifiers.id[TRACK_ID_TID]) != kRocProfVisDmResultSuccess) 
                    return 1;
            }

            if (type == "process_counter_track")
            {
                if (db->CachedTables(0)->PopulateTrackExtendedDataTemplate(db, 0, "Agent", track_params.track_indentifiers.id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess) 
                    return 1;
                db->CachedTables(0)->AddTableCell("PMC", track_params.track_indentifiers.id[TRACK_ID_TID], "id", kRPVDataTypeInt, 
                    std::to_string(track_params.track_indentifiers.id[TRACK_ID_TID]).c_str());
                std::string unit = db->CachedTables(db_index)->GetTableCell(type.c_str(), track_params.track_indentifiers.track_id, "unit");
                db->CachedTables(0)->AddTableCell("PMC", track_params.track_indentifiers.id[TRACK_ID_TID], "unit", kRPVDataTypeString, unit.c_str());
                std::string description = db->CachedTables(db_index)->GetTableCell(type.c_str(), track_params.track_indentifiers.track_id, "description");
                db->CachedTables(0)->AddTableCell("PMC", track_params.track_indentifiers.id[TRACK_ID_TID], "description", kRPVDataTypeString, description.c_str());
                std::string upid = db->CachedTables(db_index)->GetTableCell(type.c_str(), track_params.track_indentifiers.track_id, "upid");
                db->CachedTables(0)->AddTableCell("PMC", track_params.track_indentifiers.id[TRACK_ID_TID], "upid", kRPVDataTypeInt, upid.c_str());
                if (db->CachedTables(0)->PopulateTrackExtendedDataTemplate(db, 0, "PMC", track_params.track_indentifiers.id[TRACK_ID_TID]) != kRocProfVisDmResultSuccess) 
                    return 1;

            }
        }

        return 0;
    }

    rocprofvis_dm_result_t
        GoogleTraceProcessor::BuildTrackQuery(rocprofvis_dm_index_t index,
            rocprofvis_dm_index_t type,
            rocprofvis_dm_string_t& query,
            uint32_t split_count,
            uint32_t split_index)
    {
        std::stringstream ss;
        DbInstance* db_instance = (DbInstance*)TrackPropertiesAt(index)->track_indentifiers.db_instance;
        int               size = static_cast<int>(TrackPropertiesAt(index)->query[type].size());
        ROCPROFVIS_ASSERT_MSG_RETURN(size, "Error! SQL query cannot be empty!", kRocProfVisDmResultUnknownError);
        ss << query << " FROM (";
        for(int i = 0; i < size; i++)
        {
            if(i > 0) ss << " UNION ALL ";
            ss << TrackPropertiesAt(index)->query[type][i];

            ss << " WHERE track_id = " << TrackPropertiesAt(index)->track_indentifiers.id[TRACK_ID_TID];

            if (split_count > 1)
            {
                uint64_t trace_time = TraceProperties()->db_inst_end_time[db_instance->GuidIndex()] - TraceProperties()->db_inst_start_time[db_instance->GuidIndex()];
                if (trace_time > 0)
                {
                    uint64_t time_bucket_size = trace_time / split_count;
                    ss << " and " << Builder::START_SERVICE_NAME << " BETWEEN " << TraceProperties()->db_inst_start_time[db_instance->GuidIndex()] + (time_bucket_size * split_index);
                    ss << " and " << TraceProperties()->db_inst_start_time[db_instance->GuidIndex()] + (time_bucket_size * (split_index+1));
                }
            }
        }
        ss << ") ";
        query = ss.str();
        return kRocProfVisDmResultSuccess;
    }

    rocprofvis_dm_result_t GoogleTraceProcessor::LoadInformationTables(Future* future) {

        std::vector<std::thread> threads;

        std::vector<std::pair<std::string, std::string>> info_table_list = {
            {"Node", 
            "SELECT "
            "0 AS id, "
            "MAX(CASE WHEN name = 'system_name'    THEN COALESCE(str_value, CAST(int_value AS TEXT)) END) AS system_name, "
            "MAX(CASE WHEN name = 'system_version' THEN COALESCE(str_value, CAST(int_value AS TEXT)) END) AS system_version, "
            "MAX(CASE WHEN name = 'system_release' THEN COALESCE(str_value, CAST(int_value AS TEXT)) END) AS system_release, "
            "MAX(CASE WHEN name = 'system_machine' THEN COALESCE(str_value, CAST(int_value AS TEXT)) END) AS system_machine, "
            "MAX(CASE WHEN name = 'trace_uuid'     THEN COALESCE(str_value, CAST(int_value AS TEXT)) END) AS trace_uuid "
            "FROM metadata; "
            },
            {"Process", "SELECT *, 'N/A' as environment from __intrinsic_process;"},
            {"Agent", "SELECT P.id, P.pid, P.name, P.machine_id, 'GPU' as type, 0 as type_index  from __intrinsic_process P INNER JOIN process_counter_track C on C.upid == P.id;"},
            {"Thread", 
                "SELECT TT.id, T.name, TT.machine_id, upid, tid, start_ts, end_ts, is_main_thread from thread_track TT JOIN __intrinsic_thread T on TT.utid == T.id "
                " UNION ALL "
                "SELECT PT.id, PT.name, PT.machine_id, PT.upid, P.pid, start_ts, end_ts, 0 from process_track PT JOIN __intrinsic_process P on PT.upid == P.id;"
            },
            {"thread_track", "SELECT id, utid from thread_track;"},
            {"process_track", "SELECT id, upid from process_track;"},
            {"process_counter_track", "SELECT id, unit, description, upid from process_counter_track;"},
            {"event_track_items", "select track_id as id, count(*) as items_count from __intrinsic_slice group by track_id;"},
            {"counter_track_items", "select track_id as id, count(*) as items_count from counter group by track_id;"},
            {"data_flow_topology",
                "SELECT "
                "CASE "
                "WHEN COUNT(*) = COUNT(DISTINCT slice_out) THEN 'chain' "
                "ELSE 'star' "
                "END AS flow_topology "
                "FROM flow; " },
        };

        return RunCacheQueries(future, info_table_list, &CallbackCacheTable, false);
    }


    rocprofvis_dm_result_t GoogleTraceProcessor::CreateIndexes()
    {
        std::vector<std::string> vec;
        vec.push_back("CREATE INDEX IF NOT EXISTS track_ts_slice_idx ON __intrinsic_slice(track_id, ts);");
        vec.push_back("CREATE INDEX IF NOT EXISTS track_ts_counter_idx ON rocpd_op(track_id, ts);");
        return  ExecuteTransaction( vec);
    }

    rocprofvis_dm_result_t  GoogleTraceProcessor::ReadTraceMetadata(Future* future)
    {
        namespace fs = std::filesystem;
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        while (true)
        {
            ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
            TraceProperties()->events_count[kRocProfVisDmOperationLaunch] = 0;
            TraceProperties()->events_count[kRocProfVisDmOperationDispatch] = 0;
            TraceProperties()->tracks_info_restored = true;
            TraceProperties()->trace_duration = 0;
            TraceProperties()->num_db_instances = 1;
            TraceProperties()->db_inst_start_time.push_back(UINT64_MAX);
            TraceProperties()->db_inst_end_time.push_back(0);

            uint32_t load_id = 0;

            ShowProgress(50, "Read perfetto file", kRPVDbBusy, future);

            fs::path   fspath(BindObject()->config_path);
            fspath = fspath / HashFileIdentity(Path());
            fspath = fspath / "perfetto";
            std::string stem = fs::path(Path()).stem().string();
            std::string temp = stem;
            stem += ".tpdb";
            temp += ".tmp";
            fs::path temp_path(fspath / temp);
            fspath = fspath / stem;

            if (!fs::exists(fspath))
            {
                if (!TraceConverter::Convert(Path(), temp_path.string())) {
                    spdlog::error("Conversion failed");
                    break;
                }
                else
                {
                    std::error_code ec;
                    fs::rename(temp_path, fspath, ec);
                    if (ec) {
                        spdlog::error(ec.message());
                        break;
                    }
                }
            }

            CreateDbNode(fspath.string().c_str());
            Open();

            DbInstances().push_back({ SingleNodeDbInstance(), "" });

            ShowProgress(5, "Create indexes", kRPVDbBusy, future);
            CreateIndexes();

            ShowProgress(5, "Load Information Tables", kRPVDbBusy, future);
            LoadInformationTables(future);

            ShowProgress(5, "Adding event tracks", kRPVDbBusy, future );

            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, DbInstancePtrAt(0),
                "WITH child_counts AS( "
                "    SELECT parent_id, COUNT(*) AS child_count "
                "    FROM track "
                "    WHERE parent_id IS NOT NULL "
                "    GROUP BY parent_id ) "
                "SELECT t.id, t.type, t.name, t.parent_id, t.source_arg_set_id, t.machine_id, "
                "COALESCE(cc.child_count, 0) AS child_count, "
                "CASE "
                "WHEN cc.child_count IS NOT NULL    THEN 0 "
                "ELSE                                    1 "
                "END AS track_role "
                "FROM track t "
                "LEFT JOIN child_counts cc ON t.id = cc.parent_id;"
                ,&CallbackAddTrack)) break;

            ShowProgress(10, "Loading strings", kRPVDbBusy, future );

            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, DbInstancePtrAt(0), std::string("SELECT distinct(category) FROM __intrinsic_slice; ").c_str(), &CallBackAddString)) break;
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, DbInstancePtrAt(0), std::string("SELECT distinct(name) FROM __intrinsic_slice; ").c_str(), &CallBackAddString)) break;

            ShowProgress(5, "Collecting track properties", kRPVDbBusy, future);
            TraceProperties()->trace_duration = 0;
            if (kRocProfVisDmResultSuccess !=
                ExecuteQueryForAllTracksAsync(
                    kRocProfVisDmIncludePmcTracks,  kRPVPerfettoQuerySlice,
                    "SELECT MIN(startTs), MAX(endTs), MIN(event_level), MAX(event_level), ",
                    "", &CallbackGetTrackProperties,
                    [](rocprofvis_dm_track_params_t* params, rocprofvis_dm_charptr_t query) -> std::string { (void) params; return query; },
                    [](rocprofvis_dm_track_params_t* params) { (void) params; },
                    DbInstances()))
            {
                break;
            }

            ShowProgress(5, "Collecting track histogram", kRPVDbBusy, future);
            BuildHistogram(future, 500);

            TraceProperties()->metadata_loaded=true;
            BindObject()->FuncMetadataLoaded(BindObject()->trace_object);
            ShowProgress(100-future->Progress(), "Trace metadata successfully loaded", kRPVDbSuccess, future );
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
        ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);
    }



    int GoogleTraceProcessor::CallbackGetTrackProperties(void* data, int argc, sqlite3_stmt* stmt,
        char** azColName)
    {
        ROCPROFVIS_ASSERT_MSG_RETURN(argc == 5, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        void *func = (void*)&CallbackGetTrackProperties;
        rocprofvis_db_sqlite_callback_parameters* callback_params =
            (rocprofvis_db_sqlite_callback_parameters*) data;
        ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
        uint32_t db_instance = callback_params->db_instance->GuidIndex();
        GoogleTraceProcessor*            db = (GoogleTraceProcessor*) callback_params->db;
        if(callback_params->future->Interrupted()) return SQLITE_ABORT;
        uint32_t index = db->Sqlite3ColumnInt(func, stmt, azColName, 4);
        db->TrackPropertiesAt(index)->min_ts       = std::min((rocprofvis_dm_timestamp_t)db->Sqlite3ColumnInt64(func, stmt, azColName, 0),db->TrackPropertiesAt(index)->min_ts);
        db->TrackPropertiesAt(index)->max_ts       = std::max((rocprofvis_dm_timestamp_t)db->Sqlite3ColumnInt64(func, stmt, azColName, 1),db->TrackPropertiesAt(index)->max_ts);
        db->TrackPropertiesAt(index)->min_value    = std::min((rocprofvis_dm_value_t)db->Sqlite3ColumnDouble(func, stmt, azColName, 2),db->TrackPropertiesAt(index)->min_value);
        db->TrackPropertiesAt(index)->max_value    = std::max((rocprofvis_dm_value_t)db->Sqlite3ColumnDouble(func, stmt, azColName, 3),db->TrackPropertiesAt(index)->max_value);

        db->TraceProperties()->db_inst_start_time[db_instance] = std::min(db->TraceProperties()->db_inst_start_time[db_instance], db->TrackPropertiesAt(index)->min_ts);
        db->TraceProperties()->db_inst_end_time[db_instance]  = std::max(db->TraceProperties()->db_inst_end_time[db_instance],db->TrackPropertiesAt(index)->max_ts);

        db->TraceProperties()->trace_duration  = std::max(db->TraceProperties()->trace_duration,db->TraceProperties()->db_inst_end_time[db_instance]-db->TraceProperties()->db_inst_start_time[db_instance]);
        callback_params->future->CountThisRow();
        return 0;
    }

    int GoogleTraceProcessor::CallBackAddString(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        void*  func = (void*)&CallBackAddString;
        rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
        ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
        GoogleTraceProcessor* db = (GoogleTraceProcessor*)callback_params->db;
        if(callback_params->future->Interrupted()) return SQLITE_ABORT;
        std::lock_guard<std::mutex> lock(db->m_lock);
        char* str = (char*)db->Sqlite3ColumnText(func, stmt, azColName, 0);
        auto it = db->m_string_map.find(str);
        uint32_t string_index = it != db->m_string_map.end() ? it->second : db->BindObject()->FuncAddString(db->BindObject()->trace_object, str);
        db->m_string_map[str] = string_index;
        callback_params->future->CountThisRow();
        return 0;
    }


    int GoogleTraceProcessor::CallbackAddAnyRecord(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
        ROCPROFVIS_ASSERT_MSG_RETURN(argc == rocprofvis_db_perfetto_slice_query_format::NUM_PARAMS+1,
            ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        void *func = (void*)&CallbackAddAnyRecord;
        rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
        ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
        uint32_t db_instance = callback_params->db_instance->GuidIndex();
        GoogleTraceProcessor* db = (GoogleTraceProcessor*)callback_params->db;
        if(callback_params->future->Interrupted()) return SQLITE_ABORT;
        rocprofvis_db_record_data_t record;
        uint32_t track = db->Sqlite3ColumnInt(func, stmt, azColName, 9);
        record.event.id.bitfield.event_op = db->Sqlite3ColumnInt(func, stmt, azColName, 0);
        record.event.id.bitfield.event_node = callback_params->db_instance->GuidIndex();
        
        if (record.event.id.bitfield.event_op > 0) {       
            record.event.id.bitfield.event_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 5);
            record.event.timestamp = db->Sqlite3ColumnInt64(func, stmt, azColName, 1);
            record.event.duration = db->Sqlite3ColumnInt64(func, stmt, azColName, 2) - record.event.timestamp;
            record.event.timestamp-=db->TraceProperties()->db_inst_start_time[db_instance];
            std::string category = db->Sqlite3ColumnText(func, stmt, azColName, 3);
            auto it = db->m_string_map.find(category);
            uint32_t string_index = it != db->m_string_map.end() ? it->second : db->BindObject()->FuncAddString(db->BindObject()->trace_object, category.c_str());
            record.event.category = string_index;
            std::string name = db->Sqlite3ColumnText(func, stmt, azColName, 4);
            it = db->m_string_map.find(name);
            string_index = it != db->m_string_map.end() ? it->second : db->BindObject()->FuncAddString(db->BindObject()->trace_object, name.c_str());
            record.event.symbol = string_index;
            record.event.level   = static_cast<rocprofvis_dm_event_level_t>(db->Sqlite3ColumnInt64(func, stmt, azColName, 6));
            if (kRocProfVisDmResultSuccess != db->RemapStringIds(record)) return 0;
        }
        else {
            record.pmc.timestamp = db->Sqlite3ColumnInt64(func, stmt, azColName, 1);
            record.pmc.timestamp-=db->TraceProperties()->db_inst_start_time[db_instance];
            record.pmc.value = db->Sqlite3ColumnDouble(func, stmt, azColName,6);
            callback_params->future->SetRuntimeStorageValue(kRPVFutureStorageSampleValue, record.pmc.value);
        }
        if(db->BindObject()->FuncAddRecord(
            (*(slice_array_t*) callback_params->handle)[track],
            record) != kRocProfVisDmResultSuccess)
            return 1;
        callback_params->future->CountThisRow();
        return 0;

    }


    void GoogleTraceProcessor::BuildSliceQueryMap(slice_query_map_t& slice_query_map, rocprofvis_dm_track_params_t* props, rocprofvis_db_query_type_t query_type)
    {
        DbInstance* instance = (DbInstance*)props->track_indentifiers.db_instance;
        std::string q = props->query[query_type][0] + " WHERE ( track_id = "; 
        slice_query_map[q][instance->GuidIndex()] = std::to_string(props->track_indentifiers.id[TRACK_ID_TID]);
    }

    rocprofvis_dm_result_t GoogleTraceProcessor::BuildHistogram(Future* future, uint32_t desired_bins) {

        const char* start_time_substring = "%START_TIME%";

        typedef struct store_params {
            uint32_t id;
            uint32_t track_id;
            uint32_t bucket_num;
            uint32_t events_count;
            double bucket_value;
        } store_params;

        rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;

        uint64_t trace_length = TraceProperties()->trace_duration;

        uint64_t bucket_size = (trace_length + desired_bins) / desired_bins;

        TraceProperties()->histogram_bucket_size = bucket_size;
        TraceProperties()->histogram_bucket_count = (trace_length + bucket_size) / bucket_size;


        std::string histogram_query_prefix = GetHistogramQueryPrefix(bucket_size);
        std::string histogram_query_suffix = GetHistogramQuerySuffix();


        for (auto& file_node : m_db_nodes)
        {
            std::vector<store_params> v;
            TemporaryDbInstance db_instance(file_node->node_id);

            auto insert_start_time = [&](rocprofvis_dm_track_params_t* params, rocprofvis_dm_charptr_t query) -> std::string {
            DbInstance* params_db_instance = (DbInstance*)params->track_indentifiers.db_instance;
            std::string str = query;
            size_t pos = str.find(start_time_substring);
            if (pos != std::string::npos) {
                str.replace(pos, strlen(start_time_substring), std::to_string(TraceProperties()->db_inst_start_time[params_db_instance->GuidIndex()]));
            }
            return str;
            };

            result = ExecuteQueryForAllTracksAsync(
                0, kRPVPerfettoQuerySlice,
                histogram_query_prefix.c_str(),
                histogram_query_suffix.c_str(), &CallbackMakeHistogramPerTrack,
                insert_start_time,
                [](rocprofvis_dm_track_params_t* params) { (void) params; },
                DbInstances());


            if (kRocProfVisDmResultSuccess == result)
            {
                std::string histogram_query = std::string("SELECT (") + Builder::START_SERVICE_NAME + " - " +
                    start_time_substring + ") / " +
                    std::to_string(bucket_size) + " AS bucket, COUNT(*), AVG(" + Builder::COUNTER_VALUE_SERVICE_NAME+"), ";

                result = ExecuteQueryForAllTracksAsync(
                    kRocProfVisDmIncludePmcTracksOnly, kRPVPerfettoQuerySlice,
                    histogram_query.c_str(),
                    "GROUP BY bucket", &CallbackMakeHistogramPerTrack,
                    insert_start_time,
                    [](rocprofvis_dm_track_params_t* params) { (void) params; },
                    DbInstances());
            }

            if (kRocProfVisDmResultSuccess == result)
            {
                // use last known value for all missing buckets in counter's track histogram
                for (int i = 0; i < NumTracks(); i++)
                {
                    auto & data = TrackPropertiesAt(i)->histogram;

                    if (data.size() > 1)
                    {

                        auto it = data.begin();
                        auto next = std::next(it);

                        while (next != data.end()) {
                            uint32_t x0 = it->first;
                            uint32_t x1 = next->first;
                            double   y0 = it->second.second;

                            for (uint32_t x = x0 + 1; x < x1; ++x) {
                                data.emplace(x, std::make_pair(0, y0));
                            }

                            it = next;
                            ++next;
                        }
                    }
                }
            }       
        }
        for (int i = 0; i < NumTracks(); i++)
        {
            for (auto& [key, value] : TrackPropertiesAt(i)->histogram)
            {
                TraceProperties()->histogram[key] += value.first;
            }
        }
        return result;
    }


    rocprofvis_dm_result_t GoogleTraceProcessor::BuildTableStringIdFilter( 
        rocprofvis_dm_num_string_table_filters_t num_string_table_filters, 
        rocprofvis_dm_string_table_filters_t string_table_filters, 
        bool include_substring,
        bool include_category,
        bool partial_matching,
        table_string_id_filter_map_t& filter)
    {
        rocprofvis_dm_result_t result = kRocProfVisDmResultNotLoaded;
        if (num_string_table_filters > 0)
        {
            std::string query = "( ";
            for (int i = 0; i < num_string_table_filters; i++)
            {
                if (i > 0) 
                {
                    query += partial_matching ? " OR " : " AND ";
                }
                query += include_substring ? "(name " : "(LOWER(name) ";
                query += include_substring ? "LIKE '%" + std::string(string_table_filters[i]) + "%'" : "= LOWER('" + std::string(string_table_filters[i]) + "')";

                if (include_category)
                {
                    query += " OR LOWER(category) ";
                    query += include_substring ? "LIKE '%" + std::string(string_table_filters[i]) + "%'" : "= LOWER('" + std::string(string_table_filters[i]) + "')";
                }
                query += ")";
            }

            filter[kRocProfVisDmOperationLaunch][0] = std::move(query);
            result = kRocProfVisDmResultSuccess;
        }
        return result;
    }

    rocprofvis_dm_string_t GoogleTraceProcessor::GetEventOperationQuery(const rocprofvis_dm_event_operation_t operation)
    {
        switch(operation)
        {
            case kRocProfVisDmOperationLaunch:
            {
                return m_query_factory.GetPerfettoRegionTableQuery();
            }
            default:
            {
                return "";
            }
        }
    }

    void GoogleTraceProcessor::GetTrackIdentifierIndices(
            int column_index, char** azColName,
            rocprofvis_db_sqlite_track_identifier_index_t& track_ids_indices)
    {
        std::string column_name = azColName[column_index];

        if (column_name == Builder::TRACK_ID_SERVICE_NAME)
        {
            track_ids_indices.sub_process_index = column_index;
            track_ids_indices.process_index = column_index;
        }
       
    }

    bool GoogleTraceProcessor::FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance, uint32_t& out_track)
    {
        auto it = m_track_map.find(id_subprocess);
        if (it != m_track_map.end())
        {
            out_track = it->second;
            return true;
        }
        return false;
    }



    int GoogleTraceProcessor::CallbackAddExtInfo(void* data, int argc, sqlite3_stmt* stmt, char** azColName) {
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        void*  func = (void*)&CallbackAddExtInfo;
        rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
        ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
        GoogleTraceProcessor* db = (GoogleTraceProcessor*)callback_params->db;
        std::string value_type;
        std::string arg_name;
        std::string arg_value;
        std::string level;
        uint32_t track_id=0;
        
        {
            rocprofvis_db_ext_data_t record;
            if (callback_params->future->Interrupted()) return SQLITE_ABORT;
            record.category = callback_params->query[kRPVCacheTableName].c_str();


            for (int i = 0; i < argc; i++)
            {
                record.name = azColName[i];
                std::string aux_str = record.name;
                if (aux_str == Builder::START_PUBLIC_NAME || aux_str == Builder::END_PUBLIC_NAME)
                {
                    uint64_t timestamp = db->Sqlite3ColumnInt64(func, stmt, azColName, i);
                    timestamp -= db->TraceProperties()->db_inst_start_time[callback_params->db_instance->GuidIndex()];
                    aux_str = std::to_string(timestamp);
                    record.data = aux_str.c_str();
                    record.type = kRPVDataTypeInt;
                }
                else
                {
                    record.type = (rocprofvis_db_data_type_t)sqlite3_column_type(stmt, i);
                    record.data = (char*)db->Sqlite3ColumnText(func, stmt, azColName, i);
                }
                std::string record_data = record.data;
                record.category_enum = GetColumnDataCategory(*db->GetCategoryEnumMap(), callback_params->operation, record.name);
                record.db_instance = callback_params->db_instance->GuidIndex();
                if (record.data != nullptr) {
                    if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) != kRocProfVisDmResultSuccess) return 1;
                }
                if (aux_str == "value_type")
                {
                    value_type = record_data;
                } else
                if (aux_str == "arg_name")
                {
                    arg_name = record_data;
                } else
                if (aux_str == "track_id")
                {
                    track_id = std::stol(record_data.c_str());
                }
                else
                if (aux_str == "level")
                {
                    level = record_data;;
                }
                else
                if (aux_str == "int_value" || aux_str == "real_value" || aux_str == "string_value")
                {
                    if (!record_data.empty())
                    {
                        arg_value = record_data;
                    }
                }
            }

            record.category = "Track";
            record.name = "trackId";
            record.type = kRPVDataTypeInt;
            uint32_t trackId;
            if (!db->FindTrack(kRocProfVisDmRegionTrack, 0, track_id, callback_params->db_instance->GuidIndex(), trackId))
            {
                trackId = -1;
            }
            std::string trackIdStr = std::to_string(trackId);
            record.data = trackIdStr.c_str();
            record.category_enum = kRocProfVisEventEssentialDataTrack;
            record.db_instance = callback_params->db_instance->GuidIndex();
            if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
                kRocProfVisDmResultSuccess)
                return 1;

            record.category = "Track";
            record.name = "levelForTrack";
            record.type = kRPVDataTypeInt;
            record.data = level.c_str();
            record.category_enum = kRocProfVisEventEssentialDataLevel;
            if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle, record) !=
                kRocProfVisDmResultSuccess)
                return 1;

        }
        {
            rocprofvis_db_argument_data_t record;
            record.position = 0;
            record.type = value_type.c_str();
            record.name = arg_name.c_str();
            record.value = arg_value.c_str();
            if (db->BindObject()->FuncAddArgDataRecord(callback_params->handle, record) != kRocProfVisDmResultSuccess) return 1;
        }

        callback_params->future->CountThisRow();
        return 0;
    }

    int GoogleTraceProcessor::CallbackAddFlowTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        ROCPROFVIS_ASSERT_MSG_RETURN(argc == 8, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
        void*  func = (void*)&CallbackAddFlowTrace;
        rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
        ROCPROFVIS_ASSERT_MSG_RETURN(callback_params->db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, 1);
        uint32_t db_instance = callback_params->db_instance->GuidIndex();
        GoogleTraceProcessor* db = (GoogleTraceProcessor*)callback_params->db;
        if(callback_params->future->Interrupted()) return SQLITE_ABORT;
        rocprofvis_db_flow_data_t record;

        record.id.bitfield.event_op = db->Sqlite3ColumnInt(func, stmt, azColName,0 );
        record.id.bitfield.event_node = callback_params->db_instance->GuidIndex();
        if (db->FindTrack(
            kRocProfVisDmRegionTrack,0, 
            db->Sqlite3ColumnInt(func, stmt, azColName,2), 
            callback_params->db_instance->GuidIndex(), record.track_id))
        {
            record.id.bitfield.event_id = db->Sqlite3ColumnInt64(func, stmt, azColName, 1 );
            record.time = db->Sqlite3ColumnInt64(func, stmt, azColName, 3 );
            record.time-=db->TraceProperties()->db_inst_start_time[db_instance];
            record.end_time = db->Sqlite3ColumnInt64(func, stmt, azColName, 4);  
            record.end_time-=db->TraceProperties()->db_inst_start_time[db_instance];
            std::string category = db->Sqlite3ColumnText(func, stmt, azColName, 5);
            auto it = db->m_string_map.find(category );
            uint32_t string_index = it != db->m_string_map.end() ? it->second : db->BindObject()->FuncAddString(db->BindObject()->trace_object, category.c_str());
            record.category_id = string_index;
            std::string name = db->Sqlite3ColumnText(func, stmt, azColName, 6);
            it = db->m_string_map.find(name);
            string_index = it != db->m_string_map.end() ? it->second : db->BindObject()->FuncAddString(db->BindObject()->trace_object, name.c_str());
            record.symbol_id = string_index;
            record.level = static_cast<rocprofvis_dm_event_level_t>(db->Sqlite3ColumnInt64(func, stmt, azColName, 7));

            if(kRocProfVisDmResultSuccess != db->RemapStringIds(record)) return 0;
            if (db->BindObject()->FuncAddFlow(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
        }
        callback_params->future->CountThisRow();
        return 0;
    }


    rocprofvis_dm_result_t  GoogleTraceProcessor::ReadFlowTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future)
    {
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        while (true)
        {
            ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
            ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
            rocprofvis_dm_flowtrace_t flowtrace = BindObject()->FuncAddFlowTrace(BindObject()->trace_object, event_id);
            ROCPROFVIS_ASSERT_MSG_BREAK(flowtrace, ERROR_FLOW_TRACE_CANNOT_BE_NULL);
            std::stringstream query;
            if (event_id.bitfield.event_op == kRocProfVisDmOperationLaunch)
            {
                std::string flow_topology = CachedTables(DbInstancePtrAt(0)->GuidIndex())->GetTableCell("data_flow_topology", 0, "flow_topology");
                if (flow_topology == "chain")
                {

                    query << "WITH RECURSIVE params AS (SELECT " << event_id.bitfield.event_id << " AS target_slice_id ),";
                    query << R"(flow_chain AS (
                          SELECT target_slice_id AS slice_id
                          FROM params

                          UNION

                          SELECT f.slice_in
                          FROM flow_chain fc
                          JOIN flow f ON f.slice_out = fc.slice_id

                          UNION

                          SELECT f.slice_out
                          FROM flow_chain fc
                          JOIN flow f ON f.slice_in = fc.slice_id
                        )
                        SELECT
                          CASE
                            WHEN fc.slice_id = p.target_slice_id THEN 0
                            WHEN EXISTS (
                              SELECT 1 FROM flow f
                              WHERE f.slice_out = p.target_slice_id
                                AND f.slice_in  = fc.slice_id
                            )                                     THEN 2
                            ELSE                                       1
                          END          AS direction,
                          fc.slice_id,
                          s.track_id,
                          s.ts         AS start,
                          (s.ts + s.dur) AS end,
                          s.category,
                          s.name,
                          s.depth
                        FROM flow_chain fc
                        JOIN params p ON 1 = 1
                        JOIN __intrinsic_slice s ON s.id = fc.slice_id
                        WHERE fc.slice_id != p.target_slice_id 
                        ORDER BY s.ts
                        )";
                }
                else
                {
                    query << "select 2, F.slice_in, SI.track_id, SI.ts as start, (SI.ts + si.dur) as end, SI.category, SI.name, SI.depth "
                        "from flow F "
                        "INNER JOIN __intrinsic_slice SO on SO.id = F.slice_out "
                        "INNER JOIN __intrinsic_slice SI on SI.id = F.slice_in "
                        "where F.slice_out = ";
                    query << event_id.bitfield.event_id;
                    query << " UNION "
                        "select 1, F.slice_out, SO.track_id, SO.ts as start, (SO.ts + SO.dur) as end, SO.category, SO.name, SO.depth "
                        "from flow F "
                        "INNER JOIN __intrinsic_slice SO on SO.id = F.slice_out "
                        "INNER JOIN __intrinsic_slice SI on SI.id = F.slice_in "
                        "where F.slice_in = ";
                    query << event_id.bitfield.event_id;
                }
                ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
                if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, DbInstancePtrAt(0), query.str().c_str(), flowtrace, &CallbackAddFlowTrace)) break;
            }
            ShowProgress(100, "Flow trace successfully loaded!",kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
        ShowProgress(0, "Flow trace not loaded!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);
    }


    rocprofvis_dm_result_t  GoogleTraceProcessor::ReadExtEventInfo(
        rocprofvis_dm_event_id_t event_id, 
        Future* future){
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        while (true)
        {
            ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
            ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
            rocprofvis_dm_extdata_t extdata = BindObject()->FuncAddExtData(BindObject()->trace_object, event_id);
            ROCPROFVIS_ASSERT_MSG_BREAK(extdata, ERROR_EXT_DATA_CANNOT_BE_NULL);
            std::stringstream query;
            if (event_id.bitfield.event_op == kRocProfVisDmOperationLaunch)
            {
                query << "select S.id, S.ts as timestamp, S.dur as duration, S.category, S.name, S.depth as level, S.track_id, "
                    "A.key as arg_name, A.int_value, A.string_value, A.real_value, A.value_type "
                    " from __intrinsic_slice S JOIN __intrinsic_args A on S.arg_set_id = A.arg_set_id"
                    " where S.id == ";
                query << event_id.bitfield.event_id << ";";  
                ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
                if(kRocProfVisDmResultSuccess !=
                    ExecuteSQLQuery(
                        future, DbInstancePtrAt(0),query.str().c_str(), "Properties", extdata,
                        (rocprofvis_dm_event_operation_t) event_id.bitfield.event_op,
                        &CallbackAddExtInfo))
                    break;
                
            } else    
            {
                ShowProgress(0, "Extended data not available for specified operation type!", kRPVDbError, future );
                return future->SetPromise(kRocProfVisDmResultInvalidParameter);
            }
            ShowProgress(50, "Extended data successfully loaded!",kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
        ShowProgress(0, "Extended data  not loaded!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);    
    }



    int GoogleTraceProcessor::CallbackAddStackTrace(void* data, int argc, sqlite3_stmt* stmt,
            char** azColName)
    {
        ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
        ROCPROFVIS_ASSERT_MSG_RETURN(argc == 4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
        void*  func = (void*)&CallbackAddStackTrace;
        rocprofvis_db_sqlite_callback_parameters* callback_params =
            (rocprofvis_db_sqlite_callback_parameters*) data;
        GoogleTraceProcessor*           db = (GoogleTraceProcessor*) callback_params->db;
        rocprofvis_db_stack_data_t record = {"","","",0,0};
        static const char * empty_blob = "{}";
        if(callback_params->future->Interrupted()) return 1;
        enum sqlite_callstack_param_index {
            CALLSTACK_REGION_ID,
            CALLSTACK_SYMBOL,
            CALLSTACK_ARGS,
            CALLSTACK_DEPTH,
        };
        record.id  = db->Sqlite3ColumnInt64(func, stmt, azColName, CALLSTACK_REGION_ID);
        std::string symbol = (char*) db->Sqlite3ColumnText(func, stmt, azColName, CALLSTACK_SYMBOL);
        std::string args = (char*) db->Sqlite3ColumnText(func, stmt, azColName, CALLSTACK_ARGS);
        record.depth = db->Sqlite3ColumnInt(func, stmt, azColName, CALLSTACK_DEPTH);

        jt::Json json_symbol;
        std::string symbol_blob;
        json_symbol["name"] = (symbol + " : " + args);
        symbol_blob = json_symbol.toString();
        record.symbol = symbol_blob.c_str();

        if (db->BindObject()->FuncAddStackFrame(callback_params->handle, record) !=
            kRocProfVisDmResultSuccess)
            return 1;
        callback_params->future->CountThisRow();
        return 0;
    } 

    rocprofvis_dm_result_t  GoogleTraceProcessor::ReadStackTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future)
    {

        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        while (true)
        {
            ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
            ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
            rocprofvis_dm_stacktrace_t stacktrace = BindObject()->FuncAddStackTrace(BindObject()->trace_object, event_id);
            ROCPROFVIS_ASSERT_MSG_BREAK(stacktrace, ERROR_STACK_TRACE_CANNOT_BE_NULL);
            if (event_id.bitfield.event_op == kRocProfVisDmOperationLaunch)
            {
                std::string query = "WITH RECURSIVE params AS ( SELECT ";
                query += std::to_string(event_id.bitfield.event_id);
                query += R"( AS target_id),
                    stack_chain AS (
                        SELECT s.id, s.name, s.depth, s.arg_set_id,
                        s.parent_stack_id, s.stack_id,
                        s.ts, s.dur, s.track_id
                        FROM __intrinsic_slice s
                        JOIN params p ON s.id = p.target_id
                        UNION
                        SELECT s.id, s.name, s.depth, s.arg_set_id,
                        s.parent_stack_id, s.stack_id,
                        s.ts, s.dur, s.track_id
                        FROM __intrinsic_slice s
                        JOIN stack_chain sc ON s.stack_id = sc.parent_stack_id
                        WHERE s.track_id = sc.track_id 
                        AND s.ts      <= sc.ts 
                        AND s.ts + s.dur >= sc.ts + sc.dur  
                    ),
                    slice_args AS (
                        SELECT
                        a.arg_set_id,
                        GROUP_CONCAT(
                            a.key || ' = ' || COALESCE(
                                a.string_value,
                                CAST(a.real_value AS TEXT),
                                CAST(a.int_value AS TEXT)
                            ),
                            ', '
                        ) AS args
                        FROM stack_chain sc
                        JOIN __intrinsic_args a ON sc.arg_set_id = a.arg_set_id
                        GROUP BY a.arg_set_id
                    )
                    SELECT
                    CAST(sc.id AS INTEGER)  AS id,
                    sc.name                 AS symbol,
                    COALESCE(sa.args, '')   AS args,
                    sc.depth                AS depth
                    FROM stack_chain sc
                    LEFT JOIN slice_args sa ON sc.arg_set_id = sa.arg_set_id
                    ORDER BY sc.depth;)";

                if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, DbInstancePtrAt(0), query.c_str(),
                    stacktrace,
                    &CallbackAddStackTrace))
                {
                    break;
                }
            }
            ShowProgress(100, "Stack trace successfully loaded!",kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
        ShowProgress(0, "Stack trace not loaded!", kRPVDbError, future);
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort
            : kRocProfVisDmResultDbAccessFailed);
    }


}  // namespace DataModel
}  // namespace RocProfVis

#endif
