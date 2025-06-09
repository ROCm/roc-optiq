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

#pragma once

#include "rocprofvis_db.h"
#include "sqlite3.h" 

namespace RocProfVis
{
namespace DataModel
{

// type of sqlite3_exec callback function
typedef int (*RpvSqliteExecuteQueryCallback)(void*,int,char**,char**);
// structure to pass parameters to sqlite3_exec callbacks
typedef struct{
    // pointer tp Database object
    Database* db;
    // pointer to Future object, to check if thread has been interrupted
    Future* future;
    // pointer to container object handle, to add processed rows data to the container
    rocprofvis_dm_handle_t handle;
    // callback method pointer
    RpvSqliteExecuteQueryCallback callback;
    // pointer to query string, convenient for multiuse callback debugging
    rocprofvis_dm_charptr_t query;
    // multi-use string  
    rocprofvis_dm_charptr_t subquery;
    rocprofvis_dm_charptr_t table_subquery;
} rocprofvis_db_sqlite_callback_parameters;

// class for any Sqlite database methods and properties 
class SqliteDatabase : public Database
{
    public:
        // Database constructor
        // @param path - full path to database file
        SqliteDatabase( rocprofvis_db_filename_t path) : 
                        Database(path), 
                        m_db(nullptr),
                        m_db_status(SQLITE_ERROR) {};
        // SqliteDatabase destructor, must be defined as virtual to free resources of derived classes 
        virtual ~SqliteDatabase() {Close();}
        // Method to open sqlite database
        // @return status of operation
        rocprofvis_dm_result_t Open() override;
        // Method to open sqlite database connection
        // @connection - pointer to connection
        // @return status of operation
        rocprofvis_dm_result_t OpenConnection(sqlite3** connection);
        // Method to close sqlite database
        // @return status of operation
        rocprofvis_dm_result_t Close() override;
        // Method to check if database is open
        // @return True if open
        bool IsOpen() override {return m_db != nullptr && m_db_status == SQLITE_OK;}

    protected:
        // Method for SQL query execution without any callback
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query);
        // Method for SQL query execution with callback to process table rows 
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                RpvSqliteExecuteQueryCallback callback);

        // Method for SQL query execution with specified database connection and callback to process table rows
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(sqlite3* db_conn, 
                                               Future* future,
                                               const char*                   query,
                                               RpvSqliteExecuteQueryCallback callback);
        // Method for single row and column SQL query execution returning result of the query as string 
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param callback - sqlite3_exec callback method for data processing
        // @param value - pointer reference to string where the value will be placed
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                RpvSqliteExecuteQueryCallback callback,
                                                rocprofvis_dm_string_t* value);
        // Method for single row and column SQL query execution returning result of the query as uint64 
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param callback - sqlite3_exec callback method for data processing
        // @param value - pointer reference to uint64_t variable where the value will be placed
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                RpvSqliteExecuteQueryCallback callback,
                                                uint64_t & value);
        // Method for single row and column SQL query execution returning result of the query as uint32 
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param callback - sqlite3_exec callback method for data processing
        // @param value - pointer reference to uint32_t variable where the value will be placed
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                RpvSqliteExecuteQueryCallback callback,
                                                uint32_t & value);
        // Method for SQL query execution with  handle parameter. 
        // Used for callbacks storing data into container with rocprofvis_dm_handle_t handle
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param handle - handle of a container processed rows to be stored
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query,
                                                rocprofvis_dm_handle_t handle, 
                                                RpvSqliteExecuteQueryCallback callback);
        // Method for SQL query execution with specified database connection and handle parameter.
        // Used for callbacks storing data into container with rocprofvis_dm_handle_t
        // handle
        // @param db_conn - database connection
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param handle - handle of a container processed rows to be stored
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(sqlite3* db_conn, 
                                               Future*  future,
                                               const char*                   query,
                                               rocprofvis_dm_handle_t        handle,
                                               RpvSqliteExecuteQueryCallback callback);
         // Method for SQL query execution with multi-use subquery parameter. 
        // Used for callbacks storing data into container with rocprofvis_dm_handle_t handle
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param subquery - multi-use parameter
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query, 
                                                const char* subquery,
                                                RpvSqliteExecuteQueryCallback callback);
        // Method for SQL query execution with multi-use subquery and handle parameter. 
        // Used for callbacks storing data into container with rocprofvis_dm_handle_t handle
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param subquery - multi-use parameter
        // @param handle - handle of a container processed rows to be stored
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                const char* query,
                                                const char* subquery,
                                                rocprofvis_dm_handle_t handle, 
                                                RpvSqliteExecuteQueryCallback callback);
        // Method for SQL query execution for requesting track information and building track related data.
        // Used for callbacks storing data into container with rocprofvis_dm_handle_t
        // handle
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param timeline_subquery - timeline view events or samples subquery
        // @param table_subquery - table view events or samples subquery
        // @param handle - handle of a container processed rows to be stored
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, const char* query,
                                               const char* timeline_subquery,
                                               const char* table_subquery,
                                               RpvSqliteExecuteQueryCallback callback);

        // Method to check if table exists in database
        // @param db - pointer to sqlite3 object built from the sqlite3 amalgamation
        // @param table - name of the table
        static int DetectTable(sqlite3 *db, const char* table);

        // sqlite3_exec callback to read value from single column and single row
        // @param data - pointer to callback caller argument
        // @param argc - number of columns in the query
        // @param argv - pointer to row values
        // @param azColName - pointer to column names  
        // @return SQLITE_OK if successful
        static int CallbackGetValue(void* data, int argc, char** argv, char** azColName);  
        // sqlite3_exec callback to store all requested rows into Table container
        // @param data - pointer to callback caller argument
        // @param argc - number of columns in the query
        // @param argv - pointer to row values
        // @param azColName - pointer to column names  
        // @return SQLITE_OK if successful
        static int CallbackRunQuery(void *data, int argc, char **argv, char **azColName); 

        static rocprofvis_dm_result_t ExecuteSQLQueryStatic(
            SqliteDatabase* db, 
            rocprofvis_db_connection_t conn,
            Future* future, 
            const char* query,
            RpvSqliteExecuteQueryCallback callback);

    private:     

        // sqlite3_exec callback to succeed if table exists in database and fail otherwise
        // @param data - pointer to callback caller argument
        // @param argc - number of columns in the query
        // @param argv - pointer to row values
        // @param azColName - pointer to column names  
        // @return SQLITE_OK if successful
        static int CallbackTableExists(void *data, int argc, char **argv, char **azColName);

        // pointer to sqlite3 object built from the sqlite3 amalgamation
        sqlite3 *m_db;
        // curent database status
        int m_db_status;

        // method to run SQL query
        // @param db_conn - database connection 
        // @param query - SQL query
        // @param params - set of parameters to be passed to sqlite3_exec callback
        rocprofvis_dm_result_t ExecuteSQLQuery(sqlite3 *db_conn, const char* query, rocprofvis_db_sqlite_callback_parameters * params);

        // method to mimic slite3_exec using sqlite3_prepare_v2
        // @param db - database connection
        // @param query - SQL query
        // @param callback - sqlite3_exec type callback
        // @param user_data - user parameters
        int SqliteDatabase::Sqlite3Exec(sqlite3* db, const char* query,
                                        int (*callback)(void*, int, char**, char**),
                                        void* user_data);

};

}  // namespace DataModel
}  // namespace RocProfVis