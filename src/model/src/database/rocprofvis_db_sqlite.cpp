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

#include "rocprofvis_db_sqlite.h"
#include "rocprofvis_core_profile.h"
#include <sstream>

namespace RocProfVis
{
namespace DataModel
{

int SqliteDatabase::CallbackGetValue(void* data, int argc, sqlite3_stmt* stmt, char** azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    std::string * string_ptr = (rocprofvis_dm_string_t*)callback_params->handle;
    ROCPROFVIS_ASSERT_MSG_RETURN(string_ptr, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    *string_ptr = (char*) sqlite3_column_text(stmt,0);
    return 0;
} 

bool
SqliteDatabase::isServiceColumn(char* name)
{
    static const std::vector<std::string> service_columns = { "const", "agentId", "queueId", "streamId" };
    for (std::string service_column : service_columns)
    {
        if(service_column == name) return true;
    }
    return false;
}

int SqliteDatabase::CallbackRunQuery(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    SqliteDatabase* db = (SqliteDatabase*)callback_params->db;

    if (callback_params->future->Interrupted()) return 1;
    if(0 == callback_params->future->GetProcessedRowsCount())
    {
        for (int i=0; i < argc; i++)
        {
            if(db->isServiceColumn(azColName[i])) continue;
            if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumn(callback_params->handle,azColName[i])) return 1;
        }
    }
    rocprofvis_dm_table_row_t row = db->BindObject()->FuncAddTableRow(callback_params->handle);
    ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 1);
    for (int i=0; i < argc; i++)
    {
        if(db->isServiceColumn(azColName[i])) continue;
        if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableRowCell(row, (char*) sqlite3_column_text(stmt,i))) return 1;
    }
    callback_params->future->CountThisRow();
    return 0;
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQueryStatic(
                                    SqliteDatabase* db, 
                                    Future* future,
                                    const char* query,
                                    RpvSqliteExecuteQueryCallback callback)
{
    return future->SetPromise(
        db->ExecuteSQLQuery(future, query, callback)
    );
}

int
SqliteDatabase::DetectTable(sqlite3* conn, const char* table, bool is_view)
{
    int rc = SQLITE_ERROR;
    if(conn)
    {
        sqlite3_mutex_enter(sqlite3_db_mutex(conn));
        std::stringstream query;
        char*             zErrMsg = 0;
        query << "SELECT COUNT(name) FROM sqlite_master WHERE type="
              << (is_view ? "'view'" : "'table'") << "AND name='" << table << "';";
        rc = sqlite3_exec(
            conn, query.str().c_str(),
            [](void* data, int argc, char** argv, char** azColName) {
                uint32_t num = std::stol(argv[0]);
                return num > 0 ? 0 : 1;
            },
            nullptr, &zErrMsg);
        if(rc != SQLITE_OK)
        {
            spdlog::debug("Detect table error ");
            spdlog::debug(std::to_string(rc).c_str());
            spdlog::debug(":");
            spdlog::debug(zErrMsg);
            sqlite3_free(zErrMsg);
        }
        sqlite3_mutex_leave(sqlite3_db_mutex(conn));
    }
    else
    {
        spdlog::debug("Error : Connection cannot be nullptr!");
    }
    return rc;
}


rocprofvis_dm_result_t SqliteDatabase::Open()
{
    sqlite3* conn = GetConnection();
    if(nullptr == conn)
    {
        spdlog::debug("Can't open database:");
        return kRocProfVisDmResultDbAccessFailed;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t SqliteDatabase::OpenConnection(sqlite3** connection)
{
    *connection = nullptr;
    if(sqlite3_open(Path(), connection) != SQLITE_OK)
    {
        spdlog::debug("Cannot open database connection - {}",
                      sqlite3_errmsg(*connection));
        sqlite3_close(*connection);
        *connection = nullptr;
        return kRocProfVisDmResultUnknownError;
    }
    else
    {
        sqlite3_exec(*connection, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
        sqlite3_exec(*connection, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t SqliteDatabase::Close()
{
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    if (m_connections_inuse.size() != 1)
    {
        spdlog::debug("Error : At the time of closing only one active connection should remain!");
        result = kRocProfVisDmResultUnknownError;
    }
    ReleaseConnection(*m_connections_inuse.begin());

    for (auto it = m_available_connections.begin(); it != m_available_connections.end(); ++it)
    {
        if(sqlite3_close(*it) != SQLITE_OK)
        {
            spdlog::debug("Can't close database connection:");
            spdlog::debug(sqlite3_errmsg(*it));
            result = kRocProfVisDmResultUnknownError;
        }
    }  
    return result;
}

sqlite3* SqliteDatabase::GetConnection() 
{
    std::unique_lock<std::mutex> lock(m_mutex);
    if(!m_available_connections.empty())
    {

        auto it = std::prev(m_available_connections.end());
        m_connections_inuse.insert(*it);
        sqlite3* conn = *it;
        m_available_connections.erase(it);
        return conn;
    }
    else
    {
        
        sqlite3* conn;
        size_t   thread_count = std::thread::hardware_concurrency();
        if(m_connections_inuse.size() > thread_count ||
           kRocProfVisDmResultSuccess != OpenConnection(&conn))
        {
           m_inuse_cv.wait(lock, [&] { return !m_available_connections.empty(); });
           
           auto it = std::prev(m_available_connections.end());
           m_connections_inuse.insert(*it);
           sqlite3* conn = *it;
           m_available_connections.erase(it);
           return conn;
        }
        else
        {
            m_connections_inuse.insert(conn);
            return conn;
        }
    }
}

sqlite3*
SqliteDatabase::GetServiceConnection()
{
    if (!m_connections_inuse.empty())
    {
        return *m_connections_inuse.begin();
    }
    return nullptr;
}

void SqliteDatabase::ReleaseConnection(sqlite3* conn)
{
    std::unique_lock<std::mutex> lock(m_mutex);
    auto it = m_connections_inuse.find(conn);
    if (it != m_connections_inuse.end())
    {
        m_available_connections.insert(*it);
        m_connections_inuse.erase(it);
        m_inuse_cv.notify_one();
    }
}


rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query){
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        nullptr,
        nullptr,
        { query },
        static_cast<rocprofvis_dm_track_id_t>(-1)
    };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        nullptr,
        callback,
        { query },
        static_cast<rocprofvis_dm_track_id_t>(-1)
    };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}


rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, 
                                                RpvSqliteExecuteQueryCallback callback,
                                                rocprofvis_dm_string_t* value){
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        (rocprofvis_dm_handle_t) value,
        callback,
        { query },
        static_cast<rocprofvis_dm_track_id_t>(-1)
    };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, 
                                                RpvSqliteExecuteQueryCallback callback,
                                                uint64_t & value){
    std::string str_value;
    rocprofvis_dm_result_t result = ExecuteSQLQuery(future, query, callback, &str_value);
    if (result == kRocProfVisDmResultSuccess)
    {
        value = std::stoll(str_value);
    }
    return result;
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, 
                                                RpvSqliteExecuteQueryCallback callback,
                                                uint32_t & value){
    std::string str_value;
    rocprofvis_dm_result_t result = ExecuteSQLQuery(future, query, callback, &str_value);
    if (result == kRocProfVisDmResultSuccess)
    {
        value = std::stol(str_value);
    }
    return result;
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query,
                                                        rocprofvis_dm_handle_t handle, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        handle,
        callback,
        { query },
        static_cast<rocprofvis_dm_track_id_t>(-1)
    };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}


rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query,
                                                        const char* cache_table_name,
                                                        rocprofvis_dm_handle_t handle, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        handle,
        callback,
        { query, cache_table_name },
        static_cast<rocprofvis_dm_track_id_t>(-1)
    };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        std::vector<const char*> query,
                                                        RpvSqliteExecuteQueryCallback callback)
{
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        nullptr,
        callback,
        { query[kRPVSourceQueryTrackByQueue], query[kRPVSourceQueryTrackByStream],
          query[kRPVSourceQueryLevel], query[kRPVSourceQuerySliceByQueue],
          query[kRPVSourceQuerySliceByStream],
          query[kRPVSourceQueryTable] },
        static_cast<rocprofvis_dm_track_id_t>(-1)
    };
    rocprofvis_dm_result_t result =
        SqliteDatabase::ExecuteSQLQuery(query[kRPVSourceQueryTrackByQueue], &params);
    if(result == kRocProfVisDmResultSuccess &&
       strlen(query[kRPVSourceQueryTrackByStream]) > 0)
    {
        return SqliteDatabase::ExecuteSQLQuery(query[kRPVSourceQueryTrackByStream],
                                               &params);
    }
    return result;
}

int SqliteDatabase::Sqlite3Exec(sqlite3* db, const char* query,
                            int (*callback)(void*, int, sqlite3_stmt*, char**),
                              void* user_data)
{
    int rc=0;
    sqlite3_stmt* stmt = nullptr;
    sqlite3_mutex_enter(sqlite3_db_mutex(db));
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    if(rc == SQLITE_OK)
    {
        int                cols = sqlite3_column_count(stmt);
        std::vector<char*> col_names;
        for(int i = 0; i < cols; ++i)
        {
            col_names.push_back(const_cast<char*>(sqlite3_column_name(stmt, i)));
        }

        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            bool null_data_in_the_row = false;

            for(int i = 0; i < cols; ++i)
            {
                if(sqlite3_column_type(stmt, i) == SQLITE_NULL)
                {
                    null_data_in_the_row = true;
                    spdlog::debug("NULL data in column {}", col_names[i]);
                    break;
                }
            }
            if(null_data_in_the_row == false)
            {
                rc = callback(user_data, cols, stmt, col_names.data());
                if(rc != 0) break;
            }
        }

        sqlite3_finalize(stmt);
    }
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    return rc;
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(const char* query, rocprofvis_db_sqlite_callback_parameters * params)
{
    PROFILE;
    rocprofvis_dm_result_t result  = kRocProfVisDmResultSuccess;
    char *zErrMsg = 0;
    sqlite3* conn = GetConnection();
    int   rc = Sqlite3Exec(conn, query, params->callback, params);       
    if( rc != SQLITE_OK ) {
        spdlog::debug("Query: "); spdlog::debug(query);
        spdlog::debug("SQL error "); spdlog::debug(std::to_string(rc).c_str()); spdlog::debug(":"); 
        spdlog::debug(sqlite3_errmsg(conn));
        result = kRocProfVisDmResultDbAccessFailed;
    } 
    ReleaseConnection(conn);
    return result;
}

rocprofvis_dm_result_t
SqliteDatabase::DropSQLTable(const char* table_name)
{
    sqlite3* conn = GetConnection();
    sqlite3_mutex_enter(sqlite3_db_mutex(conn));

    std::string query = "DROP TABLE IF EXISTS ";
    query += table_name;
    query += ";";
    sqlite3_exec(conn, query.c_str(), nullptr, nullptr, nullptr);

    sqlite3_mutex_leave(sqlite3_db_mutex(conn));
    ReleaseConnection(conn);
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
SqliteDatabase::CreateSQLTable(
                                const char* table_name, 
                                SQLInsertParams* parameters, 
                                uint8_t num_cols, 
                                size_t num_row,
                                std::function<void(sqlite3_stmt* stmt, int index)> insert_func)
{
    sqlite3* conn = GetConnection();
    sqlite3_mutex_enter(sqlite3_db_mutex(conn));
    while(true)
    {
        std::string query = "DROP TABLE IF EXISTS ";
        query += table_name;
        query += ";";
        if(sqlite3_exec(conn, query.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
        {
            break;
        }
        query = "CREATE TABLE IF NOT EXISTS ";
        query += table_name;
        query += "(";
        for(int i = 0; i < num_cols; i++)
        {
            if(i > 0)
            {
                query += ", ";
            }
            query += parameters[i].column;
            query += " ";
            query += parameters[i].type;
        }
        query += ") WITHOUT ROWID;";
        if(sqlite3_exec(conn, query.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
        {
            break;
        }

        if(sqlite3_exec(conn, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) !=
           SQLITE_OK)
        {
            break;
        }

        query = "INSERT INTO ";
        query += table_name;
        query += "(";

        for(int i = 0; i < num_cols; i++)
        {
            if(i > 0)
            {
                query += ", ";
            }
            query += parameters[i].column;
        }
        query += ") VALUES (";

        for(int i = 0; i < num_cols; i++)
        {
            if(i > 0)
            {
                query += ", ";
            }
            query += "?";
        }
        query += ");";

        sqlite3_stmt* stmt;
        sqlite3_prepare_v2(conn, query.c_str(), -1, &stmt, nullptr);

        for(int i = 0; i < num_row; i++)
        {
            insert_func(stmt, i);
            int rc = sqlite3_step(stmt);
            if(rc != SQLITE_DONE)
            {
                spdlog::debug("Insert failed");
            }
            sqlite3_reset(stmt);
        }

        sqlite3_finalize(stmt);

        if(sqlite3_exec(conn, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
        {
            break;
        }
        break;
    }
    sqlite3_mutex_leave(sqlite3_db_mutex(conn));
    ReleaseConnection(conn);
    return kRocProfVisDmResultSuccess;
}

}  // namespace DataModel
}  // namespace RocProfVis
