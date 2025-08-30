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

#include "rocprofvis_db_profile.h"

#include <unordered_map>

namespace RocProfVis
{
namespace DataModel
{

// class for reading old schema Rocpd database
class RocpdDatabase : public ProfileDatabase
{
    // type of map array for string indexes remapping
    typedef std::unordered_map<uint64_t, uint32_t> string_index_map_t;

    // map array for fast non-PMC track ID search
    typedef std::map<uint32_t, uint32_t> sub_process_map_t;
    typedef std::map<uint32_t, sub_process_map_t> track_find_map_t;

    // map array for fast PMC track ID search
    typedef std::map<std::string, uint32_t> sub_process_map_pmc_t;
    typedef std::map<uint32_t, sub_process_map_pmc_t> track_find_pmc_map_t;
    
public:
    // class constructor
    // @param path - database file path
    RocpdDatabase(rocprofvis_db_filename_t path) :
        ProfileDatabase(path) {}
    // class destructor, not really required, unless declared as virtual
    ~RocpdDatabase()override{};
    // worker method to read trace metadata
    // @param object - future object providing asynchronous execution mechanism 
    // @return status of operation
    rocprofvis_dm_result_t  ReadTraceMetadata(
                                                Future* object) override;
    // worker method to read flow trace info
    // @param event_id - 60-bit event id and 4-bit operation type  
    // @param object - future object providing asynchronous execution mechanism 
    // @return status of operation
    rocprofvis_dm_result_t  ReadFlowTraceInfo(
                                                rocprofvis_dm_event_id_t event_id,
                                                Future* object) override;
    // worker method to read stack trace info
    // @param event_id - 60-bit event id and 4-bit operation type  
    // @param object - future object providing asynchronous execution mechanism 
    // @return status of operation
    rocprofvis_dm_result_t  ReadStackTraceInfo(
                                                rocprofvis_dm_event_id_t event_id,
                                                Future* object) override;
    // worker method to read extended info
    // @param event_id - 60-bit event id and 4-bit operation type  
    // @param object - future object providing asynchronous execution mechanism 
    // @return status of operation
    rocprofvis_dm_result_t  ReadExtEventInfo(
        rocprofvis_dm_event_id_t event_id, 
        Future* object) override;
    
    // get class memory usage
    // @return size of memory used by the class
    rocprofvis_dm_size_t GetMemoryFootprint(void) override;

    rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t start,
                                           rocprofvis_dm_timestamp_t end,
                                           rocprofvis_dm_charptr_t new_db_path,
                                           Future* future) override;
    
private:
    // sqlite3_exec callback to process track information query and add track object to Trace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallBackAddTrack(void* data, int argc, sqlite3_stmt* stmt, char** azColName);
    // sqlite3_exec callback to process string list query and add string object to Trace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallBackAddString(void *data, int argc, sqlite3_stmt* stmt, char **azColName);
    // sqlite3_exec callback to process stack trace information query and add stack trace object to StackTrace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallbackAddStackTrace(void *data, int argc, sqlite3_stmt* stmt, char **azColName);

    // map array for string indexes remapping. Main reason for remapping is older rocpd schema keeps duplicated symbols, one per GPU 
    string_index_map_t m_string_map;

    // method to remap string IDs. Main reason for remapping is older rocpd schema keeps duplicated symbols, one per GPU 
    // @param record - event record structure
    // @return status of operation
    rocprofvis_dm_result_t RemapStringIds(rocprofvis_db_record_data_t & record) override;
    rocprofvis_dm_result_t RemapStringIds(rocprofvis_db_flow_data_t& record) override;

    // finds and returns track id by 3 input parameters  (Node, Agent/PID, QueueId/PmcId/Metric name) 
    // @param node_id - node id
    // @param process_id - process id 
    // @param sub_process_name - metric name
    // @param operation - operation of event that requesting track id
    // @return status of operation
    rocprofvis_dm_result_t          FindTrackId(
                                                        uint64_t node,
                                                        uint32_t process,
                                                        const char* subprocess,
                                                        rocprofvis_dm_op_t operation,
                                                        rocprofvis_dm_track_id_t& track_id) override;

    // method to remap single string ID. Main reason for remapping is older rocpd schema keeps duplicated symbols, one per GPU 
    // @param id - string id to be remapped
    // @return True if remapped
    const bool RemapStringId(uint64_t & id);

    const rocprofvis_event_data_category_map_t* GetCategoryEnumMap() override
    {
        return &s_rocpd_categorized_data;
    };
    const rocprofvis_null_data_exceptions_int* GetNullDataExceptionsInt() override
    {
        return &s_null_data_exception_int;
    }
    const rocprofvis_null_data_exceptions_string* GetNullDataExceptionsString() override
    {
        return &s_null_data_exception_string;
    }
    const rocprofvis_null_data_exceptions_skip* GetNullDataExceptionsSkip() override
    {
        return &s_null_data_exceptions_skip;
    }
    private:
        rocprofvis_dm_result_t CreateIndexes();

    private:
        track_find_map_t find_track_map;
        track_find_pmc_map_t find_track_pmc_map;

        inline static const rocprofvis_event_data_category_map_t
            s_rocpd_categorized_data = {
                {
                    kRocProfVisDmOperationNoOp,
                    {
                        { "id", kRocProfVisEventEssentialDataId },
                        { "start", kRocProfVisEventEssentialDataStart },
                        { "end", kRocProfVisEventEssentialDataEnd },
                        { "duration", kRocProfVisEventEssentialDataDuration },
                    },
                },
                {
                    kRocProfVisDmOperationLaunch,
                    {
                        { "apiName", kRocProfVisEventEssentialDataCategory },
                        { "args", kRocProfVisEventEssentialDataName },
                        { "pid", kRocProfVisEventEssentialDataProcess },
                        { "tid", kRocProfVisEventEssentialDataThread },
                    },
                },
                {
                    kRocProfVisDmOperationDispatch,
                    {
                        { "opType", kRocProfVisEventEssentialDataCategory },
                        { "description", kRocProfVisEventEssentialDataName },
                        { "agent_type", kRocProfVisEventEssentialDataAgentType },
                        { "gpuId", kRocProfVisEventEssentialDataAgentIndex },
                        { "queueId", kRocProfVisEventEssentialDataQueue },
                        { "stream", kRocProfVisEventEssentialDataStream },
                    }
                }
            };

         inline static const rocprofvis_null_data_exceptions_skip
            s_null_data_exceptions_skip = {

            };

        inline static const rocprofvis_null_data_exceptions_int
            s_null_data_exception_int = {

            };
        inline static const rocprofvis_null_data_exceptions_string
            s_null_data_exception_string = {

            };

};

}  // namespace DataModel
}  // namespace RocProfVis