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

#ifndef RPV_SQLITE_DATABASE_H
#define RPV_SQLITE_DATABASE_H

#include "Database.h"
#include "sqlite3/sqlite3.h" 

typedef int (*RpvSqliteExecuteQueryCallback)(void*,int,char**,char**);

typedef struct{
    Database* db;
    Future* future;
    rocprofvis_dm_handle_t handle;
    RpvSqliteExecuteQueryCallback callback;
    roprofvis_dm_track_category_t track_category;
    roprofvis_dm_event_operation_t event_op;
    rocprofvis_dm_charptr_t ext_data_category;
    rocprofvis_dm_index_t row_counter;
} rocprofvis_db_sqlite_callback_parameters;

class SqliteDatabase : public Database
{
    public:
        SqliteDatabase( rocprofvis_db_filename_t path) : 
                        Database(path), 
                        m_db(nullptr),
                        m_db_status(SQLITE_ERROR) {};
        rocprofvis_dm_result_t Open() override;
        rocprofvis_dm_result_t Close() override;
        bool IsOpen() override {return m_db != nullptr && m_db_status == SQLITE_OK;}
        static rocprofvis_db_type_t Detect(rocprofvis_db_filename_t filename);
        static int CallbackTableExists(void *data, int argc, char **argv, char **azColName);
        static int DetectTable(sqlite3 *db, const char* table);
    protected:
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, const char* query);
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, const char* query, 
                                                RpvSqliteExecuteQueryCallback callback);
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query,
                                                rocprofvis_dm_handle_t handle, 
                                                RpvSqliteExecuteQueryCallback callback);
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                RpvSqliteExecuteQueryCallback callback, 
                                                roprofvis_dm_track_category_t track_category);
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                RpvSqliteExecuteQueryCallback callback, 
                                                roprofvis_dm_track_category_t track_category, 
                                                rocprofvis_dm_handle_t handle);
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                RpvSqliteExecuteQueryCallback callback, 
                                                roprofvis_dm_track_category_t track_category, 
                                                rocprofvis_dm_handle_t handle, 
                                                roprofvis_dm_event_operation_t event_op);
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                RpvSqliteExecuteQueryCallback callback, 
                                                rocprofvis_dm_handle_t handle, 
                                                roprofvis_dm_event_operation_t event_op);
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                RpvSqliteExecuteQueryCallback callback, 
                                                rocprofvis_dm_charptr_t ext_data_category, 
                                                rocprofvis_dm_handle_t handle, 
                                                roprofvis_dm_event_operation_t event_op);
        
    private:       
        sqlite3 *m_db;
        int m_db_status;

        rocprofvis_dm_result_t ExecuteSQLQuery(const char* query, rocprofvis_db_sqlite_callback_parameters * params);
};
#endif //RPV_SQLITE_DATABASE_H