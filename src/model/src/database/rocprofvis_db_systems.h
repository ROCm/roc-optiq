// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_future.h"
#include "rocprofvis_db_cache.h"
#include "rocprofvis_db_track.h"
#include "rocprofvis_db_version.h"
#include "rocprofvis_db.h"
#include "rocprofvis_db_packed_storage.h"
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

typedef std::map<uint64_t, std::map<std::string, rocprofvis_event_data_category_enum_t>> rocprofvis_event_data_category_map_t;
// type of map array for storing slice handlers for multi-track request
typedef std::unordered_map<uint32_t, rocprofvis_dm_slice_t> slice_array_t;

class Database;

// Helper class to lock processes in order of database instances
class OrderedMutex {
public:
    void init(uint32_t num_instances) { for (uint32_t i = 0; i < num_instances; i++) { m_instances.insert(i); } }

    void lock(uint32_t id) {
        std::unique_lock<std::mutex> lock(m_lock);
        m_cv.wait(lock, [&] { return id == *m_instances.begin(); }); 
    }

    void unlock(uint32_t id) {
        {
            std::lock_guard<std::mutex> lock(m_lock);
            m_instances.erase(id);
        }
        m_cv.notify_all();
    }


private:
    std::set<uint32_t> m_instances;
    std::mutex m_lock;
    std::condition_variable m_cv;
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

typedef struct rocprofvis_db_string_id_hash_t
{
    size_t operator()(const rocprofvis_db_string_id_t& s) const noexcept
    {
        size_t h1 = std::hash<uint64_t>{}(s.m_string_id);
        size_t h2 = std::hash<uint32_t>{}(s.m_guid_id);
        size_t h3 = std::hash<rocprofvis_db_string_type_t>{}(s.m_string_type);

        size_t seed = h1;
        seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
} rocprofvis_db_string_id_hash_t;

class SystemDatabase : public Database
{
    public:
        // Database constructor
        // @param path - full path to database file
        SystemDatabase(   
                    rocprofvis_db_filename_t path):
                    Database(path),
                    m_track_lookup(this) {
        };
        // Database destructor, must be defined as virtual to free resources of derived classes 
        virtual ~SystemDatabase(){};

        // Get amount of memory used by database resource
        // @return memory size
        virtual rocprofvis_dm_size_t    GetMemoryFootprint(void) override; 

        // Method to cleanup database
        // @return status of operation
        rocprofvis_dm_result_t  CleanupAsync(rocprofvis_db_future_t object, bool rebuild);
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
                                                                rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                                rocprofvis_db_num_of_tracks_t num,
                                                                rocprofvis_db_track_selection_t tracks,
                                                                rocprofvis_db_future_t object);

        // Asynchronously read a PMC time slice (records from specified track for specified time frame) from database  
        // @param start - start timestamp of time slice 
        // @param end - end timestamp of time slice 
        // @param track - track ID  
        // @param left_neighbor - include the left neighbor of the time range
        // @param right_neighbor - include the right neighbor of the time range
        // @param object - future object providing asynchronous execution mechanism         
        // @return status of operation
        rocprofvis_dm_result_t          ReadTracePMCSliceAsync( 
                                                                rocprofvis_dm_timestamp_t start,
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                                rocprofvis_db_track_selection_t track,
                                                                bool left_neighbor,
                                                                bool right_neighbor,
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

       virtual rocprofvis_dm_result_t BuildTableQuery(
                                                                rocprofvis_dm_table_use_case_enum_t use_case,
                                                                rocprofvis_dm_timestamp_t start, 
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_db_num_of_tracks_t num, 
                                                                rocprofvis_db_track_selection_t tracks,
                                                                rocprofvis_dm_processor_identifiers_ptr processor,
                                                                rocprofvis_dm_charptr_t filter,
                                                                rocprofvis_dm_charptr_t group,
                                                                rocprofvis_dm_charptr_t group_cols, 
                                                                rocprofvis_dm_charptr_t sort_column, 
                                                                rocprofvis_dm_sort_order_t sort_order,
                                                                uint64_t max_count, 
                                                                uint64_t offset,
                                                                bool count_only,
                                                                rocprofvis_dm_string_t& query) = 0;

       virtual rocprofvis_dm_result_t BuildEventSearchQuery(    
                                                                rocprofvis_dm_timestamp_t start, 
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_db_num_of_tracks_t num, 
                                                                rocprofvis_db_track_selection_t ops,
                                                                rocprofvis_dm_processor_identifiers_ptr processor,
                                                                rocprofvis_dm_num_string_table_filters_t num_string_table_filters,
                                                                rocprofvis_dm_string_table_filters_t string_table_filters,
                                                                bool include_substring,
                                                                bool include_category,
                                                                bool partial_matching,
                                                                rocprofvis_dm_charptr_t sort_column,
                                                                rocprofvis_dm_sort_order_t sort_order,
                                                                uint64_t max_count,
                                                                uint64_t offset,
                                                                bool count_only,
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


       virtual rocprofvis_dm_result_t GetEventTablesAsync(
           std::vector<std::pair<DbInstance*, std::string>>& queries,
           Future* parent,
           rocprofvis_dm_handle_t handle,
           RpvCallback callback) {
           return kRocProfVisDmResultNotSupported;
       };



    private:
    /************************static methods to be used as a parameter to std::thread**********************/

        //static method to read metadata. Required to launch a unique thread for asynchronous metadata read 
        // @param db - pointer to database object 
        // @param object - future object providing asynchronous execution mechanism   
        static rocprofvis_dm_result_t   ReadTraceMetadataStatic(
                                                                Database* db, 
                                                                Future* object);




        static rocprofvis_dm_result_t  CleanupStatic(Database* db, Future* future, bool rebuild);

    /************************pure virtual worker methods to be implemented in derived classes**********************/

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
                                                                rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                                rocprofvis_db_num_of_tracks_t num,
                                                                rocprofvis_db_track_selection_t tracks,
                                                                Future* object) = 0;

        virtual rocprofvis_dm_result_t  ReadTracePMCSlice(
                                                                rocprofvis_dm_timestamp_t start,
                                                                rocprofvis_dm_timestamp_t end,
                                                                rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                                rocprofvis_db_track_selection_t track,
                                                                bool left_neighbor,
                                                                bool right_neighbor,
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

        // method to export the results of a table query to .CSV
        // @param query - database query
        // @param file_path - .CSV output path
        // @return status of operation 
        virtual rocprofvis_dm_result_t ExportTableCSV(          rocprofvis_dm_charptr_t query,
                                                                rocprofvis_dm_charptr_t file_path,
                                                                Future* future) = 0;

        virtual rocprofvis_dm_result_t  Cleanup(Future* future, bool rebuild) { (void) future; (void) rebuild; return kRocProfVisDmResultSuccess; };

    private:
        // vector array of track parameters. Used as a reference for data model Track objects and for Database component to generate proper database queries 
        std::vector<std::unique_ptr<rocprofvis_dm_track_params_t>> m_track_properties;
        TrackLookup   m_track_lookup;
        StringTable   m_string_table;


    protected:
        // ---------------------------------------------Getters---------------------------------------
        // returns pointer to last registered Track properties structure
        rocprofvis_dm_track_params_t*   TrackPropertiesLast() { return m_track_properties.back().get(); }
        // returns track properties begin iterator
        rocprofvis_dm_track_params_it   TrackPropertiesBegin() { return m_track_properties.begin(); }
        // returns track properties end iterator
        rocprofvis_dm_track_params_it   TrackPropertiesEnd() { return m_track_properties.end(); }

        TrackLookup*                    TrackTracker() { return& m_track_lookup; }
        // return current number of tracks
        rocprofvis_dm_size_t            NumTracks() { return m_track_properties.size(); }
        // returns pointer to track properties structure. Takes index of track as a parameter 
        rocprofvis_dm_track_params_t*   TrackPropertiesAt(rocprofvis_dm_index_t index) { return m_track_properties[index].get(); }
        // validated track index
        bool                            IsTrackIndexValid(rocprofvis_dm_index_t index) { return index < m_track_properties.size(); }
        StringTable&                    StringTableReference() { return m_string_table; };

        // ---------------------------------------------Helpers---------------------------------------
        // register new track
        // @param props - track properties structure
        // @return status of operation
        rocprofvis_dm_result_t          AddTrackProperties(
                                                                rocprofvis_dm_track_params_t& props);
        // remap string IDs in new event record structure
        // @param record - event data record
        // @return status of operation
        virtual rocprofvis_dm_result_t  RemapStringIds(
                                                                rocprofvis_db_record_data_t & record) { (void) record; return kRocProfVisDmResultSuccess;};
        virtual rocprofvis_dm_result_t  RemapStringIds(
                                                                rocprofvis_db_flow_data_t & record) { (void) record; return kRocProfVisDmResultSuccess;};
        virtual rocprofvis_dm_result_t  StringIndexToId(        
                                                                rocprofvis_dm_index_t index, std::vector<rocprofvis_db_string_id_t>& id) { (void) index; (void) id; return kRocProfVisDmResultSuccess;};

        virtual rocprofvis_dm_result_t RemapStringId(uint64_t id, rocprofvis_db_string_type_t type, uint32_t node, uint64_t & result) = 0;

        // return suffix to sub-process name for provided track category ('TID', 'Queue')
        // @param category - track category
        // @return track sub-process name suffix  ('TID', 'Queue')  
        static const char*              SubProcessNameSuffixFor(rocprofvis_dm_track_category_t category);
        
        // create tracks ranking so they can be sorted accordingly in UI
        void                            CreateTracksOrderRanking();

        virtual void GetTrackIdentifierIndices(int column_index, char** azColName, rocprofvis_db_track_identifier_index_t& track_ids_indices) = 0;
        virtual bool FindTrack(rocprofvis_dm_track_category_t category, uint64_t id_process, uint64_t id_subprocess, uint32_t db_instance, uint32_t& out_track) = 0;

        //--------------------------------------Table accessors-----------------------------------------------------------------
        virtual std::string TableColumnText(void* func, void* handle, char** azColName, int index) = 0;
        virtual int TableColumnInt(void* func, void* handle, char** azColName, int index) = 0;
        virtual int64_t TableColumnInt64(void* func, void* handle, char** azColName, int index) = 0;
        virtual double TableColumnDouble(void* func, void* handle, char** azColName, int index) = 0;

    public:
        // declare DatabaseCache as friend class, for having access to protected members
        friend class DatabaseCache;
        friend class TableProcessor;
        friend class TrackLookup;
        friend class PackedTable;
        friend class SqliteDatabase;
};

}  // namespace DataModel
}  // namespace RocProfVis
