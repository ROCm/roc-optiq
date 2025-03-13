// MIT License
//
// Copyright (c) 2023 Advanced Micro Devices, Inc. All rights reserved.
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
        LOG("Detect table error "); ADD_LOG(std::to_string(rc)); ADD_LOG(":"); ADD_LOG(zErrMsg);
        sqlite3_free(zErrMsg);
    }
    return rc;
}

rocprofvis_db_type_t SqliteDatabase::Detect(rocprofvis_db_filename_t filename){
    sqlite3 *db;
    if( sqlite3_open(filename, &db) != SQLITE_OK) return rocprofvis_db_type_t::kAutodetect;

    if (DetectTable(db, "rocpd_region") == SQLITE_OK) {
        sqlite3_close(db);
        return rocprofvis_db_type_t::kRocprofSqlite;
    }

    if (DetectTable(db, "rocpd_api") == SQLITE_OK) {
        sqlite3_close(db);
        return rocprofvis_db_type_t::kRocpdSqlite;
    }
    
    sqlite3_close(db);
    return rocprofvis_db_type_t::kAutodetect;
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
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, nullptr, kRocProfVisDmNotATrack, kRocProfVisDmOperationNoOp,"",0 };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback, kRocProfVisDmNotATrack, kRocProfVisDmOperationNoOp,"",0 };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query,
                                                        rocprofvis_dm_handle_t handle, 
                                                        RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback, kRocProfVisDmNotATrack, kRocProfVisDmOperationNoOp,"",0 };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query, 
                                                        RpvSqliteExecuteQueryCallback callback, 
                                                        roprofvis_dm_track_category_t track_category)
{
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback, track_category, kRocProfVisDmOperationNoOp,"",0 };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query, 
                                                        RpvSqliteExecuteQueryCallback callback, 
                                                        roprofvis_dm_track_category_t track_category, 
                                                        rocprofvis_dm_handle_t handle)
{
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback, track_category, kRocProfVisDmOperationNoOp,"",0 };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, 
                                                        const char* query, 
                                                        RpvSqliteExecuteQueryCallback callback, 
                                                        roprofvis_dm_track_category_t track_category, 
                                                        rocprofvis_dm_handle_t handle,
                                                        roprofvis_dm_event_operation_t event_op)
{
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback, track_category, event_op,"",0};
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery( Future* future, 
                                                        const char* query, 
                                                        RpvSqliteExecuteQueryCallback callback, 
                                                        rocprofvis_dm_handle_t handle, 
                                                        roprofvis_dm_event_operation_t event_op){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback, kRocProfVisDmNotATrack, event_op,"",0};
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}
rocprofvis_dm_result_t SqliteDatabase::ExecuteSQLQuery( Future* future, 
                                                        const char* query, 
                                                        RpvSqliteExecuteQueryCallback callback, 
                                                        rocprofvis_dm_charptr_t ext_data_category, 
                                                        rocprofvis_dm_handle_t handle, 
                                                        roprofvis_dm_event_operation_t event_op){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, handle, callback, kRocProfVisDmNotATrack, event_op, ext_data_category};
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(const char* query, rocprofvis_db_sqlite_callback_parameters * params)
{
    PRINT_TIME_USAGE;
    if (IsOpen())
    {
        char *zErrMsg = 0;
        sqlite3_mutex_enter(sqlite3_db_mutex(m_db));
        int rc = sqlite3_exec(m_db, query, params->callback, params, &zErrMsg);
        sqlite3_mutex_leave(sqlite3_db_mutex(m_db));
        if( rc != SQLITE_OK ) {
            LOG("Query: "); ADD_LOG(query);
            LOG("SQL error "); ADD_LOG(std::to_string(rc)); ADD_LOG(":");ADD_LOG(zErrMsg);
            sqlite3_free(zErrMsg);
            return kRocProfVisDmResultDbAccessFailed;
        } 
        return kRocProfVisDmResultSuccess;
    }
    return kRocProfVisDmResultNotLoaded;
}