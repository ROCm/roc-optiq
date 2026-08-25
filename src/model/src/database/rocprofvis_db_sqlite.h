// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db.h"
#include "sqlite3.h" 
#include <set>
#include <mutex>
#include <condition_variable>
#include "rocprofvis_db_query_builder.h"

namespace RocProfVis
{
namespace DataModel
{

#define MAX_CONNECTIONS 100
#define SINGLE_THREAD_RECORDS_COUNT_LIMIT 50000
#define NO_THREAD_RECORDS_COUNT_LIMIT 1000


// type of sqlite3_exec callback function
typedef int (*RpvSqliteExecuteQueryCallback)(void*, int, sqlite3_stmt*, char**);
typedef struct SQLInsertParam
{
    const char* column;
    const char* type;
} SQLInsertParam;

typedef std::vector<SQLInsertParam>  SQLInsertParams;


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
    std::vector<std::string> query;
    rocprofvis_dm_track_id_t track_id;
    rocprofvis_dm_event_operation_t operation;
    DbInstance* db_instance;
} rocprofvis_db_sqlite_callback_parameters;

typedef struct rocprofvis_db_sqlite_db_node_t
{
    uint32_t node_id;
    std::string filepath;
    std::set<sqlite3*> m_available_connections;
    std::set<sqlite3*> m_connections_inuse;
    std::mutex         m_mutex;
    std::condition_variable      m_inuse_cv;
}rocprofvis_db_sqlite_db_node_t;

typedef std::map<void*, std::map<std::string, uint64_t>> rocprofvis_null_data_exceptions_int;
typedef std::map<void*, std::map<std::string, std::string>> rocprofvis_null_data_exceptions_string;
typedef std::map<void*, std::set<std::string>> rocprofvis_null_data_exceptions_skip;

// class for any Sqlite database methods and properties 
class SqliteDatabase : public Database
{
    public:
        // Database constructor
        // @param path - full path to database file
        SqliteDatabase( rocprofvis_db_filename_t path) : 
                        Database(path) {};
        // SqliteDatabase destructor, must be defined as virtual to free resources of derived classes 
        virtual ~SqliteDatabase() {Close();}
        // Method to open sqlite database
        // @return status of operation
        rocprofvis_dm_result_t Open() override;
        // Method to close sqlite database
        // @return status of operation
        rocprofvis_dm_result_t Close() override;
        void  InterruptQuery(void* connection) override;
        // check if table present in database
        bool CheckTableExists(const std::string& table_name, uint32_t db_node_id);

    protected:
        // ---------------------------------------SQL operations-----------------------------------------
        // Method to create SQL table
        // @param table_name - table name 
        // @param parameters - column insert parameters 
        // @param num_cols - number of columns
        // @param num_row - number of rows
        // @param insert_func - lambda method for data insertion
        // @return status of operation
        rocprofvis_dm_result_t CreateSQLTable(const char* table_name, 
                                              SQLInsertParams parameters,
                                              size_t num_row,
                                              std::function<void(sqlite3_stmt* stmt, int index)> insert_func,
                                              uint32_t db_node_id=0);
        // Method to delete SQL table
        // @param table_name - table name 
        // @return status of operation
        rocprofvis_dm_result_t DropSQLTable(const char* table_name, uint32_t db_node_id=0);
        rocprofvis_dm_result_t DropSQLIndex(const char* table_name, uint32_t db_node_id=0);
        std::vector<std::string> GetRocpdIndexes(uint32_t db_node_id);
        // Method for SQL query execution without any callback
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                DbInstance* db_instance,
                                                const char* query);
        // Method for SQL query execution with callback to process table rows 
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                DbInstance* db_instance,
                                                uint32_t load_id,
                                                std::vector<std::string> query, 
                                                RpvSqliteExecuteQueryCallback find_callback,
                                                RpvSqliteExecuteQueryCallback load_callback);
        // Method for single row and column SQL query execution returning result of the query as string 
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param callback - sqlite3_exec callback method for data processing
        // @param value - pointer reference to string where the value will be placed
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                DbInstance* db_instance,
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
                                                DbInstance* db_instance,
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
                                                DbInstance* db_instance,
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
                                                DbInstance* db_instance,
                                                const char* query,
                                                rocprofvis_dm_handle_t handle, 
                                                RpvSqliteExecuteQueryCallback callback);
        // Method for SQL query execution with  handle and query index parameter. 
        // Used for callbacks storing data into container with rocprofvis_dm_handle_t handle
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param handle - handle of a container processed rows to be stored
        // @param index - query index
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future,
            DbInstance* db_instance,
            const char* query,
            rocprofvis_dm_handle_t handle,
            uint32_t index,
            RpvSqliteExecuteQueryCallback callback);
         // Method for SQL query execution with multi-use subquery parameter. 
        // Used for callbacks storing data into container with rocprofvis_dm_handle_t handle
        // @param future - future object for asynchronous execution status
        // @param query - SQL query
        // @param subquery - multi-use parameter
        // @param callback - sqlite3_exec callback method for data processing
        // @return status of operation
        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                                DbInstance* db_instance,
                                                const char* query, 
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
                                                DbInstance* db_instance,
                                                const char* query,
                                                const char* cache_table_name,
                                                rocprofvis_dm_handle_t handle, 
                                                RpvSqliteExecuteQueryCallback callback);

        rocprofvis_dm_result_t ExecuteSQLQuery(Future* future, 
                                               DbInstance* db_instance,
                                               const char* query,
                                               const char*  cache_table_name,
                                               rocprofvis_dm_handle_t handle,
                                               rocprofvis_dm_event_operation_t op,
                                               RpvSqliteExecuteQueryCallback   callback);
        // method to run SQL query
        // @param db_conn - database connection 
        // @param query - SQL query
        // @param params - set of parameters to be passed to sqlite3_exec callback
        rocprofvis_dm_result_t ExecuteSQLQuery(
                                               DbInstance* db_instance, 
                                               const char* query, 
                                               rocprofvis_db_sqlite_callback_parameters * params);

        rocprofvis_dm_result_t ExecuteTransaction(
                                               std::vector<std::string> queries, 
                                               uint32_t db_node_id = 0);

        // Method to check if table exists in database
        // @param is_view - true if view
        // @param table - name of the table
        // @param conn - connection
        static int DetectTable(sqlite3* conn, const char* table, bool is_view = true);

        // ---------------------------------------Thread workers--------------------------------------------
        static rocprofvis_dm_result_t ExecuteSQLQueryStatic(
            SqliteDatabase* db, 
            Future* future, 
            DbInstance* db_instance,
            const char* query,
            RpvSqliteExecuteQueryCallback callback);
        static rocprofvis_dm_result_t ExecuteSQLQueryStaticWithHandle(
            SqliteDatabase* db,
            Future* future,
            DbInstance* db_instance,
            const char* query,
            rocprofvis_dm_handle_t handle,
            uint32_t query_index,
            RpvSqliteExecuteQueryCallback callback);


        // ------------------------------Wrappers around SQL getters-------------------------------------
        char* Sqlite3ColumnText(void* func, sqlite3_stmt* stmt, char** azColName, int index);
        int Sqlite3ColumnInt(void* func, sqlite3_stmt* stmt, char** azColName, int index);
        int64_t Sqlite3ColumnInt64(void* func, sqlite3_stmt* stmt, char** azColName, int index);
        double Sqlite3ColumnDouble(void* func, sqlite3_stmt* stmt, char** azColName, int index);

        // ---------------------------------------Callbacks--------------------------------------------
        static int CallbackGetValue(void* data, int argc, sqlite3_stmt* stmt, char** azColName);  
        static int CallbackRunQuery(void *data, int argc, sqlite3_stmt* stmt, char **azColName); 
        static int CallbackMakeHistogramPerTrack(void* data, int argc, sqlite3_stmt* stmt, char** azColName);

        // ---------------------------------------Helpers--------------------------------------------        

        sqlite3* GetServiceConnection(uint32_t db_node_id=0);
        void CreateDbNodes(std::vector<std::string>& multinode_files);
        void CreateDbNode(rocprofvis_db_filename_t filepath);
        // Append one more db node at runtime (used by the incremental AddNode path) and
        // prime a connection for it. Returns the new node's file-node id.
        uint32_t AddDbNodeRuntime(rocprofvis_db_filename_t filepath);
        // Find the file-node index for a given path, or -1 if not present.
        int FindDbNodeIndex(rocprofvis_db_filename_t filepath) const;
        // Close and free one db node's sqlite connections (used by in-place remove). The
        // node slot is kept (emptied) so higher node ids stay stable.
        void CloseDbNode(uint32_t db_node_id);
        // Close every connection in both of a node's pools (available + in-use), clearing the
        // pools. Uses sqlite3_close_v2 so a still-busy handle is closed lazily rather than
        // leaked. Returns false if any close reported an error. Caller must hold node.m_mutex.
        bool CloseNodeConnections(rocprofvis_db_sqlite_db_node_t& node);
        virtual MetadataVersionControl* GetMetadataVersionControl() { return nullptr; };

        // --------------------------------Null value handlers-------------------------------------- 
        virtual const rocprofvis_null_data_exceptions_int* GetNullDataExceptionsInt() = 0;
        virtual const rocprofvis_null_data_exceptions_string* GetNullDataExceptionsString() = 0;
        virtual const rocprofvis_null_data_exceptions_skip* GetNullDataExceptionsSkip() = 0;
        uint64_t GetNullExceptionInt(void* func, char* column);
        char* GetNullExceptionString(void* func, char* column);
        bool NullExceptionSkip(void* func, char* column);


    private:     
      

        // method to mimic slite3_exec using sqlite3_prepare_v2
        // @param db - database connection
        // @param query - SQL query
        // @param callback - sqlite3_exec type callback
        // @param user_data - user parameters
        int Sqlite3Exec(sqlite3* db, const char* query,
                                        int (*callback)(void*, int, sqlite3_stmt*, char**),
                                        void* user_data);
        // Method to open sqlite database connection
        // @connection - pointer to connection
        // @return status of operation
        rocprofvis_dm_result_t OpenConnection(uint32_t db_node_id, sqlite3** connection);
        // allocate another connection
        sqlite3* GetConnection(uint32_t db_node_id); 
        // release connection
        void ReleaseConnection(sqlite3* conn);
        void ReleaseConnection(sqlite3* conn, uint32_t db_node_id);

        static void ReplaceAllSubstrings(std::string& str, const std::string& from, const std::string& to);

    protected:

        std::vector<std::unique_ptr<rocprofvis_db_sqlite_db_node_t>> m_db_nodes;
       
};

}  // namespace DataModel
}  // namespace RocProfVis