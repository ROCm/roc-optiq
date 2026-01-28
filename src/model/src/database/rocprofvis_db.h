// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_future.h"
#include <vector>
#include <map>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

namespace RocProfVis
{
namespace DataModel
{

//types for creating multi-dimensional map array to cache non-essential track information
typedef std::map<std::string, std::string> table_dict_t;
typedef std::map<uint64_t, table_dict_t> table_map_t;
typedef std::map<std::string, table_map_t> ref_map_t;

typedef std::vector<std::unique_ptr<rocprofvis_dm_track_params_t>>::iterator rocprofvis_dm_track_params_it;

// type of map array for generating time slice query for multiple tracks
typedef std::unordered_map<std::string, std::unordered_map<uint32_t,std::string>> slice_query_map_t;
// type of map array for storing slice handlers for multi-track request
typedef std::unordered_map<uint32_t, rocprofvis_dm_slice_t> slice_array_t;
// type of map array for storing string id filters for op table queries 
typedef std::unordered_map<rocprofvis_dm_event_operation_t, std::unordered_map<uint32_t, std::string>> table_string_id_filter_map_t;

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

typedef enum class rocprofvis_db_string_type:uint32_t
{
    kRPVStringTypeNameOrCategory,
    kRPVStringTypeKernelSymbol
} rocprofvis_db_string_type_t;

typedef struct rocprofvis_db_string_id_t
{
    uint64_t m_string_id;
    uint32_t m_guid_id;
    rocprofvis_db_string_type_t m_string_type;

    bool operator==(const rocprofvis_db_string_id_t& other) const {
        return m_string_id == other.m_string_id && m_guid_id == other.m_guid_id && m_string_type == other.m_string_type;
    }
} rocprofvis_db_string_id_t;

class Database;

// Helper class to lock processes in order of database instances
class OrderedMutex {
public:
    explicit OrderedMutex() : m_current_id(0), m_db_instances(nullptr) {}

    void Init(guid_list_t& guid_list) { m_current_id = 0; m_db_instances = &guid_list; }

    void lock(uint32_t id) {
        std::unique_lock<std::mutex> lock(m_lock);
        m_cv.wait(lock, [&] { return id == m_current_id; });  // Wait for your turn
    }

    void lock(std::string guid) {
        std::unique_lock<std::mutex> lock(m_lock);
        m_cv.wait(lock, [&] { return guid == m_db_instances[0][m_current_id].second; });  // Wait for your turn
    }

    void unlock() {
        std::lock_guard<std::mutex> lock(m_lock);
        ++m_current_id;      // Next thread in sequence can proceed
        m_cv.notify_all();   // Wake up waiting threads
    }

    void reset() { m_current_id = 0; }

private:
    std::mutex m_lock;
    std::condition_variable m_cv;
    uint32_t m_current_id;
    guid_list_t* m_db_instances;
};

// Helper class to manage cached information tables (node, agent, queue, process, thread information)

class TableCache
{
public:
    struct Row {
        uint64_t id;
        std::vector<std::string> values;
    };

    uint32_t AddColumn(const std::string& name)
    {
        uint32_t index = 0;
        auto it = m_column_index.find(name);
        if (it == m_column_index.end())
        {
            index = m_columns.size();
            m_column_index[name] = index;
            m_columns.push_back( name );
        }
        else
        {
            index = it->second;
        }
        return index;
    }

    void AddRow(uint64_t id)
    {
        auto it = m_row_index.find(id);
        if (it == m_row_index.end())
        {
            m_row_index[id] = m_rows.size();
            m_rows.push_back({ id });
            m_rows.back().values.resize(m_columns.size());
        }
    }

    void AddCell(uint32_t column_index, uint64_t row_id, std::string cell)
    {
        auto it = m_row_index.find(row_id);
        if (it != m_row_index.end())
        {
            m_rows[it->second].values[column_index] = cell;
        }
    }

    const char* GetCell(uint64_t row_id, const char* column_name)
    {
        auto it_row = m_row_index.find(row_id);
        auto it_column = m_column_index.find(column_name);
        if (it_row != m_row_index.end() && it_column!=m_column_index.end())
        {
            return m_rows[it_row->second].values[it_column->second].c_str();
        }
        return "";
    }

    const char* GetCell(uint64_t row_index, uint32_t column_index)
    {
        if (row_index < m_rows.size() && column_index < m_columns.size())
        {
            return m_rows[row_index].values[column_index].c_str();
        }
        return "";
    }

    const char* GetCellByIndex(uint64_t row_index, const char* column_name)
    {
        auto it_column = m_column_index.find(column_name);
        if (row_index < m_rows.size() && it_column!=m_column_index.end())
        {
            return m_rows[row_index].values[it_column->second].c_str();
        }
        return "";
    }

    const char* GetColumn(uint32_t column_index)
    {
        if (column_index < m_columns.size())
        {
            return m_columns[column_index].c_str();
        }
        return "";
    }

    void* GetRow(uint64_t row_index)
    {
        if (row_index < m_rows.size())
        {
            return &m_rows[row_index];
        }
        return nullptr;
    }

    uint32_t NumColumns() { return m_columns.size(); }
    size_t NumRows() { return m_rows.size(); }

private:
    std::string m_name;
    std::vector<std::string> m_columns;
    std::vector<Row> m_rows;
    std::unordered_map<std::string, uint32_t> m_column_index;
    std::unordered_map<uint64_t, uint32_t> m_row_index;
};

class DatabaseCache 
{
    public:
        void AddTableCell(const char* table_name, uint64_t row_id, uint32_t column_index, const char* cell_value)
        {
            tables[table_name].AddCell(column_index, row_id, cell_value);
        }
        void AddTableCell(const char* table_name, uint64_t row_id, const char* column_name, const char* cell_value)
        {
            uint32_t column_index = tables[table_name].AddColumn(column_name);
            tables[table_name].AddCell(column_index, row_id, cell_value);
        }
        void AddTableRow(const char* table_name, uint64_t row_id)
        {
            tables[table_name].AddRow(row_id);
        }
        void AddTableColumn(const char* table_name, const char* column_name)
        {
            tables[table_name].AddColumn(column_name);
        }
        const char* GetTableCell(const char* table_name, uint64_t row_id, const char* column_name)
        {
            return tables[table_name].GetCell(row_id,column_name);
        }
        const char* GetTableCellByIndex(const char* table_name, uint32_t row_index, const char* column_name)
        {
            return tables[table_name].GetCellByIndex(row_index,column_name);
        }

        void* GetTableHandle(const char* table_name)
        {
            return &tables[table_name];
        }
      
        // Populate track extended data objects with table content
        rocprofvis_dm_result_t PopulateTrackExtendedDataTemplate(Database * db, uint32_t node_id, const char* table_name, uint64_t instance_id ); 
        // Get amount of memory used by the cached values map
        rocprofvis_dm_size_t    GetMemoryFootprint(void); 

    private:
        
        std::map<std::string, TableCache> tables;

};

class DatabaseVersion
{
public:

    void SetVersion(const char* version);
    bool IsVersionEqual(const char*);
    bool IsVersionGreaterOrEqual(const char*);

    uint32_t GetMajorVersion()
    {
        return m_db_version.size() > 0 ? m_db_version[0] : 0;
    }
    uint32_t GetMinorVersion()
    {
        return m_db_version.size() > 1 ? m_db_version[1] : 0;
    }
    uint32_t GetPatchVersion()
    {
        return m_db_version.size() > 2 ? m_db_version[2] : 0;
    }
private:
    std::vector<uint32_t>  ConvertVersionStringToInt(const char* version);
    std::vector<uint32_t> m_db_version;
};

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
        // Asynchronously read trace metadata from database
        // @param object - future object providing asynchronous execution mechanism
        // @return status of operation
        rocprofvis_dm_result_t          ReadTraceMetadataAsync( 
                                                                rocprofvis_db_future_t object);
        // Asynchronously read a time slice (records from specified number of tracks for specified time frame) from database  
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param num - number of tracks
        // @param tracks - uint32_t array with track IDs  
        // @param object - future object providing asynchronous execution mechanism         
        // @return status of operation                                            
        rocprofvis_dm_result_t          ReadTraceSliceAsync( 
                                                                rocprofvis_dm_timestamp_t start,
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_db_num_of_tracks_t num,
                                                                rocprofvis_db_track_selection_t tracks,
                                                                rocprofvis_db_future_t object);
        // Asynchronously read different types of event properties (flowtrace, stacktrace, extdata) for event ID
        // @param type - event property type (flowtrace, stacktrace, extdata) 
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        rocprofvis_dm_result_t          ReadEventPropertyAsync(
                                                                rocprofvis_dm_event_property_type_t type,
                                                                rocprofvis_dm_event_id_t event_id,
                                                                rocprofvis_db_future_t object);
        // Asynchronously run any table query and store results into Table object 
        // @param query - database query 
        // @param description - database description
        // @param object - future object providing asynchronous execution mechanism 
        // @param id new id is assigned to the table and returned using this reference pointer
        // @return status of operation
        rocprofvis_dm_result_t          ExecuteQueryAsync(
                                                                rocprofvis_dm_charptr_t query,
                                                                rocprofvis_dm_charptr_t description,
                                                                rocprofvis_db_future_t object,
                                                                rocprofvis_dm_table_id_t* id);
        // Asynchronously run compute table query and store results into Table object 
        // @param query - database query 
        // @param description - database description
        // @param object - future object providing asynchronous execution mechanism 
        // @param id new id is assigned to the table and returned using this reference pointer
        // @return status of operation
        rocprofvis_dm_result_t          ExecuteComputeQueryAsync(
                                                                rocprofvis_db_compute_use_case_enum_t use_case,
                                                                rocprofvis_dm_charptr_t query,
                                                                rocprofvis_db_future_t object,
                                                                rocprofvis_dm_table_id_t* id);
       // method to build a query for compute use case 
       // @param use_case - use case enumeration
       // @param num - number of parameters
       // @param params -parameters array 
       // @param query - reference to query string   
       // @return status of operation  
       virtual rocprofvis_dm_result_t BuildComputeQuery(
                                                               rocprofvis_db_compute_use_case_enum_t use_case, 
                                                               rocprofvis_db_num_of_params_t num, 
                                                               rocprofvis_db_compute_params_t params,
                                                               rocprofvis_dm_string_t& query) = 0;

       virtual rocprofvis_dm_result_t BuildTableQuery(
                                                                rocprofvis_dm_timestamp_t start, 
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_db_num_of_tracks_t num, 
                                                                rocprofvis_db_track_selection_t tracks,
                                                                rocprofvis_dm_charptr_t where,
                                                                rocprofvis_dm_charptr_t filter,
                                                                rocprofvis_dm_charptr_t group,
                                                                rocprofvis_dm_charptr_t group_cols, 
                                                                rocprofvis_dm_charptr_t sort_column, 
                                                                rocprofvis_dm_sort_order_t sort_order,
                                                                rocprofvis_dm_num_string_table_filters_t num_string_table_filters, 
                                                                rocprofvis_dm_string_table_filters_t string_table_filters,
                                                                uint64_t max_count, 
                                                                uint64_t offset,
                                                                bool count_only,
                                                                bool summary,
                                                                rocprofvis_dm_string_t& query) = 0;


        // Asynchronously writes the results of a table query to .CSV
        // @param query - database query 
        // @param file_path - .CSV output path
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
       rocprofvis_dm_result_t ExportTableCSVAsync(rocprofvis_dm_string_t query,
                                                  rocprofvis_dm_string_t file_path,
                                                  rocprofvis_db_future_t object);

       virtual rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t start,
                                                      rocprofvis_dm_timestamp_t end,
                                                      rocprofvis_dm_charptr_t new_db_path,
                                                      Future* future) = 0;

       rocprofvis_dm_result_t SaveTrimmedDataAsync(rocprofvis_dm_timestamp_t start,
                                                   rocprofvis_dm_timestamp_t end,
                                                   rocprofvis_dm_string_t new_db_path, 
                                                   rocprofvis_db_future_t object);

       static rocprofvis_dm_result_t SaveTrimmedDataStatic(Database* db, 
                                                    rocprofvis_dm_timestamp_t start,
                                                    rocprofvis_dm_timestamp_t end, 
                                                    rocprofvis_dm_string_t new_db_path,
                                                    Future* object);
       virtual void InterruptQuery(void* connection) {};

       // returns pointer to cached tables map array
       DatabaseCache*                  CachedTables(uint32_t node_id) {return &m_cached_tables[node_id];}

       // returns pointer to track properties structure. Takes index of track as a parameter 
       rocprofvis_dm_track_params_t*   TrackPropertiesAt(rocprofvis_dm_index_t index) { return m_track_properties[index].get(); }

       bool IsTrackIndexValid(rocprofvis_dm_index_t index) { return index < m_track_properties.size(); }


       static rocprofvis_dm_table_t GetInfoTableHandle(const rocprofvis_dm_database_t object, rocprofvis_dm_node_id_t node, rocprofvis_dm_charptr_t table_name);
       static size_t GetInfoTableNumColumns(rocprofvis_dm_table_t object);
       static size_t GetInfoTableNumRows(rocprofvis_dm_table_t object);
       static const char* GetInfoTableColumnName(rocprofvis_dm_table_t object, size_t column_index);
       static rocprofvis_dm_table_row_t GetInfoTableRowHandle(rocprofvis_dm_table_t object, size_t row_index);
       static const char* GetInfoTableRowCellValue(rocprofvis_dm_table_row_t object, size_t column_index);
       static const size_t GetInfoTableRowNumCells(rocprofvis_dm_table_row_t object);

    private:
    /************************static methods to be used as a parameter to std::thread**********************/

        //static method to read metadata. Required to launch a unique thread for asynchronous metadata read 
        // @param db - pointer to database object 
        // @param object - future object providing asynchronous execution mechanism   
        static rocprofvis_dm_result_t   ReadTraceMetadataStatic(
                                                                Database* db, 
                                                                Future* object);
        //static method to read time slice. Required to launch a unique thread for asynchronous time slice read
        // @param db - pointer to database object 
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param num - number of tracks
        // @param tracks - uint32_t array with track IDs  
        // @param object - future object providing asynchronous execution mechanism   
        // @return status of operation
        static rocprofvis_dm_result_t   ReadTraceSliceStatic(
                                                                Database* db,
                                                                rocprofvis_dm_timestamp_t start,
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_db_num_of_tracks_t num,
                                                                rocprofvis_db_track_selection_t tracks,
                                                                Future* object);
        //static method to read Event properties. Required to launch a unique thread for asynchronous event properties read
        // @param db - pointer to database object
        // @param type - event property type (flowtrace, stacktrace, extdata) 
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        static rocprofvis_dm_result_t   ReadEventPropertyStatic(
                                                                Database* db, 
                                                                rocprofvis_dm_event_property_type_t type,
                                                                rocprofvis_dm_event_id_t event_id,
                                                                Future* object);
        //static method to launch any query. Required to launch a unique thread for asynchronous database query
        // @param db - pointer to database object
        // @param query - database query 
        // @param description - database description
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        static rocprofvis_dm_result_t   ExecuteQueryStatic(
                                                                Database* db,
                                                                rocprofvis_dm_charptr_t query,
                                                                rocprofvis_dm_charptr_t description,
                                                                Future* object);
        //static method to launch compute query. 
        // @param db - pointer to database object
        // @param query - database query 
        // @param description - database description
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        static rocprofvis_dm_result_t   ExecuteComputeQueryStatic(
                                                                Database* db,
                                                                rocprofvis_db_compute_use_case_enum_t use_case,
                                                                rocprofvis_dm_charptr_t query,
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

        // static method to export the results of a table query to .CSV
        // @param db - pointer to database object
        // @param query - database query
        // @param file_path - .CSV output path
        // @param future - future object providing asynchronous execution mechanism 
        // @return status of operation 
        static rocprofvis_dm_result_t   ExportTableCSVStatic(  Database* db,
                                                               rocprofvis_dm_string_t query,
                                                               rocprofvis_dm_string_t file_path,
                                                               Future* future);

    /************************pure virtual worker methods to be implemented in derived classes**********************/

        // worker method to read trace metadata 
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        virtual rocprofvis_dm_result_t  ReadTraceMetadata(
                                                                Future* object) = 0;
        // worker method to read time slice
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param num - number of tracks
        // @param tracks - uint32_t array with track IDs  
        // @param object - future object providing asynchronous execution mechanism   
        // @return status of operation
        virtual rocprofvis_dm_result_t  ReadTraceSlice(
                                                                rocprofvis_dm_timestamp_t start,
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_db_num_of_tracks_t num,
                                                                rocprofvis_db_track_selection_t tracks,
                                                                Future* object) = 0;
        // worker method to read flow trace info, called from ReadEventPropertyStatic
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        virtual rocprofvis_dm_result_t  ReadFlowTraceInfo(
                                                                rocprofvis_dm_event_id_t event_id,
                                                                Future* object) = 0;
        // worker method to read stack trace info, called from ReadEventPropertyStatic
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        virtual rocprofvis_dm_result_t  ReadStackTraceInfo(
                                                                rocprofvis_dm_event_id_t event_id,
                                                                Future* object) = 0;
        // worker method to read extended info, called from ReadEventPropertyStatic
        // @param event_id - 60-bit event id and 4-bit operation type  
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        virtual rocprofvis_dm_result_t  ReadExtEventInfo(
                                                                rocprofvis_dm_event_id_t event_id,
                                                                Future* object) = 0;
        // worker method to execute any database query
        // @param query - database query 
        // @param description - database description
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        virtual rocprofvis_dm_result_t  ExecuteQuery(
                                                                rocprofvis_dm_charptr_t query,
                                                                rocprofvis_dm_charptr_t description,
                                                                Future* object) = 0;
        // worker method to execute compute database query
        // @param query - database query 
        // @param description - database description
        // @param object - future object providing asynchronous execution mechanism 
        // @return status of operation
        virtual rocprofvis_dm_result_t  ExecuteComputeQuery(
                                                                rocprofvis_db_compute_use_case_enum_t use_case,
                                                                rocprofvis_dm_charptr_t query,
                                                                Future* future) = 0;
        // method to build a query to read time slice of records for single track 
        // @param index - track index 
        // @param type - query type
        // @param query - reference to output query string  
        // @return status of operation
        virtual rocprofvis_dm_result_t  BuildTrackQuery(           
                                                                rocprofvis_dm_index_t index, 
                                                                rocprofvis_dm_index_t type,
                                                                rocprofvis_dm_string_t & query,
                                                                uint32_t split_count,
                                                                uint32_t split_index) = 0;

        // method to build a query to read time slice of records for all tracks in one shot 
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param num - number of tracks
        // @param tracks - uint32_t array with track IDs 
        // @param query - reference to query string 
        // @param slices - reference map array for storing slice handlers for multi-track request   
        // @return status of operation                                                      
        virtual rocprofvis_dm_result_t  BuildSliceQuery(      
                                                                rocprofvis_dm_timestamp_t start, 
                                                                rocprofvis_dm_timestamp_t end, 
                                                                rocprofvis_db_num_of_tracks_t num, 
                                                                rocprofvis_db_track_selection_t tracks, 
                                                                rocprofvis_dm_string_t& query, 
                                                                slice_array_t& slices) = 0;

        // method to export the results of a table query to .CSV
        // @param query - database query
        // @param file_path - .CSV output path
        // @return status of operation 
        virtual rocprofvis_dm_result_t ExportTableCSV(          rocprofvis_dm_charptr_t query,
                                                                rocprofvis_dm_charptr_t file_path,
                                                                Future* future);
    private:
        // pointer to a binding information structure physically located in Trace object and passed to Database object during binding
        // binding structure contains methods to transfer data between database and trace objects 
        rocprofvis_dm_db_bind_struct *m_binding_info;
        // database file path
        std::string m_path;
        // vector array of track parameters. Used as a reference for data model Track objects and for Database component to generate proper database queries 
        std::vector<std::unique_ptr<rocprofvis_dm_track_params_t>> m_track_properties;
        // map array of cached tables, mostly with non-essential Track information
        std::unordered_map<uint32_t, DatabaseCache> m_cached_tables;
        guid_list_t   m_db_instances;

    protected:
        guid_list_t& DbInstances() { return m_db_instances; }
        uint32_t NumDbInstances() { return m_db_instances.size(); }
        std::string GuidAt(int index) { return index < m_db_instances.size() ? m_db_instances[index].second : std::string(); }
        std::string GuidSymAt(int index) { std::string s = GuidAt(index); std::replace(s.begin(), s.end(), '_', '-'); return s; }
        DbInstance* DbInstancePtrAt(int index) { return index < m_db_instances.size() ? &m_db_instances[index].first : nullptr; }
        // returns pointer to database file path
        rocprofvis_db_filename_t        Path() {return m_path.c_str();}
        // return current number of tracks
        rocprofvis_dm_size_t            NumTracks() { return m_track_properties.size(); }
        // returns pointer to last registered Track properties structure
        rocprofvis_dm_track_params_t*   TrackPropertiesLast() { return m_track_properties.back().get(); }
        // returns track properties begin iterator
        rocprofvis_dm_track_params_it   TrackPropertiesBegin() { return m_track_properties.begin(); }
        // returns track properties end iterator
        rocprofvis_dm_track_params_it   TrackPropertiesEnd() { return m_track_properties.end(); }
        // returns pointer to trace properties, which contains shared trace information
        rocprofvis_dm_trace_params_t*   TraceProperties() { return m_binding_info->trace_properties; }
        // register new track
        // @param props - track properties structure
        // @return status of operation
        rocprofvis_dm_result_t          AddTrackProperties(
                                                                rocprofvis_dm_track_params_t& props);
        // finds and return iterator to track properties array
        // @process track process identifiers structure
        rocprofvis_dm_track_params_it   FindTrack( rocprofvis_dm_process_identifiers_t& process, DbInstance* db_instance);
        // adds a new query to the track queries collection 
        // multiple queries for single track are required to support data from multiple database tables on single track,
        // like Kernel Dispatch, Memory Copy and Memory Allocation
        // @param it - track properties array iterator
        // @param newprops - new track properties structure
        // @param newquery - new track records query. One track can have multiple queries.
        void                            UpdateQueryForTrack(rocprofvis_dm_track_params_it it, 
                                                            rocprofvis_dm_track_params_t& newprops,
                                                            rocprofvis_dm_charptr_t*      newqueries);
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
        // remap string IDs in new event record structure
        // @param record - event data record
        // @return status of operation
        virtual rocprofvis_dm_result_t  RemapStringIds(
                                                                rocprofvis_db_record_data_t & record) {return kRocProfVisDmResultSuccess;};
        virtual rocprofvis_dm_result_t RemapStringIds(
                                                                rocprofvis_db_flow_data_t& record) {return kRocProfVisDmResultSuccess;};
        virtual rocprofvis_dm_result_t  StringIndexToId(        
                                                                rocprofvis_dm_index_t index, std::vector<rocprofvis_db_string_id_t>& id) {return kRocProfVisDmResultSuccess;};

        // return suffix to process name for provided track category ('PID', 'Agent')
        // @param category - track category
        // @return track process name suffix  ('PID', 'Agent')      
        static const char*              ProcessNameSuffixFor(   
                                                                rocprofvis_dm_track_category_t category);
        // return suffix to sub-process name for provided track category ('TID', 'Queue')
        // @param category - track category
        // @return track sub-process name suffix  ('TID', 'Queue')  
        static const char*              SubProcessNameSuffixFor(
                                                                rocprofvis_dm_track_category_t category);
        // check if string is number
        // @param s - string to check
        // @return True if number
        static bool IsNumber(const std::string& s);

    public:
        // declare DatabaseCache as friend class, for having access to protected members
        friend class DatabaseCache;
        friend class TableProcessor;
};

}  // namespace DataModel
}  // namespace RocProfVis