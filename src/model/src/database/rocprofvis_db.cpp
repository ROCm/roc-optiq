// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db.h"
#include "rocprofvis_db_profile.h"
#include <cstdio>
#include <fstream> 
#include <sstream>
#include <cstring>
#include <cfloat>

namespace RocProfVis
{
namespace DataModel
{

bool Database::IsNumber(const std::string& s) {
    std::istringstream iss(s);
    uint64_t d;
    return iss >> std::noskipws >> d && iss.eof();
}

rocprofvis_dm_result_t Database::AddTrackProperties(
                                                    rocprofvis_dm_track_params_t& props) {
    try {
        m_track_properties.push_back(std::make_unique<rocprofvis_dm_track_params_t>(props));
    }
    catch (std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, kRocProfVisDmResultAllocFailure);
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
                                                    rocprofvis_dm_db_bind_struct * binding_info){
    m_binding_info = binding_info;
    m_binding_info->FuncFindCachedTableValue = FindCachedTableValue;
    m_binding_info->FuncGetInfoTableNumColumns = GetInfoTableNumColumns;
    m_binding_info->FuncGetInfoTableNumRows = GetInfoTableNumRows;
    m_binding_info->FuncGetInfoTableColumnName = GetInfoTableColumnName;
    m_binding_info->FuncGetInfoTableRowHandle = GetInfoTableRowHandle;
    m_binding_info->FuncGetInfoTableRowCellValue = GetInfoTableRowCellValue;
    m_binding_info->FuncGetInfoTableRowNumCells = GetInfoTableRowNumCells;
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ReadTraceMetadataAsync(
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    try {
        future->SetWorker(std::move(std::thread(Database::ReadTraceMetadataStatic, this, future)));
    }
    catch (std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
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
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    rocprofvis_dm_result_t result = BindObject()->FuncCheckSliceExists(BindObject()->trace_object, start, end, num, tracks); 
    if(result != kRocProfVisDmResultNotLoaded)
    {
        spdlog::debug("Slice ({},{}) exists!", start, end);
        return future->SetPromise(result);
    }
    try {
        future->SetWorker(std::move(std::thread(Database::ReadTraceSliceStatic, this, start, end, num, tracks, future)));
    }
    catch (std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t   Database::ReadEventPropertyAsync(
                                                    rocprofvis_dm_event_property_type_t type,
                                                    rocprofvis_dm_event_id_t event_id,
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    rocprofvis_dm_result_t result =  BindObject()->FuncCheckEventPropertyExists(BindObject()->trace_object, type, event_id);
    if(result != kRocProfVisDmResultNotLoaded)
    {
        return future->SetPromise(kRocProfVisDmResultResourceBusy);
    }
    try {
        future->SetWorker(std::move(std::thread(ReadEventPropertyStatic, this, type, event_id, future)));
    }
    catch (std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t Database::ExportTableCSVAsync(rocprofvis_dm_string_t query,
                                                     rocprofvis_dm_string_t file_path,
                                                     rocprofvis_db_future_t object)
{
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(!file_path.empty(), "Output path cannot be empty.",
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED,
                                 kRocProfVisDmResultResourceBusy);
    try
    {
        future->SetWorker(std::move(std::thread(&ExportTableCSVStatic, this, query, file_path, future)));
    } catch(std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t Database::ExportTableCSVStatic(Database* db,
                                                      rocprofvis_dm_string_t query,
                                                      rocprofvis_dm_string_t file_path,
                                                      Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(!file_path.empty(), "New DB path cannot be empty.",
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);    
    return db->ExportTableCSV(query.c_str(), file_path.c_str(), future);
}

rocprofvis_dm_result_t Database::ExportTableCSV(rocprofvis_dm_charptr_t query,
                                                rocprofvis_dm_charptr_t file_path, 
                                                Future* future)
{
    (void) query;
    (void) file_path;
    (void) future;
    return kRocProfVisDmResultNotSupported;
}

rocprofvis_dm_result_t
Database::SaveTrimmedDataAsync(rocprofvis_dm_timestamp_t start,
                               rocprofvis_dm_timestamp_t end,
                               rocprofvis_dm_string_t new_db_path,
                               rocprofvis_db_future_t object)
{
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(!new_db_path.empty(), "New DB path cannot be empty.",
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED,
                                 kRocProfVisDmResultResourceBusy);
    rocprofvis_dm_result_t result = kRocProfVisDmResultUnknownError;
    try
    {
        future->SetWorker(std::move(std::thread(&SaveTrimmedDataStatic, this, start, end, new_db_path, future)));
    } catch(std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t Database::SaveTrimmedDataStatic(Database* db, rocprofvis_dm_timestamp_t start,
    rocprofvis_dm_timestamp_t end, rocprofvis_dm_string_t new_db_path, Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(!new_db_path.empty(), "New DB path cannot be empty.",
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);


    //check if a db file exists and if it does delete it (we will overwrite it)
    std::ifstream file(new_db_path);
    if(file.good())
    {
        file.close();
        int remove_result = std::remove(new_db_path.c_str());
        if(remove_result != 0)
        {
            spdlog::error("Failed to overwrite existing file: {}, code: {}", new_db_path,
                          remove_result);
            
            db->ShowProgress(0, "Failed to trim track! Could not overwrite existing file.", kRPVDbError, future);
            future->SetPromise(kRocProfVisDmResultDbAccessFailed);
            return kRocProfVisDmResultDbAccessFailed;
        }
    }

    return db->SaveTrimmedData(start, end, new_db_path.c_str(), future);
}


rocprofvis_dm_result_t  Database::ExecuteQueryAsync(
                                                    rocprofvis_dm_charptr_t query,
                                                    rocprofvis_dm_charptr_t description,
                                                    rocprofvis_db_future_t object, 
                                                    rocprofvis_dm_table_id_t* id)
{
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    *id = std::hash<std::string>{}(query);
    rocprofvis_dm_result_t   result = BindObject()->FuncCheckTableExists(BindObject()->trace_object, *id);
    if(result != kRocProfVisDmResultNotLoaded)
    {
        return future->SetPromise(result);
    }
    try {
        future->SetWorker(std::move(std::thread(ExecuteQueryStatic, this, query, description, future)));
    }
    catch (std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ExecuteComputeQueryAsync(
    rocprofvis_db_compute_use_case_enum_t use_case,
    rocprofvis_dm_charptr_t query,
    rocprofvis_db_future_t object, 
    rocprofvis_dm_table_id_t* id)
{
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    *id = std::hash<std::string>{}(query);
    rocprofvis_dm_result_t   result = BindObject()->FuncCheckTableExists(BindObject()->trace_object, *id);
    if(result != kRocProfVisDmResultNotLoaded)
    {
        return future->SetPromise(result);
    }
    try {
        future->SetWorker(std::move(std::thread(ExecuteComputeQueryStatic, this, use_case, query, future)));
    }
    catch (std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
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
    ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotSupported); 
}



rocprofvis_dm_result_t   Database::ExecuteQueryStatic(
                                                    Database* db,
                                                    rocprofvis_dm_charptr_t query,
                                                    rocprofvis_dm_charptr_t description,
                                                    Future* object){
    return db->ExecuteQuery(query,description,object);
}

rocprofvis_dm_result_t   Database::ExecuteComputeQueryStatic(
    Database* db,
    rocprofvis_db_compute_use_case_enum_t use_case,
    rocprofvis_dm_charptr_t query,
    Future* object){
    return db->ExecuteComputeQuery(use_case, query,object);
}

const char* Database::ProcessNameSuffixFor(rocprofvis_dm_track_category_t category){
    switch(category){
        case kRocProfVisDmPmcTrack:
        case kRocProfVisDmKernelDispatchTrack:
        case kRocProfVisDmMemoryAllocationTrack:
        case kRocProfVisDmMemoryCopyTrack:
            return "GPU:";
        case kRocProfVisDmRegionSampleTrack: 
            return "Sample PID:";
        case kRocProfVisDmRegionTrack:
        case kRocProfVisDmRegionMainTrack:
            return "Thread PID:";
        case kRocProfVisDmStreamTrack: 
            return "STREAM:";
    }
    return "";
}

const char* Database::SubProcessNameSuffixFor(rocprofvis_dm_track_category_t category){
    switch(category){
        case kRocProfVisDmPmcTrack:
        case kRocProfVisDmKernelDispatchTrack:
        case kRocProfVisDmMemoryAllocationTrack:
        case kRocProfVisDmMemoryCopyTrack:
            return "Queue ";
        case kRocProfVisDmRegionTrack:
        case kRocProfVisDmRegionMainTrack:
        case kRocProfVisDmRegionSampleTrack:
            return "Thread ";
    }
    return "";
}

rocprofvis_dm_result_t   Database::FindCachedTableValue(  
                                                        const rocprofvis_dm_database_t object, 
                                                        rocprofvis_dm_charptr_t table, 
                                                        const rocprofvis_dm_id_t id, 
                                                        rocprofvis_dm_charptr_t column,
                                                        rocprofvis_dm_node_id_t node,
                                                        rocprofvis_dm_charptr_t* value){
    Database* db = (Database*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(db, ERROR_DATABASE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    *value = db->CachedTables(node)->GetTableCell(table, id, column); 
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_table_t Database::GetInfoTableHandle(const rocprofvis_dm_database_t object, rocprofvis_dm_node_id_t node, rocprofvis_dm_charptr_t table_name){
    Database* db = (Database*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(db, ERROR_DATABASE_CANNOT_BE_NULL, nullptr);
    return db->CachedTables(node)->GetTableHandle(table_name);
}
size_t Database::GetInfoTableNumColumns(rocprofvis_dm_table_t object){
    TableCache* table = (TableCache*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, 0);
    return table->NumColumns();
}
size_t Database::GetInfoTableNumRows(rocprofvis_dm_table_t object){
    TableCache* table = (TableCache*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, 0);
    return table->NumRows();
}
const char* Database::GetInfoTableColumnName(rocprofvis_dm_table_t object, size_t column_index){
    TableCache* table = (TableCache*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, 0);
    return table->GetColumnName(column_index);
}
rocprofvis_dm_table_row_t Database::GetInfoTableRowHandle(rocprofvis_dm_table_t object, size_t row_index){
    TableCache* table = (TableCache*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, 0);
    return table->GetRow(row_index);
}
const char* Database::GetInfoTableRowCellValue(rocprofvis_dm_table_row_t object, size_t column_index){
    TableCache::Row * row = (TableCache::Row*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 0);
    return row->values[column_index].c_str();
}

const size_t Database::GetInfoTableRowNumCells(rocprofvis_dm_table_row_t object){
    TableCache::Row * row = (TableCache::Row*)object;
    ROCPROFVIS_ASSERT_MSG_RETURN(row, ERROR_TABLE_ROW_CANNOT_BE_NULL, 0);
    return row->values.size();
}

rocprofvis_dm_size_t DatabaseCache::GetMemoryFootprint()
{
    size_t size = 0;
    return size;
}

rocprofvis_dm_size_t Database::GetMemoryFootprint()
{
    rocprofvis_dm_size_t size = 0;
    for (auto guid : m_db_instances)
    {
        size+= m_cached_tables[guid.first.GuidIndex()].GetMemoryFootprint();
    }
    size+=NumTracks()*(sizeof(rocprofvis_dm_track_params_t)+sizeof(std::unique_ptr<rocprofvis_dm_track_params_t>));
    size+=strlen(Path());
    return size;
}

void
Database::UpdateQueryForTrack(  rocprofvis_dm_track_params_it it, 
                                rocprofvis_dm_track_params_t& newprops,
                                rocprofvis_dm_charptr_t*      newqueries)
{

    int slice_query_category        = newprops.track_indentifiers.category == kRocProfVisDmStreamTrack
                                          ? kRPVQuerySliceByStream
                                          : kRPVQuerySliceByQueue;
    int slice_source_query_category = newprops.track_indentifiers.category == kRocProfVisDmStreamTrack
                                          ? kRPVSourceQuerySliceByStream
                                          : kRPVSourceQuerySliceByQueue;
    if (it != TrackPropertiesEnd()) {
            std::vector<rocprofvis_dm_string_t>::iterator s = 
            std::find_if(it->get()->query[slice_query_category].begin(), 
                            it->get()->query[slice_query_category].end(),
                [newqueries, slice_source_query_category](rocprofvis_dm_string_t& str) {
                                 return str == newqueries[slice_source_query_category];
                         });
            if(s == it->get()->query[slice_query_category].end())
            {
                it->get()->query[slice_query_category].push_back(
                    newqueries[slice_source_query_category]);
            }
            
            s = std::find_if(it->get()->query[kRPVQueryTable].begin(),
                             it->get()->query[kRPVQueryTable].end(),
                             [newqueries](rocprofvis_dm_string_t& str) {
                                 return str == newqueries[kRPVSourceQueryTable];
                             });
            if(s == it->get()->query[kRPVQueryTable].end())
            {
                it->get()->query[kRPVQueryTable].push_back(newqueries[kRPVSourceQueryTable]);
            }
            s = std::find_if(it->get()->query[kRPVQueryLevel].begin(),
                             it->get()->query[kRPVQueryLevel].end(),
                             [newqueries](rocprofvis_dm_string_t& str) {
                                 return str == newqueries[kRPVSourceQueryLevel];
                             });
            if(s == it->get()->query[kRPVQueryLevel].end())
            {
                it->get()->query[kRPVQueryLevel].push_back(newqueries[kRPVSourceQueryLevel]);
            }
        return;
    } 
    newprops.query[slice_query_category].push_back(newqueries[slice_source_query_category]);
    newprops.query[kRPVQueryTable].push_back(newqueries[kRPVSourceQueryTable]);
    newprops.query[kRPVQueryLevel].push_back(newqueries[kRPVSourceQueryLevel]); 
}

void DatabaseVersion::SetVersion(const char* version) {
    m_db_version = ConvertVersionStringToInt(version);
}

std::vector<uint32_t>
DatabaseVersion::ConvertVersionStringToInt(const char* version)
{
    std::vector<uint32_t> version_array;
    std::stringstream ss(version);
    std::string       token;
    while(std::getline(ss, token, '.'))
    {
        version_array.push_back(std::stoi(token));
    }
    return version_array;
}

bool DatabaseVersion::IsVersionEqual(const char* version)
{
    std::vector<uint32_t> db_version = ConvertVersionStringToInt(version);

    for (int i = 0; i < db_version.size(); i++)
    {
        uint32_t token = (m_db_version.size() > i) ? m_db_version[i] : 0;
        if(db_version[i] != token)
        {
            return false;
        }
    }
    return true;
}

bool DatabaseVersion::IsVersionGreaterOrEqual(const char* version) 
{
    std::vector<uint32_t> db_version = ConvertVersionStringToInt(version);

    for(int i = 0; i < db_version.size(); i++)
    {
        uint32_t token = (m_db_version.size() > i) ? m_db_version[i] : 0;
        if(token > db_version[i])
        {
            return true;
        } else if (token < db_version[i])
        {
            return false;
        }
    }
    return true;
}

}  // namespace DataModel
}  // namespace RocProfVis