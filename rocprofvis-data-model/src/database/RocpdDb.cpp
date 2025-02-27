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

#include "RocpdDb.h"
#include <sstream>


rocprofvis_dm_result_t RocpdDatabase::UpdateStringMap(StringPair ids) {
    try {
        m_string_map.insert(ids);
    } 
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE,kRocProfVisDmResultAllocFailure);
    }
    return kRocProfVisDmResultSuccess;
}

const bool RocpdDatabase::ReIndexStringId(const uint64_t dbStringId, uint32_t& stringId)
{
    StringMap::const_iterator pos = m_string_map.find(dbStringId);
    if (pos == m_string_map.end()) return false;
    stringId = pos->second;
    return true;
}

int RocpdDatabase::CallbackGetMinTime(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Stopped()) return 1;
    db->TraceParameters()->start_time = std::stoll( argv[0] ); 
    return 0;
}

int RocpdDatabase::CallbackGetMaxTime(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Stopped()) return 1;
    db->TraceParameters()->end_time = std::stoll( argv[0] );       
    return 0;
}

int RocpdDatabase::CallBackAddTrack(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_dm_track_params_t track_params;
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Stopped()) return 1;
    track_params.track_id= db->TrackPropertiesSize();
    track_params.process = argv[0];
    track_params.name = argv[1]; 
    track_params.track_category = callback_params->track_category;
    track_params.node_id=0;       
    if (kRocProfVisDmResultSuccess != db->AddTrackProperties(track_params)) return 1;
    return db->BindObject()->FuncAddTrack(db->BindObject()->trace_object, db->TrackPropertiesLast()) != kRocProfVisDmResultSuccess ? 1 : 0;   
}

int RocpdDatabase::CallBackAddString(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Stopped()) return 1;
    std::string str = argv[0]; 
    std::stringstream ids(argv[1]);
    uint32_t stringId = db->BindObject()->FuncAddString(db->BindObject()->trace_object, argv[0]);
    if (stringId == INVALID_INDEX) return 1;
    
    for (uint64_t id; ids >> id;) {   
        if (kRocProfVisDmResultSuccess!=db->UpdateStringMap(StringPair(id,stringId))) return 1;
        if (ids.peek() == ',') ids.ignore();
    }
    return 0;
}

int RocpdDatabase::CallbackAddEventRecord(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==5, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    rocprofvis_db_record_data_t record;
    record.event.op = callback_params->event_op;
    record.event.id = std::stoll(argv[4]);
    record.event.timestamp = std::stoll( argv[0] );
    record.event.duration = std::stoll( argv[1] );
    if (db->ReIndexStringId(std::stoll( argv[2]), record.event.category ) &&
        db->ReIndexStringId(std::stoll( argv[3] ), record.event.symbol ))
    {
        if (db->BindObject()->FuncAddRecord(callback_params->slice,record) != kRocProfVisDmResultSuccess) return 1;  
    } 
    return 0;
}

int RocpdDatabase::CallbackAddPmcRecord(void *data, int argc, char **argv, char **azColName){
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    rocprofvis_db_record_data_t record;
    ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    record.pmc.timestamp = std::stoll( argv[0] );
    record.pmc.value = std::stod( argv[1] );
    if (db->BindObject()->FuncAddRecord(callback_params->slice,record) != kRocProfVisDmResultSuccess) return 1;
    return 0;
}

rocprofvis_dm_result_t  RocpdDatabase::ReadTraceMetadata(Future* future)
{
    ASSERT_MSG_RETURN(future, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ResetProgress();
    while (true)
    {
        ASSERT_MSG_BREAK(BindObject()->trace_parameters, ERROR_TRACE_PARAMETERS_CANNOT_BE_NULL);
        TraceParameters()->strings_offset=0;
        TraceParameters()->symbols_offset=0;

        ShowProgress(10, "Create CPU tracks indexes", kRPVDbBusy, ProgressCallback() );
        ExecuteSQLQuery(future,"CREATE INDEX pid_tid_idx ON rocpd_api(pid,tid);");

        ShowProgress(10, "Create GPU tracks indexes", kRPVDbBusy, ProgressCallback() );
        ExecuteSQLQuery(future,"CREATE INDEX gid_qid_idx ON rocpd_op(gpuId, queueId);");
        
        ShowProgress(10, "Create PMC tracks indexes", kRPVDbBusy, ProgressCallback() );
        ExecuteSQLQuery(future,"CREATE INDEX monitorTypeIdx on rocpd_monitor(monitorType);");

        ShowProgress(1, "Getting minimum timestamp", kRPVDbBusy, ProgressCallback() );
        if (!ExecuteSQLQuery(future,"SELECT MIN(start) FROM rocpd_api;", &CallbackGetMinTime)) break;

        ShowProgress(1, "Get maximum timestamp", kRPVDbBusy, ProgressCallback() );
        if (!ExecuteSQLQuery(future, "select MAX(start) from rocpd_op;", &CallbackGetMaxTime)) break;

        ShowProgress(5, "Adding CPU tracks", kRPVDbBusy, ProgressCallback() );
        if (!ExecuteSQLQuery(future, "select DISTINCT pid, tid from rocpd_api;", &CallBackAddTrack, kRocProfVisDmRegionTrack)) break;

        ShowProgress(5, "Adding GPU tracks", kRPVDbBusy, ProgressCallback() );
        if (!ExecuteSQLQuery(future, "select DISTINCT gpuId, queueId from rocpd_op;", &CallBackAddTrack, kRocProfVisDmKernelTrack)) break;

        ShowProgress(5, "Adding PMC tracks", kRPVDbBusy, ProgressCallback() );
        if (!ExecuteSQLQuery(future, "select DISCTINCT deviceId, monitorType from rocpd_monitor where deviceId > 0;", &CallBackAddTrack, kRocProfVisDmPmcTrack)) break;

        ShowProgress(20, "Loading strings", kRPVDbBusy, ProgressCallback() );
        if (!ExecuteSQLQuery(future, "SELECT string, GROUP_CONCAT(id) AS ids FROM rocpd_string GROUP BY string;", &CallBackAddString)) break;

        TraceParameters()->metadata_loaded=true;
        ShowProgress(100-Progress(), "Trace metadata successfully loaded", kRPVDbSuccess, ProgressCallback() );
        return future->SetPromise(kRocProfVisDmResultSuccess);

    }
    ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, ProgressCallback() );
    return future->SetPromise(future->Stopped() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
}
rocprofvis_dm_result_t  RocpdDatabase::ReadTraceSlice( 
                                                    rocprofvis_dm_timestamp_t start,
                                                    rocprofvis_dm_timestamp_t end,
                                                    rocprofvis_db_num_of_tracks_t num,
                                                    rocprofvis_db_track_selection_t tracks,
                                                    Future* future) {
    ASSERT_MSG_RETURN(future, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ASSERT_MSG_BREAK(BindObject()->trace_parameters, ERROR_TRACE_PARAMETERS_CANNOT_BE_NULL);
        ASSERT_MSG_BREAK(BindObject()->trace_parameters->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
        ResetProgress();
        double step = 100.0/num;
        for (int i = 0; i < num; i++)
        {
            ASSERT_MSG_BREAK(tracks[i] < TrackPropertiesSize(), ERROR_INDEX_OUT_OF_RANGE);
            rocprofvis_dm_slice_t slice = BindObject()->FuncAddSlice(BindObject()->trace_object,tracks[i],start,end);
            ASSERT_MSG_BREAK(slice, ERROR_SLICE_CANNOT_BE_NULL);
            static std::stringstream query;
            if (TrackPropertiesAt(tracks[i])->track_category == kRocProfVisDmRegionTrack)
            {
                query << "select start, (end-start), apiName_id, args_id, id from rocpd_api where pid == " << TrackPropertiesAt(tracks[i])->process
                    << " and tid == " << TrackPropertiesAt(tracks[i])->name
                    << " and start >= " << start
                    << " and end < " << end
                    << " ORDER BY start;";
                    ShowProgress(step, query.str().c_str(),kRPVDbBusy, ProgressCallback());
                    if (!ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddEventRecord, kRocProfVisDmRegionTrack, slice, kRocProfVisDmOperationLaunch )) break;
            } else
            if (TrackPropertiesAt(tracks[i])->track_category == kRocProfVisDmKernelTrack)
            {
                query << "select start, (end-start), opType_id, description_id, id from rocpd_op where gpuId == " << TrackPropertiesAt(tracks[i])->process
                    << " and queueId == " << TrackPropertiesAt(tracks[i])->name
                    << " and start >= " << start
                    << " and end < " << end
                    << " ORDER BY start;";
                    ShowProgress(step, query.str().c_str(),kRPVDbBusy, ProgressCallback());
                    if (!ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddEventRecord, kRocProfVisDmKernelTrack, slice, kRocProfVisDmOperationDispatch )) break;
            } else     
            if (TrackPropertiesAt(tracks[i])->track_category == kRocProfVisDmPmcTrack)
            {
                query << "select start, value from rocpd_monitor where deviceId == " << TrackPropertiesAt(tracks[i])->process
                    << " and monitorType == \"" << TrackPropertiesAt(tracks[i])->name << "\"" 
                    << " and start >= " << start
                    << " and end < " << end
                    << " ORDER BY start;";
                    ShowProgress(step, query.str().c_str(),kRPVDbBusy, ProgressCallback());
                    if (!ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddPmcRecord, kRocProfVisDmPmcTrack, slice )) break;
            }            
        }
    }
}

rocprofvis_dm_result_t  RocpdDatabase::ReadFlowTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* object)
{
    ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY,kRocProfVisDmResultNotSupported);   
}
rocprofvis_dm_result_t  RocpdDatabase::ReadStackTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* object)
{
    ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY,kRocProfVisDmResultNotSupported); 
}
