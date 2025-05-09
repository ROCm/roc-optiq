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

#include "rocprofvis_db.h"
#include "rocprofvis_db_profile.h"
#include <sstream>
#include <cstring>

namespace RocProfVis
{
namespace DataModel
{

bool Database::IsNumber(const std::string& s) {
    std::istringstream iss(s);
    uint64_t d;
    return iss >> std::noskipws >> d && iss.eof();
}

rocprofvis_db_type_t Database::Autodetect(
                                                    rocprofvis_db_filename_t filename){
    rocprofvis_db_type_t db_type = ProfileDatabase::Detect(filename);
    if (db_type!=rocprofvis_db_type_t::kAutodetect)
            return db_type;
    return rocprofvis_db_type_t::kAutodetect;
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
    try {
        future->SetWorker(std::move(std::thread(Database::ReadTraceSliceStatic, this, start, end, num, tracks, future)));
    }
    catch (std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t Database::ReadTableSliceAsync(rocprofvis_dm_timestamp_t start, rocprofvis_dm_timestamp_t end,
    rocprofvis_db_num_of_tracks_t   num,
    rocprofvis_db_track_selection_t tracks,
    rocprofvis_dm_sort_columns_t sort_column, uint64_t max_count, uint64_t offset,
    rocprofvis_db_future_t object, rocprofvis_dm_slice_t* output_slice)
{
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                                 kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED,
                                 kRocProfVisDmResultResourceBusy);
    try
    {
        future->SetWorker(std::move(std::thread(Database::ReadTableSliceStatic, this,
                                                start, end, num, tracks, sort_column, max_count, offset, future, output_slice)));
    } catch(std::exception ex)
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
    try {
        future->SetWorker(std::move(std::thread(ReadEventPropertyStatic, this, type, event_id, future)));
    }
    catch (std::exception ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t  Database::ExecuteQueryAsync(
                                                    rocprofvis_dm_charptr_t query,
                                                    rocprofvis_dm_charptr_t description,
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    try {
        future->SetWorker(std::move(std::thread(ExecuteQueryStatic, this, query, description, future)));
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

rocprofvis_dm_result_t Database::ReadTableSliceStatic(Database* db, rocprofvis_dm_timestamp_t start,
    rocprofvis_dm_timestamp_t end, rocprofvis_db_num_of_tracks_t num,
    rocprofvis_db_track_selection_t tracks,
    rocprofvis_dm_sort_columns_t sort_column, uint64_t max_count,
    uint64_t offset, Future* object, rocprofvis_dm_slice_t* output_slice)
{
    return db->ReadTableSlice(start, end, num, tracks, sort_column, max_count, offset, object, output_slice);
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

const char* Database::ProcessNameSuffixFor(rocprofvis_dm_track_category_t category){
    switch(category){
        case kRocProfVisDmPmcTrack:
        case kRocProfVisDmKernelTrack:
            return "GPU:";
        case kRocProfVisDmRegionTrack:
            return "PID:";
    }
    return "";
}

const char* Database::SubProcessNameSuffixFor(rocprofvis_dm_track_category_t category){
    switch(category){
        case kRocProfVisDmPmcTrack:
        case kRocProfVisDmKernelTrack:
            return "Queue:";
        case kRocProfVisDmRegionTrack:
            return "TID:";
    }
    return "";
}

rocprofvis_dm_result_t DatabaseCache::PopulateTrackExtendedDataTemplate(Database * db, const char* table_name, uint64_t instance_id ){
    uint64_t id_to_str_conv_array[2] = { 0 };
    id_to_str_conv_array[0] = instance_id;
    rocprofvis_dm_track_params_t* track_properties = db->TrackPropertiesLast();
    auto m = references[table_name][instance_id];
    for(std::map<std::string,std::string>::iterator it = m.begin(); it != m.end(); ++it) {
        rocprofvis_db_ext_data_t record;
        record.name = it->first.c_str();
        record.data = (rocprofvis_dm_charptr_t)id_to_str_conv_array;
        record.category = table_name;
        rocprofvis_dm_result_t result = db->BindObject()->FuncAddExtDataRecord(track_properties->extdata, record);
        if (result != kRocProfVisDmResultSuccess) return result;
    }
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t   Database::FindCachedTableValue(  
                                                        const rocprofvis_dm_database_t object, 
                                                        rocprofvis_dm_charptr_t table, 
                                                        const rocprofvis_dm_id_t id, 
                                                        rocprofvis_dm_charptr_t column,
                                                        rocprofvis_dm_charptr_t* value){
    Database* db = (Database*) object;
    *value = db->CachedTables()->GetTableCell(table, id, column); 
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_size_t DatabaseCache::GetMemoryFootprint()
{
    size_t size = 0;
    for (std::map<std::string, table_map_t>::iterator it = references.begin(); it != references.end(); ++it) {
        size+=sizeof(std::string);
        size+=sizeof(table_map_t);
        size+=3 * sizeof(void*);
        size+=it->first.length();
        for (std::map<uint64_t, table_dict_t>::iterator it1 = it->second.begin(); it1 != it->second.end(); ++it1) {
            size += sizeof(uint64_t);
            size += sizeof(table_dict_t);
            size += 3 * sizeof(void*);
            for (std::map<std::string, std::string>::iterator it2 = it1->second.begin(); it2 != it1->second.end(); ++it2) {
                size += sizeof(std::string) * 2;
                size += 3 * sizeof(void*);
                size += it2->first.length();
                size += it2->second.length();
            }
        }
    }
    return size;
}

rocprofvis_dm_size_t Database::GetMemoryFootprint()
{
    rocprofvis_dm_size_t size = m_cached_tables.GetMemoryFootprint();
    size+=NumTracks()*(sizeof(rocprofvis_dm_track_params_t)+sizeof(std::unique_ptr<rocprofvis_dm_track_params_t>));
    size+=strlen(Path());
    return size;
}


bool Database::TrackExist( rocprofvis_dm_track_params_t & newprops, rocprofvis_dm_charptr_t newquery){
    std::vector<std::unique_ptr<rocprofvis_dm_track_params_t>>::iterator it = 
        std::find_if(TrackPropertiesBegin(), TrackPropertiesEnd(), [newprops] (std::unique_ptr<rocprofvis_dm_track_params_t> & params) {
                if(params.get()->track_category == newprops.track_category)
                {
                    for(int i = 0; i < NUMBER_OF_TRACK_IDENTIFICATION_PARAMETERS; i++)
                    {
                        if(newprops.process_id_numeric[i])
                        {
                            if(params.get()->process_id[i] != newprops.process_id[i])
                                return false;
                        }
                        else
                        {
                            if(params.get()->process_name[i] != newprops.process_name[i])
                                return false;
                        }
                    }
                    return true;
                }
                return false;
        });
    if (it != TrackPropertiesEnd()) {
            std::vector<rocprofvis_dm_string_t>::iterator s = 
            std::find_if(it->get()->query.begin(), it->get()->query.end(), [newquery] (rocprofvis_dm_string_t & str) {
                return str == newquery;});
            if (s == it->get()->query.end()) {
                it->get()->query.push_back(newquery);
            }
        return true;
    } 
    newprops.query.push_back(newquery);
    return false;    
}

}  // namespace DataModel
}  // namespace RocProfVis