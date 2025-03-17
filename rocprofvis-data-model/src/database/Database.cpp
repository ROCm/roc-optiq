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

#include "Database.h"
#include "SqliteDb.h"

rocprofvis_db_type_t Database::Autodetect(
                                                    rocprofvis_db_filename_t filename){
    rocprofvis_db_type_t db_type = SqliteDatabase::Detect(filename);
    if (db_type!=rocprofvis_db_type_t::kAutodetect)
            return db_type;
    return rocprofvis_db_type_t::kAutodetect;
}


rocprofvis_dm_result_t Database::FindTrackIdByName(
                                                    const char* process, 
                                                    const char* name, 
                                                    rocprofvis_dm_track_id_t & track_id ){
    std::vector<std::unique_ptr<rocprofvis_dm_track_params_t>>::iterator it = 
        std::find_if(m_track_properties.begin(), m_track_properties.end(), [process, name](std::unique_ptr<rocprofvis_dm_track_params_t> & params) {
        return  params.get()->process == process && params.get()->name == name;});
    if (it != m_track_properties.end()) {
        track_id = (rocprofvis_dm_track_id_t)(it - m_track_properties.begin());
        return kRocProfVisDmResultSuccess;
    } 
    return kRocProfVisDmResultNotLoaded;
}

rocprofvis_dm_result_t Database::AddTrackProperties(
                                                    rocprofvis_dm_track_params_t& props) {
    try {
        m_track_properties.push_back(std::make_unique<rocprofvis_dm_track_params_t>(props));
    }
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, kRocProfVisDmResultAllocFailure);
    }
    return kRocProfVisDmResultSuccess;
}

void  Database::ShowProgress(
                                                    double step, 
                                                    rocprofvis_dm_charptr_t action, 
                                                    rocprofvis_db_status_t status, 
                                                    Future* future){
    future->ShowProgress(Path(), step, action, status);
}

rocprofvis_dm_result_t Database::BindTrace(
                                                    rocprofvis_dm_db_bind_struct binding_info){
    m_binding_info = binding_info;
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ReadTraceMetadataAsync(
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    try {
        future->SetWorker(std::move(std::thread(Database::ReadTraceMetadataStatic, this, future)));
    }
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ReadTraceSliceAsync(
                                                    rocprofvis_dm_timestamp_t start,
                                                    rocprofvis_dm_timestamp_t end,
                                                    rocprofvis_db_num_of_tracks_t num,
                                                    rocprofvis_db_track_selection_t tracks,
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    try {
        future->SetWorker(std::move(std::thread(Database::ReadTraceSliceStatic, this, start, end, num, tracks, future)));
    }
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t   Database::ReadEventPropertyAsync(
                                                    rocprofvis_dm_event_property_type_t type,
                                                    rocprofvis_dm_event_id_t event_id,
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    try {
        future->SetWorker(std::move(std::thread(ReadEventPropertyStatic, this, type, event_id, future)));
    }
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ExecuteQueryAsync(
                                                    rocprofvis_dm_charptr_t query,
                                                    rocprofvis_dm_charptr_t description,
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    try {
        future->SetWorker(std::move(std::thread(ExecuteQueryStatic, this, query, description, future)));
    }
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ReadTraceMetadataStatic(
                                                    Database* db, 
                                                    Future* object){
    return db->ReadTraceMetadata(object);
}

rocprofvis_dm_result_t  Database::ReadTraceSliceStatic(
                                                    Database* db,
                                                    rocprofvis_dm_timestamp_t start,
                                                    rocprofvis_dm_timestamp_t end,
                                                    rocprofvis_db_num_of_tracks_t num,
                                                    rocprofvis_db_track_selection_t tracks,
                                                    Future* object){
    return db->ReadTraceSlice(start, end, num, tracks, object);
}


rocprofvis_dm_result_t   Database::ReadEventPropertyStatic(
                                                    Database* db, 
                                                    rocprofvis_dm_event_property_type_t type,
                                                    rocprofvis_dm_event_id_t event_id,
                                                    Future* object){
    switch (type) {
        case kRPVDMEventFlowTrace:
            return db->ReadFlowTraceInfo(event_id,object);
        case kRPVDMEventStackTrace:
            return db->ReadStackTraceInfo(event_id,object);
        case kRPVDMEventExtData:
            return db->ReadExtEventInfo(event_id,object);           
    }  
    ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotSupported); 
}



rocprofvis_dm_result_t   Database::ExecuteQueryStatic(
                                                    Database* db,
                                                    rocprofvis_dm_charptr_t query,
                                                    rocprofvis_dm_charptr_t description,
                                                    Future* object){
    return db->ExecuteQuery(query,description,object);
}

