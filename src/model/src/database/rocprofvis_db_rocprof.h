// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_db_profile.h"
#include "rocprofvis_db_query_factory.h"

#include <profiler-hub/reader.hpp>
#include <profiler-hub/storage.hpp>

namespace RocProfVis
{
namespace DataModel
{

typedef struct rocprofvis_db_string_id_hash_t
{
    size_t operator()(const rocprofvis_db_string_id_t& s) const noexcept
    {
        size_t h1 = std::hash<uint64_t>{}(s.m_string_id);
        size_t h2 = std::hash<uint32_t>{}(s.m_guid_id);
        size_t h3 = std::hash<rocprofvis_db_string_type_t>{}(s.m_string_type);

        size_t seed = h1;
        seed ^= h2 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        seed ^= h3 + 0x9e3779b97f4a7c15ULL + (seed << 6) + (seed >> 2);
        return seed;
    }
} rocprofvis_db_string_id_hash_t;

typedef enum rocprofvis_db_memalloc_type_t
: uint8_t
{
    kRPVMemActivityAlloc,
    kRPVMemActivityFree,
    kRPVMemActivityRealloc,
    kRPVMemActivityReclaim,
    kRPVMemActivityNumTypes
} rocprofvis_db_memalloc_type_t;

typedef enum rocprofvis_db_memalloc_level_t
: uint8_t
{
    kRPVMemLevelReal,
    kRPVMemLevelVirtual,
    kRPVMemLevelScratch,
    kRPVMemLevelNumLevels
} rocprofvis_db_memalloc_level_t;

typedef struct rocprofvis_db_memalloc_activity_t
{
    uint64_t                       start;
    uint64_t                       end;
    uint64_t                       address;
    uint64_t                       size;
    uint32_t                       id;
    uint32_t                       pid;
    uint16_t                       stream_id;
    uint8_t                        agent_id;
    uint8_t                        queue_id;
    uint32_t                       track_id;
    rocprofvis_db_memalloc_type_t  type;
    rocprofvis_db_memalloc_level_t level;
} rocprofvis_db_memalloc_activity_t;

class RocprofDatabase : public ProfileDatabase
{
    // type of map array for string indexes remapping
    typedef std::unordered_map<rocprofvis_db_string_id_t, rocprofvis_dm_index_t,
                               rocprofvis_db_string_id_hash_t>
        string_index_map_t;
    typedef std::unordered_map<rocprofvis_dm_index_t,
                               std::vector<rocprofvis_db_string_id_t>>
                                                      string_id_map_t;
    typedef std::unordered_map<std::string, uint32_t> string_map_t;
    typedef std::map<uint32_t, std::vector<rocprofvis_db_memalloc_activity_t>>
        memalloc_activity_t;
    typedef std::map<uint32_t, std::unordered_map<uint32_t, uint32_t>>
        mem_free_stream_to_agent_t;

    // TASK 037: Task 028 sealed the (event_type, row_id) pair inside the opaque
    // event_id_t, and tasks 032/033 removed flow_edge_t's *_type/*_opaque_id fields. The flow
    // index is now keyed directly on the opaque event_id_t (hashable + ordered), which
    // uniquely names an event across all per-type tables with no companion type tag. The
    // endpoint's event_type_t (needed for the leg-type filter and op mapping that the
    // reader cannot decode from the opaque handle) is carried in the payload instead of
    // the key.
    struct ReaderFlowPayload
    {
        rocprofvis_dm_timestamp_t start;
        rocprofvis_dm_timestamp_t end;
        int                       level;
        uint64_t                  col4;  // FindTrack id_process (pid|agent)
        uint64_t                  col5;  // FindTrack id_subproc (tid|queue)
        profiler_hub::reader_types::event_type_t type;  // endpoint type (was in the key)
        std::string                              category;
        std::string                              symbol;
    };

    typedef std::map<profiler_hub::reader_types::event_id_t,
                     std::set<profiler_hub::reader_types::event_id_t>>
        reader_flow_topology_t;
    typedef std::map<profiler_hub::reader_types::event_id_t, ReaderFlowPayload>
        reader_flow_payload_t;

    // TASK 037: Reader-backed event registry (Option A, owner-approved 2026-07-22). The
    // 52-bit UI-handle event_id slot can no longer carry a decodable row id (028
    // opacity), so it carries an Optiq-minted surrogate: the index into
    // m_event_registry[guid]. This round-trips surrogate <-> opaque event_id_t and
    // caches, per event, the two nesting levels and two swimlane track ids the detail
    // panel's "Essential Info" needs
    // (trackId/levelForTrack/streamTrackId/levelForStreamTrack) — replacing the
    // numeric-id- keyed roc_optiq_event_levels_* tables + FindTrack-at-click for reader
    // tracks. Built eagerly once per shard (BuildReaderEventRegistry) by scanning
    // get_tracks() + get_interval_track() over every interval track type, so both
    // levels are always available regardless of which slices are loaded.
    struct ReaderEventInfo
    {
        profiler_hub::reader_types::event_id_t id;
        uint32_t                               home_track_id    = INVALID_INDEX;
        uint32_t                               stream_track_id  = INVALID_INDEX;
        int                                    level_for_queue  = -1;
        int                                    level_for_stream = -1;
        // TASK 037: the event's operation type, taken from its native/home track during
        // the eager scan (gpu_queue->Dispatch, dma->MemoryCopy, memory->MemoryAllocate).
        // The reader seals the type inside the opaque event_id_t (028), so a stream track
        // — which mixes those three types in one lane — cannot tell them apart at mint
        // time. Recording the home-track op here lets the slice-mint stamp each stream
        // event's UI handle with the correct event_op. NoOp when no native track set it
        // (e.g. regions, which never appear on stream tracks and use the track-level op
        // instead).
        rocprofvis_dm_event_operation_t home_op = kRocProfVisDmOperationNoOp;
    };
    typedef std::unordered_map<profiler_hub::reader_types::event_id_t, uint64_t>
        reader_event_surrogate_t;

public:
    RocprofDatabase(rocprofvis_db_filename_t path)
    : ProfileDatabase(path)
    , m_query_factory(this)
    , m_metadata_version_control(this)
    {
        CreateDbNode(path);
    };
    RocprofDatabase(rocprofvis_db_filename_t  path,
                    std::vector<std::string>& multinode_files)
    : ProfileDatabase(path)
    , m_query_factory(this)
    , m_metadata_version_control(this)
    {
        CreateDbNodes(multinode_files);
    };

    // class destructor, not really required, unless declared as virtual
    ~RocprofDatabase() override {};
    // worker method to read trace metadata
    // @param object - future object providing asynchronous execution mechanism
    // @return status of operation
    rocprofvis_dm_result_t ReadTraceMetadata(Future* object) override;

    // worker method to read flow trace info
    // @param event_id - 60-bit event id and 4-bit operation type
    // @param object - future object providing asynchronous execution mechanism
    // @return status of operation
    rocprofvis_dm_result_t ReadFlowTraceInfo(rocprofvis_dm_event_id_t event_id,
                                             Future*                  object) override;
    // worker method to read stack trace info
    // @param event_id - 60-bit event id and 4-bit operation type
    // @param object - future object providing asynchronous execution mechanism
    // @return status of operation
    rocprofvis_dm_result_t ReadStackTraceInfo(rocprofvis_dm_event_id_t event_id,
                                              Future*                  object) override;
    // worker method to read extended info
    // @param event_id - 60-bit event id and 4-bit operation type
    // @param object - future object providing asynchronous execution mechanism
    // @return status of operation
    rocprofvis_dm_result_t ReadExtEventInfo(rocprofvis_dm_event_id_t event_id,
                                            Future*                  object) override;

    rocprofvis_dm_result_t SaveTrimmedData(rocprofvis_dm_timestamp_t start,
                                           rocprofvis_dm_timestamp_t end,
                                           rocprofvis_dm_charptr_t   new_db_path,
                                           Future*                   future) override;

    rocprofvis_dm_result_t BuildTableStringIdFilter(
        rocprofvis_dm_num_string_table_filters_t num_string_table_filters,
        rocprofvis_dm_string_table_filters_t     string_table_filters,
        table_string_id_filter_map_t&            filters) override;

    rocprofvis_dm_string_t GetEventOperationQuery(
        const rocprofvis_dm_event_operation_t operation) override;

    rocprofvis_dm_result_t StringIndexToId(
        rocprofvis_dm_index_t index, std::vector<rocprofvis_db_string_id_t>& id) override;

    rocprofvis_dm_result_t RemapStringId(uint64_t id, rocprofvis_db_string_type_t type,
                                         uint32_t node, uint64_t& result) override;

private:
    // sqlite3_exec callback to process string list query and add string object to Trace
    // container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallbackCaptureMemoryActivity(void* data, int argc, sqlite3_stmt* stmt,
                                             char** azColName);
    // sqlite3_exec callback to process string list query and add string object to Trace
    // container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallBackAddString(void* data, int argc, sqlite3_stmt* stmt,
                                 char** azColName);
    // sqlite3_exec callback to detect nodes and table names in the database
    // object to StackTrace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallbackNodeEnumeration(void* data, int argc, sqlite3_stmt* stmt,
                                       char** azColName);
    // sqlite3_exec callback to parse metadata table of new schema rocprof database
    // object to StackTrace container
    // @param data - pointer to callback caller argument
    // @param argc - number of columns in the query
    // @param argv - pointer to row values
    // @param azColName - pointer to column names
    // @return SQLITE_OK if successful
    static int CallbackParseMetadata(void* data, int argc, sqlite3_stmt* stmt,
                                     char** azColName);
    // method to remap string IDs. Main reason for remapping is having strings and kernel
    // symbol names in one array
    // @param record - event record structure
    // @return status of operation
    rocprofvis_dm_result_t RemapStringIds(rocprofvis_db_record_data_t& record) override;
    rocprofvis_dm_result_t RemapStringIds(rocprofvis_db_flow_data_t& record) override;

    int ProcessTrack(rocprofvis_dm_track_params_t& track_params,
                     rocprofvis_dm_charptr_t*      newqueries) override;

    // Open (once, lazily) the profiler-hub reader for a shard, keyed by GuidIndex.
    // Returns nullptr if the reader cannot be constructed for this shard.
    profiler_hub::reader_t* GetReader(DbInstance* db_instance);

    // Translate a reader cpu_thread track into an Optiq track_params, mirroring the
    // fields CallBackLoadTrack derives from SQL (identity/category/op/tags/stats) and
    // stashing the reader track id so slice/detail reads route back to the reader.
    void ReaderTrackInfoToTrackParams(
        const profiler_hub::reader_types::track_info_t& info, DbInstance* db_instance,
        rocprofvis_dm_track_params_t& track_params);

    // Reader-backed cpu_thread discovery: replaces the region-main/region-sample SQL
    // discovery blocks with get_tracks() filtered to cpu_thread, per shard.
    rocprofvis_dm_result_t AddReaderRegionTracks(Future* future);

    // Reader-backed gpu_queue and stream discovery: replaces the kernel-dispatch SQL
    // discovery blocks with get_tracks() filtered to gpu_queue and stream, per shard.
    rocprofvis_dm_result_t AddReaderGpuQueueAndStreamTracks(Future* future);

    // Adapt a reader gpu_queue track into Optiq track_params (identity slots, category,
    // op). Mirrors ReaderTrackInfoToTrackParams for cpu_thread.
    void ReaderGpuQueueTrackToTrackParams(
        const profiler_hub::reader_types::track_info_t& info, DbInstance* db_instance,
        rocprofvis_dm_track_params_t& track_params);

    // Adapt a reader stream track into Optiq track_params (identity slots, category).
    // op is per-event (from op_kind), not fixed at track level.
    void ReaderStreamTrackToTrackParams(
        const profiler_hub::reader_types::track_info_t& info, DbInstance* db_instance,
        rocprofvis_dm_track_params_t& track_params);

    // Reader-backed memory-alloc discovery: replaces the standalone memory-alloc SQL
    // block with get_tracks() filtered to track_type_t::memory, per shard.
    rocprofvis_dm_result_t AddReaderMemoryTracks(Future* future);

    // Adapt a reader memory track into Optiq track_params (identity slots, category, op).
    void ReaderMemoryTrackToTrackParams(
        const profiler_hub::reader_types::track_info_t& info, DbInstance* db_instance,
        rocprofvis_dm_track_params_t& track_params);

    // Reader-backed memory-copy discovery: replaces the standalone (queue-keyed)
    // memory-copy SQL block with get_tracks() filtered to track_type_t::dma, per
    // shard.
    rocprofvis_dm_result_t AddReaderDmaTracks(Future* future);

    // Adapt a reader dma (memory-copy) track into Optiq track_params (identity slots,
    // category, op). Keyed by destination agent (agent_info from dst_agent_id).
    void ReaderDmaTrackToTrackParams(const profiler_hub::reader_types::track_info_t& info,
                                     DbInstance*                   db_instance,
                                     rocprofvis_dm_track_params_t& track_params);

    // Reader-backed counter (scalar/PMC) discovery: replaces the sample-based "SMI
    // performance counters" SQL block with get_tracks() filtered to
    // track_type_t::counter, per shard. Kernel-dispatch PMC and memory-activity blocks
    // stay on SQL (the reader has no track type for either).
    rocprofvis_dm_result_t AddReaderCounterTracks(Future* future);

    // Adapt a reader counter track into Optiq track_params (identity slots, category,
    // op). COUNTER slot keyed by the real pmc_id (pmc_info->pmc_id), so ProcessTrack's
    // PMC name/panel lookups behave identically to the SQL path.
    void ReaderCounterTrackToTrackParams(
        const profiler_hub::reader_types::track_info_t& info, DbInstance* db_instance,
        rocprofvis_dm_track_params_t& track_params);

    // Reader-backed slice load for a cpu_thread track (routed from ReadTraceSlice on a
    // non-sentinel reader_track_id). Emits interval events with Optiq's exact overlap
    // window semantics.
    rocprofvis_dm_result_t ReadReaderTraceSlice(rocprofvis_dm_timestamp_t     start,
                                                rocprofvis_dm_timestamp_t     end,
                                                rocprofvis_dm_track_params_t* props,
                                                slice_array_t&                slices,
                                                Future* future) override;

    // Reader-backed per-track histogram recompute for a cpu_thread track. Reproduces the
    // exact bucket-overlap math of GetHistogramQuerySuffix over the reader's interval
    // events, since the SQL histogram passes skip reader tracks. Invoked from the base
    // BuildHistogram virtual hook before gap-fill.
    void BuildReaderTrackHistogram(rocprofvis_dm_track_params_t* props,
                                   uint64_t                      bucket_size) override;

    // Intern a reader-supplied string (interval category / display name) into the trace
    // string table, deduping via m_string_map so repeated slice loads reuse one index.
    // The SQL path remaps DB string ids; reader records carry the resolved string
    // instead.
    rocprofvis_dm_id_t InternReaderString(const std::string& str);

    // Build (once per db-instance, cached for the db lifetime) the two reader-backed flow
    // indexes: a TOPOLOGY index (undirected stack-clique adjacency, keyed on the opaque
    // event_id_t) from a single get_flows() call, and a PAYLOAD index (per-endpoint
    // identity/timing/level/type/strings) from get_tracks()+get_interval_track() over
    // the four native single-table track types. Builds the event registry first (for
    // surrogate minting). Idempotent; guarded by m_flow_index_mutex.
    rocprofvis_dm_result_t BuildReaderFlowIndexes(DbInstance* db_instance);

    // Emit one flow endpoint into the flow-trace object, mirroring CallbackAddFlowTrace:
    // resolve the endpoint track via FindTrack (skip if not found), fill the flow record
    // (event_id slot = the endpoint's surrogate), intern reader strings, and call
    // FuncAddFlow.
    void EmitReaderFlow(rocprofvis_dm_flowtrace_t flowtrace, uint32_t guid_index,
                        const profiler_hub::reader_types::event_id_t& endpoint,
                        const ReaderFlowPayload&                      payload);

    // TASK 037: Build (once per shard, lazily, cached for the db lifetime) the reader
    // event registry: scan get_tracks() + get_interval_track() over every interval
    // track type, minting a stable surrogate per opaque event_id_t and recording its
    // home/stream Optiq track ids and per-track nesting levels. Prerequisite for every
    // reader detail/stack/ flow/slice path that round-trips the UI handle. Idempotent;
    // guarded by m_event_registry_mutex.
    rocprofvis_dm_result_t BuildReaderEventRegistry(DbInstance* db_instance);

    // TASK 037: Get (minting if new) the 52-bit UI-handle surrogate for an opaque
    // event_id_t within a shard. Requires BuildReaderEventRegistry(db_instance) to have
    // run. Returns INVALID_INDEX if the handle is unknown to the registry.
    uint64_t ReaderSurrogateFor(uint32_t                                      guid_index,
                                const profiler_hub::reader_types::event_id_t& id);

    // TASK 037: Resolve a UI-handle surrogate back to its opaque event_id_t and the
    // cached per-event nav context (home/stream track ids + levels). Requires
    // BuildReaderEventRegistry(db_instance) to have run. Returns nullptr if the surrogate
    // is out of range for the shard.
    const ReaderEventInfo* ReaderEventInfoFor(uint32_t guid_index, uint64_t surrogate);

protected:
    const rocprofvis_event_data_category_map_t* GetCategoryEnumMap() override
    {
        return &s_rocprof_categorized_data;
    };
    const rocprofvis_null_data_exceptions_int* GetNullDataExceptionsInt() override
    {
        return &s_null_data_exceptions_int;
    }
    const rocprofvis_null_data_exceptions_string* GetNullDataExceptionsString() override
    {
        return &s_null_data_exceptions_string;
    }
    const rocprofvis_null_data_exceptions_skip* GetNullDataExceptionsSkip() override
    {
        return &s_null_data_exceptions_skip;
    }
    rocprofvis_dm_track_category_t GetRegionTrackCategory() override
    {
        return kRocProfVisDmRegionMainTrack;
    }
    MetadataVersionControl* GetMetadataVersionControl() override
    {
        return &m_metadata_version_control;
    };

    rocprofvis_dm_result_t Cleanup(Future* future, bool rebuild) override
    {
        return m_metadata_version_control.CleanupDatabase(future, rebuild);
    };

private:
    rocprofvis_dm_result_t CreateIndexes();
    rocprofvis_dm_result_t LoadInformationTables(Future* future);
    rocprofvis_dm_result_t PopulateStreamToHardwareFlowProperties(
        uint32_t stream_track_index, uint32_t db_instance);
    rocprofvis_dm_result_t PopulateUnusedAgents(uint32_t db_instance);
    rocprofvis_dm_result_t CreateMemoryActivityTable(Future* future);
    rocprofvis_dm_result_t CreateAgentFriendlyMemoryAllocationTable(Future* future);
    rocprofvis_dm_result_t LoadMemoryActivityData(Future* future);
    rocprofvis_dm_result_t GenerateInterdependencyTables(Future* future);
    rocprofvis_dm_result_t RunCacheQueriesAsync(
        Future* future, std::vector<std::pair<std::string, std::string>>& info_table_lis);

protected:
    uint64_t    GetMemoryActivityTableSchemaHash();
    std::string GetLevelSchemaHashStr();

private:
    QueryFactory m_query_factory;
    std::string  m_db_version;
    // One profiler-hub reader per shard, indexed by GuidIndex; opened lazily and
    // kept alive for the database lifetime so slice/detail reads can reuse it.
    std::vector<std::unique_ptr<profiler_hub::reader_t>> m_readers;
    std::mutex                                           m_readers_mutex;
    // Reader-backed flow indexes, one pair per shard (indexed by GuidIndex), built
    // eagerly once and cached for the database lifetime alongside m_readers.
    std::vector<reader_flow_topology_t> m_flow_topology;
    std::vector<reader_flow_payload_t>  m_flow_payload;
    std::vector<bool>                   m_flow_index_built;
    std::mutex                          m_flow_index_mutex;
    // TASK 037: reader event registry, one per shard (indexed by GuidIndex). The vector's
    // index IS the 52-bit UI-handle surrogate; the map dedupes so a given opaque
    // event_id_t always mints the same surrogate. Built eagerly once and cached for the
    // db lifetime.
    std::vector<std::vector<ReaderEventInfo>> m_event_registry;
    std::vector<reader_event_surrogate_t>     m_event_surrogate;
    std::vector<bool>                         m_event_registry_built;
    std::mutex                                m_event_registry_mutex;
    // map array for string indexes remapping. Main reason for remapping is older rocpd
    // schema keeps duplicated symbols, one per GPU
    string_index_map_t            m_string_index_map;  // id to index
    string_id_map_t               m_string_id_map;     // index to id
    string_map_t                  m_string_map;        // temporary map to reuse string
    memalloc_activity_t           m_memalloc_activity;
    mem_free_stream_to_agent_t    m_memfree_stream_to_agent;
    RocprofMetadataVersionControl m_metadata_version_control;

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
            { kRocProfVisDmOperationMemoryCopy,
              {
                  { "dst_agent_type", kRocProfVisEventEssentialDataAgentType },
                  { "dst_agent_type_index", kRocProfVisEventEssentialDataAgentIndex },
              } }
        };

    inline static const rocprofvis_null_data_exceptions_skip
        s_null_data_exceptions_skip = { { (void*) &CallBackAddTrack,
                                          {
                                              Builder::AGENT_ID_SERVICE_NAME,
                                              Builder::QUEUE_ID_SERVICE_NAME,
                                          } },
                                        { (void*) &CallbackGetTrackProperties,
                                          { "MIN(startTs)", "MAX(endTs)" } } };

    inline static const rocprofvis_null_data_exceptions_int s_null_data_exceptions_int = {
        {

        }
    };
    inline static const rocprofvis_null_data_exceptions_string
        s_null_data_exceptions_string = {
            {
                (void*) &CallbackCacheTable,
                { { "name", "N/A" }, { "start", "0" }, { "end", "0" } },
            },
            {
                (void*) &CallbackRunQuery,
                { { "name", "N/A" }, { "start", "0" }, { "end", "0" } },
            }

        };
    // Define the SQL schema for the memory activity table. Each entry corresponds to a
    // column in the table that describes a memory allocation/free event and its metadata.
    inline static SQLInsertParams s_mem_activity_schema_params = {
        { "id", "INTEGER PRIMARY KEY" },
        { "nid", "INTEGER" },
        { "pid", "INTEGER" },
        { "agent_id", "INTEGER" },
        { "queue_id", "INTEGER" },
        { "stream_id", "INTEGER" },
        { "pmc_id", "INTEGER" },
        { "type", "TEXT" },
        { "level", "TEXT" },
        { "start", "INTEGER" },
        { "end", "INTEGER" },
        { "address", "INTEGER" },
        { "size", "INTEGER" },
        { "track_id", "INTEGER" },
    };

    inline static SQLInsertParams s_level_schema_params = {
        { "eid", "INTEGER PRIMARY KEY" },
        { "level", "INTEGER" },
        { "level_for_stream", "INTEGER" },
        { "parent_id", "INTEGER" }
    };

    friend class RocprofMetadataVersionControl;
};

}  // namespace DataModel
}  // namespace RocProfVis