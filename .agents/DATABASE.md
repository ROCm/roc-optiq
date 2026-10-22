# Database and data model

`src/model/` opens trace files, builds their SQL, and holds the in-memory trace that the controller reads through a C ABI. Read it before changing `src/model/` or the `rocprofvis_db_*` and `rocprofvis_dm_*` functions.

## Overview

The `datamodel` library: SQLite-backed database adapters and the in-memory `Trace` they fill, behind a C ABI that only the controller calls.

**Code:** `src/model/inc/` (public C ABI), `src/model/src/common/`, `src/model/src/database/`, `src/model/src/datamodel/`, `src/model/src/tests/`, `src/model/python/`

**Related:** [Layers and boundaries](ARCHITECTURE.md#layers-and-boundaries) (include rules), [Controller overview](CONTROLLER.md#overview) (the only caller), [Tests](ARCHITECTURE.md#tests) (targets and fixtures)

- The model may include `src/core/`, SQLite, spdlog, jsoncpp, yaml-cpp, and the controller's public `rocprofvis_controller_enums.h`, never controller `src/` or View headers. It exports `rocprofvis_shared_types.h` (topology property enums), which the controller re-exports to the View.
- Each async call runs on one worker thread owned by its [database future](#database-futures). A worker that fans out registers sub-workers with `parent->AddSubFuture()`, so cancellation reaches them, and ends with `WaitAndDeleteSubFuture`. Each `TableProcessor` also owns a `RestartableTimer` thread.
- Every handle-visible object derives from `DmBase` and exposes its `std::shared_mutex` through `Mutex()`. The C getters take it shared, so writers take it unique, through `TimedLock<LockType>(mutex, __func__, this)` with a non-null `DmBase*` owner. `TimedLock` logs at `spdlog::debug` when a wait passes `LOCK_WAITED_TOO_LONG_WARNING_TIME_LIMIT_US` (5 ms) or a hold passes `LOCK_HELD_TOO_LONG_WARNING_TIME_LIMIT_US` (500 ms), both in `rocprofvis_dm_base.h`.
- A new ABI function or adapter gets a case in `datamodel-system-tests` or `datamodel-compute-tests`. The CMake option `TEST` builds with `-DTEST -DDEBUG`, which adds `GetPropertySymbol` and makes `rocprofvis_db_future_wait` timeouts 100 times longer.

## C ABI and Python bindings

The C functions in `src/model/inc/` that open databases, run async reads into a bound trace, build queries, and read properties.

**Code:** `src/model/inc/`, `src/model/src/common/rocprofvis_c_interface.cpp`, `src/model/src/database/rocprofvis_db_future.{h,cpp}`, `src/model/python/`

**Related:** [C ABI and properties](CONTROLLER.md#c-abi-and-properties) (the ABI built on this one), [Building blocks](CONTROLLER.md#building-blocks) (controller futures), [Data model](#data-model) (what the reads fill)

### Headers and handles

- `rocprofvis_interface.h` and `rocprofvis_interface_types.h` are the bare headers the CFFI script parses: no `extern "C"`, overloads, or new preprocessor conditionals. C and C++ code includes `rocprofvis_c_interface.h` (which adds `extern "C"` and the status-returning getters) and `rocprofvis_c_interface_types.h`.
- Handles are `void*`, cast without a type check: system calls cast to `SystemDatabase*`, compute calls to `ComputeDatabase*`, which is not a `SystemDatabase`. The wrong family is undefined behavior, so branch on `rocprofvis_db_identify_type`.
- Order: `rocprofvis_dm_create_trace`, `rocprofvis_db_open_database(path, kAutodetect)`, `rocprofvis_dm_bind_trace_to_database(trace, db, config_path)`, then `rocprofvis_db_read_metadata_async`; other reads fail until metadata has loaded. `rocprofvis_dm_delete_trace` deletes the database too, so finish or cancel every future first. `rocprofvis_db_open_database_multi` opens several files as one multinode `RocprofDatabase` ([Trace compare](FEATURES.md#trace-compare)).
- Results stay in the trace until a `rocprofvis_dm_delete_*` call ([Memory and segments](CONTROLLER.md#memory-and-segments) drives slice deletion). Slice keys hash the range with a tag (`rocprofvis_dm_hash_combine_timestamp`), so `kRocProfVisDmHashedTimestampTagTrackSlice` and `kRocProfVisDmHashedTimestampTagAnalysis` reads of one range stay separate.
- `rocprofvis_db_execute_query_async` and `rocprofvis_db_execute_compute_query_async` key the result table by a hash of the SQL and return an existing one without running; copy the rows, then call `rocprofvis_dm_delete_table_at`. Both keep the `query` pointer, so keep the string alive until the future completes.
- The value-returning `rocprofvis_dm_get_property_as_*` overloads return 0 or `nullptr` on failure; use the status form when 0 is valid.
- `rocprofvis_dm_event_id_t` packs a 52-bit row id, an 8-bit instance index, and a 4-bit `rocprofvis_dm_event_operation_t` (set them through `.bitfield`). Table-query track IDs carry the operation in their top 4 bits (`TABLE_QUERY_PACK_OP_TYPE`, `TABLE_QUERY_UNPACK_OP_TYPE`, `TABLE_QUERY_UNPACK_TRACK_ID`).

### Database futures

- A `rocprofvis_db_future_t` is a `DataModel::Future`, unrelated to controller futures.
- Every `*Async` method follows `SystemDatabase::ReadTraceSliceAsync`: reject a null future, return `kRocProfVisDmResultResourceBusy` if it `IsWorking()`, call `SetPromise` at once when the binding reports the result loaded (`FuncCheckSliceExists`, `FuncCheckTableExists`, `FuncCheckEventPropertyExists`), and otherwise `future->SetWorker(std::thread(...))` inside a try/catch.
- The worker calls `SetPromise` exactly once on every path, failing with `kRocProfVisDmResultDbAbort` when `Interrupted()` and `kRocProfVisDmResultDbAccessFailed` otherwise. A skipped path hangs every infinite wait, as the early error returns in `MetadataVersionControl::CleanupDatabase` do today. Likewise, check the `rocprofvis_db_*_async` return code before waiting: a failed call leaves no promise.
- `rocprofvis_db_future_wait(future, seconds)`: `UINT64_MAX` waits forever, `0` polls (`kRocProfVisDmResultTimeout`), and other timeouts set the interrupt flag on expiry, then join the worker. A finished wait resets the future for reuse; `rocprofvis_db_future_free` on a running future interrupts and joins it.
- Cancellation is cooperative: `rocprofvis_db_future_cancel` interrupts every sub-future, calls `sqlite3_interrupt` on the connection the running query registered through `Future::LinkDatabase`, and sets the flag that callbacks poll with `Interrupted()`.
- A controller job registers each database future with `AddDependentFuture`, so cancelling the controller future cancels it, and then waits with `UINT64_MAX` on the job thread.

### Python bindings

`python/rocprofvis_cffi_build.py` does not work today: `ffi.cdef` cannot parse the C++ in `rocprofvis_interface_types.h` (`c_optional_uint64_t` constructors, an `#ifdef __cplusplus` block), and it links `rocprofvis-datamodel` while the target is `datamodel`. Add no more such syntax; validate any repair by running the script.

## Database adapters and trace formats

The classes that recognize a trace file's format, open it, and run SQL against it.

**Code:** `src/model/src/database/rocprofvis_db.h`, `rocprofvis_db_systems.{h,cpp}`, `rocprofvis_db_sqlite.{h,cpp}`, `rocprofvis_db_query_manager.{h,cpp}`, `rocprofvis_db_profile.{h,cpp}`, `rocprofvis_db_rocpd.{h,cpp}`, `rocprofvis_db_rocprof.{h,cpp}`, `rocprofvis_db_trace_processor.{h,cpp}`

**Related:** [System traces](CONTROLLER.md#system-traces) (the caller), [Query pipeline](#query-pipeline) (the SQL they run), [Versioning and cleanup](#versioning-and-cleanup) (the tables they derive)

- `SystemDatabase` adds the timeline async API and track registry (`m_track_properties`) to `Database`, `QueryManager` adds `SqliteDatabase` and the slice, table, and CSV engine, and `ProfileDatabase` adds the rocpd-family track callbacks, event levels, and histogram that `RocpdDatabase` and `RocprofDatabase` share. `GoogleTraceProcessor` extends `QueryManager`; `ComputeDatabase` is only `Database` plus `SqliteDatabase`.
- `SqliteDatabase` is not a `Database`, so a class inheriting both must override `Database::InterruptQuery` to call `InterruptAsSqlite`, or cancel never reaches SQLite.
- Run SQL only through `ExecuteSQLQuery`, `ExecuteTransaction`, `CreateSQLTable`, and `DropSQLTable`, which manage the connection pool (`GetConnection` is private); only `ExecuteSQLQuery` links the future for cancel and replaces `%GUID%` with the instance GUID. A file's pool grows until in-use connections exceed `std::thread::hardware_concurrency()`, then callers wait; `MAX_CONNECTIONS` is unused.
- Callbacks (`RpvSqliteCallback`) read columns with the `Sqlite3Column*` helpers, passing their own address as `func`: a NULL becomes the value `GetNullDataExceptionsInt` or `GetNullDataExceptionsString` registers for that callback and column (else 0 or empty), and a NULL in a column `GetNullDataExceptionsSkip` lists drops the row.
- When `Interrupted()`, return `SQLITE_ABORT` from the callback so the query ends as `kRocProfVisDmResultDbAbort`; other non-zero returns log an SQL error. An interrupted `sqlite3_step` just ends the row loop and reports success with partial rows, so check `Interrupted()` after a query too.
- `CreateSQLTable(name, params, rows, insert_func, node)` drops and recreates the table `WITHOUT ROWID` (one column must be the `PRIMARY KEY`) and inserts all rows in one transaction, the lambda binding columns from 1. It returns success even when a statement fails.
- Multinode: a database node is a file (`m_db_nodes`), an instance is a GUID inside one (`DbInstance`), and rocprof table names carry the GUID. `OrderedMutex` (`m_add_track_mutex`) adds tracks in instance order so track IDs are deterministic, and all instances must share one rocpd schema version.

`rocprofvis_db_identify_type` tries `GoogleTraceProcessor::Detect` (only with `ROCPROFVIS_PERFETTO_ENABLED`), then `ProfileDatabase::Detect`, which opens SQLite without `SQLITE_OPEN_CREATE` so probing creates nothing.

| Type | Recognized by | Adapter |
|---|---|---|
| `kRocprofMultinodeSqlite` | `.yaml` manifest whose `rocprofiler-sdk.rocpd.files` lists node files, relative to it, that each have `rocpd_event` | `RocprofDatabase` |
| `kRocprofSqlite` | SQLite with `rocpd_event` | `RocprofDatabase` |
| `kRocpdSqlite` | SQLite with an `api` table (legacy rocpd, `.rpd`) | `RocpdDatabase` |
| `kComputeSqlite` | SQLite with `compute_metadata` | `ComputeDatabase` |
| `kChromeTrace`, `kPerfettoTrace` | JSON starting with `[` or `{`; gzip or Perfetto protobuf | `GoogleTraceProcessor` |

- Rocpd region tracks are `kRocProfVisDmRegionTrack`; rocprof splits them into `kRocProfVisDmRegionMainTrack` and `kRocProfVisDmRegionSampleTrack`.
- Chrome and Perfetto sources are converted on first load by `TraceConverter::Convert` into `<config_path>/<HashFileIdentity>/perfetto/<stem>.tpdb` (through a `.tmp` file and a rename), which later opens reuse; that is why binding takes `config_path`. `GoogleTraceProcessor::Detect` reports a converted file as `kGoogleSqlite`, but nothing opens that type. These traces have no trim and no derived tables.

**Recipe: support a new trace format.**

1. Append a value to `rocprofvis_db_type_t`; it is public, so never renumber.
2. Recognize it in `ProfileDatabase::Detect` (SQLite schemas) or `GoogleTraceProcessor::Detect` (formats Perfetto converts), without creating or changing the file.
3. Write the adapter: a timeline format extends `QueryManager` (or `ProfileDatabase` for a rocpd-family schema) and implements its pure virtuals, with `GoogleTraceProcessor` as the smallest complete example; anything else derives `Database` and `SqliteDatabase` and overrides `InterruptQuery`.
4. In `ReadTraceMetadata`, add tracks, strings, and topology through the binding, set `metadata_loaded`, call `FuncMetadataLoaded`, and end with `SetPromise`.
5. Add the branch in `rocprofvis_db_open_database`, the case in `rocprofvis_controller_alloc`, the extension in `SystemTrace::Load` and `TRACE_EXTENSIONS` / `ALL_EXTENSIONS` (`rocprofvis_appwindow.cpp`), and a fixture test.

## Compute database

The rocprof-compute adapter: workloads, kernels, metrics, roofline, and PC-sampling tables, with no timeline.

**Code:** `src/model/src/database/rocprofvis_db_compute.{h,cpp}`

**Related:** [Compute traces](CONTROLLER.md#compute-traces) (the caller), [Compute ISA view](UI.md#compute-isa-view) (the PC-sampling consumer)

- `ComputeDatabase` has no slices, event properties, trim, cleanup, or derived tables. Metadata load passes `schema_version` from `compute_metadata` to `ComputeQueryFactory::SetVersion` (`0.0.0` if absent), detects the optional `memory_chart_extdata` column, adds indexes (the PC-sampling ones only if `compute_pc_sample_state` exists), and caches the lookups that metric queries resolve against.
- Callers pair `rocprofvis_db_build_compute_query` with `rocprofvis_db_execute_compute_query_async` for the same use case. Factory methods return `kRocProfVisDmResultNotSupported` below their schema gate and `kRocProfVisDmResultInvalidParameter` for wrong params: most gate at `1.2.0`, `GetComputeMetricValuesByWorkload` at `1.3.0` (it reads `compute_workload_metric_view`), PC sampling at `2.2.0`, and `GetComputeMetadata` never. Inner `1.3.0` and `1.4.0` checks only pick table names.
- Params are spliced into the SQL as text (`param_str`), and only `GetComputeKernelSourceFiles` checks that they are numbers: pass IDs, never user text.
- Most use cases fill a `Table` of string cells through `CallbackGetComputeGeneric`, which tags each column with the `rocprofvis_db_compute_column_enum_t` from `ColumnNameToEnum`; an alias missing there is silently dropped. Top kernels, the roofline ceiling, and the kernel metrics matrix (whose query is a JSON plan) have their own callbacks.
- Metric selectors are `"category.table.entry:value_name"` strings parsed by `ParseMetricParam`. Selectors that resolve to nothing give a successful zero-row query (`WHERE 0`), a request without selectors is invalid, and kernels from a second workload are ignored.
- PC sampling reads twelve schema-2.2 ISA, source, and sample tables, scoped by `compute_kernel_symbol.kernel_uuid` through `kRPVComputeParamKernelId` (`GetComputeSourceFileSourceLines` takes `kRPVComputeParamSourceFileUuid`). `issue_count` and `stall_count` keep SQL NULL as an empty cell, so host-trap's unavailable counts differ from a stochastic zero; `total_count` is coalesced to 0. Top kernels carry `has_isa_lines` so the ISA tab can start without ISA rows. No model test covers PC sampling yet.

**Recipe: add a compute query.**

1. Append a `rocprofvis_db_compute_use_case_enum_t` value, plus any new `rocprofvis_db_compute_column_enum_t` values, in `rocprofvis_interface_types.h`.
2. Add a `ComputeQueryFactory::GetCompute*` method: `kRocProfVisDmResultNotSupported` unless `IsVersionGreaterOrEqual` passes for the first schema with its tables, then `kRocProfVisDmResultInvalidParameter` until the params match, then `kRocProfVisDmResultSuccess` with the SQL written.
3. Add its case to `BuildComputeQuery`, an entry to `SupportedUseCases` (execution fails without one), and its callback in `ExecuteComputeQuery`, usually `CallbackGetComputeGeneric`.
4. Map every new SQL alias in `ColumnNameToEnum`.
5. Cover it in `rocprofvis_dm_compute_tests.cpp` with a fixture new enough to pass the gate.

## Data model

The in-memory tree a bound database fills, and the function-pointer table through which the two sides talk.

**Code:** `src/model/src/datamodel/`, `src/model/src/common/rocprofvis_common_types.h`

**Related:** [System traces](CONTROLLER.md#system-traces) (the controller's mirror of this tree), [C ABI and Python bindings](#c-abi-and-python-bindings) (lifecycle and deletion)

- A `Trace` owns `Track`s (one per lane) of `TrackSlice`s (`EventTrackSlice`, `PmcTrackSlice`) of `EventRecord`s or `PmcRecord`s, plus per-event `FlowTrace`, `StackTrace`, and `ExtData`, result `Table`s, `InfoTable`s, strings, and a `TopologyNodeRoot`. Each class answers `GetPropertyAs*` for its own property enum.
- The trace owns neither `Track::m_track_params`, which points into the database's `m_track_properties`, nor the data behind an `InfoTable`, which it reads through `FuncGetInfoTable*`.
- Records are PODs carved from per-slice `MemoryPool` blocks of `kMemPoolBitSetSize` (1024) records and die with their slice, never one by one. Their timestamps are relative to the instance's start (`db_inst_start_time`). A new record type needs a `rocprofvis_db_record_data_t` variant, a POD, and a slice subclass that uses `Allocate`.
- A slice worker calls `FuncAddSlice`, fills the slice with `FuncAddRecord`, marks it with `FuncCompleteSlice`, and on failure removes it with `FuncRemoveSlice`. A concurrent read of the same key waits in `FuncCheckSliceExists` until completion, so complete the slice on every path before removing it.
- Adapters dedupe strings before calling `FuncAddString`, which appends without a lock. `FuncMetadataLoaded` builds the sort order behind `FuncGetStringOrder` once, so strings added later have none.
- `FuncAddTopologyNode` has `TopologyNodeRoot::AddNode` create missing levels down to the leaf, whose `kRPVControllerTopologyNodeTrack` property holds the model track ID the controller's mirror tree resolves; property IDs come from `rocprofvis_shared_types.h`. A new level subclasses `TopologyNode` (`GetNodeName`, `GetLevelId`, `GetLevelTag`, `GetPropertiesMap`).
- Binding: `rocprofvis_dm_db_bind_struct` lives in the `Trace` as `m_binding_info`. On bind, `Trace::BindDatabase` fills the trace-side callbacks (`FuncAdd*`, `FuncCheck*`, `FuncGetString*`, `FuncMetadataLoaded`), `trace_object`, `trace_properties`, and `config_path`, and `Database::BindTrace` adds `FuncFindCachedTableValue` and `FuncGetInfoTable*` and keeps the pointer. Write slots only then.
- Adapters reach the trace only through `BindObject()->Func*`, and the trace never calls a `Database` method, so one `Trace` serves every adapter. A new callback needs a typedef and slot in `rocprofvis_common_types.h`, a static `Trace` or `Database` method, and an assignment in that bind function.

## Query pipeline

How system SQL is assembled, and how table results are merged, filtered, grouped, sorted, and paged in memory.

**Code:** `src/model/src/database/rocprofvis_db_query_builder.{h,cpp}`, `rocprofvis_db_query_factory.{h,cpp}`, `rocprofvis_db_table_processor.{h,cpp}`, `rocprofvis_db_packed_storage.{h,cpp}`, `rocprofvis_db_expression_filter.{h,cpp}`, `rocprofvis_db_cache.{h,cpp}`, `rocprofvis_db_track.{h,cpp}`

**Related:** [Tables](UI.md#tables) (the UI these queries page), [Ask Optiq](FEATURES.md#ask-optiq) (writes filters in this language), [Compute database](#compute-database) (compute SQL is separate)

- `Builder::Select` overloads turn a filled format struct (`rocprofvis_db_sqlite_slice_query_format` and its siblings) into SQL, composed with `From`, the joins, `QParam`, and `Concat`. Aliases come from `Builder`'s name constants (`START_SERVICE_NAME`, ...), so every source yields the same columns.
- `From` and the joins default to `MultiNode::Yes`, which appends `_%GUID%` for `ExecuteSQLQuery` to fill in; use `MultiNode::No` for a table without the suffix.
- `QueryFactory` holds the rocprof (`GetRocprof*Query`) and Perfetto (`GetPerfetto*`) SQL, and `RocpdDatabase` builds its own. `QueryFactory` and `ComputeQueryFactory` derive from `DatabaseVersion`, so branch on schema with `IsVersionGreaterOrEqual`. Metadata load stores each track's queries in its `rocprofvis_dm_track_params_t`, and slice and table reads reuse them through `BuildTrackQuery` and `BuildSliceQueryMap`.
- `rocprofvis_db_build_table_query` and `rocprofvis_db_build_event_search_query` return a compound query: `<sql>; <track>; <instance>` lines plus `-- CMD:` lines (`TYPE`, `FILTER`, `GROUP`, `SORT`, `COUNT`, `LIMIT`, `OFFSET`) from `TableProcessor::BuildTableSemanticSubQuery`. `QueryManager::ExecuteQuery` accepts only this form; raw SQL returns `kRocProfVisDmResultNotSupported`.
- `TYPE` selects one of three `TableProcessor`s (event, sample, search), each with its own cache. When a request repeats the last one's tracks and SQL, `ExecuteCompoundQuery` runs no SQL and only reapplies filter, group, sort, and paging to the cached `m_merged_table`; otherwise it reruns every per-track query in parallel and merges the `PackedTable`s, dropping removed tracks and duplicates. The cache clears after 60 s idle (`RestartableTimer`), and without `max_count` a page is 100 rows.
- Search resolves free-text terms to string-ID `IN (...)` filters in `BuildTableStringIdFilter`: `include_substring` matches strings that contain a term, `include_category` also matches categories, and `partial_matching` accepts any term instead of all.
- `PackedTable` rows are fixed-width. `Builder::table_view_schema` gives each alias its `ColumnType` and `table_view_schema_index_t` slot; an alias missing there is dropped, and `PlaceValue` casts to the declared width, so a type that is too narrow truncates silently.
- `FilterExpression::Parse` and `Evaluate(row_map)` implement the filter language: `AND`, `OR`, `NOT`, comparisons, arithmetic, case-insensitive `LIKE` (`%`, `_`, `ESCAPE`), and `RegisterFunction` functions; `ParseAggregationSpec` reads the `COUNT`, `SUM`, `AVG`, `MIN`, and `MAX` columns of `GROUP`. Parsing stops at the first word that is not `AND` or `OR` and ignores the rest, so emit only this syntax; a filter that throws yields an empty result and an `spdlog::error`.

**Recipe: add a system query.**

1. Add a `QueryFactory` method (a `RocpdDatabase` one for legacy rocpd) that fills the matching format struct and returns `Builder::Select(...)`, joining GUID-suffixed tables with `MultiNode::Yes` and putting schema differences behind `IsVersionGreaterOrEqual`.
2. Keep the column count and order the format fixes; the callback asserts `argc` against its `NUM_PARAMS`.
3. For a new table column, add its alias to `table_view_schema_index_t` and `Builder::table_view_schema` with a wide enough `ColumnType`.
4. Wire it in: track, level, slice, and table queries join the per-track query list in `RocprofDatabase::ReadTraceMetadata` (table queries also `GetEventOperationQuery`), and event-property queries go in `ReadFlowTraceInfo`, `ReadStackTraceInfo`, or `ReadExtEventInfo`.
5. If a derived table is built from it, see [Versioning and cleanup](#versioning-and-cleanup); run `datamodel-system-tests` on both fixtures.

## Versioning and cleanup

Optiq writes derived `roc_optiq_*` tables into rocpd and rocprof files and records what built them, so a newer build rebuilds stale tables instead of misreading them. Cleanup and trimmed saves live here too.

**Code:** `src/model/src/database/rocprofvis_db_version.{h,cpp}`; `SaveTrimmedData` in `rocprofvis_db_rocprof.cpp` and `rocprofvis_db_rocpd.cpp`

**Related:** [Trace view](UI.md#trace-view) (the cleanup and trim commands), [Database adapters and trace formats](#database-adapters-and-trace-formats) (the builders)

| Table | Format | Scope | Trimmed save |
|---|---|---|---|
| `roc_optiq_track_info`, `roc_optiq_histogram` | rocprof, rocpd | per file | dropped |
| `roc_optiq_event_levels_*` (dispatch, launch, launch sample, memory allocation, memory copy, HIP event) | rocprof | per GUID | dropped |
| `roc_optiq_event_levels_op`, `roc_optiq_event_levels_api` | rocpd | per file | dropped |
| `roc_optiq_memory_activity_*` (memory events replayed at load) | rocprof | per GUID | copied |
| `roc_optiq_memory_allocate_*` | rocprof | per GUID | dropped |

- `RocprofMetadataVersionControl` and `RocpdMetadataVersionControl` register their tables in a `roc_optiq_tables` enum and in `m_roc_optiq_table_properties`. `VerifyRocOptiqTablesVersions` runs early in metadata load, and builders skip a table unless `MustRebuild(file, id)` or a wrapper such as `MustRebuildLevels` says otherwise.
- The record is a per-file `roc_optiq_metadata` table named with a hash of every entry's name, schema hash, scope, trim type, and dependency mask, and of the trace's file paths. When no table has that name, every derived table is dropped and rebuilt (`m_rebuild_all`), so adding a table, editing a hashed query, or moving the trace file rebuilds everything.
- Otherwise each row is checked in enum order: a missing table, a new `version` or hash, or a dependency on a table rebuilt earlier in the pass drops it for rebuild. A dependency therefore only reaches tables later in `roc_optiq_tables`.
- Hashes cover the SQL: level tables hash their query plus `GetLevelSchemaHashStr()`, memory activity its queries and columns (`GetMemoryActivityTableSchemaHash`), and track info and histogram `GetHistogramQueryAndSchemaHash`; memory allocate hashes nothing. Any other change, such as the C++ that computes the rows, needs a bump in `roc_optiq_table_version_t`.
- All level tables share `kRocOptiqTableVersionForLevelCalculation`. Both histogram entries are registered with `kRocOptiqTableVersionMemoryCopyLevel`, so bumping `kRocOptiqTableVersionHistogram` alone changes nothing.
- Cleanup (`rocprofvis_db_cleanup_async`, which runs `MetadataVersionControl::CleanupDatabase` on the worker) drops each file's derived tables and `rocpd_%` indexes, and adds a `VACUUM` when `rebuild` is true. The tables come back on the next metadata load. Only rocpd and rocprof implement cleanup; the base `Database::Cleanup` never calls `SetPromise`, so never wait on cleanup of any other trace.
- Trim (`rocprofvis_db_trim_save_async(db, start, end, path, future)`) deletes any file at `path` and writes one new database, even from multinode input, holding every table and view except SQLite internals and those `DisposeTableWhenTrimming` names (the metadata table and each `kRocOptiqTableDisposeWhenTrimmed` entry), with event tables cut to rows that overlap the range and the rows they reference. `start` and `end` are offsets from each instance's start; dropped tables rebuild when the file is opened.

**Recipe: add a derived table with a version.**

1. Append its id to the `roc_optiq_tables` enum of each `MetadataVersionControl` subclass that builds it, after the tables it depends on and before `kRocOptiqNumTables`.
2. Add a `kRocOptiqTableVersion*` value and register a `roc_optiq_metadata_t` entry in that subclass's constructor: a `roc_optiq_` name (ending in `_` for `kRocOptiqTablePerGuid`, which appends the GUID), scope, trim type, `kRocOptiqTableDependentOn*` mask (plus a new bit if others depend on it), version, and a hash of its SQL and columns, or 0.
3. Build it in the adapter's `ReadTraceMetadata` with `CreateSQLTable` only when `MustRebuild(file, id)` is true, and read a per-GUID table through `Builder::From(name, MultiNode::Yes)`.
4. Bump the version whenever its rows change without its hashed SQL changing.
5. Run `datamodel-system-tests`, which covers metadata load, trim, and cleanup.
