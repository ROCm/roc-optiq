// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_db_profiler_hub.h"
#include "rocprofvis_db_table_processor.h"
#include "rocprofvis_db_query_builder.h"
#include <sstream>
//#include "json.h"

namespace RocProfVis
{
namespace DataModel
{

    std::string ProfilerHub::TableColumnText(void* func, void* handle, char** azColName, int index) {
        ProfilerHubTableRow* row = (ProfilerHubTableRow*)handle;
        auto cell = row->CellAt(index);
        if (std::holds_alternative<std::string>(cell->value))
            return std::get<std::string>(cell->value);
        else if (std::holds_alternative<size_t>(cell->value))
            return std::to_string(std::get<size_t>(cell->value));
        else if (std::holds_alternative<double>(cell->value))
            return std::to_string(std::get<double>(cell->value));        
        return "";
    }
    int ProfilerHub::TableColumnInt(void* func, void* handle, char** azColName, int index) {
        ProfilerHubTableRow* row = (ProfilerHubTableRow*)handle;
        auto cell = row->CellAt(index);
        if (std::holds_alternative<size_t>(cell->value))
            return std::get<size_t>(cell->value);
        return 0;
    }
    int64_t ProfilerHub::TableColumnInt64(void* func, void* handle, char** azColName, int index) {
        ProfilerHubTableRow* row = (ProfilerHubTableRow*)handle;
        auto cell = row->CellAt(index);
        if (std::holds_alternative<size_t>(cell->value))
            return std::get<size_t>(cell->value);
        return 0;
    }
    double ProfilerHub::TableColumnDouble(void* func, void* handle, char** azColName, int index) {
        ProfilerHubTableRow* row = (ProfilerHubTableRow*)handle;
        auto cell = row->CellAt(index);
        if (std::holds_alternative<double>(cell->value))
            return std::get<double>(cell->value);
        return 0;
    }

    rocprofvis_dm_result_t ProfilerHub::ConvertProfilerHubResult(profiler_hub_result_t ph_result)
    {
        switch (ph_result)
        {
        case kProfilerHubStatusSuccess:
            return kRocProfVisDmResultSuccess;
        case kProfilerHubStatusUnknownError:
            return kRocProfVisDmResultUnknownError;
        case kProfilerHubStatusNotSupported:
            return kRocProfVisDmResultNotSupported;
        case kProfilerHubStatusInvalidArgument:
            return kRocProfVisDmResultInvalidParameter;
        case kProfilerHubStatusTimeout:
            return kRocProfVisDmResultTimeout;
        case kProfilerHubStatusNotLoaded:
            return kRocProfVisDmResultNotLoaded;
        }
        return kRocProfVisDmResultUnknownError;
    }

    rocprofvis_dm_event_operation_t ProfilerHub::ConvertProfilerHubEventType(profiler_hub::reader_types::event_type_t event_type)
    {
        switch (event_type)
        {
        case profiler_hub::reader_types::event_type_t::region:
            return kRocProfVisDmOperationLaunch;
        case profiler_hub::reader_types::event_type_t::kernel_dispatch:
            return kRocProfVisDmOperationDispatch;
        case profiler_hub::reader_types::event_type_t::memory_copy:
            return kRocProfVisDmOperationMemoryCopy;
        case profiler_hub::reader_types::event_type_t::memory_allocate:
            return kRocProfVisDmOperationMemoryAllocate;
        case profiler_hub::reader_types::event_type_t::sample:
            return kRocProfVisDmOperationLaunchSample;
        case profiler_hub::reader_types::event_type_t::pmc_event:
            return kRocProfVisDmOperationNoOp;
        }
        return kRocProfVisDmOperationNoOp;
    }


    rocprofvis_dm_result_t ProfilerHub::Open() {
       
        m_ph_trace = profiler_hub_open_trace(Path());
        return m_ph_trace ? kRocProfVisDmResultSuccess : kRocProfVisDmResultUnknownError;
    }

    rocprofvis_dm_result_t ProfilerHub::Close() {

        return ConvertProfilerHubResult(profiler_hub_close_trace(m_ph_trace));
    }

    void ProfilerHub::IdentificatorNamesUpdate(rocprofvis_dm_track_params_t& track_params)
    {
        ROCPROFVIS_ASSERT_MSG_RETURN(track_params.track_indentifiers.db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, );
        DbInstance* db_instance = (DbInstance*)track_params.track_indentifiers.db_instance;
        if(track_params.track_indentifiers.category == kRocProfVisDmRegionMainTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmRegionSampleTrack)
        {
            if (track_params.track_indentifiers.name[TRACK_ID_PID].empty())
            {
                track_params.track_indentifiers.name[TRACK_ID_PID] = CachedTables(db_instance->GuidIndex())->GetTableCell("Process", track_params.track_indentifiers.id[TRACK_ID_PID], "command");
                track_params.track_indentifiers.name[TRACK_ID_PID] += "(";
                track_params.track_indentifiers.name[TRACK_ID_PID] += std::to_string(track_params.track_indentifiers.id[TRACK_ID_PID]);
                track_params.track_indentifiers.name[TRACK_ID_PID] += ")";
            }

            if (track_params.track_indentifiers.name[TRACK_ID_TID].empty())
            {
                track_params.track_indentifiers.name[TRACK_ID_TID] = CachedTables(db_instance->GuidIndex())->GetTableCell("Thread", track_params.track_indentifiers.id[TRACK_ID_TID], "name");
                track_params.track_indentifiers.name[TRACK_ID_TID] += "(";
                track_params.track_indentifiers.name[TRACK_ID_TID] += std::to_string(track_params.track_indentifiers.id[TRACK_ID_TID]);
                track_params.track_indentifiers.name[TRACK_ID_TID] += ")";
            }
        }
        else if(track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack ||
            track_params.track_indentifiers.category == kRocProfVisDmPmcTrack)
        {
            if (track_params.track_indentifiers.name[TRACK_ID_AGENT].empty())
            {
                track_params.track_indentifiers.name[TRACK_ID_AGENT] = CachedTables(db_instance->GuidIndex())->GetTableCell("Agent", track_params.track_indentifiers.id[TRACK_ID_AGENT], "product_name");
                track_params.track_indentifiers.name[TRACK_ID_AGENT] += "(";
                track_params.track_indentifiers.name[TRACK_ID_AGENT] += CachedTables(db_instance->GuidIndex())->GetTableCell("Agent", track_params.track_indentifiers.id[TRACK_ID_AGENT], "type_index");
                track_params.track_indentifiers.name[TRACK_ID_AGENT] += ")";
            }
            if(track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack)
            {
                if (track_params.track_indentifiers.name[TRACK_ID_QUEUE].empty())
                {
                    track_params.track_indentifiers.name[TRACK_ID_QUEUE] = CachedTables(db_instance->GuidIndex())->GetTableCell("Queue", track_params.track_indentifiers.id[TRACK_ID_QUEUE], "name");
                }
            }
            else {
                if (track_params.track_indentifiers.name[TRACK_ID_COUNTER].empty())
                {
                    track_params.track_indentifiers.name[TRACK_ID_COUNTER] = CachedTables(db_instance->GuidIndex())->GetTableCell("PMC", track_params.track_indentifiers.id[TRACK_ID_QUEUE], "symbol");
                }
            }

        }
        else if(track_params.track_indentifiers.category == kRocProfVisDmStreamTrack)
        {
            if (track_params.track_indentifiers.name[TRACK_ID_STREAM].empty())
            {
                track_params.track_indentifiers.name[TRACK_ID_STREAM] = CachedTables(db_instance->GuidIndex())->GetTableCell("Stream", track_params.track_indentifiers.id[TRACK_ID_STREAM], "name");
                track_params.track_indentifiers.name[TRACK_ID_PID] = track_params.track_indentifiers.name[TRACK_ID_STREAM];
            }
        }
    }

    rocprofvis_dm_result_t ProfilerHub::ProcessTrack(rocprofvis_dm_track_params_t& track_params)
    {
        ROCPROFVIS_ASSERT_MSG_RETURN(track_params.track_indentifiers.db_instance != nullptr, ERROR_NODE_KEY_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        DbInstance* db_instance = (DbInstance*)track_params.track_indentifiers.db_instance;
        track_params.track_indentifiers.source_type = kRPVSystemSourceRocprof;
        rocprofvis_dm_track_params_it it = TrackTracker()->FindTrackParamsIterator(track_params.track_indentifiers, db_instance->GuidIndex());
        if(it == TrackPropertiesEnd())
        {
            TrackTracker()->AddTrack(track_params.track_indentifiers, db_instance->GuidIndex(), track_params.track_indentifiers.track_id);

            IdentificatorNamesUpdate(track_params);

            if (kRocProfVisDmResultSuccess != AddTrackProperties(track_params)) return kRocProfVisDmResultUnknownError;

            if (BindObject()->FuncAddTrack(BindObject()->trace_object, TrackPropertiesLast()) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError; 

            if (BindObject()->FuncAddTopologyNode(BindObject()->trace_object, &track_params.track_indentifiers) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError; 

            if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(
                this,
                db_instance->GuidIndex(),  
                "Node", 
                track_params.track_indentifiers.id[TRACK_ID_NODE]) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError;
            if(track_params.track_indentifiers.category == kRocProfVisDmRegionMainTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmRegionSampleTrack)
            {
                if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(
                    this, db_instance->GuidIndex(),  
                    "Process", 
                    track_params.track_indentifiers.id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError;
                if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(
                    this, db_instance->GuidIndex(), 
                    "Thread", 
                    track_params.track_indentifiers.id[TRACK_ID_TID]) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError;
            } 
            else if(track_params.track_indentifiers.category == kRocProfVisDmStreamTrack)
            {
                if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(
                    this, 
                    db_instance->GuidIndex(),  
                    "Process", 
                    track_params.track_indentifiers.id[TRACK_ID_PID]) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError;
                if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(
                    this, 
                    db_instance->GuidIndex(), 
                    "Stream", 
                    track_params.track_indentifiers.id[TRACK_ID_STREAM]) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError;         
            }
            else if(track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack ||
                track_params.track_indentifiers.category == kRocProfVisDmPmcTrack)
            {
                if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(
                    this, 
                    db_instance->GuidIndex(), 
                    "Agent", 
                    track_params.track_indentifiers.id[TRACK_ID_AGENT]) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError; 
                if(track_params.track_indentifiers.category == kRocProfVisDmKernelDispatchTrack ||
                    track_params.track_indentifiers.category == kRocProfVisDmMemoryAllocationTrack ||
                    track_params.track_indentifiers.category == kRocProfVisDmMemoryCopyTrack)
                {
                    if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(
                        this, 
                        db_instance->GuidIndex(), 
                        "Queue", 
                        track_params.track_indentifiers.id[TRACK_ID_QUEUE]) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError;
                }
                else
                {
                    if(track_params.track_indentifiers.id[TRACK_ID_QUEUE] > 0)
                        if (CachedTables(db_instance->GuidIndex())->PopulateTrackExtendedDataTemplate(
                            this, 
                            db_instance->GuidIndex(), 
                            "PMC", 
                            track_params.track_indentifiers.id[TRACK_ID_COUNTER]) != kRocProfVisDmResultSuccess) return kRocProfVisDmResultUnknownError;
                }
            }
            return kRocProfVisDmResultSuccess;
        }
    
        return kRocProfVisDmResultNotLoaded;
    }


    rocprofvis_dm_result_t  ProfilerHub::ReadTraceMetadata(Future* future)
    {
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        while (true)
        {
            ROCPROFVIS_ASSERT_MSG_BREAK(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
            std::string value;
            rocprofvis_dm_result_t result = kRocProfVisDmResultSuccess;
            profiler_hub_future_handle_t ph_future = future->AddProfilerHubFuture();
            ShowProgress(1, "Call profiler hub to read trace metadata", kRPVDbBusy, future);
            result = ConvertProfilerHubResult(profiler_hub_read_metadata(ph_future, m_ph_trace));
            if (result != kRocProfVisDmResultSuccess)
                break;

            result = ConvertProfilerHubResult(future->WaitAndDeleteProfilerHubFuture(ph_future));
            if (result != kRocProfVisDmResultSuccess)
                break;
            
            TraceProperties()->metadata_loaded=true;
            BindObject()->FuncMetadataLoaded(BindObject()->trace_object);
            ShowProgress(100-future->Progress(), "Trace metadata successfully loaded", kRPVDbSuccess, future );
            return future->SetPromise(kRocProfVisDmResultSuccess);

        }
        ShowProgress(0, "Trace metadata not loaded!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);
    }

    rocprofvis_dm_result_t  ProfilerHub::ExecuteQuery(
        rocprofvis_dm_charptr_t query,
        rocprofvis_dm_charptr_t description,
        Future* future) {
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
        return kRocProfVisDmResultDbAccessFailed;
    }

    rocprofvis_dm_result_t ProfilerHub::ExportTableCSV(
        rocprofvis_dm_charptr_t query,
        rocprofvis_dm_charptr_t file_path,
        Future* future) {
        return kRocProfVisDmResultNotSupported;
    }

    rocprofvis_dm_result_t ProfilerHub::BuildTableQuery(
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
        rocprofvis_dm_string_t& query) {

        ParamSerializer ser;
        ser.set("start",start);
        ser.set("end",end);
        if (processor)
        {
            if (processor->node_id)
            {
                ser.set("node",*processor->node_id);
                if (processor->agent_id)
                {
                    ser.set("agent",*processor->agent_id);
                }
            }
        }
        std::vector<uint32_t> v_tracks;
        for (int i = 0; i < num; i++)
        {
            v_tracks.push_back(tracks[i]);
        }
        ser.setArray("tracks", std::move(v_tracks));
        query = ser.toString();
        
        bool sample_query = false;
        if(TABLE_QUERY_UNPACK_OP_TYPE(tracks[0]) == 0)
        {
            sample_query =
                TrackPropertiesAt(tracks[0])->track_indentifiers.category ==
                kRocProfVisDmPmcTrack;
        }
        else
        {
            sample_query =
                (rocprofvis_dm_event_operation_t) TABLE_QUERY_UNPACK_OP_TYPE(
                    tracks[0]) == kRocProfVisDmOperationNoOp;
        }

        return TableProcessor::BuildTableSemanticSubQuery(
            use_case,  
            filter, 
            group, 
            group_cols, 
            sort_column, 
            sort_order, 
            max_count, 
            offset, 
            count_only, 
            sample_query,
            query);

    }

    rocprofvis_dm_result_t ProfilerHub::BuildEventSearchQuery(
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
        rocprofvis_dm_string_t& query) {

        ParamSerializer ser;
        ser.set("start",start);
        ser.set("end",end);
        if (processor)
        {
            if (processor->node_id)
            {
                ser.set("node",*processor->node_id);
                if (processor->agent_id)
                {
                    ser.set("agent",*processor->agent_id);
                }
            }
        }
        std::vector<uint32_t> v_ops;
        for (int i = 0; i < num; i++)
        {
            v_ops.push_back(ops[i]);
        }
        ser.setArray("tracks", std::move(v_ops));

        std::vector<std::string> v_filters;
        for (int i = 0; i < num_string_table_filters; i++)
        {
            v_filters.push_back(string_table_filters[i]);
        }
        ser.setArray("tracks", std::move(v_ops));
        ser.setArray("filters", std::move(v_filters));
        ser.set("include_substring",include_substring);
        ser.set("include_category",include_category);
        ser.set("partial_matching",partial_matching);

        query = ser.toString();

        return TableProcessor::BuildTableSemanticSubQuery(
            kRPVDMTableUseCaseEventSearch,  
            nullptr, 
            nullptr, 
            nullptr, 
            sort_column, 
            sort_order, 
            max_count, 
            offset, 
            count_only, 
            false,
            query);
    }

    rocprofvis_dm_result_t ProfilerHub::SaveTrimmedData(
        rocprofvis_dm_timestamp_t start,
        rocprofvis_dm_timestamp_t end,
        rocprofvis_dm_charptr_t new_db_path,
        Future* future) {
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        while(true)
        {
            profiler_hub_future_handle_t ph_future = future->AddProfilerHubFuture();
            profiler_hub_result_t result = profiler_hub_trim_save_trace(ph_future, m_ph_trace, start, end, new_db_path);
            if (result != kProfilerHubStatusSuccess)
                break;
            result = future->WaitAndDeleteProfilerHubFuture(ph_future);
            if (result != kProfilerHubStatusSuccess)
                break;
            ShowProgress(100 - future->Progress(), "Trace successfully trimmed!", kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }

        ShowProgress(0, "Failed trimming trace!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed); 
    }

    rocprofvis_dm_result_t  ProfilerHub::ReadFlowTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future) {
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        while (true)
        {
            profiler_hub_future_handle_t ph_future = future->AddProfilerHubFuture();
            rocprofvis_dm_flowtrace_t container = BindObject()->FuncAddFlowTrace(BindObject()->trace_object, event_id);
            profiler_hub_result_t result = profiler_hub_get_event_data_flow(
                ph_future,
                m_ph_trace,
                event_id.bitfield.event_node,
                container,
                (profiler_hub_event_operation_t)event_id.bitfield.event_op,
                event_id.bitfield.event_id);
            if (result != kProfilerHubStatusSuccess)
                break;
            result = future->WaitAndDeleteProfilerHubFuture(ph_future);
            if (result != kProfilerHubStatusSuccess)
                break;
            ShowProgress(100 - future->Progress(), "Successfully read flow trace!", kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
        ShowProgress(0, "Failed reading flow trace!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed); 
    }

    rocprofvis_dm_result_t  ProfilerHub::ReadStackTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future) {
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        while (true)
        {
            profiler_hub_future_handle_t ph_future = future->AddProfilerHubFuture();
            rocprofvis_dm_stacktrace_t container = BindObject()->FuncAddFlowTrace(BindObject()->trace_object, event_id);
            profiler_hub_result_t result = profiler_hub_get_event_stack_trace(
                ph_future,
                m_ph_trace,
                event_id.bitfield.event_node,
                container,
                (profiler_hub_event_operation_t)event_id.bitfield.event_op,
                event_id.bitfield.event_id);
            if (result != kProfilerHubStatusSuccess)
                break;
            result = future->WaitAndDeleteProfilerHubFuture(ph_future);
            if (result != kProfilerHubStatusSuccess)
                break;
            ShowProgress(100 - future->Progress(), "Successfully read flow trace!", kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
        ShowProgress(0, "Failed reading flow trace!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed); 
    }

    rocprofvis_dm_result_t  ProfilerHub::ReadExtEventInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* future) {
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        while (true)
        {
            profiler_hub_future_handle_t ph_future = future->AddProfilerHubFuture();
            rocprofvis_dm_extdata_t container = BindObject()->FuncAddFlowTrace(BindObject()->trace_object, event_id);
            profiler_hub_result_t result = profiler_hub_get_event_extended_data(
                ph_future,
                m_ph_trace,
                event_id.bitfield.event_node,
                container,
                (profiler_hub_event_operation_t)event_id.bitfield.event_op,
                event_id.bitfield.event_id);
            if (result != kProfilerHubStatusSuccess)
                break;
            result = future->WaitAndDeleteProfilerHubFuture(ph_future);
            if (result != kProfilerHubStatusSuccess)
                break;
            ShowProgress(100 - future->Progress(), "Successfully read flow trace!", kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
        ShowProgress(0, "Failed reading flow trace!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);
    }

    rocprofvis_dm_result_t  ProfilerHub::ReadTraceSlice( 
        rocprofvis_dm_timestamp_t start,
        rocprofvis_dm_timestamp_t end,
        rocprofvis_dm_hashed_timestamp_tag_t tag,
        rocprofvis_db_num_of_tracks_t num,
        rocprofvis_db_track_selection_t tracks,
        Future* future) {
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);
        ROCPROFVIS_ASSERT_MSG(num == 1, ERROR_UNSUPPORTED_FEATURE);

        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(*tracks);
        if(props->track_indentifiers.category == kRocProfVisDmPmcTrack)
        {
            return ReadTracePMCSlice(start, end, tag, tracks, true, true, future);
        }
        else
        {
            while (true)
            {
                std::string slice_query;
                slice_array_t slices;

                auto it = std::find_if(TrackPropertiesBegin(), TrackPropertiesEnd(), 
                    [tracks](std::unique_ptr<rocprofvis_dm_track_params_t>& params) {
                        return params.get()->track_indentifiers.track_id == *tracks;
                    });
                if (it == TrackPropertiesEnd()) break;
                
                slices[*tracks]=BindObject()->FuncAddSlice(BindObject()->trace_object, *tracks, start, end, tag);
                profiler_hub_future_handle_t ph_future = future->AddProfilerHubFuture();
                profiler_hub_result_t result =
                    profiler_hub_get_time_slice(
                        ph_future,
                        m_ph_trace,
                        it->get()->track_indentifiers.track_id,
                        slices[*tracks],
                        start,
                        end);
                if (result == kProfilerHubStatusSuccess)
                {
                    result = future->WaitAndDeleteProfilerHubFuture(ph_future);
                }
                if (result == kProfilerHubStatusSuccess)
                {
                    BindObject()->FuncCompleteSlice(slices[*tracks]);
                }
                else
                {
                    BindObject()->FuncRemoveSlice(BindObject()->trace_object, *tracks, slices[*tracks]);
                    break;
                }
                ShowProgress(100 - future->Progress(), "Time slice successfully loaded!", kRPVDbSuccess, future);
                return future->SetPromise(kRocProfVisDmResultSuccess);
            }
        }

        ShowProgress(0, "Not all tracks are loaded!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);    
    }

    rocprofvis_dm_result_t  ProfilerHub::ReadTracePMCSlice(
        rocprofvis_dm_timestamp_t start,
        rocprofvis_dm_timestamp_t end,
        rocprofvis_dm_hashed_timestamp_tag_t tag,
        rocprofvis_db_track_selection_t tracks,
        bool left_neighbor,
        bool right_neighbor,
        Future* future) {
        ROCPROFVIS_ASSERT_MSG_RETURN(future, ERROR_FUTURE_CANNOT_BE_NULL, kRocProfVisDmResultInvalidParameter);
        ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties, ERROR_TRACE_PROPERTIES_CANNOT_BE_NULL);
        ROCPROFVIS_ASSERT_MSG(BindObject()->trace_properties->metadata_loaded, ERROR_METADATA_IS_NOT_LOADED);

        rocprofvis_dm_track_params_t* props = TrackPropertiesAt(*tracks);
        while (true)
        {
            std::string slice_query;
            slice_array_t slices;

            auto it = std::find_if(TrackPropertiesBegin(), TrackPropertiesEnd(), 
                [tracks](std::unique_ptr<rocprofvis_dm_track_params_t>& params) {
                    return params.get()->track_indentifiers.track_id == *tracks;
                });
            if (it == TrackPropertiesEnd()) break;

            slices[*tracks]=BindObject()->FuncAddSlice(BindObject()->trace_object, *tracks, start, end, tag);
            profiler_hub_future_handle_t ph_future = future->AddProfilerHubFuture();
            profiler_hub_result_t result =
                profiler_hub_get_pmc_time_slice(
                    ph_future,
                    m_ph_trace,
                    it->get()->track_indentifiers.track_id,
                    slices[*tracks],
                    start,
                    end,
                    left_neighbor,
                    right_neighbor);
            future->DeleteProfilerHubFuture(ph_future);
            if (result == kProfilerHubStatusSuccess)
            {
                BindObject()->FuncCompleteSlice(slices[*tracks]);
            }
            else
            {
                BindObject()->FuncRemoveSlice(BindObject()->trace_object, *tracks, slices[*tracks]);
                break;
            }
            ShowProgress(100 - future->Progress(), "Time slice successfully loaded!", kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }

        ShowProgress(0, "Not all tracks are loaded!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed); 
    }

    rocprofvis_dm_result_t ProfilerHub::GetEventTablesAsync(
        std::vector<std::pair<DbInstance*, std::string>>& queries,
        Future* future,
        rocprofvis_dm_handle_t handle,
        RpvCallback callback) {
        for (auto query : queries)
        { 
            ParamDeserializer des(query.second);
            uint64_t start, end, node, agent;
            uint64_t* node_prt = &node;
            uint64_t* agent_ptr = &agent;
            std::vector<size_t> tracks;
            std::vector<std::string> filters;
            bool include_substring, include_category, partial_matching;
            if (!des.get("start", start))
                return kRocProfVisDmResultInvalidParameter;
            if (!des.get("end", end))
                return kRocProfVisDmResultInvalidParameter;
            if (!des.get("node", node))
                node_prt=nullptr;
            if (!des.get("agent", agent))
                agent_ptr=nullptr;
            bool tracks_selected = des.getArray("tracks", tracks);
            bool search_query = des.getArray("filters",  filters);

            if (!des.get("include_substring", include_substring))
                return kRocProfVisDmResultInvalidParameter;
            if (!des.get("include_category", include_category))
                return kRocProfVisDmResultInvalidParameter;
            if (!des.get("partial_matching", partial_matching))
                return kRocProfVisDmResultInvalidParameter;

            //todo : create mask and send it to pprofiler hub

            while (true)
            {
                profiler_hub_result_t result = kProfilerHubStatusUnknownError;
                if (search_query)
                {
                    if (!tracks_selected || tracks.size() == 0)
                        return kRocProfVisDmResultInvalidParameter;
                    if (filters.size() == 0)
                        return kRocProfVisDmResultInvalidParameter;
                    std::vector<const char*> filters_cc;
                    for (auto filter : filters)
                        filters_cc.push_back(filter.c_str());
                    std::vector<ProfilerHubTableHandler> table_handles;
                    uint32_t chunk_number = 0;
                    for (auto& operation : tracks)
                    {
                        profiler_hub_future_handle_t ph_future = future->AddProfilerHubFuture();
                        table_handles.push_back({ handle, chunk_number++, future, ph_future, (rocprofvis_dm_event_operation_t)operation, query.first});
                        result = profiler_hub_get_search_time_slice(
                            ph_future,
                            m_ph_trace,
                            query.first->GuidIndex(),
                            &table_handles.back(),
                            (profiler_hub_event_operation_t)operation,
                            start,
                            end,
                            filters_cc.size(),
                            filters_cc.data());
                        if (result != kProfilerHubStatusSuccess)
                            break;
                        result = future->WaitAndDeleteProfilerHubFuture(ph_future);
                    }
                    for (auto table_handle : table_handles)
                    {
                        result = future->WaitAndDeleteProfilerHubFuture(table_handle.ph_future);
                    }
                }
                else
                {
                    if (!tracks_selected || tracks.size() != 1)
                        return kRocProfVisDmResultInvalidParameter;
                    profiler_hub_future_handle_t ph_future = future->AddProfilerHubFuture();
                    ProfilerHubTableHandler table_handle = { handle, 0, future, ph_future, (rocprofvis_dm_event_operation_t)TrackPropertiesAt(tracks[0])->op, query.first};
                    result = profiler_hub_get_table_time_slice(
                        ph_future,
                        m_ph_trace,
                        &table_handle,
                        tracks[0],
                        start,
                        end);
                    if (result != kProfilerHubStatusSuccess)
                        break;
                    result = future->WaitAndDeleteProfilerHubFuture(ph_future);
                }
                if (result != kProfilerHubStatusSuccess)
                    break;
            }
            ShowProgress(100 - future->Progress(), "Table data successfully loaded!", kRPVDbSuccess, future);
            return future->SetPromise(kRocProfVisDmResultSuccess);
        }
        ShowProgress(0, "Failed to load table data!", kRPVDbError, future );
        return future->SetPromise(future->Interrupted() ? kRocProfVisDmResultDbAbort : kRocProfVisDmResultDbAccessFailed);     
    }

    rocprofvis_dm_result_t ProfilerHub::RemapStringId(
        uint64_t id,
        rocprofvis_db_string_type_t type,
        uint32_t node,
        uint64_t& result) {
        result = id;
        return kRocProfVisDmResultNotSupported;
    }

    void ProfilerHub::GetTrackIdentifierIndices(
        int column_index,
        char** azColName,
        rocprofvis_db_track_identifier_index_t&
        track_ids_indices) {

        std::string column_name = azColName[column_index];

        if (column_name == Builder::NODE_ID_SERVICE_NAME)
        {
            track_ids_indices.nid_index = column_index;
        }
        else if (column_name == Builder::AGENT_ID_SERVICE_NAME)
        {
            track_ids_indices.process_index = column_index;
        }
        else if (column_name == Builder::QUEUE_ID_SERVICE_NAME)
        {
            track_ids_indices.sub_process_index = column_index;
        }
        else if (column_name == Builder::STREAM_ID_SERVICE_NAME)
        {
            track_ids_indices.stream_index = column_index;
        }
        else if (column_name == Builder::PROCESS_ID_SERVICE_NAME)
        {
            track_ids_indices.process_index = column_index;
        }
        else if (column_name == Builder::THREAD_ID_SERVICE_NAME)
        {
            track_ids_indices.sub_process_index = column_index;
        }
        else if (column_name == Builder::COUNTER_ID_SERVICE_NAME)
        {
            track_ids_indices.is_pmc_identifier = true;
            track_ids_indices.sub_process_index = column_index;
        }
        else if (column_name == Builder::COUNTER_NAME_SERVICE_NAME)
        {
            track_ids_indices.is_pmc_identifier = true;
            track_ids_indices.is_rocpd_pmc = true;
            track_ids_indices.sub_process_index = column_index;
        }
        else if (column_name == Builder::PROCESS_ID_PUBLIC_NAME)
        {
            track_ids_indices.pid_index = column_index;
        }
    }

    bool ProfilerHub::FindTrack(
        rocprofvis_dm_track_category_t category, 
        uint64_t id_process, 
        uint64_t id_subprocess, 
        uint32_t db_instance, 
        uint32_t& out_track
    )
    {
        return TrackTracker()->FindTrack(category, id_process, id_subprocess, db_instance, out_track);
    }

    ProfilerHubCell::ProfilerHubCell(profiler_hub_string_t     name,
        profiler_hub_value_type_t type,
        ProfilerHubCellValue      value)
        : name(name)
        , type(type)
        , value(std::move(value))
    {
    }

    ProfilerHubTableRow::ProfilerHubTableRow( profiler_hub_table_handle_t table_handle, size_t num_columns):m_table_handle(table_handle) {
        m_cells.resize(num_columns, ProfilerHubCell(nullptr, kPprofilerHubDataTypeUndefined, std::monostate{}));
    }

    void ProfilerHubTableRow::SetCell(size_t index,
        profiler_hub_string_t     name,
        profiler_hub_value_type_t type,
        ProfilerHubCellValue      value) {
        if (index < m_cells.size()) {
            m_cells[index] = ProfilerHubCell(name, type, std::move(value));
        }
    }

    size_t ProfilerHubTableRow::NumCells() const {
        return m_cells.size();
    }

    const ProfilerHubCell* ProfilerHubTableRow::CellAt(size_t index) const {
        return index < m_cells.size() ? &m_cells[index] : nullptr;
    }

    ProfilerHubCell* ProfilerHubTableRow::CellAt(size_t index) {
        return index < m_cells.size() ? &m_cells[index] : nullptr;
    }

    profiler_hub_table_handle_t ProfilerHubTableRow::TableHandle() {
        return m_table_handle;
    }

    ProfilerHubTableRow* ProfilerHubTableRowArray::AddRow(profiler_hub_table_handle_t table_handle, size_t num_columns) {
        auto row = std::make_unique<ProfilerHubTableRow>(table_handle, num_columns);
        ProfilerHubTableRow* ptr = row.get();
        std::unique_lock lock(m_mutex);
        m_rows.push_back(std::move(row));
        return ptr;  
    }

    bool ProfilerHubTableRowArray::RowExists(ProfilerHubTableRow* row) {
        std::unique_lock lock(m_mutex);
        auto it = std::find_if(m_rows.begin(), m_rows.end(),
            [row](const std::unique_ptr<ProfilerHubTableRow>& p) {
                return p.get() == row;
            });
        if (it == m_rows.end()) return false;
        return true;
    }

    bool ProfilerHubTableRowArray::RemoveRow(ProfilerHubTableRow* row) {
        std::unique_lock lock(m_mutex);
        auto it = std::find_if(m_rows.begin(), m_rows.end(),
            [row](const std::unique_ptr<ProfilerHubTableRow>& p) {
                return p.get() == row;
            });
        if (it == m_rows.end()) return false;
        m_rows.erase(it); 
        return true;
    }

    size_t ProfilerHubTableRowArray::NumRows() const {
        std::shared_lock lock(m_mutex);
        return m_rows.size();
    }

    void ProfilerHubTableRowArray::Clear() {
        std::unique_lock lock(m_mutex);
        m_rows.clear(); 
    }

}  // namespace DataModel
}  // namespace RocProfVis
