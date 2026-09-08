// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_future.h"
#include "rocprofvis_db_cache.h"
#include "rocprofvis_db_track.h"
#include "rocprofvis_db_version.h"
#include "rocprofvis_db_packed_storage.h"
#ifdef USE_PROFILER_HUB
#include "profiler_hub_client_interface.h"
#endif
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <shared_mutex>
#include <condition_variable>
#include <filesystem>

namespace RocProfVis
{
namespace DataModel
{

typedef std::pair<DbInstance, std::string> GuidInfo;
typedef std::vector<GuidInfo> guid_list_t;


class TemporaryDbInstance : public DbInstance
{
public:
    TemporaryDbInstance(uint32_t file_index) : DbInstance(file_index, 0) {};
};

class SingleNodeDbInstance : public DbInstance
{
public:
    SingleNodeDbInstance() : DbInstance(0, 0) {};
};

// type of sqlite3_exec callback function
typedef int (*RpvCallback)(void*, int, void*, char**);

// structure to pass parameters to query callbacks
typedef struct{
    // pointer tp Database object
    Database* db;
    // pointer to Future object, to check if thread has been interrupted
    Future* future;
    // pointer to container object handle, to add processed rows data to the container
    rocprofvis_dm_handle_t handle;
    // callback method pointer
    RpvCallback callback;
    // pointer to query string, convenient for multiuse callback debugging
    std::vector<std::string> query;
    rocprofvis_dm_track_id_t track_id;
    rocprofvis_dm_event_operation_t operation;
    DbInstance* db_instance;
} rocprofvis_db_query_callback_parameters;


class Database
{
    public:
        // Database constructor
        // @param path - full path to database file
        Database(   
                    rocprofvis_db_filename_t path):
                    m_path(path),
                    m_binding_info(nullptr) {
        };
        // Database destructor, must be defined as virtual to free resources of derived classes 
        virtual ~Database(){};

        // Method to open database, must be overriden by derived classes
        // @return status of operation
        virtual rocprofvis_dm_result_t  Open() = 0;
        // Method to close database, must be overriden by derived classes
        // @return status of operation
        virtual rocprofvis_dm_result_t  Close() = 0;

        // Get amount of memory used by database resource
        // @return memory size
        virtual rocprofvis_dm_size_t    GetMemoryFootprint(void); 

        // Bind database to trace
        // @param binding_info - pointer to binding info structure 
        // @return status of operation
        rocprofvis_dm_result_t          BindTrace(
                                                                rocprofvis_dm_db_bind_struct * binding_info);
        // returns pointer to binding structure
        rocprofvis_dm_db_bind_struct *  BindObject() {return m_binding_info;}
        // Method to cleanup database
        // @return status of operation
        rocprofvis_dm_result_t  CleanupAsync(rocprofvis_db_future_t object, bool rebuild);
        // Asynchronously read trace metadata from database
        // @param object - future object providing asynchronous execution mechanism
        // @return status of operation
        rocprofvis_dm_result_t          ReadTraceMetadataAsync( 
                                                                rocprofvis_db_future_t object);
        
       virtual void InterruptQuery(void* connection) { (void) connection; };


    private:
    /************************static methods to be used as a parameter to std::thread**********************/

        //static method to read metadata. Required to launch a unique thread for asynchronous metadata read 
        // @param db - pointer to database object 
        // @param object - future object providing asynchronous execution mechanism   
        static rocprofvis_dm_result_t   ReadTraceMetadataStatic(
                                                                Database* db, 
                                                                Future* object);
        
        // static method to find a value in cached tables by specifying reserved table name, instance id and column name
        // @param object - database handler
        // @param table_name - a name of cached table assigned at the time of caching
        // @param instance_id - cached table instance id
        // @param column_name - cached table column name 
        // @param node - internal database node id
        // @param value - reference pointer to database cell value
        // @return status of operation 
        static rocprofvis_dm_result_t   FindCachedTableValue(  const rocprofvis_dm_database_t object, 
            rocprofvis_dm_charptr_t table_name, 
            const rocprofvis_dm_id_t instance_id, 
            rocprofvis_dm_charptr_t column_name,
            rocprofvis_dm_node_id_t node,
            rocprofvis_dm_charptr_t* value); 

        static rocprofvis_dm_result_t  CleanupStatic(Database* db, Future* future, bool rebuild);


    /************************pure virtual worker methods to be implemented in derived classes**********************/

        // worker method to read trace metadata 
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        virtual rocprofvis_dm_result_t  ReadTraceMetadata(
                                                                Future* object) = 0;

        virtual rocprofvis_dm_result_t  Cleanup(Future* future, bool rebuild) { (void) future; (void) rebuild; return kRocProfVisDmResultSuccess; };

    private:
        // pointer to a binding information structure physically located in Trace object and passed to Database object during binding
        // binding structure contains methods to transfer data between database and trace objects 
        rocprofvis_dm_db_bind_struct *m_binding_info;
        // map array of cached tables, mostly with non-essential Track information
        std::unordered_map<uint32_t, DatabaseCache> m_cached_tables;
        // database file path
        std::string m_path;
        // app config path
        std::string m_config_path;
        guid_list_t   m_db_instances;


    protected:
        // ---------------------------------------------Getters---------------------------------------
        guid_list_t& DbInstances() { return m_db_instances; }
        uint32_t NumDbInstances() { return static_cast<uint32_t>(m_db_instances.size()); }
        std::string GuidAt(int index) { return index < m_db_instances.size() ? m_db_instances[index].second : std::string(); }
        std::string GuidSymAt(int index) { std::string s = GuidAt(index); std::replace(s.begin(), s.end(), '_', '-'); return s; }
        DbInstance* DbInstancePtrAt(int index) { return index < m_db_instances.size() ? &m_db_instances[index].first : nullptr; }
        // returns pointer to cached tables map array
        DatabaseCache*                  CachedTables(uint32_t node_id) {return &m_cached_tables[node_id];}
        // returns pointer to database file path
        rocprofvis_db_filename_t        Path() {return m_path.c_str();}
        // returns pointer to trace properties, which contains shared trace information
        rocprofvis_dm_trace_params_t*   TraceProperties() { return m_binding_info->trace_properties; }

        // ---------------------------------------------Helpers---------------------------------------


        // calls Future object callback method, if provided. The callback method is optionally provided by caller in order to display or save current database progress.
        // @param step - approximate percentage of single database operation
        // @param action - database operation description
        // @param status - database operation status 
        // @param future - future object providing callback mechanism
        void                            ShowProgress(
                                                                double step, 
                                                                rocprofvis_dm_charptr_t action, 
                                                                rocprofvis_db_status_t status, 
                                                                Future* future);
        
        //--------------------------------------Direct interface to info tables-----------------------------------------------------------------
        static rocprofvis_dm_table_t GetInfoTableHandle(const rocprofvis_dm_database_t object, rocprofvis_dm_node_id_t node, rocprofvis_dm_charptr_t table_name);
        static size_t GetInfoTableNumColumns(rocprofvis_dm_table_t object);
        static size_t GetInfoTableNumRows(rocprofvis_dm_table_t object);
        static const char* GetInfoTableColumnName(rocprofvis_dm_table_t object, size_t column_index);
        static rocprofvis_dm_table_row_t GetInfoTableRowHandle(rocprofvis_dm_table_t object, size_t row_index);
        static const char* GetInfoTableRowCellValue(rocprofvis_dm_table_row_t object, size_t column_index);
        static const size_t GetInfoTableRowNumCells(rocprofvis_dm_table_row_t object);
        //--------------------------------------Static helpers-----------------------------------------------------------------
        static bool SanitizeFilePath(const std::string& filename, std::filesystem::path& out_path);
        static bool IsNumber(const std::string& s);

    public:
        // declare DatabaseCache as friend class, for having access to protected members
        friend class DatabaseCache;
        friend class TableProcessor;
        friend class TrackLookup;
        friend class PackedTable;
        friend class SqliteDatabase;
        friend class ProfilerHubClientMethods;
};

}  // namespace DataModel
}  // namespace RocProfVis
