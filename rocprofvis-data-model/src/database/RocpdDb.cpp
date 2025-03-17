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

#include "RocpdDb.h"
#include <sstream>
#include <string.h>


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

rocprofvis_dm_size_t RocpdDatabase::GetMemoryFootprint()
{
    rocprofvis_dm_size_t size = sizeof(RocpdDatabase);
    size+=m_string_map.size()*sizeof(StringPair);
    size+=NumTrackProperties()*(sizeof(rocprofvis_dm_track_params_t)+sizeof(std::unique_ptr<rocprofvis_dm_track_params_t>));
    size+=strlen(Path());
    return size;
}

int RocpdDatabase::CallbackGetMinTime(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    db->TraceParameters()->start_time = std::stoll( argv[0] ); 
    callback_params->row_counter++;
    return 0;
}

int RocpdDatabase::CallbackGetMaxTime(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==1, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    db->TraceParameters()->end_time = std::stoll( argv[0] );       
    callback_params->row_counter++;
    return 0;
}

int RocpdDatabase::CallBackAddTrack(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_dm_track_params_t track_params = {0};
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    track_params.track_id = (rocprofvis_dm_track_id_t)db->NumTrackProperties();
    track_params.process = argv[0];
    track_params.name = argv[1]; 
    track_params.track_category = callback_params->track_category;
    track_params.node_id=0;   
    if (kRocProfVisDmResultSuccess != db->AddTrackProperties(track_params)) return 1;
    if (db->BindObject()->FuncAddTrack(db->BindObject()->trace_object, db->TrackPropertiesLast()) != kRocProfVisDmResultSuccess) return 1;   
    rocprofvis_db_ext_data_t record;
    record.name = azColName[0];
    record.data = argv[0];
    record.category = "Properties";
    if (db->BindObject()->FuncAddExtDataRecord(db->TrackPropertiesLast()->extdata,record) != kRocProfVisDmResultSuccess) return 1;
    record.name = azColName[1];
    record.data = argv[1];
    if (db->BindObject()->FuncAddExtDataRecord(db->TrackPropertiesLast()->extdata,record) != kRocProfVisDmResultSuccess) return 1;
    callback_params->row_counter++;
    return 0;
}

int RocpdDatabase::CallBackAddString(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    std::string str = argv[0]; 
    std::stringstream ids(argv[1]);
    uint32_t stringId = db->BindObject()->FuncAddString(db->BindObject()->trace_object, argv[0]);
    if (stringId == INVALID_INDEX) return 1;
    
    for (uint64_t id; ids >> id;) {   
        if (kRocProfVisDmResultSuccess!=db->UpdateStringMap(StringPair(id,stringId))) return 1;
        if (ids.peek() == ',') ids.ignore();
    }
    callback_params->row_counter++;
    return 0;
}

int RocpdDatabase::CallbackAddEventRecord(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(argc==5, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    rocprofvis_db_record_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.event.id.bitfiled.event_op = callback_params->event_op;
    record.event.id.bitfiled.event_id = std::stoll(argv[4]);
    record.event.timestamp = std::stoll( argv[0] );
    record.event.duration = std::stoll( argv[1] );
    if (db->ReIndexStringId(std::stoll( argv[2]), record.event.category ) &&
        db->ReIndexStringId(std::stoll( argv[3] ), record.event.symbol ))
    {
        if (db->BindObject()->FuncAddRecord(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;  
    } 
    callback_params->row_counter++;
    return 0;
}

int RocpdDatabase::CallbackAddPmcRecord(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ASSERT_MSG_RETURN(argc==2, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    rocprofvis_db_record_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.pmc.timestamp = std::stoll( argv[0] );
    record.pmc.value = std::stod( argv[1] );
    if (db->BindObject()->FuncAddRecord(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    callback_params->row_counter++;
    return 0;
}

int RocpdDatabase::CallbackGetFlowTrace(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ASSERT_MSG_RETURN(argc==4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    rocprofvis_db_flow_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.id.bitfiled.event_id = std::stoll( argv[0] );
    record.id.bitfiled.event_op = callback_params->event_op;
    record.time = std::stoll( argv[3] );
    if (db->FindTrackIdByName(argv[1], argv[2], record.track_id) == kRocProfVisDmResultSuccess) {
        if (db->BindObject()->FuncAddFlow(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    }
    callback_params->row_counter++;
    return 0;
}

int RocpdDatabase::CallbackGetStackTrace(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    ASSERT_MSG_RETURN(argc==4, ERROR_DATABASE_QUERY_PARAMETERS_MISMATCH, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    rocprofvis_db_stack_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.symbol = argv[0];
    record.args = argv[1];
    record.line = argv[2];
    record.depth = std::stol( argv[3] );
    if (db->BindObject()->FuncAddStackFrame(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    callback_params->row_counter++;
    return 0;
}

int RocpdDatabase::CallbackAddExtInfo(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    rocprofvis_db_ext_data_t record;
    if (callback_params->future->Interrupted()) return 1;
    record.category = callback_params->ext_data_category;
    for (int i = 0; i < argc; i++)
    {
        record.name = azColName[i];
        record.data = argv[i];
    }
    if (db->BindObject()->FuncAddExtDataRecord(callback_params->handle,record) != kRocProfVisDmResultSuccess) return 1;
    callback_params->row_counter++;
    return 0;
}

int RocpdDatabase::CallbackRunQuery(void *data, int argc, char **argv, char **azColName){
    ASSERT_MSG_RETURN(data, ERROR_SQL_QUERY_PARAMETERS_CANNOT_BE_NULL, 1);
    rocprofvis_db_sqlite_callback_parameters* callback_params = (rocprofvis_db_sqlite_callback_parameters*)data;
    RocpdDatabase* db = (RocpdDatabase*)callback_params->db;
    if (callback_params->future->Interrupted()) return 1;
    if (0 == callback_params->row_counter)
    {
        for (int i=0; i < argc; i++)
        {
            if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableColumn(callback_params->handle,azColName[i])) return 1;
        }
    }
    rocprofvis_dm_table_row_t row = db->BindObject()->FuncAddTableRow(callback_params->handle);
    ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 1);
    for (int i=0; i < argc; i++)
    {
        if (kRocProfVisDmResultSuccess != db->BindObject()->FuncAddTableRowCell(row, argv[i])) return 1;
    }
    callback_params->row_counter++;
    return 0;
}

rocprofvis_dm_result_t  RocpdDatabase::ReadTraceMetadata(Future* future)
{
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ASSERT_MSG_BREAK(BindObject()->trace_parameters, ERROR_TRACE_PARAMETERS_CANNOT_BE_NULL);
        TraceParameters()->strings_offset=0;
        TraceParameters()->symbols_offset=0;

        ShowProgress(10, "Create CPU tracks indexes", kRPVDbBusy, future);
        ExecuteSQLQuery(future,"CREATE INDEX pid_tid_idx ON rocpd_api(pid,tid);");

        ShowProgress(10, "Create GPU tracks indexes", kRPVDbBusy, future );
        ExecuteSQLQuery(future,"CREATE INDEX gid_qid_idx ON rocpd_op(gpuId, queueId);");
        
        ShowProgress(10, "Create PMC tracks indexes", kRPVDbBusy, future );
        ExecuteSQLQuery(future,"CREATE INDEX monitorTypeIdx on rocpd_monitor(monitorType);");

        ShowProgress(1, "Getting minimum timestamp", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future,"SELECT MIN(start) FROM rocpd_api;", &CallbackGetMinTime)) break;

        ShowProgress(1, "Get maximum timestamp", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "select MAX(start) from rocpd_op;", &CallbackGetMaxTime)) break;

        ShowProgress(5, "Adding CPU tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "select DISTINCT pid, tid from rocpd_api;", &CallBackAddTrack, kRocProfVisDmRegionTrack)) break;

        ShowProgress(5, "Adding GPU tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "select DISTINCT gpuId, queueId from rocpd_op;", &CallBackAddTrack, kRocProfVisDmKernelTrack)) break;

        ShowProgress(5, "Adding PMC tracks", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "select DISTINCT deviceId, monitorType from rocpd_monitor where deviceId > 0;", &CallBackAddTrack, kRocProfVisDmPmcTrack)) break;

        ShowProgress(20, "Loading strings", kRPVDbBusy, future );
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, "SELECT string, GROUP_CONCAT(id) AS ids FROM rocpd_string GROUP BY string;", &CallBackAddString)) break;

        TraceParameters()->metadata_loaded=true;
        ShowProgress(100-future->Progress(), "Trace metadata successfully loaded", kRPVDbSuccess, future );
        return future->SetPromise(kRocProfVisDmResultSuccess);

    }
    ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t  RocpdDatabase::ReadTraceSlice( 
                                                    rocprofvis_dm_timestamp_t start,
                                                    rocprofvis_dm_timestamp_t end,
                                                    rocprofvis_db_num_of_tracks_t num,
                                                    rocprofvis_db_track_selection_t tracks,
                                                    Future* future) {
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ASSERT_MSG_BREAK(BindObject()->trace_parameters, ERROR_TRACE_PARAMETERS_CANNOT_BE_NULL);
        ASSERT_MSG_BREAK(BindObject()->trace_parameters->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
        double step = 100.0/num;
        int i=0;
        for (; i < num; i++)
        {
            ASSERT_MSG_BREAK(tracks[i] < NumTrackProperties(), ERROR_INDEX_OUT_OF_RANGE);
            rocprofvis_dm_slice_t slice = BindObject()->FuncAddSlice(BindObject()->trace_object,tracks[i],start,end);
            ASSERT_MSG_BREAK(slice, ERROR_SLICE_CANNOT_BE_NULL);
            std::stringstream query;
            if (TrackPropertiesAt(tracks[i])->track_category == kRocProfVisDmRegionTrack)
            {
                query << "select start, (end-start), apiName_id, args_id, id from rocpd_api where pid == " << TrackPropertiesAt(tracks[i])->process
                    << " and tid == " << TrackPropertiesAt(tracks[i])->name
                    << " and start >= " << start
                    << " and end < " << end
                    << " ORDER BY start;";
                    ShowProgress(step, query.str().c_str(),kRPVDbBusy, future);
                    if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddEventRecord, kRocProfVisDmRegionTrack, slice, kRocProfVisDmOperationLaunch )) break;
            } else
            if (TrackPropertiesAt(tracks[i])->track_category == kRocProfVisDmKernelTrack)
            {
                query << "select start, (end-start), opType_id, description_id, id from rocpd_op where gpuId == " << TrackPropertiesAt(tracks[i])->process
                    << " and queueId == " << TrackPropertiesAt(tracks[i])->name
                    << " and start >= " << start
                    << " and end < " << end
                    << " ORDER BY start;";
                    ShowProgress(step, query.str().c_str(),kRPVDbBusy, future);
                    if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddEventRecord, kRocProfVisDmKernelTrack, slice, kRocProfVisDmOperationDispatch )) break;
            } else     
            if (TrackPropertiesAt(tracks[i])->track_category == kRocProfVisDmPmcTrack)
            {
                query << "select start, value from rocpd_monitor where deviceId == " << TrackPropertiesAt(tracks[i])->process
                    << " and monitorType == \"" << TrackPropertiesAt(tracks[i])->name << "\"" 
                    << " and start >= " << start
                    << " and end < " << end
                    << " ORDER BY start;";
                    ShowProgress(step, query.str().c_str(),kRPVDbBusy, future);
                    if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddPmcRecord, kRocProfVisDmPmcTrack, slice )) break;
            }            
        }

        if (i < num) break;
        ShowProgress(100- future->Progress(), "Time slice successfully loaded!", kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Not all tracks are loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t  RocpdDatabase::ReadFlowTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future)
{
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        rocprofvis_dm_flowtrace_t flowtrace = BindObject()->FuncAddFlowTrace(BindObject()->trace_object, event_id);
        ASSERT_MSG_RETURN(flowtrace, ERROR_FLOW_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
        std::stringstream query;
        if (event_id.bitfiled.event_op == kRocProfVisDmOperationLaunch)
        {
            query << "select rocpd_api_ops.op_id, gpuId, queueId, rocpd_op.start from rocpd_api_ops "
                "INNER JOIN rocpd_api on rocpd_api_ops.api_id = rocpd_api.id "
                "INNER JOIN rocpd_op on rocpd_api_ops.op_id = rocpd_op.id where rocpd_api_ops.api_id = ";
            query << event_id.bitfiled.event_id;  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackGetFlowTrace, flowtrace, kRocProfVisDmOperationLaunch)) break;
        } else
        if (event_id.bitfiled.event_op == kRocProfVisDmOperationDispatch)
        {
            query << "select rocpd_api_ops.api_id, pid, tid, rocpd_api.end from rocpd_api_ops "
                "INNER JOIN rocpd_api on rocpd_api_ops.api_id = rocpd_api.id "
                "INNER JOIN rocpd_op on rocpd_api_ops.op_id = rocpd_op.id where rocpd_api_ops.op_id = ";
            query << event_id.bitfiled.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackGetFlowTrace, flowtrace, kRocProfVisDmOperationDispatch)) break;
        } else
        {
            ShowProgress(0, "Flow trace is not available for specified operation type!", kRPVDbError, future );
            return future->SetPromise(kRocProfVisDmResultInvalidParameter);
        }
        ShowProgress(100, "Flow trace successfully loaded!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Flow trace not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
}

rocprofvis_dm_result_t  RocpdDatabase::ReadStackTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future)
{
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        rocprofvis_dm_stacktrace_t stacktrace = BindObject()->FuncAddStackTrace(BindObject()->trace_object, event_id);
        ASSERT_MSG_RETURN(stacktrace, ERROR_STACK_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
        std::stringstream query;
        if (event_id.bitfiled.event_op == kRocProfVisDmOperationLaunch || event_id.bitfiled.event_op == kRocProfVisDmOperationMemoryAllocate)
        {
            query << "select s2.string as hip_api, s3.string as args, s1.string as frame, sf.depth "
                     "from rocpd_stackframe sf join rocpd_string s1 on sf.name_id=s1.id join rocpd_api ap on "
                     "sf.api_ptr_id=ap.id join rocpd_string s2 on ap.apiName_id=s2.id join rocpd_string s3 on ap.args_id=s3.id where ap.id  == ";
            query << event_id.bitfiled.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackGetStackTrace,  stacktrace, kRocProfVisDmOperationLaunch)) break;
        } else
        {
            ShowProgress(0, "Stack trace is not available for specified operation type!", kRPVDbError, future );
            return future->SetPromise(kRocProfVisDmResultInvalidParameter);
        }
        ShowProgress(100, "Stack trace successfully loaded!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Stack trace not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);
}


rocprofvis_dm_result_t  RocpdDatabase::ReadExtEventInfo(
            rocprofvis_dm_event_id_t event_id, 
            Future* future){
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        rocprofvis_dm_extdata_t extdata = BindObject()->FuncAddExtData(BindObject()->trace_object, event_id);
        ASSERT_MSG_RETURN(extdata, ERROR_EXT_DATA_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
        std::stringstream query;
        if (event_id.bitfiled.event_op == kRocProfVisDmOperationLaunch)
        {
            query << "select * from api where id == ";
            query << event_id.bitfiled.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddExtInfo, "Properties", extdata, (roprofvis_dm_event_operation_t)event_id.bitfiled.event_op)) break;
            query.str("");
            query << "select * from copy where id == ";
            query << event_id.bitfiled.event_id << ";";  
            ShowProgress(50, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddExtInfo, "Copy", extdata, (roprofvis_dm_event_operation_t)event_id.bitfiled.event_op)) break;
        } else
        if (event_id.bitfiled.event_op == kRocProfVisDmOperationDispatch)
        {
            query << "select * from op where id == ";
            query << event_id.bitfiled.event_id << ";";  
            ShowProgress(0, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddExtInfo, "Properties", extdata, (roprofvis_dm_event_operation_t)event_id.bitfiled.event_op)) break;
            query.str("");
            query << "select * from copyop where id == ";
            query << event_id.bitfiled.event_id << ";";  
            ShowProgress(25, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddExtInfo, "Copy", extdata, (roprofvis_dm_event_operation_t)event_id.bitfiled.event_op)) break;
            query.str("");
            query << "select * from kernel where id == ";
            query << event_id.bitfiled.event_id << ";";  
            ShowProgress(50, query.str().c_str(),kRPVDbBusy, future);
            if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query.str().c_str(), &CallbackAddExtInfo, "Dispatch", extdata, (roprofvis_dm_event_operation_t)event_id.bitfiled.event_op)) break;
        } else    
        {
            ShowProgress(0, "Extended data not available for specified operation type!", kRPVDbError, future );
            return future->SetPromise(kRocProfVisDmResultInvalidParameter);
        }
        ShowProgress(50, "Extended data successfully loaded!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Extended data  not loaded!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed);    
}


rocprofvis_dm_result_t  RocpdDatabase::ExecuteQuery(
        rocprofvis_dm_charptr_t query,
        rocprofvis_dm_charptr_t description,
        Future* future){
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        rocprofvis_dm_table_t table = BindObject()->FuncAddTable(BindObject()->trace_object, query, description);
        ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
        if (kRocProfVisDmResultSuccess != ExecuteSQLQuery(future, query, table, &CallbackRunQuery)) break;
        ShowProgress(100, "Query successfully executed!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Query could not be executed!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultTimeout : kRocProfVisDmResultDbAccessFailed); 
}
