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

#include "SqliteDb.h"
#include <sstream>


int SqliteDatabase::CallbackGetValue(void* data, int argc, char** argv, char** azColName){
    ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    std::string * string_ptr = (rocprofvis_dm_string_t*)callback_params->handle;
    ASSERT_MSG_RETURN(string_ptr, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    *string_ptr = argv[0];
    return 0;
} 

int SqliteDatabase::CallbackRunQuery(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    SqliteDatabase* db = (SqliteDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    if (0 == callback_params->row_counter)
    {
        for (int i=0; i < argc; i++)
        {
            if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumn(callback_params->handle,azColName[i])) return 1;
        }
    }
    rocprofvis_dm_table_row_t row = db->BindObject()->FuncAddTableRow(callback_params->handle);
    ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 1);
    for (int i=0; i < argc; i++)
    {
        if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableRowCell(row, argv[i])) return 1;
    }
    callback_params->row_counter++;
    return 0;
}



int SqliteDatabase::CallbackTableExists(void *data, int argc, char **argv, char **azColName){
    uint32_t num = std::stol(argv[0]);
    return num > 0 ? 0 : 1;
}

int SqliteDatabase::DetectTable(sqlite3 *db, const char* table){
    sqlite3_mutex_enter(sqlite3_db_mutex(db));
    std::stringstream query;
    char* zErrMsg = 0;
    query << "SELECT COUNT(name) FROM sqlite_master WHERE type='table' AND name='" << table << "';";
    int rc = sqlite3_exec(db, query.str().c_str(), CallbackTableExists, db, &zErrMsg);
    sqlite3_mutex_leave(sqlite3_db_mutex(db));
    if (rc != SQLITE_OK) {
        LOG("Detect table error "); ADD_LOG(std::to_string(rc).c_str()); ADD_LOG(":"); ADD_LOG(zErrMsg);
        sqlite3_free(zErrMsg);
    }
    return rc;
}


rocprofvis_dm_result_t SqliteDatabase::Open()
{
    if( (m_db_status = sqlite3_open(Path(), &m_db)) != SQLITE_OK) {
        LOG("Can't open database:");
        ADD_LOG(sqlite3_errmsg(m_db));
        return kRocProfVisDmResultDbAccessFailed;
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t SqliteDatabase::Close()
{
    if (IsOpen())
    {
        if( sqlite3_close(m_db) != SQLITE_OK ) {

            LOG("Can't close database:");
            ADD_LOG(sqlite3_errmsg(m_db));
            return kRocProfVisDmResultDbAccessFailed;
        }
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}


rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, nullptr,0,""};
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback,0,""};
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, 
                                                RpvSqliteExecuteQueryCallback callback,
                                                rocprofvis_dm_string_t* value){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, (rocprofvis_dm_handle_t)value, callback,0,""};
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
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback,0,""};
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query,
                                                        const char* subquery,
                                                        rocprofvis_dm_handle_t handle, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback,0,subquery};
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}


rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query, 
                                                        const char* subquery,
                                                        RpvSqliteExecuteQueryCallback callback)
{
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback,0,subquery};
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(const char* query, rocprofvis_db_sqlite_callback_parameters * params)
{
    PROFILE;
    if (IsOpen())
    {
        char *zErrMsg = 0;
        sqlite3_mutex_enter(sqlite3_db_mutex(m_db));
        int rc = sqlite3_exec(m_db, query, params->callback, params, &zErrMsg);
        sqlite3_mutex_leave(sqlite3_db_mutex(m_db));
        if( rc != SQLITE_OK ) {
            LOG("Query: "); ADD_LOG(query);
            LOG("SQL error "); ADD_LOG(std::to_string(rc).c_str()); ADD_LOG(":");ADD_LOG(zErrMsg);
            sqlite3_free(zErrMsg);
            return kRocProfVisDmResultDbAccessFailed;
        } 
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}


