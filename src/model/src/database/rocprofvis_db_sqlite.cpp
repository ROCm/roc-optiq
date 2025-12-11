// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_sqlite.h"
#include "rocprofvis_core_profile.h"
#include <sstream>

namespace RocProfVis
{
namespace DataModel
{

void SqliteDatabase::CreateDbNodes(std::vector<std::string>& multinode_files) {
    uint32_t node_id = 0;
    for (auto file : multinode_files)
    {
        m_db_nodes.push_back(std::make_unique<rocprofvis_db_sqlite_db_node_t>());
        m_db_nodes.back()->node_id = node_id++;
        m_db_nodes.back()->filepath = file;
    }
}
void SqliteDatabase::CreateDbNode(rocprofvis_db_filename_t filepath) {
    m_db_nodes.push_back(std::make_unique<rocprofvis_db_sqlite_db_node_t>());
    m_db_nodes.back()->node_id = 0;
    m_db_nodes.back()->filepath = filepath;
}

int SqliteDatabase::CallbackGetValue(void* data, int argc, sqlite3_stmt* stmt, char** azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    void*  func = (void*)&CallbackGetValue;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    SqliteDatabase* db = (SqliteDatabase*) callback_params->db;
    std::string * string_ptr = (rocprofvis_dm_string_t*)callback_params->handle;
    ROCPROFVIS_ASSERT_MSG_RETURN(string_ptr, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    *string_ptr = db->Sqlite3ColumnText(func, stmt, azColName, 0);
    return 0;
} 


uint64_t
SqliteDatabase::GetNullExceptionInt(void* func, char* column) {
    spdlog::debug("Column {} value is NULL!", column);
    const rocprofvis_null_data_exceptions_int* exceptions_map =
        GetNullDataExceptionsInt();
    if(exceptions_map != nullptr)
    {
        auto fcit = exceptions_map->find(func);
        if(fcit != exceptions_map->end())
        {
            auto it = fcit->second.find(column);
            if(it != fcit->second.end())
            {
                return it->second;
                spdlog::debug("Column {} value is NULL, replace with {}", column, it->second);
            }
        }
    }
    spdlog::debug("Column {} value is NULL, replace with 0", column);
    return 0;
}

char*
SqliteDatabase::GetNullExceptionString(void* func, char* column) {
    const rocprofvis_null_data_exceptions_string* exceptions_map =
        GetNullDataExceptionsString();
    if(exceptions_map != nullptr)
    {
        auto fcit = exceptions_map->find(func);
        if(fcit != exceptions_map->end())
        {
            auto it = fcit->second.find(column);
            if(it != fcit->second.end())
            {
                spdlog::debug("Column {} value is NULL, replace with {}", column, it->second.c_str());
                return (char*) it->second.c_str();
            }
        }
    }
    spdlog::debug("Column {} value is NULL, replace with empty string", column);
    return "";
}

bool
SqliteDatabase::NullExceptionSkip(void* func, char* column)
{
    const rocprofvis_null_data_exceptions_skip* exceptions_map =
        GetNullDataExceptionsSkip();
    if(exceptions_map != nullptr)
    {
        auto fcit = exceptions_map->find(func);
        if(fcit != exceptions_map->end())
        {
            auto it = fcit->second.find(column);
            if(it != fcit->second.end())
            {
                spdlog::debug("Column {} value is NULL, skip column", column);
                return true;
            }
        }
    }
    return false;
}

char*
SqliteDatabase::Sqlite3ColumnText(void* func, sqlite3_stmt* stmt, char** azColName, int index) {

    if(sqlite3_column_type(stmt, index) == SQLITE_NULL)
    {
        return GetNullExceptionString(func, azColName[index]);
    }
    else
    {
        return (char*) sqlite3_column_text(stmt, index);
    }
}

int
SqliteDatabase::Sqlite3ColumnInt(void* func, sqlite3_stmt* stmt, char** azColName, int index) {
    if(sqlite3_column_type(stmt, index) == SQLITE_NULL)
    {
        return GetNullExceptionInt(func, azColName[index]);
    }
    else
    {
        return sqlite3_column_int(stmt, index);
    }
}
int64_t
SqliteDatabase::Sqlite3ColumnInt64(void* func, sqlite3_stmt* stmt, char** azColName, int index) {
    if(sqlite3_column_type(stmt, index) == SQLITE_NULL)
    {
        return GetNullExceptionInt(func, azColName[index]);
    }
    else
    {
        return sqlite3_column_int64(stmt, index);
    }
}
double
SqliteDatabase::Sqlite3ColumnDouble(void* func, sqlite3_stmt* stmt, char** azColName, int index) {
    if(sqlite3_column_type(stmt, index) == SQLITE_NULL)
    {
        return GetNullExceptionInt(func, azColName[index]);
    }
    else
    {
        return sqlite3_column_double(stmt, index);
    }
}



int SqliteDatabase::CallbackRunQuery(void *data, int argc, sqlite3_stmt* stmt, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    SqliteDatabase* db = (SqliteDatabase*)callback_params->db;
    void* func = (void*)&CallbackRunQuery;
    if (callback_params->future->Interrupted()) return 1;
    rocprofvis_dm_table_row_t row =
        db->BindObject()->FuncAddTableRow(callback_params->handle);
    ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 1);
    
    if(0 == callback_params->future->GetProcessedRowsCount())
    {
        for (int i=0; i < argc; i++)
        {
            if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumn(callback_params->handle,azColName[i])) return 1;
        }
    }
    for (int i=0; i < argc; i++)
    {
        std::string column_text = db->Sqlite3ColumnText(func, stmt, azColName, i);
        if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableRowCell(row, column_text.c_str())) return 1;
    }

    callback_params->future->CountThisRow();
    return 0;
}


rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQueryStatic(
                                    SqliteDatabase* db, 
                                    Future* future,
                                    DbInstance* db_instance,
                                    const char* query,
                                    RpvSqliteExecuteQueryCallback callback)
{
    return future->SetPromise(
        db->ExecuteSQLQuery(future, db_instance, query, callback)
    );
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQueryStaticWithHandle(
    SqliteDatabase* db, 
    Future* future,
    DbInstance* db_instance,
    const char* query,
    rocprofvis_dm_handle_t handle,
    uint32_t query_index,
    RpvSqliteExecuteQueryCallback callback)
{
    return future->SetPromise(
        db->ExecuteSQLQuery(future, db_instance, query, handle, query_index, callback)
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
    for (auto & node : m_db_nodes)
    {
        sqlite3* conn = GetConnection(node->node_id);
        if (nullptr == conn)
        {
            spdlog::debug("Can't open database:");
            return kRocProfVisDmResultDbAccessFailed;
        }
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t SqliteDatabase::OpenConnection(uint32_t db_node_id, sqlite3** connection)
{
    *connection = nullptr;
    if(sqlite3_open(m_db_nodes[db_node_id]->filepath.c_str(), connection) != SQLITE_OK)
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
    for (auto & node : m_db_nodes)
    {
        if (node->m_connections_inuse.size() != 1)
        {
            spdlog::debug("Error : At the time of closing only one active connection should remain!");
            result = kRocProfVisDmResultUnknownError;
        }
        ReleaseConnection(*node->m_connections_inuse.begin());

        for (auto it = node->m_available_connections.begin(); it != node->m_available_connections.end(); ++it)
        {
            if (sqlite3_close(*it) != SQLITE_OK)
            {
                spdlog::debug("Can't close database connection:");
                spdlog::debug(sqlite3_errmsg(*it));
                result = kRocProfVisDmResultUnknownError;
            }
        }
    }
    return result;
}

void
SqliteDatabase::InterruptQuery(void* connection) {
    if (connection != nullptr)
    {
        sqlite3_interrupt((sqlite3*) connection);
    }
}

sqlite3* SqliteDatabase::GetConnection(uint32_t db_node_id) 
{
    ROCPROFVIS_ASSERT_MSG_RETURN(db_node_id < m_db_nodes.size(), ERROR_INDEX_OUT_OF_RANGE, nullptr);
    std::unique_lock<std::mutex> lock(m_db_nodes[db_node_id]->m_mutex);
    if(!m_db_nodes[db_node_id]->m_available_connections.empty())
    {

        auto it = std::prev(m_db_nodes[db_node_id]->m_available_connections.end());
        m_db_nodes[db_node_id]->m_connections_inuse.insert(*it);
        sqlite3* conn = *it;
        m_db_nodes[db_node_id]->m_available_connections.erase(it);
        return conn;
    }
    else
    {
        
        sqlite3* conn;
        size_t   thread_count = std::thread::hardware_concurrency();
        if(m_db_nodes[db_node_id]->m_connections_inuse.size() > thread_count ||
           kRocProfVisDmResultSuccess != OpenConnection(db_node_id, &conn))
        {
            m_db_nodes[db_node_id]->m_inuse_cv.wait(lock, [&] { return !m_db_nodes[db_node_id]->m_available_connections.empty(); });
           
           auto it = std::prev(m_db_nodes[db_node_id]->m_available_connections.end());
           m_db_nodes[db_node_id]->m_connections_inuse.insert(*it);
           sqlite3* conn = *it;
           m_db_nodes[db_node_id]->m_available_connections.erase(it);
           return conn;
        }
        else
        {
            m_db_nodes[db_node_id]->m_connections_inuse.insert(conn);
            return conn;
        }
    }
}

sqlite3*
SqliteDatabase::GetServiceConnection(uint32_t db_node_id)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(db_node_id < m_db_nodes.size(), ERROR_INDEX_OUT_OF_RANGE, nullptr);
    if (!m_db_nodes[db_node_id]->m_connections_inuse.empty())
    {
        return *m_db_nodes[db_node_id]->m_connections_inuse.begin();
    }
    return nullptr;
}

void SqliteDatabase::ReleaseConnection(sqlite3* conn, uint32_t db_node_id)
{
    std::unique_lock<std::mutex> lock(m_db_nodes[db_node_id]->m_mutex);
    auto it = m_db_nodes[db_node_id]->m_connections_inuse.find(conn);
    if (it != m_db_nodes[db_node_id]->m_connections_inuse.end())
    {
        m_db_nodes[db_node_id]->m_available_connections.insert(*it);
        m_db_nodes[db_node_id]->m_connections_inuse.erase(it);
        m_db_nodes[db_node_id]->m_inuse_cv.notify_one();
    }
}

void SqliteDatabase::ReleaseConnection(sqlite3* conn)
{
    for (int i = 0; i < m_db_nodes.size(); i++)
    {
        ReleaseConnection(conn, i);
    }
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(
    Future* future, 
    DbInstance* db_instance,
    const char* query
){
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        nullptr,
        nullptr,
        { query },
        static_cast<rocprofvis_dm_track_id_t>(-1)
    };
    return SqliteDatabase::ExecuteSQLQuery(db_instance, query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(
                                               Future* future, 
                                               DbInstance* db_instance,
                                               const char* query,
                                               RpvSqliteExecuteQueryCallback callback)
{
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        nullptr,
        callback,
        { query },
        static_cast<rocprofvis_dm_track_id_t>(-1)
    };
    return SqliteDatabase::ExecuteSQLQuery(db_instance, query, &params);
}


rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(
                                                Future* future,
                                                DbInstance* db_instance,
                                                const char* query, 
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
    return SqliteDatabase::ExecuteSQLQuery(db_instance, query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(
                                               Future* future, 
                                               DbInstance* db_instance,
                                               const char* query, 
                                               RpvSqliteExecuteQueryCallback callback,
                                               uint64_t & value){
    std::string str_value;
    rocprofvis_dm_result_t result = ExecuteSQLQuery(future, db_instance, query, callback, &str_value);
    if (result == kRocProfVisDmResultSuccess)
    {
        value = std::stoll(str_value);
    }
    return result;
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(
                                               Future* future,
                                               DbInstance* db_instance,
                                               const char* query, 
                                               RpvSqliteExecuteQueryCallback callback,
                                               uint32_t & value){
    std::string str_value;
    rocprofvis_dm_result_t result = ExecuteSQLQuery(future, db_instance, query, callback, &str_value);
    if (result == kRocProfVisDmResultSuccess)
    {
        value = std::stol(str_value);
    }
    return result;
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(
                                               Future* future,
                                               DbInstance* db_instance,
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
    return SqliteDatabase::ExecuteSQLQuery(db_instance, query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(
                                               Future* future, 
                                               DbInstance* db_instance,
                                               const char* query,
                                               rocprofvis_dm_handle_t handle, 
                                               uint32_t index,
                                               RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        handle,
        callback,
        { query },
        static_cast<rocprofvis_dm_track_id_t>(index)
    };
    return SqliteDatabase::ExecuteSQLQuery(db_instance, query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(
                                              Future* future,
                                              DbInstance* db_instance,
                                              const char* query,
                                              const char* cache_table_name,
                                              rocprofvis_dm_handle_t handle,
                                              rocprofvis_dm_event_operation_t op,
                                              RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        handle,
        callback,
        { query, cache_table_name },
        static_cast<rocprofvis_dm_track_id_t>(-1),
        op
    };
    return SqliteDatabase::ExecuteSQLQuery(db_instance, query, &params);
}


rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(
                                              Future* future, 
                                              DbInstance* db_instance,
                                              const char* query,
                                              const char* cache_table_name,
                                              rocprofvis_dm_handle_t handle,
                                              RpvSqliteExecuteQueryCallback   callback)
{
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        handle,
        callback,
        { query, cache_table_name },
        static_cast<rocprofvis_dm_track_id_t>(-1),
    };
    return SqliteDatabase::ExecuteSQLQuery(db_instance, query, &params);
}


rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future,
                                                        DbInstance* db_instance,
                                                        uint64_t load_hash,
                                                        uint32_t load_id,
                                                        std::vector<std::string> query,
                                                        RpvSqliteExecuteQueryCallback find_callback,
                                                        RpvSqliteExecuteQueryCallback load_callback)
{
    rocprofvis_db_sqlite_callback_parameters params = {
        this,
        future,
        nullptr,
        load_callback,
        { query[kRPVSourceQueryTrackByQueue].c_str(),
          query[kRPVSourceQueryTrackByStream].c_str(),
          query[kRPVSourceQueryLevel].c_str(), 
          query[kRPVSourceQuerySliceByQueue].c_str(),
          query[kRPVSourceQuerySliceByStream].c_str(),
          query[kRPVSourceQueryTable].c_str() },
        static_cast<rocprofvis_dm_track_id_t>(load_id)
    };

    rocprofvis_dm_result_t result  = kRocProfVisDmResultSuccess;
    std::string load_table_name = std::string("track_info_")+std::to_string(load_hash);
    
    if (CheckTableExists(load_table_name, db_instance->FileIndex()))
    {
        std::string load_query = std::string("SELECT * FROM ") + load_table_name + " WHERE load_id = " + std::to_string(load_id);
        std::string guid = GuidAt(db_instance->GuidIndex());
        if (!guid.empty())
        {
            load_query += " AND guid = '" + guid+"'";
        }
        result = ExecuteSQLQuery(db_instance, load_query.c_str(), &params);
    } else
    {
        TraceProperties()->tracks_info_restored = false;
        params.callback = find_callback;
        result = ExecuteSQLQuery(
            db_instance,
            query[kRPVSourceQueryTrackByQueue].c_str(), &params);
        if (result == kRocProfVisDmResultSuccess &&
            query[kRPVSourceQueryTrackByStream].length() > 0)
        {
            result = ExecuteSQLQuery(
                db_instance,
                query[kRPVSourceQueryTrackByStream].c_str(),
                &params);
        }
    }
    return result;
}

int SqliteDatabase::Sqlite3Exec(sqlite3* db, const char* query,
                            int (*callback)(void*, int, sqlite3_stmt*, char**),
                              void* user_data)
{
    int rc=0;
    sqlite3_stmt* stmt = nullptr;
    rocprofvis_db_sqlite_callback_parameters* callback_params =
        (rocprofvis_db_sqlite_callback_parameters*) user_data;
    if (callback_params->future != nullptr)
    {
        callback_params->future->LinkDatabase(this, db);
    }
    sqlite3_mutex_enter(sqlite3_db_mutex(db));
    rc = sqlite3_prepare_v2(db, query, -1, &stmt, nullptr);
    if(rc == SQLITE_OK)
    {
        int                cols = sqlite3_column_count(stmt);
        std::vector<std::string> col_names_strorage;
        std::vector<char*> col_names;

        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            if (col_names.size() == 0)
            {
                for(int i = 0; i < cols; ++i)
                {
                    col_names_strorage.push_back(const_cast<char*>(sqlite3_column_name(stmt, i)));
                }
                for (auto & col_name : col_names_strorage)
                {
                    col_names.push_back((char*)col_name.c_str());
                }
            }
            bool skip_this_row = false;

            for(int i = 0; i < cols; ++i)
            {
                if(sqlite3_column_type(stmt, i) == SQLITE_NULL)
                {
                    skip_this_row = NullExceptionSkip((void*)callback, col_names[i]);
                    if(skip_this_row)
                    {
                        break;
                    }
                }
            }

            if(skip_this_row == false)
            {
                rc = callback(user_data, cols, stmt, col_names.data());
                if(rc != 0)
                {
                    break;
                }
            }
        }

        sqlite3_finalize(stmt);
    }
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    if(callback_params->future != nullptr)
    {
        callback_params->future->LinkDatabase(nullptr, nullptr);
    }
    return rc;
}

void SqliteDatabase::ReplaceAllSubstrings(std::string& str, const std::string& from, const std::string& to)
{
    if (from.empty())
        return;

    size_t start = 0;
    while ((start = str.find(from, start)) != std::string::npos) {
        str.replace(start, from.length(), to);
        start += to.length(); 
    }
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(DbInstance* db_instance, const char* query, rocprofvis_db_sqlite_callback_parameters * params)
{
    PROFILE;
    ROCPROFVIS_ASSERT_MSG_RETURN(db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_result_t result  = kRocProfVisDmResultSuccess;
    char *zErrMsg = 0;
    params->db_instance = db_instance;
    sqlite3* conn = GetConnection(db_instance->FileIndex());
    std::string query_str = query;
    std::string guid_str = GuidAt(db_instance->GuidIndex());
    ReplaceAllSubstrings(query_str, "%GUID%", guid_str);
    query = query_str.c_str();
    int   rc = Sqlite3Exec(conn, query, params->callback, params);       
    if(rc != SQLITE_OK)
    {
        if (rc == SQLITE_ABORT)
        {
            result = kRocProfVisDmResultDbAbort;
        } else
        {
            spdlog::debug("Query: "); spdlog::debug(query);
            spdlog::debug("SQL error "); spdlog::debug(std::to_string(rc).c_str()); spdlog::debug(":"); 
            spdlog::debug(sqlite3_errmsg(conn));
            result = kRocProfVisDmResultDbAccessFailed;
        }
    } 
    ReleaseConnection(conn);
    return result;
}

bool SqliteDatabase::CheckTableExists(const std::string& table_name, uint32_t db_node_id) {
    sqlite3* conn = GetConnection(db_node_id);
    if (!conn || table_name.empty())
    {
        if (conn)
            ReleaseConnection(conn);
        return false;
    }

    const char* sql =
        "SELECT name FROM sqlite_master "
        "WHERE type='table' AND name=?1;";

    sqlite3_stmt* stmt = nullptr;
    bool exists = false;

    if (sqlite3_prepare_v2(conn, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, table_name.c_str(), -1, SQLITE_STATIC);

        if (sqlite3_step(stmt) == SQLITE_ROW) {
            exists = true;  
        }
    }

    sqlite3_finalize(stmt);
    ReleaseConnection(conn);
    return exists;
}

rocprofvis_dm_result_t
SqliteDatabase::DropSQLTable(const char* table_name, uint32_t db_node_id)
{
    sqlite3* conn = GetConnection(db_node_id);
    sqlite3_mutex_enter(sqlite3_db_mutex(conn));

    std::string query = "DROP TABLE IF EXISTS ";
    query += table_name;
    query += ";";
    sqlite3_exec(conn, query.c_str(), nullptr, nullptr, nullptr);

    sqlite3_mutex_leave(sqlite3_db_mutex(conn));
    ReleaseConnection(conn, db_node_id);
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
SqliteDatabase::DropSQLIndex(const char* index_name, uint32_t db_node_id)
{
    sqlite3* conn = GetConnection(db_node_id);
    sqlite3_mutex_enter(sqlite3_db_mutex(conn));

    std::string query = "DROP INDEX IF EXISTS ";
    query += index_name;
    query += ";";
    sqlite3_exec(conn, query.c_str(), nullptr, nullptr, nullptr);

    sqlite3_mutex_leave(sqlite3_db_mutex(conn));
    ReleaseConnection(conn, db_node_id);
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
SqliteDatabase::ExecuteTransaction(std::vector<std::string> queries, uint32_t db_node_id)
{
    sqlite3* conn = GetConnection(db_node_id);
    rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
    while (true)
    {
        if (sqlite3_exec(conn, "BEGIN TRANSACTION;", nullptr, nullptr, nullptr) != SQLITE_OK)
        {
            result = kRocProfVisDmResultDbAccessFailed;
            break;
        }

        for (auto query : queries)
        {
            if (sqlite3_exec(conn, query.c_str(), nullptr, nullptr, nullptr) != SQLITE_OK)
            {
                result = kRocProfVisDmResultDbAccessFailed;
                break;
            }
        }

        if (sqlite3_exec(conn, "COMMIT;", nullptr, nullptr, nullptr) != SQLITE_OK)
        {
            result = kRocProfVisDmResultDbAccessFailed;
            break;
        }
        break;
    }
    ReleaseConnection(conn, db_node_id);
    return result;
}

rocprofvis_dm_result_t
SqliteDatabase::CreateSQLTable(
                                const char* table_name, 
                                SQLInsertParams* parameters, 
                                uint8_t num_cols, 
                                size_t num_row,
                                std::function<void(sqlite3_stmt* stmt, int index)> insert_func,
                                uint32_t db_node_id)
{
    sqlite3* conn = GetConnection(db_node_id);
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
    ReleaseConnection(conn,db_node_id);
    return kRocProfVisDmResultSuccess;
}


}  // namespace DataModel
}  // namespace RocProfVis
