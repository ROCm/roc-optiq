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

int SqliteDatabase::CallbackGetValue(void* data, int argc, char** argv, char** azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    std::string * string_ptr = (rocprofvis_dm_string_t*)callback_params->handle;
    ROCPROFVIS_ASSERT_MSG_RETURN(string_ptr, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    *string_ptr = argv[0];
    return 0;
} 

int SqliteDatabase::CallbackRunQuery(void *data, int argc, char **argv, char **azColName){
    ROCPROFVIS_ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    SqliteDatabase* db = (SqliteDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    if(0 == callback_params->future->GetProcessedRowsCount())
    {
        for (int i=0; i < argc; i++)
        {
            if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumn(callback_params->handle,azColName[i])) return 1;
        }
    }
    rocprofvis_dm_table_row_t row = db->BindObject()->FuncAddTableRow(callback_params->handle);
    ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 1);
    for (int i=0; i < argc; i++)
    {
        if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableRowCell(row, argv[i])) return 1;
    }
    callback_params->future->CountThisRow();
    return 0;
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQueryStatic(
                                    SqliteDatabase* db, 
                                    rocprofvis_db_connection_t conn, 
                                    Future* future,
                                    const char* query,
                                    RpvSqliteExecuteQueryCallback callback)
{
    return future->SetPromise(
        db->ExecuteSQLQuery((sqlite3*) conn, future, query, callback)
    );
}

int SqliteDatabase::CallbackTableExists(void *data, int argc, char **argv, char **azColName){
    uint32_t num = std::stol(argv[0]);
    return num > 0 ? 0 : 1;
}

int SqliteDatabase::DetectTable(sqlite3 *db, const char* table){
    sqlite3_mutex_enter(sqlite3_db_mutex(db));
    std::stringstream query;
    char* zErrMsg = 0;
    query << "SELECT COUNT(name) FROM sqlite_master WHERE type='view' AND name='" << table << "';";
    int rc = sqlite3_exec(db, query.str().c_str(), CallbackTableExists, db, &zErrMsg);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    if (rc != SQLITE_OK) {
        spdlog::debug("Detect table error "); spdlog::debug(std::to_string(rc).c_str()); spdlog::debug(":"); spdlog::debug(zErrMsg);
        sqlite3_free(zErrMsg);
    }
    return rc;
}


rocprofvis_dm_result_t SqliteDatabase::Open()
{
    if( (m_db_status = sqlite3_open(Path(), &m_db)) != SQLITE_OK) {
        spdlog::debug("Can't open database:");
        spdlog::debug(sqlite3_errmsg(m_db));
        return kRocProfVisDmResultDbAccessFailed;
    }
    sqlite3_exec(m_db, "PRAGMA journal_mode=WAL;", nullptr, nullptr, nullptr);
    sqlite3_exec(m_db, "PRAGMA synchronous = NORMAL;", nullptr, nullptr, nullptr);
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
    if (IsOpen())
    {
        for (int i = 0; i < NumTracks(); i++)
        {
            if (TrackPropertiesAt(i)->db_connection != nullptr)
            {
                if(sqlite3_close((sqlite3*)TrackPropertiesAt(i)->db_connection) != SQLITE_OK)
                {
                    spdlog::debug("Can't close database connection:");
                    spdlog::debug(sqlite3_errmsg(m_db));
                }
            }
        }
        if( sqlite3_close(m_db) != SQLITE_OK ) {

            spdlog::debug("Can't close database:");
            spdlog::debug(sqlite3_errmsg(m_db));
            return kRocProfVisDmResultDbAccessFailed;
        }
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}


rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, nullptr,query,"",""};
    return SqliteDatabase::ExecuteSQLQuery(m_db, query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback,query,"",""};
    return SqliteDatabase::ExecuteSQLQuery(m_db, query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery( sqlite3* db_conn,
                                                        Future* future, 
                                                        const char* query,
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback,query,"",""};
    return SqliteDatabase::ExecuteSQLQuery((db_conn!=nullptr?db_conn:m_db), query, &params);
}


rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, 
                                                RpvSqliteExecuteQueryCallback callback,
                                                rocprofvis_dm_string_t* value){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, (rocprofvis_dm_handle_t)value, callback,query,"",""};
    return SqliteDatabase::ExecuteSQLQuery(m_db, query, &params);
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
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback,query,"",""};
    return SqliteDatabase::ExecuteSQLQuery(m_db, query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery( sqlite3* db_conn,
                                                        Future* future, 
                                                        const char* query,
                                                        rocprofvis_dm_handle_t handle, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback,query,"",""};
    return SqliteDatabase::ExecuteSQLQuery((db_conn!=nullptr?db_conn:m_db), query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query,
                                                        const char* subquery,
                                                        rocprofvis_dm_handle_t handle, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback,query,subquery,""};
    return SqliteDatabase::ExecuteSQLQuery(m_db, query, &params);
}


rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query, 
                                                        const char* subquery,
                                                        RpvSqliteExecuteQueryCallback callback)
{
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback,query,subquery,""};
    return SqliteDatabase::ExecuteSQLQuery(m_db, query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query, 
                                                        const char* timeline_subquery,
                                                        const char* table_subquery,
                                                        RpvSqliteExecuteQueryCallback callback)
{
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback,query,timeline_subquery, table_subquery};
    return SqliteDatabase::ExecuteSQLQuery(m_db, query, &params);
}

int SqliteDatabase::Sqlite3Exec(sqlite3* db, const char* query,
                              int (*callback)(void*, int, char**, char**),
                              void* user_data)
{
    int rc=0;
    sqlite3_stmt* stmt = nullptr;
    if(sqlite3_prepare_v2(db, query, -1, &stmt, nullptr) != SQLITE_OK) return 1;

    int    cols     = sqlite3_column_count(stmt);
    char** col_names = new char*[cols];
    for(int i = 0; i < cols; ++i)
    {
        col_names[i] = const_cast<char*>(sqlite3_column_name(stmt, i));
    }

    while(sqlite3_step(stmt) == SQLITE_ROW)
    {
        char** row = new char*[cols];
        for(int i = 0; i < cols; ++i)
        {
            const char* text = (const char*)sqlite3_column_text(stmt, i);
            row[i] = (char*) (text ? text : "NULL");
        }

        rc = callback(user_data, cols, row, col_names);
        delete[] row;

        if(rc != 0) break;  // respect early abort
    }

    delete[] col_names;
    sqlite3_finalize(stmt);
    return rc;
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(sqlite3 *db_conn, const char* query, rocprofvis_db_sqlite_callback_parameters * params)
{
    PROFILE;
    if (IsOpen())
    {

        char *zErrMsg = 0;
        sqlite3_mutex_enter(sqlite3_db_mutex(db_conn));
        int rc = Sqlite3Exec(db_conn, query, params->callback, params);
        sqlite3_mutex_leave(sqlite3_db_mutex(db_conn));
        if( rc != SQLITE_OK ) {
            spdlog::debug("Query: "); spdlog::debug(query);
            spdlog::debug("SQL error "); spdlog::debug(std::to_string(rc).c_str()); spdlog::debug(":"); 
            spdlog::debug(sqlite3_errmsg(m_db));
            return kRocProfVisDmResultDbAccessFailed;
        } 
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}

}  // namespace DataModel
}  // namespace RocProfVis
