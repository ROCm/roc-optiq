# Controller

The C ABI layer in `src/controller/` that turns model data into handles for the View and Python and runs slow work as jobs behind futures. Read it before changing a public function, property, object type, fetch, thread, or pooled object.

## Overview

The controller owns trace lifecycle, async jobs, the property model, the segment cache, and the memory budget, and reaches data only through the model's `rocprofvis_db_*` and `rocprofvis_dm_*` calls.

**Code:** `src/controller/` (`inc/` public C11, `src/` C++17, `tests/`)

**Related:** [Data flow](ARCHITECTURE.md#data-flow) (one request end to end), [Data provider](UI.md#data-provider) (the View's only caller), [C ABI and Python bindings](DATABASE.md#c-abi-and-python-bindings) (the model calls and futures underneath), [Tests](ARCHITECTURE.md#tests) (targets and fixtures)

- The library is `roc-optiq-controller`. `inc/` is a PUBLIC include directory and `src/` is PRIVATE, so a test that needs internals adds `src/` itself.
- `src/remote/` and `src/profiler/` belong to [Remote SSH](FEATURES.md#remote-ssh) and [Profiler launch](FEATURES.md#profiler-launch); `src/python/` is the embedded `optiq` module.
- The controller has exactly these threads:
  - `JobSystem` workers, one per `std::thread::hardware_concurrency()`, on a FIFO queue that is cancelled and joined at exit. A job holds its worker while it waits on model futures, and a profiler monitor job holds one for the whole run, so long jobs can starve the pool.
  - One LRU thread per `MemoryManager` (one per `SystemTrace`), started by `MemoryManager::Init` after a successful load and joined in its destructor.
  - With profiler and remote both built, one `SshProfilerExecutor` thread per remote run, blocked in `SshClient::ExecuteCommand` and joined in the executor's destructor.
  - The `src/python` interpreter thread, which finishes script jobs with `Job::Complete` instead of queuing them ([Controller ABI and bindings](SCRIPTING.md#controller-abi-and-bindings)).
  - Anything else is a job. A new dedicated thread needs an owner, a cancellation path, and a join.
- No global lock guards the ABI. `Future`, `StringTable`, `MemoryManager`, `SegmentTimeline`, and `PcSampling` lock internally; `Array`, `Arguments`, `Table`, `MetricsContainer`, and `SummaryMetrics` do not, so never read an output or change an input while its future is pending.

## C ABI and properties

Every controller object is an opaque handle that answers generic getters keyed by numeric property banks, and every slow call completes a future.

**Code:** `src/controller/inc/`, `src/controller/src/rocprofvis_controller.cpp`, `rocprofvis_controller_analysis.cpp`

**Related:** [Building blocks](#building-blocks) (the classes behind handles and futures), [Data provider](UI.md#data-provider) (how the View polls, reads, and frees), [Controller ABI and bindings](SCRIPTING.md#controller-abi-and-bindings) (`rocprofvis_script_*`)

### Handles and lifecycle

- `rocprofvis_controller_alloc(filename, config_path)` builds a `SystemTrace` or `ComputeTrace` from `rocprofvis_db_identify_type`, runs `Init()`, and returns null on failure; `config_path` is where the model caches converted Perfetto files ([Database adapters and trace formats](DATABASE.md#database-adapters-and-trace-formats)). `rocprofvis_controller_alloc_compare` accepts only rocprof SQLite files ([Trace compare](FEATURES.md#trace-compare)).
- Callers free what they allocate: futures, arrays, arguments, metrics containers, summary metrics (`rocprofvis_controller_summary_metric_free`), and private tables from `rocprofvis_controller_table_alloc` or `rocprofvis_controller_search_table_alloc` (`rocprofvis_controller_table_free`). Handles from getters or inside arrays are borrowed.
- Tear down in order: wait for every future, because a job references the caller's inputs, outputs, tables, and trace, and `~Future` deletes the `Job`; free arrays, arguments, and private tables, because an array free reaches the trace's `MemoryManager`; call `rocprofvis_analysis_free_trace_data`, because `Analysis` caches tables per `Trace*`; then call `rocprofvis_controller_free`, which deletes only a `SystemTrace`, so a compute controller is never freed.
- Declared but never defined: the `rocprofvis_controller_track_alloc` family (track, graph, event, sample, flow control), `rocprofvis_controller_create_analysis_view_async`, and `rocprofvis_controller_close_view`. User analysis goes through `rocprofvis_script_*`.

### Properties

- The generic `rocprofvis_controller_get_uint64` / `_double` / `_string` / `_object` and setters cast to `Handle*` and call the virtual accessor, whose switch falls through to `UnhandledProperty`: `kRocProfVisResultInvalidType` inside the handle's `[first, last)` bank, `kRocProfVisResultInvalidEnum` outside. A value kind the class never overrides returns `kRocProfVisResultNotSupported`.
- `rocprofvis_controller_get_string` with a null buffer or zero length writes the byte count; otherwise it copies up to `*length` bytes without a terminator, so allocate `length + 1` and terminate it yourself. Implement string getters with `GetStdStringImpl`.
- Banks live in `rocprofvis_controller_enums.h`:

| Base | Banks |
|---|---|
| `0x00000000` | system trace |
| `0x10000000` to `0xD0000000`, one each | timeline, view, track, sample, flow control, callstack, event, graph, array, table, future, event data, ext data |
| `0xE0000000` | table arguments; summary properties |
| `0xF0000000` | summary metrics; `rocprofvis_controller_table_type_t` values |
| `0xF1000000` to `0xF5000000` | remote parameters, prompts, responses, status, properties |
| `0x11000000` to `0x15000000` | summary aggregation levels and arguments, pivot-table arguments, script result and context |
| `0xFFFF0000` | memory usage (`kRPVControllerCommonMemoryUsageInclusive`, `Exclusive`) |
| `0` | compute trace, workload, kernel, metrics container, roofline, metric and PC-sampling arguments |

- Banks sharing a base belong to different handle classes. The zero-based compute enums overlap, so a property sent to the wrong compute handle can return another property's value instead of an error; check the handle type first.
- `PcSampling` groups properties at source `0x10000000`, ISA `0x20000000`, and stalls `0x30000000`; `GetPropertyMutex` picks the layer lock from that high nibble, so a new property joins its layer's group.

### Futures and async calls

- Trim and cleanup run as futures too ([Versioning and cleanup](DATABASE.md#versioning-and-cleanup)); `rocprofvis_controller_table_export_csv` works on system traces only.
- `rocprofvis_controller_future_wait` with `0.0f` polls and with `FLT_MAX` blocks, so the UI thread passes zero. Success means finished, not succeeded: read `kRPVControllerFutureResult`, which may be `kRocProfVisResultCancelled` or an error. `kRocProfVisResultUnknownError` from the wait means no job was issued. Progress is `kRPVControllerFutureProgressPercentage` and `kRPVControllerFutureProgressMessage`.
- `rocprofvis_controller_future_cancel` drops a still-queued job (it never runs; its result is `kRocProfVisResultCancelled`), sets the flag running jobs poll with `Future::IsCancelled()`, and cancels the model futures the job registered. Its return code does not say whether the work stopped.
- Event flow, call stack, and ext data come from `rocprofvis_controller_get_indexed_property_async` on the controller handle with a `kRPVControllerSystemEventData*Indexed` property and the event ID as `index`; an `Event` handle reaches a stub that returns `kRocProfVisResultInvalidEnum`.
- `rocprofvis_analysis_*` holds cross-cutting analyses: queue utilization, counter statistics, and six top-events tables. Each `rocprofvis_analysis_get_*` getter returns one table per trace and category, shared by every reader; `rocprofvis_analysis_events_table_alloc` returns a private one. New cross-cutting analyses go in `Analysis`.

### Recipes

**Recipe: add a public C function.**

1. Declare it in its `inc/` header inside `extern "C"` with C11 types only; a gated feature puts the declaration and the definition inside its `ROCPROFVIS_ENABLE_*` guard.
2. Define it in `rocprofvis_controller.cpp` or the feature's ABI file. Validate each typed input with its `Reference<>` typedef and return `kRocProfVisResultInvalidArgument` on failure; if both trace kinds are accepted, branch on `rocprofvis_controller_get_object_type` first, like `rocprofvis_controller_table_fetch_async`.
3. Give caller-allocated objects an `_alloc` / `_free` pair, and add a test.

**Recipe: add a property.**

1. Append it before its bank's closing sentinel (for tracks, `__kRPVControllerTrackPropertiesLast`); explicit-valued enums take the next free value. Never reorder or renumber.
2. Handle it in the owning class's private override for its value kind; controller C++ callers get a typed accessor instead.
3. Surface it through `DataProvider`, and through the `optiq` module if scripts need it.

**Recipe: add an object type.**

1. Append a `rocprofvis_controller_object_type_t` value after `kRPVControllerObjectTypeScriptResult`, or inside its feature's `#ifdef` range (remote from 205, profiler from 306).
2. Add an opaque typedef in `rocprofvis_controller_types.h` if callers need one, and a property enum with an unused base.
3. Subclass `Handle`, pass the bank's first and last values to its constructor, implement `GetType`, and keep the generic overrides private.
4. Add a `Reference<>` typedef beside the others in `rocprofvis_controller.cpp`.
5. Make it caller-owned with an `_alloc` / `_free` pair, or owned and deleted by its parent.
6. Add the `.cpp` to `rocprofvis_controller_source_files`, inside its feature's `if()` when gated.

**Recipe: add an async fetch.**

1. Add the entry point as above, taking a future and a caller-owned output.
2. In the trace class, wrap the work as `future.Set(JobSystem::Get().IssueJob(...))` with the lambda and `&future`, and return success only if `future.IsValid()`.
3. Capture scalars by value and C strings as `std::string`; caller-owned handles may be captured by reference.
4. Between steps, return `kRocProfVisResultCancelled` if `future->IsCancelled()`. For each model call, allocate a `rocprofvis_db_future_t` (with `&Future::ProgressCallback` for progress), `AddDependentFuture` before waiting so a cancel reaches the model, then `RemoveDependentFuture` and free it on every path (`DataModelFuturePtr`).
5. Write into the caller's output; pooled objects follow the recipe in [Memory and segments](#memory-and-segments).
6. Add the View request in [Data provider](UI.md#data-provider) and a test.

## Building blocks

The classes every controller feature is made of.

**Code:** `src/controller/src/rocprofvis_controller_*.{h,cpp}` for handle, reference, future, job system, data, array, arguments, string table, table, and trace

**Related:** [C ABI and properties](#c-abi-and-properties) (the surface these implement), [Memory and segments](#memory-and-segments) (pooled objects and array pinning)

- Besides the generic accessors, `Handle` carries `GetContext`, `IsDeletable`, and `IncreaseRetainCounter` for the memory manager.
- `Reference<Pointer, Type, Enum>` is the checked cast: `TrackRef` is `Reference<rocprofvis_controller_track_t, Track, kRPVControllerObjectTypeTrack>`, and `IsValid()` is false for null or another type.
- A `JobFunction` is `std::function<rocprofvis_result_t(Future*)>`. `IssueJob` queues it and `Future::Set` hands the `Job` to the `Future`, which owns it. `Job::Complete` finishes a job run by another owned thread and is a no-op once the job has left pending.
- `Future::ProgressCallback` records each model future's percentage and publishes their mean, so several reads drive one bar; jobs without model progress set the percentage and message properties directly.
- `Data` is a tagged union of `uint64_t`, `double`, an owned string, or a handle; a copy only borrows an owned handle, a move transfers it. `Arguments` maps each property to a `std::vector<Data>`.
- `Array` wraps `std::vector<Data>` (`kRPVControllerArrayNumEntries`, `kRPVControllerArrayEntryIndexed`). `SetObject` borrows, as for pooled events; `SetOwnedObject` makes the array delete the child, as for table rows and event details. Never pair `SetObject` with a manual delete.
- `StringTable::Get().AddString` interns a string permanently and returns a stable `size_t`; event names and metric names use it.
- `Table::SetupAndFetch` runs `Setup` with the call's `Arguments`, then `Fetch` for the page in `kRPVControllerTableArgsStartIndex` / `kRPVControllerTableArgsStartCount`. Query state lives in a `TableArguments` subclass and rows in `m_rows`, so a shared table shows whichever reader fetched last. A new table subclasses `Table` or `SystemTable` and overrides `Setup`, `Fetch`, `ExportCSV`, and `UnpackArguments`.

## System traces

`SystemTrace` loads rocpd, rocprof, multinode, and converted traces and serves their tracks, graphs, tables, summary, and topology.

**Code:** `src/controller/src/system/`

**Related:** [Memory and segments](#memory-and-segments) (the cache behind tracks and graphs), [Data model](DATABASE.md#data-model) (the model tracks, slices, and topology), [Query pipeline](DATABASE.md#query-pipeline) (table SQL), [View models](UI.md#view-models) (`TopologyTree`)

- `SystemTrace::Load` gates on the extension (`.rpd`, `.db`, `.yaml`, and with `ROCPROFVIS_PERFETTO_ENABLED` also `.json`, `.proto`, `.pftrace`), so a new format adds its extension there.
- `LoadRocpdTracks` skips model categories missing from its `supported_track` test; a new category also needs its expected topology link in `ValidateRocpdTrackTopology`, which logs gaps without failing the load. Each track's `Graph` is `kRPVControllerGraphTypeLine` for PMC tracks and `kRPVControllerGraphTypeFlame` otherwise.
- `Track::FetchSegments` claims missing segments with the processed bit, waits on `m_state_changed` while another request owns them, loads them through `FetchFromDataModel`, marks them valid only on success, and walks the resident ones; `Track::Fetch` reports each event once. A cancelled fetch keeps `kRocProfVisResultCancelled` so the View asks again.
- `Graph::Fetch` picks a level of detail from the span and pixel width, each level ten times coarser (`kGraphScaleFactor`), and never serves level 0. `GenerateLOD` builds missing levels from raw entries read through `Track::FetchSegments`: events at one depth that fit a window merge into a new pooled `Event` named by `CombineEventInfo`, and samples into a `SampleLOD` with `kRPVControllerSampleChild*` statistics.
- The trace owns one event, sample, and search table (`kRPVControllerSystemEventTable`, `kRPVControllerSystemSampleTable`, `kRPVControllerSystemSearchResultsTable`), shared by every reader. `SystemTrace::GetTableMutex` serializes `SystemTable` fetches per model use case; rows return as owned arrays of strings, and a node or agent filter (`kRPVControllerTableArgsNodeId`, `kRPVControllerTableArgsAgentId`) becomes a typed `rocprofvis_dm_query_criteria_t`, never SQL. A new use case is a `rocprofvis_dm_table_use_case_enum_t` value plus a branch in `SystemTable`.
- `Summary::Fetch` fills a caller-allocated `SummaryMetrics` tree (trace, node, processor); `AggregateSubMetrics` rolls children up and `PadTopKernels` adds an `"Others"` row for kernel time outside the top list.
- `TopologyRoot` mirrors the model topology. A leaf finds its track from `kRPVControllerTopologyNodeTrack` through `kRPVControllerSystemTrackIndexed`, and `Track::SetThread`, `SetQueue`, `SetStream`, or `SetCounter` records the reverse link. The View copies the tree into `TopologyTree` (`src/view/src/model/rocprofvis_topology_model.h`), so a new node kind changes both.

## Compute traces

`ComputeTrace` loads a rocprof-compute database and serves workloads, kernels, metric values, roofline geometry, pivot tables, and PC sampling.

**Code:** `src/controller/src/compute/`

**Related:** [Compute database](DATABASE.md#compute-database) (the use cases and columns behind every query), [Compute view](UI.md#compute-view) (the tabs that consume this), [Compute ISA view](UI.md#compute-isa-view) (the PC-sampling caller), [Compute memory chart](UI.md#compute-memory-chart) (parses `kRPVControllerWorkloadMemoryChartLayout`)

- `ComputeTrace::Load` accepts `.db` only and reads metadata (a failure only warns), workloads, metric catalogs, kernels, and rooflines; metric values, pivot tables, and PC sampling load on demand.
- The trace owns its `Workload`s and one `ComputePivotTable` (`kRPVControllerKernelMetricTable`); a `Workload` owns its `Kernel`s and `Roofline`; a `Kernel` owns its `PcSampling` (`kRPVControllerKernelPcSampling`).
- Every query goes through `ComputeTrace::ExecuteQuery`, which builds the use case with `rocprofvis_db_build_compute_query`, waits, copies cells into a `QueryDataStore`, deletes the model table, and calls back. Map columns with the owning class's `QueryToPropertyEnum`, not a second mapper.
- `rocprofvis_controller_metric_fetch_async` reads `kRPVControllerMetricArgs*`: kernel IDs win over a workload ID, and a selector without a table or entry ID takes the whole category or table. Pass an empty `MetricsContainer`: a filled one returns success without issuing a job, so the wait returns `kRocProfVisResultUnknownError`. A model `kRocProfVisDmResultNotSupported` counts as success with no rows.
- `ComputePivotTable` reads `kRPVControllerCPTArgs*`: the workload, `category.table.entry:value_name` selectors (`2.1.4:peak`), sort column (0 name, 1 duration by default, 2 and up metrics) and order, and per-column filters.
- PC sampling loads in three independent layers so the ISA pane appears before source and stall data: `rocprofvis_controller_pc_sampling_fetch_isa_lines_async`, `rocprofvis_controller_pc_sampling_fetch_source_async`, and `rocprofvis_controller_pc_sampling_fetch_stalls_async`. No test covers them yet.
- A layer's job holds that layer's `std::recursive_mutex` for the whole fetch and its getters take the same lock, so reading a loading layer blocks the caller until it finishes.
- All three need `kRPVControllerPcSamplingArgsKernelId`; the source fetch also needs `kRPVControllerPcSamplingArgsSourceFileUuid` (0 picks the first file). The stall fetch skips instruction samples only when `kRPVControllerPcSamplingArgsIncludeInstructionSamples` is 0, as the ISA View sends. `kRPVControllerPcSamplingArgsWorkloadId` is ignored.
- `*_loaded` flags skip repeat queries and `m_source_line_cache` keeps each file's lines. `kRPVControllerPCSamplingPcSampleStateIssueCount` and `kRPVControllerPCSamplingPcSampleStateStallCount` return `kRocProfVisResultNotLoaded` when host-trap sampling did not measure them; a zero is a real zero.

## Memory and segments

System traces cache model data in segment timelines of pooled events and samples, and one LRU thread per trace evicts segments to keep all open traces inside a shared budget.

**Code:** `src/controller/src/system/rocprofvis_controller_segment.{h,cpp}`, `rocprofvis_controller_mem_mgmt.{h,cpp}`

**Related:** [System traces](#system-traces) (the tracks and graphs that fill segments), [Building blocks](#building-blocks) (`Array` ownership)

- Each `Track` has a `SegmentTimeline` of one-second segments (`kSegmentDuration`), and each graph level has its own. The valid bit marks a resident segment and the processed bit one an in-flight fetch has claimed. `SegmentTimeline::FetchSegments` only walks resident segments; `Track::FetchSegments` and `Graph::GenerateLOD` load.
- `MemoryManager::NewEvent`, `NewSample`, and `NewSampleLOD` construct objects in block pools keyed by the owning `SegmentTimeline`; small timelines share the `kShortTracksMemoryPoolIdentifier` pool. `Delete` finds the pool by owner, so pass the allocating one. An `Event` sits in every segment it overlaps, so `Delete` frees it only when its retain count reaches zero.
- `Segment::Insert` returns false when that depth already holds an entry at that timestamp; the caller still owns the object and must `Delete` it.
- The budget is `kUseVailMemoryPercent` (30) percent of the physical memory available at the first `Init`, split across live managers by weight times trace size; usage counts whole pool blocks. Setting `kRPVControllerSystemNotifySelected` calls `Configure(2.0)`, doubling the selected trace's weight.
- Over the limit, the LRU thread try-locks the timelines it tracks (skipping busy ones), removes the least recently fetched timelines' segments that no in-use array references, and stops under the limit or after about 100 ms. `SegmentTimeline::Remove` clears the valid bit so the next fetch reloads.

**Recipe: return pooled objects from a fetch.** Follow `Track::Fetch` and `Graph::Fetch`; a skipped step lets the LRU thread delete objects the View still holds.

1. Call `EnterArrayOwnership(array.GetArrayId(), kRocProfVisOwnerTypeTrack)` (or `kRocProfVisOwnerTypeGraph`) before filling.
2. Bracket segment creation with `AddActiveArray` / `RemoveActiveArray` on every timeline touched; `SegmentTimeline::Insert` asserts an active array and registers new segments under each.
3. Pass `SegmentLRUParams` with the array ID so `Segment::Fetch` records an LRU reference for each segment read.
4. Call `Array::SetContext` with the owning `SystemTrace`, so `rocprofvis_controller_array_free` calls `CancelArrayOwnership` for both owner types.
