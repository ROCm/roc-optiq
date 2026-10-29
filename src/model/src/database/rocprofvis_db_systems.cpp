// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_systems.h"
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


rocprofvis_dm_result_t SystemDatabase::AddTrackProperties(
                                                    rocprofvis_dm_track_params_t& props) {
    try {
        m_track_properties.push_back(std::make_unique<rocprofvis_dm_track_params_t>(props));
    }
    catch (const std::exception&)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_MEMORY_ALLOCATION_FAILURE, kRocProfVisDmResultAllocFailure);
    }
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t  SystemDatabase::ReadTraceSliceAsync(
                                                    rocprofvis_dm_timestamp_t start,
                                                    rocprofvis_dm_timestamp_t end,
                                                    rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                    rocprofvis_db_num_of_tracks_t num,
                                                    rocprofvis_db_track_selection_t tracks,
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    rocprofvis_dm_result_t result = BindObject()->FuncCheckSliceExists(BindObject()->trace_object, start, end, tag, num, tracks);
    if(result != kRocProfVisDmResultNotLoaded)
    {
        spdlog::debug("Slice ({},{}) exists!", start, end);
        return future->SetPromise(result);
    }
    try {
        future->SetWorker(std::move(std::thread([this, start, end, tag, num, tracks, future] { 
            return ReadTraceSlice(start, end, tag, num, tracks, future); 
            })));
    }
    catch (const std::exception& ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
SystemDatabase::ReadTracePMCSliceAsync(
                                                    rocprofvis_dm_timestamp_t start,
                                                    rocprofvis_dm_timestamp_t end,
                                                    rocprofvis_dm_hashed_timestamp_tag_t tag,
                                                    rocprofvis_db_track_selection_t track,
                                                    bool left_neighbor, 
                                                    bool right_neighbor,
                                                    rocprofvis_db_future_t object){
    Future* future = (Future*) object;
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(!future->IsWorking(), ERROR_FUTURE_CANNOT_BE_USED, kRocProfVisDmResultResourceBusy);
    rocprofvis_dm_result_t result = BindObject()->FuncCheckSliceExists(BindObject()->trace_object, start, end, tag, 1, track);
    if(result != kRocProfVisDmResultNotLoaded)
    {
        spdlog::debug("Slice ({},{}) exists!", start, end);
        return future->SetPromise(result);
    }
    try {
        future->SetWorker(std::move(std::thread([this, start, end, tag, track, left_neighbor, right_neighbor, future] {
            return ReadTracePMCSlice(start, end, tag, track, left_neighbor, right_neighbor, future);
            })));
    }
    catch (const std::exception& ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t   SystemDatabase::ReadEventPropertyAsync(
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
        future->SetWorker(std::move(std::thread([this, type, event_id, future]{
            switch (type) {
            case kRPVDMEventFlowTrace:
                return ReadFlowTraceInfo(event_id,future);
            case kRPVDMEventStackTrace:
                return ReadStackTraceInfo(event_id,future);
            case kRPVDMEventExtData:
                return ReadExtEventInfo(event_id,future); 
            default:
                ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ERROR_UNSUPPORTED_PROPERTY, kRocProfVisDmResultNotSupported);
            }          
            })));
    }
    catch (std::exception& ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}


rocprofvis_dm_result_t SystemDatabase::ExportTableCSVAsync(rocprofvis_dm_string_t query,
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
        future->SetWorker(std::move(std::thread([this, query, file_path, future]{
            ROCPROFVIS_ASSERT_MSG_RETURN(!file_path.empty(), "New DB path cannot be empty.",
                kRocProfVisDmResultInvalidParameter);
            ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL,
                kRocProfVisDmResultInvalidParameter);    
            return ExportTableCSV(query.c_str(), file_path.c_str(), future);
            })));
    } catch(std::exception& ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t
SystemDatabase::SaveTrimmedDataAsync(rocprofvis_dm_timestamp_t start,
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
    try
    {
        future->SetWorker(std::move(std::thread([this, start, end, new_db_path, future] {
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

                    ShowProgress(0, "Failed to trim track! Could not overwrite existing file.", kRPVDbError, future);
                    future->SetPromise(kRocProfVisDmResultDbAccessFailed);
                    return kRocProfVisDmResultDbAccessFailed;
                }
            }

            return SaveTrimmedData(start, end, new_db_path.c_str(), future);
            })));
    } catch(std::exception& ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}

rocprofvis_dm_result_t SystemDatabase::ExportTableCSV(rocprofvis_dm_charptr_t query,
    rocprofvis_dm_charptr_t file_path,
    Future* future)
{
    ROCPROFVIS_ASSERT_MSG_RETURN(file_path, "Output path cannot be NULL.", kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    ROCPROFVIS_ASSERT_MSG_RETURN(BindObject()->trace_object, ERROR_TRACE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    rocprofvis_dm_result_t result = kRocProfVisDmResultInvalidParameter;
    Future* internal_future = future->AddSubFuture();
    result = ExecuteQuery(query, "ExportTableCSV", internal_future);
    future->WaitAndDeleteSubFuture(internal_future);
    if (result == kRocProfVisDmResultSuccess)
    {
        rocprofvis_db_compound_table_type data_type = kRPVTableDataTypeEvent;
        std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>> queries;
        std::vector<rocprofvis_db_compound_query_command> commands;
        std::set<uint32_t> tracks;
        TableProcessor::IsCompoundQuery(query, queries, tracks,  commands);
        auto it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "TYPE"; });
        if (it != commands.end())
        {
            data_type = (rocprofvis_db_compound_table_type)std::atol(it->parameter.c_str());
            result = m_table_processor[data_type].ExportToCSV(file_path);
            if (result == kRocProfVisDmResultSuccess)
            {
                ShowProgress(100, "CSV export success", kRPVDbSuccess, future);        
            }
        }
    }
    BindObject()->FuncRemoveTable(BindObject()->trace_object, query);
    if (result != kRocProfVisDmResultSuccess)
    {
        ShowProgress(0, "CSV export failed", kRPVDbError, future);
    }
    return future->SetPromise(result);
}

rocprofvis_dm_result_t  SystemDatabase::ExecuteQuery(
    rocprofvis_dm_charptr_t query,
    rocprofvis_dm_charptr_t description,
    Future* future){

    ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
    while (true)
    {
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
        rocprofvis_dm_table_t table = BindObject()->FuncAddTable(BindObject()->trace_object, query, description);
        ROCPROFVIS_ASSERT_MSG_RETURN(table, ERROR_TABLE_CANNOT_BE_NULL, kRocProfVisDmResultUnknownError);
        std::unordered_map<uint32_t, std::unordered_map<std::string, rocprofvis_db_compound_query_info>> queries;
        std::vector<rocprofvis_db_compound_query_command> commands;
        std::set<uint32_t> tracks;
        if (TableProcessor::IsCompoundQuery(query, queries, tracks,  commands))
        {
            auto it = std::find_if(commands.begin(), commands.end(), [](rocprofvis_db_compound_query_command& cmd) { return cmd.name == "TYPE"; });
            rocprofvis_db_compound_table_type data_type = kRPVTableDataTypeEvent;
            if (it != commands.end())
            {
                data_type = (rocprofvis_db_compound_table_type)std::atol(it->parameter.c_str());
            }
            bool query_updated = !m_table_processor[data_type].IsCurrentQuery(queries);
            m_table_processor[data_type].SaveCurrentQuery(queries);
            if (kRocProfVisDmResultSuccess != m_table_processor[data_type].ExecuteCompoundQuery(future, queries, tracks, commands, table, query_updated)) break;
        }
        else
        {
            ShowProgress(100, "Direct database query is not supported!",kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultNotSupported);
        }

        ShowProgress(100, "Query successfully executed!",kRPVDbSuccess, future);
        return future->SetPromise(kRocProfVisDmResultSuccess);
    }
    ShowProgress(0, "Query could not be executed!", kRPVDbError, future );
    return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed); 
}

rocprofvis_dm_result_t  SystemDatabase::ExecuteQueryAsync(
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
        future->SetWorker(std::move(std::thread([this, query, description, future] {
            return ExecuteQuery(query,description,future); 
            })));
    }
    catch (std::exception& ex)
    {
        ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(ex.what(), kRocProfVisDmResultUnknownError);
    }
    return kRocProfVisDmResultSuccess;
}


const char* SystemDatabase::SubProcessNameSuffixFor(rocprofvis_dm_track_category_t category){
    switch(category){
        case kRocProfVisDmPmcTrack:
        case kRocProfVisDmKernelDispatchTrack:
        case kRocProfVisDmMemoryAllocationTrack:
        case kRocProfVisDmMemoryCopyTrack:
        case kRocProfVisDmHipEventTrack:
            return "Queue ";
        case kRocProfVisDmRegionTrack:
        case kRocProfVisDmRegionMainTrack:
        case kRocProfVisDmRegionSampleTrack:
            return "Thread ";
    }
    return "";
}


void SystemDatabase::CreateTracksOrderRanking() {


    using Track = rocprofvis_dm_track_params_t;

    std::map<std::set<uint32_t>, std::vector<Track*>> partitions;

    for (auto& track_prop : m_track_properties) {
        Track* t = track_prop.get();
        partitions[t->load_id].push_back(t);
    }

    uint32_t order_base = 0;

    for (auto& [load_set, partition_tracks] : partitions) {


        std::map<uint32_t, std::vector<Track*>> groups;

        for (auto* t : partition_tracks) {
            DbInstance* db_instance = (DbInstance*)t->track_indentifiers.db_instance;
            uint32_t file_index = db_instance ? db_instance->FileIndex() : 0;
            groups[file_index].push_back(t);
        }

        for (auto& [db, vec] : groups) {
            std::sort(vec.begin(), vec.end(),[](
                const Track* a, const Track* b) {
                    return a->track_indentifiers.track_id <
                    b->track_indentifiers.track_id;
                });
        }

        size_t max_len = 0;
        for (const auto& [_, vec] : groups) {
            max_len = std::max(max_len, vec.size());
        }

        uint32_t local_order = 0;

        for (size_t i = 0; i < max_len; ++i) {
            for (auto& [db, vec] : groups) {
                if (i < vec.size()) {
                    vec[i]->order_id = order_base + local_order;
                    ++local_order;
                }
            }
        }

        // Advance the base past every track assigned in this partition so
        // order_ids stay globally unique regardless of how many tracks a
        // partition holds (a fixed stride per partition would collide once a
        // partition exceeds that stride).
        order_base += local_order;
    }

}

rocprofvis_dm_size_t SystemDatabase::GetMemoryFootprint()
{
    rocprofvis_dm_size_t size = Database::GetMemoryFootprint();
    size+=NumTracks()*(sizeof(rocprofvis_dm_track_params_t)+sizeof(std::unique_ptr<rocprofvis_dm_track_params_t>));
    size+=strlen(Path());
    return size;
}


}  // namespace DataModel
}  // namespace RocProfVis