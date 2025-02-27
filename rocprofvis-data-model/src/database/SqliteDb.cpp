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
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, nullptr, kRocProfVisDmNotTrack, kRocProfVisDmOperationNoOp };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, RpvSqliteExecuteQueryCallback callback){
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback, kRocProfVisDmNotTrack, kRocProfVisDmOperationNoOp };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, RpvSqliteExecuteQueryCallback callback, roprofvis_dm_track_category_t track_category)
{
    rocprofvis_db_sqlite_callback_parameters params = {this, future, nullptr, callback, track_category, kRocProfVisDmOperationNoOp };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, RpvSqliteExecuteQueryCallback callback, roprofvis_dm_track_category_t track_category, rocprofvis_dm_slice_t slice)
{
    rocprofvis_db_sqlite_callback_parameters params = {this, future, slice, callback, track_category, kRocProfVisDmOperationNoOp };
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(Future* future, const char* query, RpvSqliteExecuteQueryCallback callback, roprofvis_dm_track_category_t track_category, rocprofvis_dm_slice_t slice, roprofvis_dm_event_operation_t event_op)
{
    rocprofvis_db_sqlite_callback_parameters params = {this, future, slice, callback, track_category, event_op};
    return SqliteDatabase::ExecuteSQLQuery(query, &params);
}

rocprofvis_dm_result_t  SqliteDatabase::ExecuteSQLQuery(const char* query, rocprofvis_db_sqlite_callback_parameters * params)
{
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