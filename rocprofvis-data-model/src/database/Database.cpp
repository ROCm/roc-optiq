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

#include "Database.h"

rocprofvis_dm_result_t Database::BindTrace(rocprofvis_dm_db_bind_struct binding_info)
{
    m_binding_info = binding_info;
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ReadTraceMetadataAsync(rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    try {
        future->m_worker = std::thread(Database::ReadTraceMetadataStatic, this, future);
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
    try {
        future->m_worker = std::thread(Database::ReadTraceSliceStatic, this, start, end, num, tracks, future);
    }
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ReadTraceMetadataStatic(Database* db, Future* object){
    return db->ReadTraceMetadata(object);
}

rocprofvis_dm_result_t  Database::ReadTraceSliceStatic(Database* db,
                                                            rocprofvis_dm_timestamp_t start,
                                                            rocprofvis_dm_timestamp_t end,
                                                            rocprofvis_db_num_of_tracks_t num,
                                                            rocprofvis_db_track_selection_t tracks,
                                                            Future* object){
    return db->ReadTraceSlice(start, end, num, tracks, object);
}

void Database::ShowProgress(double step, rocprofvis_dm_charptr_t action, rocprofvis_db_status_t status, rocprofvis_db_read_progress progress_callback){
    m_progress += step; 
    if (progress_callback) progress_callback(m_path, (int)m_progress, status, action);
}

rocprofvis_dm_result_t Database::AddTrackProperties(rocprofvis_dm_track_params_t& props) {
    try {
        m_track_properties.push_back(props);
    }
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, kRocProfVisDmResultAllocFailure);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t   Database::ReadFlowTraceInfoAsync(
                                                        rocprofvis_dm_event_id_t event_id,
                                                        rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    try {
        future->m_worker = std::thread(Database::ReadFlowTraceInfoStatic, this,  event_id, future);
    }
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t   Database::ReadStackTraceInfoAsync(
                                                        rocprofvis_dm_event_id_t event_id,
                                                        rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    try {
        future->m_worker = std::thread(Database::ReadStackTraceInfoStatic, this,  event_id, future);
    }
    catch (std::exception ex)
    {
        ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t   Database::ReadFlowTraceInfoStatic(
                                                        Database* db, 
                                                        rocprofvis_dm_event_id_t event_id,
                                                        Future* object){
    return db->ReadFlowTraceInfo(event_id,object);
}
rocprofvis_dm_result_t   Database::ReadStackTraceInfoStatic(
                                                        Database* db, 
                                                        rocprofvis_dm_event_id_t event_id,
                                                        Future* object){
    return db->ReadStackTraceInfo(event_id,object);
}
