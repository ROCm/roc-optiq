# DATABASE.md - ROCm Optiq Model / Database Layer Guide

This document is the database/model-layer companion to
[`.agents/AGENTS.md`](./AGENTS.md) and
[`.agents/CONTROLLER.md`](./CONTROLLER.md). The main guide gives a
high-level pass over `src/model/`. **This file is the deep dive for
`src/model/`** - the SQLite-backed data model that ingests trace files
and serves typed records to the controller.

When humans and `CODING.md` disagree with this file, `CODING.md` wins.
When this file disagrees with the source under `src/model/`, the source
wins; please update this file in the same change.

---

## Table of Contents

1. Identity & Scope
2. Public C ABI Surface (`src/model/inc/`)
3. Internal Architecture & Threading Model
4. Database Layer (`src/model/src/database/`)
5. Data Model Layer (`src/model/src/datamodel/`)
6. Database <-> Trace Binding Protocol
7. Trace File Formats & Adapters (rocpd / rocprof / compute)
8. Query Pipeline (Builder, QueryFactory, ComputeQueryFactory)
9. PackedTable, Expression Filter, Aggregation
10. Metadata Versioning & Cleanup
11. CFFI / Python Bindings
12. Coding Conventions Specific to the Model Layer
13. Reuse Catalog (model edition)
14. Common Pitfalls
15. Testing
16. Quick Reference Index

---

## 1. Identity & Scope

- **Library target:** `datamodel` (CMake, see
  `src/model/CMakeLists.txt`). Test executable: `datamodel-test`.
- **Public ABI:** C / pure-C-compatible headers in `src/model/inc/`.
  Implementation is C++17 in `src/model/src/`.
- **Owner of:** opening trace databases, reading metadata + slices
  asynchronously, building SQL queries (system + compute),
  caching tables, packed in-memory row storage, the in-memory
  `Trace` -> `Track` -> `TrackSlice` -> `EventRecord/PmcRecord`
  hierarchy, the topology tree, ext-data / flow-trace / stack-trace
  records, and the metadata-versioning + cleanup machinery.
- **Does NOT own:** the main thread / UI (`src/view/`), the C ABI
  exposed to the View (`src/controller/`), GLFW / ImGui rendering.
- **Linked by:** `src/controller/`. The controller is the normal
  consumer of `src/model/` headers (the View must never include
  `src/model/` directly; see the architecture rules in
  `.agents/AGENTS.md`).
- **CFFI / Python:** `src/model/python/rocprofvis_cffi_build.py`
  builds a Python wrapper that calls `rocprofvis_dm_*` and
  `rocprofvis_db_*` directly without going through the controller.
  The view layer never depends on this path.
- **Build flags worth knowing:**
  - `CPPBUILD` - always defined when built via this CMake (`src/model/CMakeLists.txt`).
  - `TEST` - opt-in build flag. When set, headers compile with
    additional `GetPropertySymbol` debug entry points and the
    library uses a debug-friendly `-O2 -DDEBUG` build.
  - `LOCK_WAITED_TOO_LONG_WARNING_TIME_LIMIT_US` and
    `LOCK_HELD_TOO_LONG_WARNING_TIME_LIMIT_US` (`rocprofvis_dm_base.h`)
    enable lock-contention diagnostics via `spdlog::debug`.

The model layer must never include View headers. It mostly stands
below the controller, but it intentionally shares a small public enum
surface with it: `src/model/inc/rocprofvis_shared_types.h` defines
controller-facing topology property enums, and
`src/model/src/common/rocprofvis_common_types.h` includes the public
`rocprofvis_controller_enums.h` header. Do not include controller
`src/` implementation headers from the model. The model can depend on
`src/core/` (`rocprofvis_core_assert.h`,
`rocprofvis_core_profile.h`), `thirdparty/sqlite3`, `thirdparty/spdlog`,
`thirdparty/jsoncpp`, `thirdparty/yaml-cpp`, optionally TBB.

## 2. Public C ABI Surface (`src/model/inc/`)

Three headers form the entire public contract:

- `rocprofvis_interface.h` - all functions. Keep this header
  compatible with `src/model/python/rocprofvis_cffi_build.py`: avoid
  conditional compilation and re-run the CFFI build after signature
  changes. Note that the existing header already contains a C++-style
  default argument on `rocprofvis_db_future_alloc`; preserve the
  current build script's expectations rather than adding new syntax
  casually.
- `rocprofvis_interface_types.h` - all opaque handle typedefs and
  every property / status / category enum the View / controller need.
  Same CFFI constraint applies.
- `rocprofvis_c_interface.h` - C/C++-only convenience header that
  pulls in `rocprofvis_interface.h` inside `extern "C"` and adds the
  five property-getter overloads that return status by value. CFFI
  uses the bare interface header, C++ callers use this one.
- `rocprofvis_shared_types.h` - shared property bank enums for
  topology objects (`rocprofvis_controller_node_properties_t`,
  `rocprofvis_controller_processor_properties_t`, etc.). These are
  consumed both by the controller's topology classes and by the
  model's `TopologyNode` tree.

### 2.1 Handle types

Every model-layer object is opaque from the caller's perspective:

```c
typedef void* rocprofvis_dm_handle_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_trace_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_database_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_track_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_slice_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_flowtrace_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_stacktrace_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_extdata_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_table_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_table_row_t;
typedef rocprofvis_dm_handle_t rocprofvis_dm_topology_node;
typedef void* rocprofvis_db_future_t;       // distinct from controller futures
typedef rocprofvis_dm_handle_t rocprofvis_db_instance_t;
```

### 2.2 Database lifecycle

```c
rocprofvis_db_type_t       rocprofvis_db_identify_type(const char* filename);
rocprofvis_dm_database_t   rocprofvis_db_open_database(const char* filename,
                                                       rocprofvis_db_type_t);
rocprofvis_dm_size_t       rocprofvis_db_get_memory_footprint(rocprofvis_dm_database_t);
```

`rocprofvis_db_type_t` values: `kAutodetect`, `kRocpdSqlite`,
`kRocprofSqlite`, `kRocprofMultinodeSqlite`, `kComputeSqlite`. The
`Detect` helper on `ProfileDatabase` sniffs the file (and any sibling
`.db`s for multinode) and returns the right type.

### 2.3 Future / async API (separate from controller futures)

```c
rocprofvis_db_future_t   rocprofvis_db_future_alloc(callback, user_data);
rocprofvis_dm_result_t   rocprofvis_db_future_wait(future, timeout_seconds);
void                     rocprofvis_db_future_cancel(future);
void                     rocprofvis_db_future_free(future);
```

The progress callback `rocprofvis_db_progress_callback_t` receives:

```c
void (*)(rocprofvis_db_filename_t,
         rocprofvis_db_future_id_t,
         rocprofvis_db_progress_percent_t,
         rocprofvis_db_status_t,            // kRPVDbSuccess | kRPVDbError | kRPVDbBusy
         rocprofvis_db_status_message_t,
         void* user_data);
```

The controller's `Future::ProgressCallback` is registered with each DB
future via `AddDependentFuture` so progress is folded into the
controller-side `Future`. **Never block the controller's
`JobFunction` on a DB future without also rolling that DB future into
the controller `Future` chain** - that is what allows the View's
"X% loading..." bar to track real DB progress.

### 2.4 Asynchronous database calls

These run on dedicated `std::thread`s owned by the
`rocprofvis_db_future_t` and use the binding interface (section 6) to
populate the data model on the trace handle that has been bound to the
database.

```c
rocprofvis_dm_result_t rocprofvis_db_read_metadata_async(database, future);
rocprofvis_dm_result_t rocprofvis_db_cleanup_async(database, future, rebuild);
rocprofvis_dm_result_t rocprofvis_db_read_trace_slice_async(
    database, start_ns, end_ns, hashed_timestamp_tag, num_tracks, track_id_array, future);
rocprofvis_dm_result_t rocprofvis_db_read_event_property_async(
    database, kRPVDMEventFlowTrace|kRPVDMEventStackTrace|kRPVDMEventExtData,
    event_id, future);
rocprofvis_dm_result_t rocprofvis_db_execute_query_async(
    database, sql_query, description, future, &out_table_id);
rocprofvis_dm_result_t rocprofvis_db_execute_compute_query_async(
    database, use_case, sql_query, future, &out_table_id);
rocprofvis_dm_result_t rocprofvis_db_export_table_csv_async(
    database, sql_query, file_path, future);
rocprofvis_dm_result_t rocprofvis_db_trim_save_async(
    database, start_ns, end_ns, new_db_path, future);
```

### 2.5 Query builders

The View / controller never hand-build SQL. They request a query string
from the model layer:

```c
rocprofvis_dm_result_t rocprofvis_db_build_table_query(
    database, use_case, start, end, num_tracks, tracks,
    where, filter, group, group_cols,
    sort_column, sort_order,
    num_string_table_filters, string_table_filters,
    max_count, offset, count_only,
    char** out_query);   // caller frees

rocprofvis_dm_result_t rocprofvis_db_build_compute_query(
    database, compute_use_case, num_params, params, char** out_query);
```

`rocprofvis_dm_table_use_case_enum_t` covers the four system table
shapes (`kRPVDMTableUseCaseEventTrackTable`, `kRPVDMTableUseCaseSampleTrackTable`,
`kRPVDMTableUseCaseEventSearch`, `kRPVDMTableUseCaseAnalysis`).
`rocprofvis_db_compute_use_case_enum_t` covers all the compute query
shapes (workload list, top kernels, kernels list, metric definitions,
roofline ceilings, kernel intensities, metric values, kernel metric
matrix, etc.).

### 2.6 Trace lifecycle (data model)

```c
rocprofvis_dm_trace_t   rocprofvis_dm_create_trace(void);
rocprofvis_dm_result_t  rocprofvis_dm_bind_trace_to_database(trace, database);
rocprofvis_dm_result_t  rocprofvis_dm_delete_trace(trace);
```

`bind_trace_to_database` plugs the trace's binding callbacks (section 6)
into the database so subsequent async DB calls can populate the trace.

### 2.7 Trace deletion / GC helpers

Once the trace is built up, callers can throw away parts of it that
are no longer needed without tearing down the whole trace:

```c
rocprofvis_dm_result_t rocprofvis_dm_delete_time_slice(trace, start, end);
rocprofvis_dm_result_t rocprofvis_dm_delete_time_slice_handle(trace, track_id, slice);
rocprofvis_dm_result_t rocprofvis_dm_delete_all_time_slices(trace);

rocprofvis_dm_result_t rocprofvis_dm_delete_event_property_for(trace, type, event_id);
rocprofvis_dm_result_t rocprofvis_dm_delete_event_property(trace, type, handle);
rocprofvis_dm_result_t rocprofvis_dm_delete_all_event_properties_for(trace, type);

rocprofvis_dm_result_t rocprofvis_dm_delete_table_at(trace, table_id);
rocprofvis_dm_result_t rocprofvis_dm_delete_all_tables(trace);
```

The controller's `MemoryManager` (see `.agents/CONTROLLER.md` section
7) drives a lot of these calls so the View's segment cache stays
within the configured memory budget.

### 2.8 Universal property accessors

Every object exposed via a `rocprofvis_dm_handle_t` answers to the
five typed getters. There are two variants:

```c
// Status-by-return + value-by-pointer (use this in C/C++)
rocprofvis_dm_result_t rocprofvis_dm_get_property_as_uint64(handle, prop, index, uint64_t*);
rocprofvis_dm_result_t rocprofvis_dm_get_property_as_int64 (handle, prop, index, int64_t*);
rocprofvis_dm_result_t rocprofvis_dm_get_property_as_double(handle, prop, index, double*);
rocprofvis_dm_result_t rocprofvis_dm_get_property_as_charptr(handle, prop, index, char**);
rocprofvis_dm_result_t rocprofvis_dm_get_property_as_handle(handle, prop, index, void**);

// Value-by-return (CFFI-friendly; nullptr/0 on failure)
uint64_t  rocprofvis_dm_get_property_as_uint64(handle, prop, index);
int64_t   rocprofvis_dm_get_property_as_int64 (handle, prop, index);
double    rocprofvis_dm_get_property_as_double(handle, prop, index);
char*     rocprofvis_dm_get_property_as_charptr(handle, prop, index);
void*     rocprofvis_dm_get_property_as_handle(handle, prop, index);
```

Property enums are per-object-type:

| Object              | Enum                                       |
|---------------------|--------------------------------------------|
| `Trace`             | `rocprofvis_dm_trace_property_t`           |
| `Track`             | `rocprofvis_dm_track_property_t`           |
| `TrackSlice` (event/pmc) | `rocprofvis_dm_slice_property_t`      |
| `FlowTrace`         | `rocprofvis_dm_flowtrace_property_t`       |
| `StackTrace`        | `rocprofvis_dm_stacktrace_property_t`      |
| `ExtData`           | `rocprofvis_dm_extdata_property_t`         |
| `Table` / `InfoTable` | `rocprofvis_dm_table_property_t`         |
| `TableRow` / `InfoTableRow` | `rocprofvis_dm_table_row_property_t` |
| Topology nodes      | `rocprofvis_controller_*_properties_t`     |
|                     | (in `rocprofvis_shared_types.h`)           |

Property values from these enums are passed straight through to
`DmBase::GetPropertyAs*`. Each subclass's overrides know which IDs
they own; unhandled IDs return
`kRocProfVisDmResultInvalidProperty` / `kRocProfVisDmResultNotSupported`.

### 2.9 The 64-bit `rocprofvis_dm_event_id_t`

```c
typedef union {
    struct {
        uint64_t event_id  : 52;   // raw DB id
        uint64_t event_node: 8;    // node index (multinode)
        uint64_t event_op  : 4;    // rocprofvis_dm_event_operation_t
    } bitfield;
    uint64_t value;
} rocprofvis_dm_event_id_t;
```

Operation values come from `rocprofvis_dm_event_operation_t`
(`NoOp`, `Launch`, `Dispatch`, `MemoryAllocate`, `MemoryCopy`,
`LaunchSample`). When iterating across operations, do
`for (op = 0; op < kRocProfVisDmNumOperation; ++op)`.

`TABLE_QUERY_PACK_OP_TYPE` / `TABLE_QUERY_UNPACK_OP_TYPE` /
`TABLE_QUERY_UNPACK_TRACK_ID` (`rocprofvis_interface_types.h`) pack the
operation type into the high bits of a 32-bit "track id" so the table
query API can refer to "track X for operation Y" with one integer.

## 3. Internal Architecture & Threading Model

```
                 +----------------------------------+
   Caller        |  rocprofvis_db_* / rocprofvis_dm_*|  C ABI
   (controller   |  (extern "C", in                  |
   or Python)    |   src/model/src/common/*.cpp)     |
                 +-----------------+----------------+
                                   |
                                   v
                 +----------------------------------+
                 |  Database (abstract)             |
                 |  +-- SqliteDatabase (abstract)   |
                 |       +-- ProfileDatabase        |  rocpd / rocprof
                 |       |    +-- RocpdDatabase     |  legacy schema
                 |       |    +-- RocprofDatabase   |  modern + multinode
                 |       +-- ComputeDatabase        |  rocprof-compute
                 +-----------------+----------------+
                                   | uses binding callbacks
                                   v
                 +----------------------------------+
                 |   Trace (DmBase)                 |
                 |   +-- m_tracks: vector<Track>    |
                 |   +-- m_flow_traces / m_stack_traces / m_ext_data
                 |   +-- m_tables / m_info_tables   |
                 |   +-- m_strings                  |  (string interning)
                 |   +-- m_topology_root            |
                 +----------------------------------+
```

### Threads at runtime

- **One top-level worker thread per async `rocprofvis_db_future_t`.** Each async call
  (`rocprofvis_db_read_metadata_async`, `read_trace_slice_async`,
  `read_event_property_async`, `execute_query_async`,
  `execute_compute_query_async`, `export_table_csv_async`,
  `trim_save_async`, `cleanup_async`) calls one of the static
  `Database::*Static` entry points which spawns a `std::thread`,
  attaches it to the future via `Future::SetWorker(std::move(...))`,
  and returns immediately.
- **Inside the worker**, queries fan out across the multinode database
  files (`m_db_nodes`). Per-DB-node connection pools
  (`rocprofvis_db_sqlite_db_node_t::m_available_connections` /
  `m_connections_inuse`, gated by a mutex + condition variable)
  cap the number of concurrent SQLite connections at `MAX_CONNECTIONS`.
- **Database object methods are thread-safe** through a combination of
  per-object `std::shared_mutex` (e.g.
  `Trace::m_lock`, `Trace::m_event_property_lock[type]`,
  `Track::m_lock`, `TrackSlice::m_lock`, `FlowTrace::m_lock`,
  `StackTrace::m_lock`, `ExtData::m_lock`, `Table::m_lock`) and
  the per-DB-node connection mutex.

### Threading rules

1. The C ABI is reentrant per-handle for getters. Setters / `AddRecord`
   are protected by per-object mutexes.
2. **New top-level async DB calls go through the future system.** Use
   the `Database::FooStatic -> Future::SetWorker` pattern. Internal
   fan-out can use sub-futures and short-lived `std::thread` vectors
   when the existing code already does so (multinode / table
   aggregation / compute matrix paths), and `TableProcessor` owns a
   sanctioned `RestartableTimer` thread for cache expiry.
3. Bridging to the controller: the controller `Future` chains DB
   futures via `AddDependentFuture` so cancel and progress propagate.
   Do not wait blocking on a DB future from inside the controller's
   worker pool.
4. Cancel cooperatively. The `Future::Interrupted()` flag is checked
   inside long callbacks (see `RpvSqliteExecuteQueryCallback`
   implementations) - new callbacks must do the same.

### Lock-contention diagnostics

`rocprofvis_dm_base.h` defines `LOCK_WAITED_TOO_LONG_WARNING_TIME_LIMIT_US`
(default `5000`) and `LOCK_HELD_TOO_LONG_WARNING_TIME_LIMIT_US`
(default `500000`) which the `TimedLock<LockType>` RAII wrapper uses
to log `spdlog::debug` traces when a lock is held / waited too long.
Use this wrapper around any new shared-state lock so contention shows
up in debug logs rather than silent stalls.

## 4. Database Layer (`src/model/src/database/`)

### 4.1 `Database` base class (`rocprofvis_db.h`)

The abstract class for any backing store. Notable members and methods:

```cpp
class Database
{
public:
    Database(rocprofvis_db_filename_t path);
    virtual ~Database();

    virtual rocprofvis_dm_result_t Open()  = 0;
    virtual rocprofvis_dm_result_t Close() = 0;
    virtual rocprofvis_dm_size_t   GetMemoryFootprint();

    rocprofvis_dm_result_t  BindTrace(rocprofvis_dm_db_bind_struct*);
    rocprofvis_dm_result_t  CleanupAsync(future, rebuild);
    rocprofvis_dm_result_t  ReadTraceMetadataAsync(future);
    rocprofvis_dm_result_t  ReadTraceSliceAsync(start, end, num, tracks, future);
    rocprofvis_dm_result_t  ReadEventPropertyAsync(type, event_id, future);
    rocprofvis_dm_result_t  ExecuteQueryAsync(query, description, future, &out_id);
    rocprofvis_dm_result_t  ExecuteComputeQueryAsync(use_case, query, future, &out_id);
    rocprofvis_dm_result_t  ExportTableCSVAsync(query, file_path, future);
    rocprofvis_dm_result_t  SaveTrimmedDataAsync(start, end, new_db_path, future);

    DatabaseCache*  CachedTables(node_id);
    TrackLookup*    TrackTracker();
    rocprofvis_dm_size_t          NumTracks();
    rocprofvis_dm_track_params_t* TrackPropertiesAt(index);

    static rocprofvis_dm_table_t      GetInfoTableHandle(...);   // helpers
    static size_t                     GetInfoTableNumColumns(...);
    static const char*                GetInfoTableColumnName(...);
    static rocprofvis_dm_table_row_t  GetInfoTableRowHandle(...);
    static const char*                GetInfoTableRowCellValue(...);
};
```

Public async calls (`*Async`) just dispatch to `*Static` worker
functions on a fresh thread; the worker calls the **pure virtual**
methods (`ReadTraceMetadata`, `ReadTraceSlice`, `ReadFlowTraceInfo`,
`ReadStackTraceInfo`, `ReadExtEventInfo`, `ExecuteQuery`,
`ExecuteComputeQuery`, `BuildTrackQuery`, `BuildSliceQuery`,
`BuildTableQuery`, `BuildComputeQuery`, `SaveTrimmedData`,
`Cleanup`) on the concrete subclass.

Three friend classes have privileged access to private members:
`DatabaseCache`, `TableProcessor`, `TrackLookup`.

`OrderedMutex` (same file) is a serial-lock helper that orders
multinode database operations by instance index so add-track logic
does not race when multiple nodes are loaded concurrently.

### 4.2 `SqliteDatabase` (`rocprofvis_db_sqlite.h`)

Adds SQLite plumbing on top of `Database`. Key concepts:

- **DB nodes (`rocprofvis_db_sqlite_db_node_t`):** one per file in a
  multinode set. Carries the `node_id`, file path, and the
  available/in-use connection pools. `MAX_CONNECTIONS = 100`.
- **Connection management:** `OpenConnection`, `GetConnection`,
  `GetServiceConnection`, `ReleaseConnection`, `InterruptQuery`. New
  query paths must release every connection they take, even on early
  exit.
- **`Sqlite3Exec` / `ExecuteSQLQuery` overloads:** the canonical way
  to run SQL. The internal `Sqlite3Exec` mimics `sqlite3_exec` using
  `sqlite3_prepare_v2` so callbacks receive a real `sqlite3_stmt*`
  and can use the typed `Sqlite3Column*` helpers. There are eight
  `ExecuteSQLQuery` overloads covering: result-less queries,
  single-row scalars (string / uint64 / uint32), multi-row queries
  with handle context, multi-row queries with cache table name, and
  the fan-out variant that takes a `vector<string>` of queries
  (one per DB node) and a separate "find" + "load" callback pair.
- **`RpvSqliteExecuteQueryCallback`:** signature
  `int(void* user_data, int argc, sqlite3_stmt* stmt, char** azColName)`.
  Use this signature for any new callback you register.
- **Null exception tables:** `rocprofvis_null_data_exceptions_int`,
  `..._string`, `..._skip`. Subclasses provide these via
  `GetNullDataExceptionsInt/String/Skip` so that columns known to be
  NULL in some rows can be substituted (`s_null_data_exception_*`
  static maps) or skipped (`NullExceptionSkip`). The
  `Sqlite3ColumnInt/Int64/Double/Text` helpers consult these maps
  before failing on a NULL.
- **Transactions:** `ExecuteTransaction(queries, db_node_id)` runs
  multiple statements inside a `BEGIN; ... COMMIT;` (with rollback on
  failure).
- **Service connection:** `GetServiceConnection(db_node_id)` returns a
  long-lived per-node connection used for cleanup and metadata
  maintenance, separate from query workers.
- **Schema helpers:** `CreateSQLTable(name, params, row_count, insert_lambda)`,
  `DropSQLTable`, `DropSQLIndex`, `GetRocpdIndexes`, `DetectTable`.
  Use these instead of inline DDL.

### 4.3 `ProfileDatabase` (`rocprofvis_db_profile.h`)

Common base for `RocpdDatabase` and `RocprofDatabase`. Key
responsibilities:

- Owns the global string table (`StringTable m_string_table`).
- Holds level-calculation cache:
  `m_event_levels[op]` (`unordered_map<guid, vector<rocprofvis_db_event_level_t>>`)
  and `m_event_levels_id_to_index[op]`.
- Holds the four `TableProcessor`s, one per
  `rocprofvis_db_compound_table_type` (event, sample, search,
  analysis).
- Exposes `Detect(filename, multinode_files)` to identify between
  rocpd, rocprof, and rocprof-multinode SQLite formats.
- Implements the shared `BuildTrackQuery` / `BuildSliceQuery` /
  `BuildTableQuery`, `ReadTraceSlice`, `ExecuteQuery`,
  `ExportTableCSV` flows. Compute queries are explicitly stubbed out
  here (`ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN("Systems database does
  not build compute query")`).
- Houses every standard query callback used during metadata load:
  `CallBackAddTrack`, `CallBackLoadTrack`, `CallbackCacheTable`,
  `CallbackAddFlowTrace`, `CallbackAddStackTrace`,
  `CallbackAddExtInfo`, `CallbackAddEssentialInfo`,
  `CallbackAddArgumentsInfo`, `CalculateEventLevels`,
  `CallbackGetTrackProperties`, `CallbackGetTrackRecordsCount`,
  `CallbackTrimTableQuery`, `CallbackMakeHistogramPerTrack`,
  `CallBackLoadHistogram`.
- Owns the histogram pipeline (`BuildHistogram(future, desired_bins)`,
  `s_histogram_schema_params`, `GetHistogramQueryAndSchemaHash`).
- `kRocProfVisDmIncludePmcTracks` / `kRocProfVisDmIncludeStreamTracks` /
  `kRocProfVisDmTrySplitTrack` / `kRocProfVisDmIncludePmcTracksOnly`
  flags drive `ExecuteQueryForAllTracksAsync`.
- `SINGLE_THREAD_RECORDS_COUNT_LIMIT = 50000`,
  `NO_THREAD_RECORDS_COUNT_LIMIT = 1000` are the heuristics that
  decide whether a slice request runs single-threaded or fans out.

### 4.4 `RocpdDatabase` (`rocprofvis_db_rocpd.h`)

The legacy SQLite schema. Important traits:

- Single-node only (its constructor always calls `CreateDbNode(path)`
  with one file).
- Has its own string-index remapping map (`string_index_map_t`,
  `string_id_map_t`) because the legacy schema duplicates symbols
  per GPU - `RemapStringIdHelper` collapses duplicates.
- Owns `RocpdMetadataVersionControl` (table cleanup + rebuild logic;
  see section 10).
- Categorized data: `s_rocpd_categorized_data` maps DB column names
  to `rocprofvis_event_data_category_enum_t` for each operation type.
- Region track category is `kRocProfVisDmRegionTrack` (not the
  rocprof split into `RegionMain` / `RegionSample`).

### 4.5 `RocprofDatabase` (`rocprofvis_db_rocprof.h`)

The modern SQLite schema; supports **multinode**. Important traits:

- Two constructors: single-file and multinode (`CreateDbNodes`).
- Owns its own `QueryFactory m_query_factory` and
  `RocprofMetadataVersionControl m_metadata_version_control`.
- Has memory-activity tracking: every alloc / free / realloc / reclaim
  is captured into `m_memalloc_activity` (per-PID
  `vector<rocprofvis_db_memalloc_activity_t>`), then materialized
  into a `roc_optiq_memory_activity` SQL table during
  `LoadMemoryActivityData`. The memory-allocation level enums and
  type enums are part of the public model header
  (`kRPVMemActivityAlloc`/`Free`/`Realloc`/`Reclaim`,
  `kRPVMemLevelReal`/`Virtual`/`Scratch`).
- String interning is keyed on `(string_id, guid_id, string_type)`
  (`rocprofvis_db_string_id_t`) because rocprof unifies strings and
  kernel symbol names into one array.
- Region track category is `kRocProfVisDmRegionMainTrack`; the split
  into main + sample lets the View distinguish the executable region
  from sample-derived sub-regions.
- Categorized data (`s_rocprof_categorized_data`) covers
  `NoOp`, `Dispatch`, `MemoryAllocate`, and `MemoryCopy` mappings.

### 4.6 `ComputeDatabase` (`rocprofvis_db_compute.h`)

The rocprof-compute SQLite schema. Different shape from system
databases:

- Has no time slices, no event properties, no trim. The four
  `Read*` overrides return `kRocProfVisDmResultNotSupported` /
  assert, and so do `BuildTrackQuery` / `BuildSliceQuery` /
  `BuildTableQuery` / `SaveTrimmedData`.
- Implements the full `BuildComputeQuery` and `ExecuteComputeQuery`
  pipeline through its `ComputeQueryFactory m_query_factory`.
- Maintains kernel-stat cache (`m_kernel_stats`,
  `KernelStats { count, sum, min, max, mean, median, name,
  durations }`), per-workload metric rows (`m_metric_rows`), and
  metric ID lookup tables for quick "metric.value_name -> uuid"
  resolution.
- `MetricSelector { metric_id, value_name }` is the parsed form of
  the `"category.table.entry:value_name"` string the Compute pivot
  table expects.
- Has its own callback cluster:
  `CallbackGetComputeGeneric`,
  `CallbackGetComputeKernelWorkloadLookupTable`,
  `CallbackGetComputeRooflineCeiling`,
  `CallbackGetComputeKernelMetricsMatrix`,
  `CallbackParseMetadata`,
  `CallbackGetComputeWorkloadTopKernels`,
  `CallbackGetComputeMetricsData`,
  `CallbackStoreMetricsLookupTable`.
- Pivot construction: `BuildKernelMetricsMatrix(table, plan)` builds
  the kernel x metric pivot table from a JSON plan (`jt::Json`).
- `ComputeWorkloadTopKernelsMeanAndMedian(table)` post-processes top
  kernels to stamp mean / median into the table.

### 4.7 Other database/ files

- `rocprofvis_db_future.h` - the `Future` (`DataModel` namespace),
  `RuntimeValue`, `kRPVFutureStorageSampleValue` /
  `kRPVFutureStorageEventId` / `kRPVFutureStorageTrackId`
  side-channel store, `Future::AddSubFuture` / `DeleteSubFuture` /
  `WaitAndDeleteSubFuture` for fan-out.
- `rocprofvis_db_cache.h` - `TableCache`, `DatabaseCache`,
  `StringTable` (database-level string interning;
  `ToInt(s) -> uint32_t`, `ToString(id)`).
- `rocprofvis_db_track.h` - `TrackLookup` (`MakeKey` /
  `AddTrack` / `FindTrack` overloads, `SearchCategoryMaskLookup`
  for op-to-category lookups, internal `StringTable` for
  identifier interning).
- `rocprofvis_db_table_processor.h` - `TableProcessor`
  (manages the in-memory result of a compound table query),
  `RestartableTimer` (debounces clear-on-idle), the static
  `IsCompoundQuery` / `QueryWithoutCommands` parsers, and the
  `QUERY_COMMAND_TAG = "-- CMD:"` convention used to embed
  sort/filter/group instructions inside SQL comments.
- `rocprofvis_db_packed_storage.h` - `PackedRow`, `PackedTable`,
  `ColumnDef`, `MergedColumnDef`, `Aggregation`, `ColumnAggr`,
  `NumericWithType` (Numeric tagged union),
  `ColumnType { Null, Byte, Word, Dword, Qword, Double }`. This is
  where the column-oriented in-memory schema and the merge / sort /
  aggregate engine live.
- `rocprofvis_db_expression_filter.h` - `Tokenizer` and
  `FilterExpression`. The expression language used in
  `kRPVControllerCPTArgsFilterExpressionIndexed` and table-row
  filtering: numeric literals, string literals, hex, identifiers,
  comparison + arithmetic operators, parenthesized expressions,
  `LIKE` patterns, `AND/OR/NOT` logic, and SQL aggregations
  (`Count/Avg/Min/Max/Sum`). Function callbacks register via
  `RegisterFunction(name, handler)`.
- `rocprofvis_db_query_builder.h` - `Builder` (string assembly DSL
  for SQLite) and the per-track / per-slice / per-table query
  format structs (`rocprofvis_db_sqlite_track_query_format` etc.)
  with `Select(...)` overloads. Owns the `table_view_schema_index_t`
  enum and `table_view_schema` map that names columns and assigns
  `ColumnType`.
- `rocprofvis_db_query_factory.h` - `QueryFactory` (rocprof). Each
  domain (region, kernel dispatch, memory alloc, memory copy, PMC,
  SMI PMC, memory activity) has Track / Level / Slice / Table /
  StreamFlow / DataFlow / EssentialInfo / ArgumentsInfo SQL
  generators.
- `rocprofvis_db_compute.h` - `ComputeQueryFactory`, plus the
  `MetricIdFormat`, `KernelStats`, `MetricSelector`, `MetricRow`,
  `KernelMetricsRow` shapes the compute pipeline uses.
- `rocprofvis_db_version.h` - `DatabaseVersion`,
  `MetadataVersionControl`, `RocprofMetadataVersionControl`,
  `RocpdMetadataVersionControl`. Tracks which `roc_optiq_*` derived
  tables exist, what their versions are, what depends on what, and
  what to rebuild when the schema bumps. See section 10.

## 5. Data Model Layer (`src/model/src/datamodel/`)

The data model is the in-memory tree the database populates. Every
class below derives from `DmBase` (`rocprofvis_dm_base.h`) and answers
to the universal `GetPropertyAs*` accessors.

### 5.1 `DmBase` and `TimedLock`
File: `rocprofvis_dm_base.h`.

`DmBase` is the abstract base. Default `GetPropertyAs*` overloads
return `kRocProfVisDmResultNotSupported`; subclasses override the
ones they handle. `Mutex()` returns a per-class `std::shared_mutex*`
(or `nullptr` for stateless types).

`TimedLock<LockType>` is the RAII wrapper used for every shared
mutex acquisition that wants to log when contention exceeds the
configured limits. Use it with `std::unique_lock` /
`std::shared_lock` as the type parameter.

### 5.2 `Trace`
File: `rocprofvis_dm_trace.h`.

The root container; wraps everything tied to one bound database.
Members:

```cpp
rocprofvis_dm_trace_params_t            m_parameters;     // shared with DB
rocprofvis_dm_db_bind_struct            m_binding_info;   // callbacks
rocprofvis_dm_database_t                m_db;
std::vector<std::unique_ptr<Track>>     m_tracks;
std::vector<std::shared_ptr<FlowTrace>> m_flow_traces;
std::vector<std::shared_ptr<StackTrace>> m_stack_traces;
std::vector<std::shared_ptr<ExtData>>   m_ext_data;
std::vector<std::shared_ptr<Table>>     m_tables;
std::vector<std::unique_ptr<InfoTable>> m_info_tables;
std::vector<std::string>                m_strings;
std::vector<uint32_t>                   m_sorted_strings_lookup_array;
event_level_map_t                       m_event_level_map;
mutable std::shared_mutex               m_lock;
mutable std::shared_mutex               m_event_property_lock[kRPVDMNumEventPropertyTypes];
TopologyNodeRoot                        m_topology_root;
```

Trace exposes `BindDatabase`, `DeleteSliceAtTimeRange`,
`DeleteSliceByHandle`, `DeleteAllSlices`, `DeleteEventPropertyFor`,
`DeleteEventProperty`, `DeleteAllEventPropertiesFor`,
`DeleteTableAt`, `DeleteAllTables`, `GetStringAt(index)`, plus the
property accessors.

It also exposes a long list of **static** binding callbacks (the
implementations of `rocprofvis_dm_db_bind_struct`'s function
pointers, see section 6): `AddTrack`, `AddSlice`, `AddRecord`,
`AddString`, `GetString`, `AddFlowTrace`, `AddFlow`, `AddStackTrace`,
`AddStackFrame`, `AddExtData`, `AddExtDataRecord`, `AddArgDataRecord`,
`AddTable`, `AddInfoTable`, `AddTableRow`, `AddTableColumn`,
`AddTableColumnEnum`, `AddTableColumnType`, `AddTableRowCell`,
`AddEventLevel`, `AddTopologyNode`, `AddTopologyNodeProperty`,
`CheckSliceExists`, `CheckEventPropertyExists`, `CheckTableExists`,
`CompleteSlice`, `RemoveSlice`, `MetadataLoaded`, `GetStringOrder`,
`GetStringIndices`.

### 5.3 `Track`
File: `rocprofvis_dm_track.h`.

One Track per profiler event lane. `Track` carries a pointer to the
DB-owned `rocprofvis_dm_track_params_t` (the same struct exists on
the DB side) so track metadata stays in sync without copies.

Public surface:

- `Category`, `TrackId`, `NodeId`, `ProcessId`, `SubProcessId`,
  `Process`, `SubProcess`, `NumRecords`, `MinTimestamp`,
  `MaxTimestamp`, `MinValue`, `MaxValue`, `InstanceId`,
  `CategoryString`, `GetHistogramBucketValueAt`,
  `GetHistogramBucketNumEventsAt`.
- Slice management: `GetSliceAtIndex`, `GetSliceAtTime`,
  `GetSliceIndexAtTime`, `DeleteSliceAtTime`, `DeleteSliceByHandle`,
  `DeleteAllSlices`, `AddSlice`.
- All four typed `GetPropertyAs*` overloads.

`m_track_params` is a non-owning pointer. The `Database` owns the
storage; deleting the track does not delete its parameters.

### 5.4 `TrackSlice` and subclasses
File: `rocprofvis_dm_track_slice.h`.

`TrackSlice` is the abstract base for one time slice of one track. It
owns:

- `m_start_timestamp`, `m_end_timestamp`.
- A per-slice `MemoryPool` chain (`m_object_pools`,
  `m_current_pool`) sized to `kMemPoolBitSetSize = 1024` records per
  block, used to allocate `EventRecord` / `PmcRecord` POD storage
  contiguously.
- A `m_complete` flag and `m_cv` so a query-cancellation path can
  wake any thread that called `WaitComplete()`.

Pure virtual methods every concrete slice must implement:
`AddRecord(rocprofvis_db_record_data_t&)`, `GetMemoryFootprint`,
`GetNumberOfRecords`, `ConvertTimestampToIndex`,
`GetRecordTimestampAt`. The base provides
`GetRecordIdAt / OperationAt / OperationStringAt / ValueAt /
DurationAt / CategoryIndexAt / SymbolIndexAt / CategoryStringAt /
SymbolStringAt / GraphLevelAt` virtual fallbacks; subclasses
override only the ones that apply.

Two concrete subclasses:

- **`EventTrackSlice`** (`rocprofvis_dm_event_track_slice.h`) holds
  `vector<EventRecord*>` populated through the slice's pool. All
  event-related getters (`GetRecordIdAt`, `GetRecordDurationAt`,
  `GetRecordCategoryIndexAt`, etc.) are wired up here.
- **`PmcTrackSlice`** (`rocprofvis_dm_pmc_track_slice.h`) holds
  `vector<PmcRecord*>`. Only `GetRecordTimestampAt` and
  `GetRecordValueAt` are meaningful; the others return
  `kRocProfVisDmResultNotSupported`.

### 5.5 `EventRecord` and `PmcRecord`

POD record types stored in the slice memory pools (do not inherit
from `DmBase`).

```cpp
class EventRecord {
    rocprofvis_dm_event_id_t      m_event_id;       // packed (52 / 8 / 4)
    rocprofvis_dm_timestamp_t     m_timestamp;
    rocprofvis_dm_duration_t      m_duration;       // signed
    rocprofvis_dm_index_t         m_category_index; // -> Trace::m_strings
    rocprofvis_dm_index_t         m_symbol_index;
    rocprofvis_dm_event_level_t   m_event_level;
};

class PmcRecord {
    rocprofvis_dm_timestamp_t m_timestamp;
    rocprofvis_dm_value_t     m_value;             // double
};
```

Adding a new record type means adding a new variant to
`rocprofvis_db_record_data_t` (in `rocprofvis_common_types.h`),
adding a matching POD in `datamodel/`, and a slice subclass that
allocates from the pool.

### 5.6 `Table` / `TableRow` / `InfoTable` / `InfoTableRow`
File: `rocprofvis_dm_table.h`, `rocprofvis_dm_table_row.h`.

- **`Table`** is the dynamic-table type populated by
  `rocprofvis_db_execute_query_async` /
  `rocprofvis_db_execute_compute_query_async`. Holds `m_columns`,
  `m_column_enums`, `m_column_types` (`rocprofvis_db_data_type_t`),
  `m_rows` (`vector<shared_ptr<TableRow>>`), the original `m_query`
  and `m_description` strings, and a per-table `m_id`. `AddRow`
  appends a fresh row; `AddColumn` / `AddColumnEnum` / `AddColumnType`
  populate columns. `GetMemoryFootprint` walks rows.
- **`TableRow`** is one row of a `Table`. `AddCellValue(str)` appends
  a string cell, `GetNumberOfCells`, plus the property accessors.
- **`InfoTable`** wraps an externally-owned table handle (the DB
  stores its own info-table representation; `InfoTable` adapts it via
  the binding `FuncGetInfoTable*` callbacks). `InfoTableRow` does the
  same for row cells.

### 5.7 `FlowTrace` and `FlowRecord`
Files: `rocprofvis_dm_flow_trace.h`, `rocprofvis_dm_flow_record.h`.

`FlowTrace` is one event's set of flow endpoints (incoming + outgoing
arrows on the timeline). Carries `m_event_id`, `m_flows`
(`vector<FlowRecord>`), the per-trace context, and a shared mutex.

`FlowRecord` is the POD endpoint shape:
`{ m_start_timestamp, m_end_timestamp, m_event_id, m_track_id,
m_category_id, m_symbol_id, m_level }`.

### 5.8 `StackTrace` and `StackFrameRecord`
Files: `rocprofvis_dm_stack_trace.h`, `rocprofvis_dm_stack_record.h`.

`StackTrace` holds `m_event_id` and `m_stack_frames`
(`vector<StackFrameRecord>`).

`StackFrameRecord` carries `{ m_symbol, m_args, m_code_line, m_depth,
m_id }` - all strings + depth + region id.

### 5.9 `ExtData`, `ExtDataRecord`, `ArgumentRecord`
Files: `rocprofvis_dm_ext_data.h`, `rocprofvis_dm_ext_data_record.h`.

`ExtData` is a generic container of "category / name / value / type"
records attached to either an event or a track. It also holds a
`m_argument_records` list for kernel/dispatch argument metadata.

```cpp
class ExtDataRecord {
    rocprofvis_dm_string_t           m_category;
    rocprofvis_dm_string_t           m_name;
    rocprofvis_dm_string_t           m_data;
    rocprofvis_db_data_type_t        m_type;
    rocprofvis_event_data_category_enum_t m_category_enum;
    rocprofvis_dm_node_id_t          m_db_instance;
};

class ArgumentRecord {
    rocprofvis_dm_string_t  m_name;
    rocprofvis_dm_string_t  m_value;
    rocprofvis_dm_string_t  m_type;
    uint32_t                m_position;
};
```

`HasRecord(data)` deduplicates incoming ext-data rows;
`AddRecord(rocprofvis_db_argument_data_t&)` is the dedicated overload
for argument records.

### 5.10 Topology tree
File: `rocprofvis_dm_topology.h`.

`TopologyNode` is the polymorphic base: every level of the topology
tree (root, system node, process, processor, thread, queue,
memory allocation, memory copy, kernel dispatch, stream, counter,
plus the downstream-reference variants for streams that span agents)
is a subclass.

Each subclass declares:

- `GetType()` (`rocprofvis_controller_topology_node_type_t`).
- `GetPropertiesMap()` returning a `map<column_name, property_id>` -
  the columns of the relevant DB table that should be ingested as
  topology properties.
- `IsRelevantPropertyTableName` and `IsRelevantTopologyTableName`
  predicates, used during property dispatch.
- `GetLevelId()` (one of the `TRACK_ID_*` constants, e.g.
  `TRACK_ID_NODE`, `TRACK_ID_PID`, `TRACK_ID_AGENT`, `TRACK_ID_TID`,
  `TRACK_ID_QUEUE`, `TRACK_ID_STREAM`, `TRACK_ID_COUNTER`).
- `GetLevelTag()` - the column name used in queries
  (`Builder::NODE_ID_SERVICE_NAME`, etc.).
- `GetNodeName()` returning the human-readable label.
- Optional `DoesThisNodeMatchIdentifiers` for non-trivial matching
  (e.g. queue vs memory-alloc-on-the-same-queue distinction).

`TopologyNodeRoot::AddNode(track_identifiers)` walks down the tree
adding intermediate nodes as needed; this is the entry point used by
the binding callback `FuncAddTopologyNode`. `AddProperty` finds the
right node (via `FindRelevantPropertyNode` /
`FindRelevantTopologyNode`) and writes the value into
`m_properties[property_id]`.

`TopologyReferenceNode` is used when a downstream node (e.g. a
stream's processor) is logically a reference into another part of
the tree; its `GetPropertyAs*` overrides forward to the referenced
node via `FindReferencedNode()`.

The `rocprofvis_controller_topology_node_type_t` and
per-level property bank enums (`kRPVControllerNodeId`,
`kRPVControllerProcessorId`, `kRPVControllerThreadId`, etc.) are in
`rocprofvis_shared_types.h` so the controller's `TopologyNode`
mirror tree (see `.agents/CONTROLLER.md` section 5.7) shares the same
IDs.

## 6. Database <-> Trace Binding Protocol

The data model and the database deliberately do not call each other's
internals. They communicate through a function-pointer table
(`rocprofvis_dm_db_bind_struct`, defined in
`rocprofvis_common_types.h`) populated by the trace and consumed by
the database.

### 6.1 The binding struct

```cpp
typedef struct {
    rocprofvis_dm_trace_t        trace_object;
    rocprofvis_dm_trace_params_t* trace_properties;

    // Trace -> add things (called by DB during query callbacks)
    rocprofvis_dm_add_track_func_t        FuncAddTrack;
    rocprofvis_dm_add_slice_func_t        FuncAddSlice;
    rocprofvis_dm_add_record_func_t       FuncAddRecord;
    rocprofvis_dm_add_string_func_t       FuncAddString;
    rocprofvis_dm_add_flowtrace_func_t    FuncAddFlowTrace;
    rocprofvis_dm_add_flow_func_t         FuncAddFlow;
    rocprofvis_dm_add_stacktrace_func_t   FuncAddStackTrace;
    rocprofvis_dm_add_stack_frame_func_t  FuncAddStackFrame;
    rocprofvis_dm_add_extdata_func_t      FuncAddExtData;
    rocprofvis_dm_add_extdata_record_func_t FuncAddExtDataRecord;
    rocprofvis_dm_add_argdata_record_func_t FuncAddArgDataRecord;
    rocprofvis_dm_add_table_func_t        FuncAddTable;
    rocprofvis_dm_add_table_row_func_t    FuncAddTableRow;
    rocprofvis_dm_add_table_column_func_t FuncAddTableColumn;
    rocprofvis_dm_add_table_column_enum_func_t FuncAddTableColumnEnum;
    rocprofvis_dm_add_table_column_type_func_t FuncAddTableColumnType;
    rocprofvis_dm_add_table_row_cell_func_t FuncAddTableRowCell;
    rocprofvis_dm_add_event_level_func_t  FuncAddEventLevel;
    rocprofvis_dm_check_slice_exists_t    FuncCheckSliceExists;
    rocprofvis_dm_check_event_property_exists_t FuncCheckEventPropertyExists;
    rocprofvis_dm_check_table_exists_t    FuncCheckTableExists;
    rocprofvis_dm_complete_slice_func_t   FuncCompleteSlice;
    rocprofvis_dm_remove_slice_func_t     FuncRemoveSlice;
    rocprofvis_dm_get_string_func_t       FuncGetString;
    rocprofvis_dm_get_string_order_func_t FuncGetStringOrder;
    rocprofvis_dm_metadata_loaded_func_t  FuncMetadataLoaded;
    rocprofvis_dm_string_indices_func_t   FuncGetStringIndices;
    rocprofvis_dm_add_info_table_func_t   FuncAddInfoTable;
    rocprofvis_db_add_topology_node       FuncAddTopologyNode;
    rocprofvis_db_add_topology_node_property FuncAddTopologyNodeProperty;

    // DB -> read cached info (called by trace during property gets)
    rocprofvis_db_find_cached_table_value_func_t FuncFindCachedTableValue;
    rocprofvis_db_get_info_table_num_columns_func_t FuncGetInfoTableNumColumns;
    rocprofvis_db_get_info_table_num_rows_func_t FuncGetInfoTableNumRows;
    rocprofvis_db_get_info_table_column_func_t FuncGetInfoTableColumnName;
    rocprofvis_db_get_info_table_rows_handle_func_t FuncGetInfoTableRowHandle;
    rocprofvis_db_get_info_table_row_cell_value_func_t FuncGetInfoTableRowCellValue;
    rocprofvis_db_get_info_table_row_num_cells_func_t FuncGetInfoTableRowNumCells;
} rocprofvis_dm_db_bind_struct;
```

### 6.2 Binding sequence

1. Caller creates a Trace: `rocprofvis_dm_create_trace()`. The Trace
   constructor populates `m_binding_info` with the trace-side static
   methods.
2. Caller opens a database: `rocprofvis_db_open_database(path, type)`.
3. Caller binds: `rocprofvis_dm_bind_trace_to_database(trace, db)`.
   This calls `Trace::BindDatabase` which writes the database-side
   pointers into `m_binding_info` (`FuncFindCachedTableValue`,
   `FuncGetInfoTable*`) and hands the populated struct back to the
   database via `Database::BindTrace`. The database stores the pointer
   in `m_binding_info` and uses it on every async call.
4. Subsequent async DB calls populate the trace via
   `m_binding_info->FuncAdd*`.

### 6.3 Rules when extending the protocol

- **Add a function pointer slot** to `rocprofvis_dm_db_bind_struct`
  in `rocprofvis_common_types.h`.
- **Provide a typedef** for the function pointer right above the
  struct.
- **Implement the trace-side static method** in `Trace` (header
  declaration in `rocprofvis_dm_trace.h`, body in the matching cpp).
- **Wire it into `Trace::BindDatabase`** so it ends up in
  `m_binding_info`.
- **Use the function pointer** in the relevant database callback /
  worker.

Do not call into `Database::*` from inside Trace or vice versa
without going through this struct - the goal is that Trace stays
agnostic to which Database flavor is bound.

## 7. Trace File Formats & Adapters

ROCm Optiq consumes four trace shapes, all SQLite-backed.
`rocprofvis_db_identify_type(filename)` -> `ProfileDatabase::Detect`
sniffs:

- **`kRocpdSqlite`** - legacy rocpd schema. One file. Decoded by
  `RocpdDatabase`. String table is per-GPU duplicated and is
  collapsed by `RemapStringIdHelper`.
- **`kRocprofSqlite`** - modern rocprof schema. One file. Decoded by
  `RocprofDatabase`. Strings + kernel symbols share one table.
- **`kRocprofMultinodeSqlite`** - rocprof schema split across one
  primary file plus N node files (`.db` siblings detected by
  `DetectMultiNode`). Decoded by `RocprofDatabase` constructed with
  the multinode constructor; queries fan out per node, results merge
  through `OrderedMutex` so add-order is deterministic.
- **`kComputeSqlite`** - rocprof-compute schema. Decoded by
  `ComputeDatabase`. No timeline / event slices; instead, workload +
  kernel + metric matrices.

The list of recognized extensions is exercised at the application
layer (`AppWindow` and `Project`). Adding a new format means adding a
new `rocprofvis_db_type_t` enum value, updating
`ProfileDatabase::Detect` (or a sibling type detector), implementing
a `Database` subclass, and wiring it into
`rocprofvis_db_open_database`.

## 8. Query Pipeline

### 8.1 `Builder` (`rocprofvis_db_query_builder.h`)

The DSL for assembling SQL strings. Shapes the canonical column
schema (the `table_view_schema_index_t` enum + `table_view_schema`
map) so every query produces the same column layout.

Public surface (excerpt):

```cpp
class Builder {
public:
    static std::string Select(rocprofvis_db_sqlite_track_query_format params);
    static std::string Select(rocprofvis_db_sqlite_region_track_query_format params);
    static std::string Select(rocprofvis_db_sqlite_level_query_format params);
    static std::string Select(rocprofvis_db_sqlite_slice_query_format params);
    static std::string Select(rocprofvis_db_sqlite_launch_table_query_format params);
    static std::string Select(rocprofvis_db_sqlite_dispatch_table_query_format params);
    static std::string Select(rocprofvis_db_sqlite_memory_alloc_table_query_format params);
    static std::string Select(rocprofvis_db_sqlite_memory_copy_table_query_format params);
    static std::string Select(rocprofvis_db_sqlite_rocpd_sample_table_query_format params);
    static std::string Select(rocprofvis_db_sqlite_sample_table_query_format params);
    static std::string Select(rocprofvis_db_sqlite_rocpd_table_query_format params);
    static std::string Select(rocprofvis_db_sqlite_dataflow_query_format params);
    static std::string Select(rocprofvis_db_sqlite_essential_data_query_format params);
    static std::string Select(rocprofvis_db_sqlite_argument_data_query_format params);
    static std::string Select(rocprofvis_db_sqlite_stream_to_hw_format params);
    static std::string Select(rocprofvis_db_sqlite_memory_alloc_activity_query_format);
    static std::string Select(rocprofvis_db_sqlite_mem_act_subquery_format params);

    static std::string SelectAll(std::string query);
    static std::string From(std::string table, MultiNode = MultiNode::Yes);
    static std::string From(std::string table, std::string nick, MultiNode = MultiNode::Yes);
    static std::string InnerJoin(std::string table, std::string nick, std::string on, MultiNode);
    static std::string LeftJoin(...);
    static std::string RightJoin(...);
    static std::string Where(std::string name, std::string condition, std::string value);
    static std::string Union();
    static std::string Concat(std::vector<std::string>);
    static std::string QParam(std::string name, std::string public_name);
    static std::string QParamOperation(rocprofvis_dm_event_operation_t);
    static std::string QParamCategory(rocprofvis_dm_track_category_t);
    static std::string LevelTable(std::string operation, std::string guid = "");
    static std::string SpaceSaver(int val);
    static std::string THeader(std::string header);
    static std::string TVar(std::string tag, std::string var);
    static std::string TVar(std::string tag, std::string var1, std::string var2);
    // ... and a handful of column-name conversion helpers
};
```

The `*Format` structs are filled in by callers, then handed to
`Builder::Select` which interpolates the parameters into a
canonical SQL template. The
`MultiNode { No=false, Yes=true }` flag controls whether `From` /
`*Join` prefix the table with the right node-suffix for multinode
files.

### 8.2 `QueryFactory` (rocprof system traces, `rocprofvis_db_query_factory.h`)

Per-domain SQL generators. Each domain has up to seven query shapes
(track, level, slice, table, dataflow, essential info, arguments
info). Domains:

- Region (rocpd / rocprof-region launch + sample variants).
- Kernel dispatch.
- Memory allocate / memory free / memory copy (with stream-flow
  variants).
- Performance counters (PMC) and SMI-PMC.
- Memory activity (synthesized table populated during metadata load).

Naming convention is `Get<Domain><Shape>Query[ForStream]`. When you
add a new system query, add a method to `QueryFactory` that returns a
`std::string`, build it from `Builder::Select` + the matching
`*Format` struct, and call it from the relevant `RocprofDatabase`
worker.

### 8.3 `ComputeQueryFactory` (`rocprofvis_db_compute.h`)

The compute-side counterpart. One method per
`rocprofvis_db_compute_use_case_enum_t`:

- `GetComputeListOfWorkloads`
- `GetComputeWorkloadRooflineCeiling`
- `GetComputeWorkloadTopKernels`
- `GetComputeWorkloadKernelsList`
- `GetComputeWorkloadMetricsDefinition`
- `GetComputeWorkloadMetricValueNames`
- `GetComputeKernelRooflineIntensities`
- `GetComputeKernelMetricCategoriesList`
- `GetComputeMetricCategoryTablesList`
- `GetComputeMetricValues`
- `GetComputeMetricValuesByWorkload`
- `GetComputeKernelMetricsMatrix`

Internal helpers: `ClassifyMetricIdFormat(s)` decides whether a
metric ID is `XY`, `XYZ`, or `Other`; `ParseMetricParam(...)`
splits the `"category.table.entry:value_name"` selector into a set
of metric IDs.

### 8.4 `BuildTableQuery` flow

The View / controller calls `rocprofvis_db_build_table_query(...)`
(public ABI). The model dispatches via `Database::BuildTableQuery`
to either `ProfileDatabase::BuildTableQuery` (the four system use
cases) or `ComputeDatabase::BuildTableQuery` (asserts because compute
does not produce per-track tables - those go through
`BuildComputeQuery` instead).

Args passed all the way through:

- `start`, `end` - time window in ns.
- `num_tracks`, `tracks` - subset of track IDs to include.
- `where`, `filter`, `group`, `group_cols` - SQL fragment overrides.
- `sort_column`, `sort_order` - per-page sorting.
- `num_string_table_filters`, `string_table_filters` - free-text
  search; the database resolves these via `BuildTableStringIdFilter`
  which finds matching string IDs and rewrites them into a
  `WHERE IN (...)`.
- `max_count`, `offset` - paging.
- `count_only` - return a `SELECT COUNT(*) ...` shape.

The output is a `char* out_query` the caller is responsible for
freeing.

## 9. PackedTable, Expression Filter, Aggregation

### 9.1 `PackedRow` / `PackedTable` (`rocprofvis_db_packed_storage.h`)

The model layer's column-oriented in-memory schema. A `PackedRow` is
a `vector<uint8_t>` with templated `Set<T>(offset, value)` /
`Get<T>(offset)` helpers; rows are uniform-sized, columns are
described by `ColumnDef { name, orig_index, ColumnType, offset,
schema_index }`.

`PackedTable` glues columns + rows + an optional
`Aggregation` layer:

```cpp
class PackedTable {
public:
    void  AddColumn(name, ColumnType, sql_index, schema_index);
    void  AddMergedColumn(name, op, ColumnType, offset, schema_index);
    void  AddRow();
    void  PlaceValue(col, double|uint64_t value);

    Numeric GetMergeTableValue(uint8_t op, row, col, ProfileDatabase*) const;
    uint8_t GetOperationValue(row) const;

    void  RemoveDuplicates();
    void  CreateSortOrderArray();
    void  SortByColumn(db, column_name, ascending);

    bool  SetupAggregation(agg_spec, num_threads);
    void  FinalizeAggregation();
    void  ClearAggregation();
    void  AggregateRow(db, row_index, map_index);
    void  SortAggregationByColumn(db, column, ascending);

    void  Merge(vector<unique_ptr<PackedTable>>&);
    void  ManageColumns(vector<unique_ptr<PackedTable>>&);
    void  RemoveRowsForSetOfTracks(selected, unselected, remove_all);

    static const char* ConvertSqlStringReference(db, col, idx, node, &numeric_string);
    static uint8_t     ColumnTypeSize(ColumnType);
    void               ResetTrackIdetifiers();
};
```

The `Aggregation` struct (in the same header) holds the running
state for SQL-style `Count/Sum/Avg/Min/Max` over groups:
`column_def`, `agg_params`, `aggregation_maps`, `result`,
`sort_order`, and a per-aggregation `StringTable` for interning
group keys.

`MergedColumnDef` lets a single logical column store different types
per operation (e.g. "duration" is `uint64_t` for events but
`double` for synthesized samples).

### 9.2 `FilterExpression` (`rocprofvis_db_expression_filter.h`)

The string expression language used for table-row filtering and the
compute pivot table's per-column filter. The recipe:

1. Produce a `Tokenizer` over the input string.
2. Call `FilterExpression::Parse(expr)` to get a tree of
   `Node`/`Condition`/`ExprNode`. Three operator kinds:
   - **Logic** (`And`, `Or`, `Not`, `None`).
   - **Comparison** (`Equal`, `NotEqual`, `Less`, `LessEqual`,
     `Greater`, `GreaterEqual`, `Like`).
   - **Arithmetic** inside `ExprNode` (`Add`, `Sub`, `Mul`, `Div`,
     `Mod`, `Function`, plus `ConstantNumber`, `ConstantString`,
     `Column`).
3. `Evaluate(row)` against an
   `unordered_map<column_name, variant<double, std::string>>`
   returns `bool`.
4. Custom functions can be registered via
   `FilterExpression::RegisterFunction(name, handler)`.
5. SQL-style aggregations are parsed via
   `ParseAggregationSpec(line)` returning a list of
   `SqlAggregation { column, command (Count|Avg|Min|Max|Sum), public_name }`.

`MatchLike(text, pattern)` supports SQL `%` and `_` wildcards.

The expression layer is intentionally schema-agnostic - any caller
that wants to filter rows hands it a row-as-map representation.
`PackedTable` uses the parsed expression to filter without having
to round-trip through SQLite.

### 9.3 `TableProcessor` (`rocprofvis_db_table_processor.h`)

The mediator between SQL execution and the in-memory `PackedTable`s.
Per `ProfileDatabase` there are four `TableProcessor`s, one for each
`rocprofvis_db_compound_table_type` (event / sample / search /
analysis), so concurrent queries against different "kinds" of tables
do not stomp on each other's caches.

Highlights:

- **Compound-query syntax:** `IsCompoundQuery(query, &queries,
  &tracks, &commands)` walks the SQL string for embedded
  `-- CMD: <name> <param>` directives and the explicit
  multi-track / multi-guid format used by combined views.
- **`ExecuteCompoundQuery(future, queries, tracks, commands,
  handle, type, query_updated)`** drives a multi-stage pipeline:
  run each per-node query in parallel into a `PackedTable`, merge
  via `m_merged_table.Merge(...)`, apply filter / sort / group via
  the embedded commands, then emit into the public `Table` (or to
  CSV for export).
- **`RestartableTimer m_timer`** clears the cached
  `m_merged_table` and `m_tracks` after a configurable idle window
  (default 1s) so the same table query coming back later can avoid
  re-fetching, but a long pause does not pin memory.

### 9.4 `DatabaseCache`, `TableCache`, `StringTable`
File: `rocprofvis_db_cache.h`.

- **`TableCache`** is one cached "non-essential info" table:
  columns + rows + index maps. Used to memoize lookups like
  "node info", "agent info", "queue info", "process info",
  "thread info", etc.
- **`DatabaseCache`** is the per-DB-node container of `TableCache`s.
  Adds rows / cells / columns, returns cells by `(table_name, row_id,
  column_name)`. Drives the
  `PopulateTrackExtendedDataTemplate` and
  `PopulateTrackTopologyData` flows that fill out track ext-data and
  the topology tree from cached tables.
- **`StringTable`** is the database-side string interner - distinct
  from the View-side `RocProfVis::Controller::StringTable`. Maps
  `string -> uint32_t id` and back. Used by `TrackLookup` and
  `Aggregation` to keep group keys cheap.

### 9.5 `TrackLookup` (`rocprofvis_db_track.h`)

Hash-keyed lookup from a `(id_process, id_subprocess, id_stream,
db_instance, category)` tuple to a track index. Backs:

- `AddTrack(ids, db_instance, track)` during metadata load.
- `FindTrack(...)` overloads (by full identifiers, by string
  subprocess, by category mask).
- `FindTrackParamsIterator(ids, db_instance)` returning the iterator
  into the database's `m_track_properties` vector.
- `SearchCategoryMaskLookup(op)` returning the broad category mask
  to match per-operation track types.

`StringTable m_string_lookup` interns identifier strings so the
comparison work in `TrackKeyHash` stays integer-only.

## 10. Metadata Versioning & Cleanup

ROCm Optiq stamps each trace with a private `roc_optiq_metadata`
table that lists the auxiliary tables it has built (event-level
caches, histogram, track info, memory activity, memory allocate)
and their versions. This is what makes "open the same trace in a
newer version of the tool" safe: the model layer detects stale
auxiliary tables and rebuilds them.

### 10.1 `DatabaseVersion` (`rocprofvis_db_version.h`)

Tracks a database's schema-version string
(`SetVersion("X.Y.Z")`, `IsVersionEqual`,
`IsVersionGreaterOrEqual`, `GetMajor/Minor/PatchVersion`). Used by
the rocprof / compute query factories to switch between query shapes
when the underlying schema changes.

### 10.2 `MetadataVersionControl`

Abstract base. Holds the list of "our" auxiliary tables
(`m_roc_optiq_table_properties`), each described by:

```cpp
struct roc_optiq_metadata_t {
    std::string                 name;        // SQL table name
    roc_optiq_table_type        type;        // PerFile vs PerGuid
    roc_optiq_table_trim_type   trim_type;   // CopyWhenTrimmed vs DisposeWhenTrimmed
    uint16_t                    dependency;  // bitmask of depended-on tables
    uint32_t                    version;     // current schema version
    uint64_t                    hash;        // schema hash for invalidation
    std::map<uint32_t, bool>    rebuild;     // per-DB-node rebuild flag
};
```

Public surface:

- `VerifyRocOptiqTablesVersions(future)` - reads
  `roc_optiq_metadata`, decides which tables need rebuilding (by
  version + hash + dependencies + per-node flag), and stamps the
  `rebuild` map.
- `DropAllRocOtiqTables(future, file_node_id)` - blow them all
  away.
- `CleanupDatabase(future, rebuild)` - the public worker behind
  `rocprofvis_db_cleanup_async`.
- `MustRebuild(file_node_id, table_id)` - cheap inline check used
  during async metadata load.
- `DisposeTableWhenTrimming(name)` - whether a table should be
  dropped (vs copied) when saving a trimmed trace.
- The base names protected from being misidentified as user tables:
  `event_levels_`, `histogram_`, `track_info_`, `roc_optiq`.

Subclasses:

- **`RocprofMetadataVersionControl`** lists nine tables (memory
  activity, memory allocate, track info, four per-domain level
  tables, histogram, region-sample level) and a dependency bitmask
  family (`kRocOptiqTableDependentOn*`).
- **`RocpdMetadataVersionControl`** is the leaner legacy version
  with four tables (track info, kernel-dispatch level, region level,
  histogram) and a similar dependency mask.

When you add a new derived table:

1. Append a value to the subclass's `roc_optiq_tables` enum.
2. Add an entry to `m_roc_optiq_table_properties` with the right
   version constant from `roc_optiq_table_version_t`.
3. Update `kRocOptiqTableDependentOn*` masks if needed.
4. Implement the rebuild step in the matching `RocprofDatabase` /
   `RocpdDatabase` worker (`CreateMemoryActivityTable`,
   `LoadMemoryActivityData`, `BuildHistogram`, `SaveTrackProperties`,
   `CalculateEventLevels`, etc.).

### 10.3 Cleanup vs trim

There are two distinct flows:

- **Cleanup** (`rocprofvis_db_cleanup_async`,
  `Database::CleanupAsync` -> `CleanupStatic` -> per-subclass
  `Cleanup`) runs `MetadataVersionControl::CleanupDatabase` to
  drop / rebuild stale `roc_optiq_*` tables. The `rebuild` flag
  forces a full rebuild even if versions match.
- **Trim save** (`rocprofvis_db_trim_save_async`,
  `Database::SaveTrimmedDataAsync` -> `SaveTrimmedDataStatic` ->
  per-subclass `SaveTrimmedData`) writes a new SQLite file
  containing only the time range `[start, end]`. Tables marked
  `kRocOptiqTableCopyWhenTrimmed` are copied; those marked
  `kRocOptiqTableDisposeWhenTrimmed` are dropped from the new file
  and rebuilt on next open.

## 11. CFFI / Python Bindings

`src/model/python/rocprofvis_cffi_build.py` builds a Python wrapper
for the model layer using CFFI's API mode:

```python
ffi.cdef(open("../inc/rocprofvis_interface_types.h").read())
ffi.cdef(open("../inc/rocprofvis_interface.h").read())
ffi.set_source("cffi_lib", '#include "..."', libraries=["rocprofvis-datamodel"], ...)
ffi.compile()
```

The two interface headers are kept compatible with the existing CFFI
build script. Simple includes / defines are present in
`rocprofvis_interface_types.h`; `rocprofvis_interface.h` also has an
existing C++-style default argument on `rocprofvis_db_future_alloc`.
Avoid conditional compilation and validate changes with
`rocprofvis_cffi_build.py`. The C/C++ wrapper variants live in
`rocprofvis_c_interface.h` for direct C/C++ consumers.

`src/model/python/rocprofvis_cffi_test.py` covers the Python side.

If you change a public ABI signature:
- Update `rocprofvis_interface.h` (single C-overload only).
- Update `rocprofvis_c_interface.h` if a typed-getter overload is needed.
- Re-run `python rocprofvis_cffi_build.py` to regenerate the bindings.
- The CFFI mode requires `rocprofvis-datamodel` (the built shared
  library) to be on the linker path; the build script's `lib_dirs`
  list points at the right output folder per platform.

## 12. Coding Conventions Specific to the Model Layer

These supplement `CODING.md`. When the two disagree, `CODING.md` wins.

- **Public ABI headers must remain compatible with the CFFI build
  script.** Avoid conditional compilation in `rocprofvis_interface.h`
  and `rocprofvis_interface_types.h`. Simple `#include` / `#define`
  usage already exists in `rocprofvis_interface_types.h`, and
  `rocprofvis_interface.h` already has one default argument. No
  `extern "C"` in those headers (the wrapper
  `rocprofvis_c_interface.h` adds it).
- **Implementation is C++17.** Namespace
  `RocProfVis::DataModel`. `using namespace` is forbidden in headers.
- **Locking.** Every mutable cross-thread state lives behind a
  `std::shared_mutex` (or `std::mutex` for short critical sections).
  Use `TimedLock<LockType>` so contention shows up in debug logs.
- **No new top-level async path outside the `Future` system.** Async
  work goes through `Database::*Async` -> `Database::*Static` ->
  `Future::SetWorker(std::move(thread))`. Internal worker fan-out
  and `TableProcessor::RestartableTimer` are existing exceptions.
- **No exceptions across the C ABI.** `try { ... } catch(...) {}`
  blocks live at the boundary; everywhere else, return
  `rocprofvis_dm_result_t`.
- **Logging:** `spdlog::info/warn/error`, never `printf` or
  `iostream`.
- **String handling:** any high-cardinality string goes through the
  appropriate `StringTable`. `Trace::m_strings` is the per-trace
  string table; `Database::m_string_table` is the schema-wide table;
  `TrackLookup::m_string_lookup` is the identifier table; the
  `DatabaseCache::TableCache` maps store column / cell strings.
- **`rocprofvis_dm_event_id_t`:** always pack/unpack via the
  union's `bitfield`. Do not `<< 60` on a `uint64_t` directly.
- **NULL handling:** when adding a new SQL callback, populate the
  appropriate static `s_null_data_exception_*` map for columns that
  may be NULL but have a defaultable value. Never trust raw
  `azColName[i]` to be non-null in the absence of an exception.
- **Compound queries:** any new "this is a multi-stage table"
  query should embed its instructions via the `-- CMD: name param`
  prefix and let `TableProcessor::IsCompoundQuery` parse them.
- **Asserts:** `ROCPROFVIS_ASSERT` for runtime,
  `ROCPROFVIS_ASSERT_ALWAYS_MSG_RETURN(msg, retval)` for the "error
  path that also returns from the caller" pattern used heavily in
  the SQLite layer. Asserts must be side-effect free.
- **Track parameters are shared, not owned.** `Track::m_track_params`
  is a non-owning pointer into `Database::m_track_properties`. Do not
  delete it from the trace side.
- **InfoTable wrappers are non-owning.** They forward through the
  binding callbacks to the database's actual storage; do not free
  the underlying handle.

## 13. Reuse Catalog (model edition)

| You want to...                                                    | Use this                                                                                  |
|-------------------------------------------------------------------|-------------------------------------------------------------------------------------------|
| Sniff a trace file's flavor                                       | `rocprofvis_db_identify_type(filename)` -> `rocprofvis_db_type_t`                         |
| Open a trace                                                      | `rocprofvis_db_open_database(path, type)` (`kAutodetect` if you don't know)               |
| Create a paired trace handle                                      | `rocprofvis_dm_create_trace()` + `rocprofvis_dm_bind_trace_to_database`                   |
| Run anything async                                                | `rocprofvis_db_future_alloc(callback, ud)` + `rocprofvis_db_future_wait(future, t)`       |
| Compose a DB future with a controller future                      | `Future::AddDependentFuture(db_future)` + `Future::ProgressCallback`                      |
| Read trace metadata                                               | `rocprofvis_db_read_metadata_async(database, future)`                                     |
| Read a time slice                                                 | `rocprofvis_db_read_trace_slice_async(database, start, end, tag, num, tracks, future)`         |
| Read flow / stack / ext data for an event                         | `rocprofvis_db_read_event_property_async(database, type, event_id, future)`               |
| Build a system table query                                        | `rocprofvis_db_build_table_query(...)` (then `rocprofvis_db_execute_query_async`)         |
| Build a compute query                                             | `rocprofvis_db_build_compute_query(...)` (then `rocprofvis_db_execute_compute_query_async`)|
| Export a query as CSV                                             | `rocprofvis_db_export_table_csv_async(database, query, file_path, future)`                |
| Save a trimmed trace                                              | `rocprofvis_db_trim_save_async(database, start, end, new_path, future)`                   |
| Drop / rebuild stale aux tables                                   | `rocprofvis_db_cleanup_async(database, future, rebuild)`                                  |
| Get a property                                                    | `rocprofvis_dm_get_property_as_<uint64\|int64\|double\|charptr\|handle>(handle, prop, idx)` |
| Discover an object's property bank                                | `rocprofvis_dm_<class>_property_t` enum (see section 2.8)                                 |
| Pack/unpack an event id                                           | `rocprofvis_dm_event_id_t::bitfield`                                                      |
| Pack/unpack track + op for the table query API                    | `TABLE_QUERY_PACK_OP_TYPE` / `TABLE_QUERY_UNPACK_OP_TYPE` / `TABLE_QUERY_UNPACK_TRACK_ID`  |
| Add a new database flavor                                         | Subclass `Database` (or `SqliteDatabase` / `ProfileDatabase`); register a `rocprofvis_db_type_t` |
| Add a new SQL callback                                            | `RpvSqliteExecuteQueryCallback` signature; register null-exception entries if needed      |
| Add a new query                                                   | New method on `QueryFactory` / `ComputeQueryFactory`; build with `Builder::Select(...)`   |
| Build a SELECT with filters                                       | `Builder::Where(name, condition, value)` / `Builder::Concat({...})`                       |
| Add a typed column to the canonical schema                        | Append to `table_view_schema_index_t` and `table_view_schema` map                         |
| Filter rows in memory                                             | `FilterExpression::Parse(expr)` + `Evaluate(row_map)`                                     |
| Aggregate a `PackedTable`                                         | `PackedTable::SetupAggregation(spec, num_threads)` + `AggregateRow` + `FinalizeAggregation` |
| Sort a `PackedTable`                                              | `PackedTable::CreateSortOrderArray()` + `SortByColumn(db, col, ascending)`                |
| Cache a per-node info table                                       | `DatabaseCache::AddTableColumn` / `AddTableRow` / `AddTableCell`                          |
| Resolve `(process_id, sub_id, db_inst) -> track`                  | `TrackLookup::FindTrack(...)`                                                             |
| Intern a string in the trace                                      | `Trace::AddString(value)` (binding callback) or `StringTable::ToInt(s)` for DB-side       |
| Allocate a record in a slice's pool                               | `EventTrackSlice::AddRecord` / `PmcTrackSlice::AddRecord` (do not `new EventRecord` raw)  |
| Add a new topology level                                          | Subclass `TopologyNode`; implement `GetType` / `GetLevelId` / `GetLevelTag` / properties map |
| Track a derived table for cleanup                                 | Append to `roc_optiq_tables` enum; add `roc_optiq_metadata_t`; bump version when schema changes |
| Mark a table for trim disposal                                    | Set `roc_optiq_table_trim_type` to `kRocOptiqTableDisposeWhenTrimmed`                     |
| Time a lock for diagnostics                                       | Use `TimedLock<std::unique_lock<std::shared_mutex>>` instead of bare `unique_lock`        |
| Stash per-future state across stages                              | `Future::SetRuntimeStorageValue(key, v)` / `GetRuntimeStorageValue(key, fallback)`        |

## 14. Common Pitfalls

- **Spawning a new top-level async thread outside the `Future`
  system.** Always go through `Database::*Static` +
  `Future::SetWorker`. Existing internal fan-out threads and
  `TableProcessor::RestartableTimer` are special cases; do not add
  another without following their cancellation / join patterns.
- **Forgetting to release a SQLite connection.** Every
  `GetConnection` must be paired with `ReleaseConnection` even on
  early exit / exception. Use a guard helper if your code path is
  branchy.
- **Including `src/model/` headers from `src/view/`.** Forbidden by
  the layered architecture; the View must go through
  `src/controller/` only. The model must not include View headers or
  controller implementation (`src/`) headers; its existing dependency
  on public shared controller enum headers is intentional.
- **Calling `delete` on `EventRecord` / `PmcRecord`.** They live in
  the slice's `MemoryPool`; deleting them corrupts the pool. The
  `TrackSlice::Cleanup()` path returns the records to the pool.
- **Mutating the binding struct after `BindTrace`.** Once the
  database has the pointer, every callback assumes the slot is
  still valid. Bind-time is the only time it is safe to write.
- **Adding conditional preprocessor logic to
  `rocprofvis_interface.h`.** Breaks the CFFI parse path.
  Conditional code goes in `rocprofvis_c_interface.h` or in
  implementation files; run `rocprofvis_cffi_build.py` after any
  public signature change.
- **Using `rocprofvis_db_data_type_t::kRPVDataTypeBlob` without
  considering the trim path.** Blob columns need special handling
  in `SaveTrimmedData` to avoid being silently truncated.
- **Forgetting to update `MetadataVersionControl` when changing a
  derived table's schema.** The next user opening the trace will see
  stale data; bump the matching `kRocOptiqTableVersion*` constant.
- **Mixing system and compute calls on the same database.**
  Compute databases assert on slice / event-property / table-build
  calls; system databases assert on compute-query calls. Always
  check `rocprofvis_db_identify_type` before deciding which API to
  drive.
- **Running a long callback without checking `Future::Interrupted()`.**
  Cancellation cannot be honored. Add a periodic
  `if (future->Interrupted()) return SQLITE_ABORT;` to any new
  long-running callback.
- **Touching `m_strings` directly.** Adding to the trace string
  table must go through `Trace::AddString` so the sorted-order
  lookup array gets rebuilt and the binding callback sees the new
  index.
- **Cloning DDL inline.** Use `CreateSQLTable(name, params, rows,
  insert_lambda)` so all schema operations go through the same
  primitives that respect transactions and connection pooling.

## 15. Testing

Two Catch2 binaries live in `src/model/src/tests/` (built when
`BUILD_TESTING` is on; `src/model/CMakeLists.txt` wires them up):

- **`datamodel-system-tests`** -
  `src/model/src/tests/rocprofvis_dm_system_tests.cpp`. Runs against
  `sample/trace_70b_1024_32.rpd` and `sample/rocpd-transpose.db`. It
  exercises the full open + bind + read-metadata + read-slice +
  read-event-property + table-query flow plus cleanup and trim.
- **`datamodel-compute-tests`** -
  `src/model/src/tests/rocprofvis_dm_compute_tests.cpp`. Runs against
  `sample/rocprof_compute_23ed6f36.db`. Validates workload list, top
  kernels, kernel + metric matrix, roofline ceilings, metric values,
  and the pivot table flow.

Both accept `--input_file <path>` (Catch2 + Clara). When you add a
new public ABI surface or a new database adapter, add a matching
test case here. The companion controller tests
(`src/controller/tests/`, see `.agents/CONTROLLER.md` section 13) run
the same fixtures through the controller; if your change spans
layers, update both.

There is also a freestanding `datamodel-test` driver wired up by the
top-level CMakeLists (`src/model/src/test/main.cpp`) used for
exploratory testing during development.

## 16. Quick Reference Index

### Public ABI (`src/model/inc/`)

- `rocprofvis_interface.h` -> all `rocprofvis_db_*` /
  `rocprofvis_dm_*` C functions.
- `rocprofvis_interface_types.h` -> all opaque handle typedefs,
  `rocprofvis_dm_event_id_t`, every `*_property_t` enum,
  `rocprofvis_db_type_t`, `rocprofvis_db_data_type_t`,
  `rocprofvis_dm_track_category_t`, etc.
- `rocprofvis_c_interface.h` -> C/C++ wrapper with
  `rocprofvis_dm_get_property_as_*` overloads (status-by-return).
- `rocprofvis_shared_types.h` -> shared property bank enums for
  topology objects (used by both controller and model).

### Common (`src/model/src/common/`)

- `rocprofvis_common_types.h` -> `rocprofvis_db_record_data_t`,
  `rocprofvis_dm_track_params_t`, `rocprofvis_dm_track_identifiers_t`,
  `rocprofvis_dm_trace_params_t`, `rocprofvis_db_flow_data_t`,
  `rocprofvis_db_stack_data_t`, `rocprofvis_db_ext_data_t`,
  `rocprofvis_db_argument_data_t`, the `rocprofvis_dm_db_bind_struct`,
  `DbInstance`, and `TRACK_ID_*` constants.
- `rocprofvis_error_handling.h` -> ANSI color macros + `ERROR_*`
  message strings.
- `rocprofvis_c_interface.cpp` -> `extern "C"` entry points; the
  `Detect` -> `new <Database>` dispatch in
  `rocprofvis_db_open_database`.

### Database (`src/model/src/database/`)

- `rocprofvis_db.h` -> `Database` base, `OrderedMutex`,
  `DbInstance`, `TemporaryDbInstance`, `SingleNodeDbInstance`,
  `rocprofvis_db_string_id_t`.
- `rocprofvis_db_sqlite.h` -> `SqliteDatabase`, `MAX_CONNECTIONS`,
  `RpvSqliteExecuteQueryCallback`, `rocprofvis_db_sqlite_db_node_t`,
  `rocprofvis_db_sqlite_callback_parameters`, the seven
  `ExecuteSQLQuery` overloads, `Sqlite3Column*` helpers.
- `rocprofvis_db_profile.h` -> `ProfileDatabase`, the four
  `TableProcessor`s, `m_event_levels`, the long callback list
  (`CallBackAddTrack`, `CallbackAddFlowTrace`,
  `CallbackAddStackTrace`, `CallbackAddEssentialInfo`,
  `CallbackAddArgumentsInfo`, `CalculateEventLevels`,
  `CallbackMakeHistogramPerTrack`, `CallBackLoadHistogram`,
  `CallbackTrimTableQuery`, etc.), `BuildHistogram`,
  `s_histogram_schema_params`.
- `rocprofvis_db_rocpd.h` -> `RocpdDatabase`, single-node legacy
  schema, string-index remap, `s_rocpd_categorized_data`,
  `s_level_schema_params`.
- `rocprofvis_db_rocprof.h` -> `RocprofDatabase`, multinode support,
  memory activity tracking, `kRPVMemActivity*` /
  `kRPVMemLevel*` enums, `s_rocprof_categorized_data`,
  `s_mem_activity_schema_params`, `s_level_schema_params`.
- `rocprofvis_db_compute.h` -> `ComputeDatabase`,
  `ComputeQueryFactory`, `MetricIdFormat`, `KernelStats`,
  `MetricSelector`, `MetricRow`, `KernelMetricsRow`.
- `rocprofvis_db_query_builder.h` -> `Builder` (string DSL), all
  `*_format` query structs, `table_view_schema_index_t`,
  `table_view_schema`.
- `rocprofvis_db_query_factory.h` -> `QueryFactory` (rocprof system
  domain queries).
- `rocprofvis_db_packed_storage.h` -> `PackedRow`, `PackedTable`,
  `ColumnDef`, `MergedColumnDef`, `Aggregation`, `ColumnAggr`,
  `Numeric`, `NumericWithType`, `ColumnType`,
  `rocprofvis_db_sqlite_track_identifier_index_t`.
- `rocprofvis_db_expression_filter.h` -> `Tokenizer`,
  `FilterExpression`, `SqlAggregation`, `Operator`, `LogicOp`,
  `SqlCommand`.
- `rocprofvis_db_table_processor.h` -> `TableProcessor`,
  `RestartableTimer`, `rocprofvis_db_compound_table_type`,
  `rocprofvis_db_compound_query_command`,
  `rocprofvis_db_compound_query`, `QUERY_COMMAND_TAG`.
- `rocprofvis_db_cache.h` -> `TableCache`, `DatabaseCache`,
  `StringTable`.
- `rocprofvis_db_track.h` -> `TrackLookup`, `TrackKey`, `TrackEntry`.
- `rocprofvis_db_future.h` -> `Future` (DataModel namespace),
  `RuntimeValue`, runtime-storage keys.
- `rocprofvis_db_version.h` -> `DatabaseVersion`,
  `MetadataVersionControl`, `RocprofMetadataVersionControl`,
  `RocpdMetadataVersionControl`, `roc_optiq_metadata_t`,
  `roc_optiq_table_type`, `roc_optiq_table_trim_type`,
  `roc_optiq_table_version_t`,
  `roc_optiq_table_dependency_mask_t`.

### Data model (`src/model/src/datamodel/`)

- `rocprofvis_dm_base.h` -> `DmBase`, `TimedLock<LockType>`,
  `LOCK_*_WARNING_TIME_LIMIT_US` macros.
- `rocprofvis_dm_trace.h` -> `Trace`, the binding callbacks,
  `event_level_map_t`.
- `rocprofvis_dm_track.h` -> `Track`.
- `rocprofvis_dm_track_slice.h` -> `TrackSlice`, `MemoryPool`,
  `kMemPoolBitSetSize`.
- `rocprofvis_dm_event_track_slice.h` -> `EventTrackSlice`.
- `rocprofvis_dm_pmc_track_slice.h` -> `PmcTrackSlice`.
- `rocprofvis_dm_event_record.h` -> `EventRecord` (POD).
- `rocprofvis_dm_pmc_record.h` -> `PmcRecord` (POD).
- `rocprofvis_dm_table.h` -> `Table`, `InfoTable`.
- `rocprofvis_dm_table_row.h` -> `TableRow`, `InfoTableRow`.
- `rocprofvis_dm_topology.h` -> `TopologyNode` and its 14+
  subclasses (`TopologyNodeRoot`, `TopologyNodeSystemNode`,
  `TopologyNodeRPDSystemNode`, `TopologyNodeProcess`,
  `TopologyNodeProcessor`, `TopologyNodeThread`,
  `TopologyNodeThreadInstrumented`, `TopologyNodeThreadSampled`,
  `TopologyNodeQueue`, `TopologyNodeMemoryAllocation`,
  `TopologyNodeMemoryCopy`, `TopologyNodeKernelDispatch`,
  `TopologyNodeStream`, `TopologyNodeCounter`,
  `TopologyReferenceNode`, `TopologyNodeDownStreamProcessor`,
  `TopologyNodeDownStreamQueue`).
- `rocprofvis_dm_flow_trace.h` -> `FlowTrace`.
- `rocprofvis_dm_flow_record.h` -> `FlowRecord` (POD).
- `rocprofvis_dm_stack_trace.h` -> `StackTrace`.
- `rocprofvis_dm_stack_record.h` -> `StackFrameRecord` (POD).
- `rocprofvis_dm_ext_data.h` -> `ExtData`.
- `rocprofvis_dm_ext_data_record.h` -> `ExtDataRecord`,
  `ArgumentRecord` (POD).

### Tests (`src/model/src/tests/`)

- `rocprofvis_dm_system_tests.cpp`
- `rocprofvis_dm_compute_tests.cpp`

### Python (`src/model/python/`)

- `rocprofvis_cffi_build.py`
- `rocprofvis_cffi_test.py`

---

**End of DATABASE.md.** When you change the model layer, update the
section that covers the area you touched (especially section 16 and
the Reuse Catalog). Keep this file the single source of truth for
"where does X live in the model layer" and "what should I reuse
instead of writing it." If you find a duplicate of something listed
here, prefer to delete the duplicate and route callers through the
canonical entry.
