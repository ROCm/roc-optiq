// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#ifdef USE_PROFILER_HUB

#include "rocprofvis_db_profiler_hub_client.h"
#include "rocprofvis_db_systems.h"
#include "profiler_hub_lib_interface.h"
#include "rocprofvis_db_table_processor.h"

namespace RocProfVis
{
namespace DataModel
{
typedef std::unordered_map<size_t, uint32_t> string_index_map_t;

using ProfilerHubCellValue = std::variant<std::monostate, size_t, std::string, double>;

struct ProfilerHubCell {
    profiler_hub_string_t     name;
    profiler_hub_value_type_t type;
    ProfilerHubCellValue      value;

    ProfilerHubCell(profiler_hub_string_t     name,
        profiler_hub_value_type_t type,
        ProfilerHubCellValue      value = std::monostate{});
};

class ProfilerHubTableRow {
public:
    explicit ProfilerHubTableRow( profiler_hub_table_handle_t table_handle, size_t num_columns);

    void SetCell(size_t index,
        profiler_hub_string_t     name,
        profiler_hub_value_type_t type,
        ProfilerHubCellValue      value);

    size_t                 NumCells() const;
    const ProfilerHubCell* CellAt(size_t index) const;
    ProfilerHubCell*       CellAt(size_t index);
    profiler_hub_table_handle_t TableHandle();

private:
    std::vector<ProfilerHubCell> m_cells;
    profiler_hub_table_handle_t m_table_handle;
};

class ProfilerHubTableRowArray {
public:
    ProfilerHubTableRow* AddRow(profiler_hub_table_handle_t table_handle, size_t num_columns);

    bool RemoveRow(ProfilerHubTableRow* row);

    bool RowExists(ProfilerHubTableRow* row);

    size_t NumRows() const;

    void Clear();

private:
    std::vector<std::unique_ptr<ProfilerHubTableRow>> m_rows;
    mutable std::shared_mutex                         m_mutex;
};

typedef struct ProfilerHubTableHandler
{
    profiler_hub_table_handle_t handle;
    uint32_t chunk;
    Future* dm_future;
    profiler_hub_future_handle_t ph_future;
    rocprofvis_dm_event_operation_t op;
    DbInstance* instance;
} ProfilerHubTableHandler;

class ProfilerHub : public SystemDatabase
{
public:
    // Database constructor
    // @param path - full path to database file
    ProfilerHub(rocprofvis_db_filename_t path) :
        SystemDatabase(path), m_ph_trace(nullptr),
        m_table_processor{ TableProcessor(this),TableProcessor(this),TableProcessor(this) } {
    }
    // ProfilerHub destructor, must be defined as virtual to free resources of derived classes 
    virtual ~ProfilerHub() { Close(); }
    // Method to open sqlite database
    // @return status of operation
    rocprofvis_dm_result_t Open() override;
    // Method to close sqlite database
    // @return status of operation
    rocprofvis_dm_result_t Close() override;
    void  InterruptQuery(void* connection) override {};

protected:

    rocprofvis_dm_result_t  ExecuteQuery(
        rocprofvis_dm_charptr_t query,
        rocprofvis_dm_charptr_t description,
        Future* future) override;

    rocprofvis_dm_result_t ExportTableCSV(
        rocprofvis_dm_charptr_t query,
        rocprofvis_dm_charptr_t file_path,
        Future* future) override;

    rocprofvis_dm_result_t BuildTableQuery(
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
        rocprofvis_dm_string_t& query) override;

    rocprofvis_dm_result_t BuildEventSearchQuery(
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
        rocprofvis_dm_string_t& query) override;

    rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t start,
        rocprofvis_dm_timestamp_t end,
        rocprofvis_dm_charptr_t new_db_path,
        Future* future) override;

    rocprofvis_dm_result_t  ReadTraceMetadata(
        Future* object) override;

    rocprofvis_dm_result_t  ReadFlowTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* object) override;

    rocprofvis_dm_result_t  ReadStackTraceInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* object) override;

    rocprofvis_dm_result_t  ReadExtEventInfo(
        rocprofvis_dm_event_id_t event_id,
        Future* object) override;

    rocprofvis_dm_result_t  ReadTraceSlice(
        rocprofvis_dm_timestamp_t start,
        rocprofvis_dm_timestamp_t end,
        rocprofvis_dm_hashed_timestamp_tag_t tag,
        rocprofvis_db_num_of_tracks_t num,
        rocprofvis_db_track_selection_t tracks,
        Future* object) override;

    rocprofvis_dm_result_t  ReadTracePMCSlice(
        rocprofvis_dm_timestamp_t start,
        rocprofvis_dm_timestamp_t end,
        rocprofvis_dm_hashed_timestamp_tag_t tag,
        rocprofvis_db_track_selection_t track,
        bool left_neighbor,
        bool right_neighbor,
        Future* object) override;


    rocprofvis_dm_result_t GetEventTablesAsync(
        std::vector<std::pair<DbInstance*, std::string>>& queries,
        Future* parent,
        rocprofvis_dm_handle_t handle,
        RpvCallback callback) override;

    rocprofvis_dm_result_t RemapStringId(uint64_t id,
        rocprofvis_db_string_type_t type,
        uint32_t node,
        uint64_t& result) override;

    void GetTrackIdentifierIndices(
        int column_index,
        char** azColName,
        rocprofvis_db_track_identifier_index_t&
        track_ids_indices) override;

    bool FindTrack(
        rocprofvis_dm_track_category_t category,
        uint64_t id_process,
        uint64_t id_subprocess,
        uint32_t db_instance,
        uint32_t& out_track) override;

    StringTable& DbFiles() { return m_db_files; }

    rocprofvis_dm_result_t ProcessTrack(rocprofvis_dm_track_params_t& track_params);

    //--------------------------------------Table accessors-----------------------------------------------------------------
    std::string TableColumnText(void* func, void* handle, char** azColName, int index) override;
    int TableColumnInt(void* func, void* handle, char** azColName, int index) override;
    int64_t TableColumnInt64(void* func, void* handle, char** azColName, int index) override;
    double TableColumnDouble(void* func, void* handle, char** azColName, int index) override;

private:

    static rocprofvis_dm_result_t ConvertProfilerHubResult(profiler_hub_result_t ph_result);
    static rocprofvis_dm_event_operation_t ConvertProfilerHubEventType(profiler_hub::reader_types::event_type_t event_type);
    void IdentificatorNamesUpdate(rocprofvis_dm_track_params_t& track_params);

    profiler_hub_trace_handle_t m_ph_trace;
    TableProcessor m_table_processor[kRPVTableDataTypesNum];
    StringTable    m_db_files;
    string_index_map_t m_string_index_map; // id to index
    std::mutex m_mutex;
    ProfilerHubTableRowArray m_table_rows_cache;

            inline static const rocprofvis_event_data_category_map_t
            s_rocprof_categorized_data = {
                {
                    kRocProfVisDmOperationNoOp,
                    {
                        { "id", kRocProfVisEventEssentialDataId },
                        { "category", kRocProfVisEventEssentialDataCategory },
                        { "name", kRocProfVisEventEssentialDataName },
                        { "start", kRocProfVisEventEssentialDataStart },
                        { "end", kRocProfVisEventEssentialDataEnd },
                        { "duration", kRocProfVisEventEssentialDataDuration },
                        { "nid", kRocProfVisEventEssentialDataNode },
                        { "pid", kRocProfVisEventEssentialDataProcess },
                        { "tid", kRocProfVisEventEssentialDataThread },
                        { "queue_name", kRocProfVisEventEssentialDataQueue },
                        { "stream_name", kRocProfVisEventEssentialDataStream },
                        { "stack_id", kRocProfVisEventEssentialDataInternal },
                        { "parent_stack_id", kRocProfVisEventEssentialDataInternal },
                        { "corr_id", kRocProfVisEventEssentialDataInternal },
                        { "stream_id", kRocProfVisEventEssentialDataInternal },
                        { "queue_id", kRocProfVisEventEssentialDataInternal },
                    },
                },
                {
                    kRocProfVisDmOperationDispatch,
                    {
                        { "agent_type", kRocProfVisEventEssentialDataAgentType },
                        { "agent_type_index", kRocProfVisEventEssentialDataAgentIndex },
                    },
                },
                {
                    kRocProfVisDmOperationMemoryAllocate,
                    {
                        { "agent_type", kRocProfVisEventEssentialDataAgentType },
                        { "agent_type_index", kRocProfVisEventEssentialDataAgentIndex },
                        { "type", kRocProfVisEventEssentialDataName },
                    },
                },
                {
                    kRocProfVisDmOperationMemoryCopy,
                    {
                        { "dst_agent_type", kRocProfVisEventEssentialDataAgentType },
                        { "dst_agent_type_index", kRocProfVisEventEssentialDataAgentIndex },
                    } 
                }
            };

    friend class ProfilerHubClientMethods;
};


}  // namespace DataModel
}  // namespace RocProfVis

#endif