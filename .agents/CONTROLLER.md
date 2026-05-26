# CONTROLLER.md - ROCm Optiq Controller Layer Guide

This document is the controller-layer companion to
[`.agents/AGENTS.md`](./AGENTS.md). The main guide covers the View layer
in depth and gives a high-level pass over `controller/`. **This file is
the deep dive for `src/controller/`** - the C ABI bridge between the
Model (SQLite, parsing) and the View (ImGui UI).

When humans and `CODING.md` disagree with this file, `CODING.md` wins.
When this file disagrees with the source under `src/controller/`, the
source wins; please update this file in the same change.

---

## Table of Contents

1. Identity & Scope
2. Public C ABI Surface (`src/controller/inc/`)
3. Internal Architecture & Threading Model
4. Core Building Blocks (handle / future / job system / data / array / args)
5. System Trace Domain (`src/controller/src/system/`)
6. Compute Trace Domain (`src/controller/src/compute/`)
7. Memory Manager & Segment Timeline (LOD + LRU eviction)
8. Request Lifecycle: View Call -> Future -> Pixels
9. Property Model & Object Type Tags
10. Coding Conventions Specific to the Controller
11. Reuse Catalog (controller edition)
12. Common Pitfalls
13. Testing
14. Quick Reference Index

---

## 1. Identity & Scope

- **Library target:** `roc-optiq-controller` (CMake, see
  `src/controller/CMakeLists.txt`).
- **Public ABI:** C11 headers in `src/controller/inc/`. Implementation
  is C++17 in `src/controller/src/`.
- **Owner of:** trace lifecycle, async fetch jobs, the segment-based
  LOD cache, memory budget enforcement, and all property/object shapes
  exposed to the View.
- **Does NOT own:** SQLite I/O (that is `src/model/`), ImGui rendering
  (that is `src/view/`).
- **Linked by:** the View through `DataProvider`
  (`src/view/src/rocprofvis_data_provider.h`) and from Python via the
  CFFI build script in `src/model/python/`.
- **Build flags:**
  - `COMPUTE_UI_SUPPORT` - enables `src/controller/src/compute/` and the
    compute-only public API. Wrap any new compute-only symbol in
    `#ifdef COMPUTE_UI_SUPPORT`.
  - `BUILD_TESTING` - builds `roc-optiq-controller-system-tests` and
    `roc-optiq-controller-compute-tests` (Catch2). Tests are wired
    against fixture traces under `sample/`.

The View must never include controller `src/` headers - only `inc/`.
The controller must never include View headers.

## 2. Public C ABI Surface (`src/controller/inc/`)

Three headers form the entire public contract:

- `rocprofvis_controller.h` - all functions.
- `rocprofvis_controller_types.h` - opaque handle typedefs and the full
  set of `*_properties_t` enums (the property IDs you pass to the
  generic getters).
- `rocprofvis_controller_enums.h` - `rocprofvis_result_t`,
  `rocprofvis_controller_object_type_t`,
  `rocprofvis_controller_primitive_type_t`, sort orders, the property
  banks for events / samples / tracks / tables / summary / etc.

### 2.1 Handle types

All controller objects are opaque `rocprofvis_handle_t*` from the
caller's point of view. Typedefs in `rocprofvis_controller_types.h`:

```c
typedef rocprofvis_handle_t rocprofvis_controller_t;
typedef rocprofvis_handle_t rocprofvis_controller_timeline_t;
typedef rocprofvis_handle_t rocprofvis_controller_view_t;
typedef rocprofvis_handle_t rocprofvis_controller_track_t;
typedef rocprofvis_handle_t rocprofvis_controller_graph_t;
typedef rocprofvis_handle_t rocprofvis_controller_table_t;
typedef rocprofvis_handle_t rocprofvis_controller_sample_t;
typedef rocprofvis_handle_t rocprofvis_controller_event_t;
typedef rocprofvis_handle_t rocprofvis_controller_flow_control_t;
typedef rocprofvis_handle_t rocprofvis_controller_callstack_t;
typedef rocprofvis_handle_t rocprofvis_controller_ext_data_t;
typedef rocprofvis_handle_t rocprofvis_controller_future_t;
typedef rocprofvis_handle_t rocprofvis_controller_array_t;
typedef rocprofvis_handle_t rocprofvis_controller_arguments_t;
typedef rocprofvis_handle_t rocprofvis_controller_node_t;
typedef rocprofvis_handle_t rocprofvis_controller_processor_t;
typedef rocprofvis_handle_t rocprofvis_controller_process_t;
typedef rocprofvis_handle_t rocprofvis_controller_thread_t;
typedef rocprofvis_handle_t rocprofvis_controller_queue_t;
typedef rocprofvis_handle_t rocprofvis_controller_stream_t;
typedef rocprofvis_handle_t rocprofvis_controller_counter_t;
typedef rocprofvis_handle_t rocprofvis_controller_summary_t;
typedef rocprofvis_handle_t rocprofvis_controller_summary_metrics_t;
typedef rocprofvis_handle_t rocprofvis_controller_topology_node_t;
typedef rocprofvis_handle_t rocprofvis_controller_plot_t;
#ifdef COMPUTE_UI_SUPPORT
typedef rocprofvis_handle_t rocprofvis_controller_workload_t;
typedef rocprofvis_handle_t rocprofvis_controller_kernel_t;
typedef rocprofvis_handle_t rocprofvis_controller_metrics_container_t;
#endif
```

You can always recover the runtime kind via
`rocprofvis_controller_get_object_type(handle, &type)` returning a
`rocprofvis_controller_object_type_t`.

### 2.2 Lifecycle: alloc / load / free

```c
rocprofvis_controller_t* rocprofvis_controller_alloc(const char* filename);
rocprofvis_result_t       rocprofvis_controller_load_async(
    rocprofvis_controller_t*, rocprofvis_controller_future_t*);
void                      rocprofvis_controller_free(rocprofvis_controller_t*);
```

`rocprofvis_controller_alloc` sniffs the file with
`rocprofvis_db_identify_type` (from `src/model/`). It returns either a
`SystemTrace*` (rocpd / rocprof / multinode SQLite) or, when
`COMPUTE_UI_SUPPORT` is on, a `ComputeTrace*` (compute SQLite). The
caller treats both the same way - everything dispatches through the
opaque handle and the runtime object type tag.

### 2.3 Generic property accessors

The single dispatch surface for every object:

```c
rocprofvis_result_t rocprofvis_controller_get_uint64(handle, prop, index, &value);
rocprofvis_result_t rocprofvis_controller_get_double(handle, prop, index, &value);
rocprofvis_result_t rocprofvis_controller_get_object(handle, prop, index, &out_handle);
rocprofvis_result_t rocprofvis_controller_get_string(handle, prop, index, buf, &len);
rocprofvis_result_t rocprofvis_controller_get_object_type(handle, &type);

rocprofvis_result_t rocprofvis_controller_set_uint64(handle, prop, index, value);
rocprofvis_result_t rocprofvis_controller_set_double(handle, prop, index, value);
rocprofvis_result_t rocprofvis_controller_set_object(handle, prop, index, value);
rocprofvis_result_t rocprofvis_controller_set_string(handle, prop, index, str);
```

`property` is one of the enum values from
`rocprofvis_controller_types.h`. Each property bank has a numeric range
that does not collide with any other bank (e.g. track properties live
in the `0x30000000` block, sample properties in `0x40000000`, summary
metrics in `0xF0000000`, table arguments in `0xE0000000`, etc.). This
is what lets the `Handle::Get*/Set*` overrides on each subclass key off
`property` ranges without an explicit object-type check.

### 2.4 Async fetch surface

Six async fetchers exist, all returning immediately and signalling
completion through a `rocprofvis_controller_future_t`:

```c
rocprofvis_controller_track_fetch_async(...);                    // raw track data
rocprofvis_controller_graph_fetch_async(..., x_resolution, ...); // LOD-aware
rocprofvis_controller_table_fetch_async(...);                    // tables (system + compute)
rocprofvis_controller_table_export_csv(...);                     // table -> CSV file
rocprofvis_controller_summary_fetch_async(...);                  // summary metrics
rocprofvis_controller_get_indexed_property_async(...);           // event/table/system props
#ifdef COMPUTE_UI_SUPPORT
rocprofvis_controller_metric_fetch_async(...);                   // compute metric values
#endif
rocprofvis_controller_create_analysis_view_async(...);           // analytic views (placeholder)
```

Two more async surface APIs sit on the controller handle directly,
covering trace-file maintenance:

```c
rocprofvis_controller_save_trimmed_trace(handle, start, end, path, future);
rocprofvis_controller_cleanup_trace_database(handle, rebuild, future);
```

Plus the analysis library's free function (declared in
`rocprofvis_controller_analysis.h`):

```c
rocprofvis_controller_analysis_fetch_queue_utilization(
    controller, track, start_time, end_time, future, &out_double);
```

### 2.5 Future API

```c
rocprofvis_controller_future_t* rocprofvis_controller_future_alloc(void);
rocprofvis_result_t              rocprofvis_controller_future_wait(future, timeout_seconds);
rocprofvis_result_t              rocprofvis_controller_future_cancel(future);
void                             rocprofvis_controller_future_free(future);
```

Read progress via the generic getters with
`rocprofvis_controller_future_properties_t`:

- `kRPVControllerFutureResult` - final `rocprofvis_result_t` once done.
- `kRPVControllerFutureProgressPercentage` - 0..100.
- `kRPVControllerFutureProgressMessage` - free-form string.

`timeout = 0.0f` is "poll, do not block", `FLT_MAX` is "wait forever".
Cancelling is best-effort - the rule in the header is **callers must
not assume cancellation succeeds**.

### 2.6 Arguments / Array / SummaryMetrics

The View packages typed call arguments into a generic
`rocprofvis_controller_arguments_t` (a `map<property_id, vector<Data>>`)
and the controller writes results into a `rocprofvis_controller_array_t`
(a `vector<Data>`). Both are heap-allocated by the caller; both have
matching `*_alloc` and `*_free` helpers. `Data` is a tagged-union held
in `src/controller/src/rocprofvis_controller_data.h` carrying one of
`UInt64`, `Double`, `String`, or `Object`.

For summary, results land in the dedicated
`rocprofvis_controller_summary_metrics_t` shape (a tree of
trace -> node -> processor metrics; see section 5.5).

For compute metric fetches, results land in a
`rocprofvis_controller_metrics_container_t` (a flat list of
`{metric_id, source_type, source_id, value_name, value}` rows).

## 3. Internal Architecture & Threading Model

```
                     +-----------------------------+
   View thread       |   rocprofvis_controller_*   |   public C ABI
   (single-threaded) |   (extern "C" in            |
                     |    rocprofvis_controller.cpp)|
                     +--------------+--------------+
                                    | Reference<>: type-checked cast
                                    v
                     +-----------------------------+
                     |   Handle subclasses         |   Trace, Track, Graph,
                     |   (per object type)         |   Table, Event, ...
                     +--------------+--------------+
                                    | schedules
                                    v
                     +-----------------------------+
                     |   JobSystem (thread pool)   |   + Future fences
                     +--------------+--------------+
                                    | runs JobFunction:
                                    | rocprofvis_result_t(Future*)
                                    v
                     +-----------------------------+
                     |   Trace::Fetch* / DM        |   uses src/model/
                     |   queries (model layer)     |   (rocprofvis_dm_*)
                     +-----------------------------+
                                    | results -> Array, Future done
                                    v
                     +-----------------------------+
                     |   Future::Wait returns,     |   View polls each frame
                     |   View pulls Data from Array|
                     +-----------------------------+
```

### Threads at runtime

- **One worker pool** owned by `JobSystem` (singleton via
  `JobSystem::Get()`). Workers consume `Job*` values from a
  `vector<Job*>` guarded by `m_queue_mutex` and signalled by
  `m_condition_variable`. Worker count is decided in
  `rocprofvis_controller_job_system.cpp`.
- **One LRU thread per `MemoryManager`** (`m_lru_thread` in
  `MemoryManager`), only spawned when the manager exists. It evicts
  cached segment data when total memory usage exceeds the configured
  share of `s_physical_memory_avail`.
- **Caller threads** (the View's main thread, or Python) only ever
  touch handles - they must not block the View loop on a future. The
  pattern is always "issue, poll, react".

### Threading rules

1. The `extern "C"` ABI is reentrant in the sense that any thread can
   call a getter, but each handle's mutability is governed by the
   subclass (most data types are immutable post-load; the few mutable
   ones - e.g. `Future`, `Array`, `MemoryManager` - protect their
   state with `std::mutex` / `std::shared_mutex`).
2. **Never** kick off a `std::thread` from controller code outside
   `JobSystem` or `MemoryManager`. New asynchronous work goes through
   `JobSystem::Get().IssueJob(...)`.
3. The `Future` is the only legitimate cross-thread fence between the
   View and a controller job. It exposes `Wait`, `Cancel`,
   `IsCancelled`, plus progress data via the property API.
4. Long-running database operations (load, trim, cleanup) compose a
   controller `Future` with one or more `rocprofvis_db_future_t`
   (`AddDependentFuture` / `RemoveDependentFuture` and the static
   `Future::ProgressCallback`). DB-side progress is folded into the
   controller `Future`'s percentage so the View's progress bar is a
   single source of truth.

## 4. Core Building Blocks

### 4.1 `Handle` base class
File: `src/controller/src/rocprofvis_controller_handle.{h,cpp}`.

```cpp
class Handle
{
public:
    Handle(uint32_t first_prop_index, uint32_t last_prop_index);
    virtual ~Handle() {}

    virtual rocprofvis_controller_object_type_t GetType(void) = 0;

    virtual rocprofvis_result_t GetUInt64(prop, index, uint64_t*  value);
    virtual rocprofvis_result_t GetDouble(prop, index, double*    value);
    virtual rocprofvis_result_t GetObject(prop, index, handle_t** value);
    virtual rocprofvis_result_t GetString(prop, index, char* value, uint32_t* length);

    virtual rocprofvis_result_t SetUInt64(prop, index, uint64_t           value);
    // ... and the corresponding setters

    virtual Handle* GetContext()              { return nullptr; }
    virtual bool    IsDeletable()             { return true; }
    virtual void    IncreaseRetainCounter()   {}
};
```

Every public object type (`SystemTrace`, `Track`, `Graph`, `Event`,
`Sample`, `SampleLOD`, `FlowControl`, `CallStack`, `ExtData`, `Future`,
`Array`, `Arguments`, `Table`, `Summary`, `SummaryMetrics`,
`TopologyNode` and friends, plus the compute-only `ComputeTrace`,
`Workload`, `Kernel`, `Roofline`, `MetricsContainer`,
`ComputeTable`, `ComputePivotTable`, `Plot`, `ComputePlot`,
`PlotSeries`) inherits from `Handle`.

`m_first_prop_index` / `m_last_prop_index` form a guard band so an
unhandled getter falls back to `UnhandledProperty(property)` (which
returns `kRocProfVisResultInvalidArgument`). The `GetStdStringImpl`
macro is the canonical way to copy a `std::string` into the caller's
buffer with proper sizing.

### 4.2 `Reference<>` template
File: `rocprofvis_controller_reference.h`.

```cpp
template <typename Pointer, typename Type, unsigned Enum>
class Reference;
```

Type-checked downcast helper. The C ABI receives a `rocprofvis_handle_t*`
and the entry points create one of these to validate it actually points
at the expected subclass via `GetType() == Enum` before dispatching. Every
`extern "C"` function in `rocprofvis_controller.cpp` opens with one or
more of:

```cpp
RocProfVis::Controller::SystemTraceRef trace(controller);
RocProfVis::Controller::TrackRef       track_ref(track);
RocProfVis::Controller::FutureRef      future_ref(result);
RocProfVis::Controller::ArrayRef       array_ref(output);
if (trace.IsValid() && track_ref.IsValid() && ...)
```

**Rule:** when adding a new entry point, declare a new `typedef
Reference<...>` next to the others at the top of
`rocprofvis_controller.cpp` and validate every input handle. Do not
`reinterpret_cast` directly.

### 4.3 `Future`
File: `rocprofvis_controller_future.{h,cpp}`.

`Future` is itself a `Handle` (object type
`kRPVControllerObjectTypeFuture`). It stores:

- `Job* m_job` - the worker job it fences.
- `std::vector<rocprofvis_db_future_t> m_db_futures` - DB-side futures
  it has subscribed to via `AddDependentFuture`. Removed via
  `RemoveDependentFuture` when the DB future completes.
- `std::atomic<bool> m_cancelled` and `std::mutex m_mutex` for safe
  cancel state.
- `m_progress_percentage`, `m_progress_message`, `m_progress_map` - per
  DB future percentages folded into a single percentage exposed via
  `kRPVControllerFutureProgressPercentage` /
  `kRPVControllerFutureProgressMessage`.

`Future::ProgressCallback` is the C-style callback registered with
each DB future; it locks the mutex, writes into `m_progress_map`, and
recomputes the aggregate percentage.

### 4.4 `JobSystem`
Files: `rocprofvis_controller_job_system.{h,cpp}`.

```cpp
typedef std::function<rocprofvis_result_t(Future*)> JobFunction;

class Job
{
public:
    Job(JobFunction function, Future* future);
    void Execute();
    void Cancel();
    rocprofvis_result_t GetResult() const;
    rocprofvis_result_t Wait(float timeout);
};

class JobSystem
{
public:
    static JobSystem& Get();
    Job* IssueJob(JobFunction function, Future* future);
    rocprofvis_result_t CancelJob(Job* job);
};
```

A `Job` owns a `JobFunction` and the `Future*` it should signal. The
worker pool drains `m_jobs` under `m_queue_mutex`. Cancellation flips
the `Future`'s cancel flag and the `JobFunction` is expected to check
it cooperatively.

**Rule:** any new async fetcher writes its body as a
`JobFunction` lambda capturing the request inputs by value, calls into
`Trace::*Fetch*` / `Table::Fetch` / `Plot::Fetch`, writes results into
the caller's `Array` or `MetricsContainer`, then returns its
`rocprofvis_result_t`. Issue it with `JobSystem::Get().IssueJob(...)`.

### 4.5 `Data` tagged union
File: `rocprofvis_controller_data.{h,cpp}`.

`Data` carries one of: `uint64_t`, `double`, owned `char*`, or
`rocprofvis_handle_t*`. `m_type` holds the
`rocprofvis_controller_primitive_type_t`. `Data` is move-aware and
properly frees its owned string in the destructor / on
reassignment. It is the single primitive that flows through `Array`,
`Arguments`, and `Table::m_rows`.

### 4.6 `Array`
File: `rocprofvis_controller_array.{h,cpp}`.

A `Handle` wrapping `std::vector<Data>`. Properties:

- `kRPVControllerArrayNumEntries` - get/set the count (on set, resizes
  the vector).
- `kRPVControllerArrayEntryIndexed` - get/set a single cell.

`SetContext(Handle*)` is used for graph data: when a `Graph` fills an
`Array` with `Event*` / `Sample*` allocated through the
`MemoryManager`, the array carries the owning `SystemTrace*` so that
`rocprofvis_controller_array_free` can call
`MemoryManager::CancelArrayOwnership(...)` and let the LRU evictor
release the underlying segments.

### 4.7 `Arguments`
File: `rocprofvis_controller_arguments.{h,cpp}`.

A `Handle` wrapping `std::map<rocprofvis_property_t, std::vector<Data>>`.
The View calls `Set*(property, index, value)` to package call
parameters. The controller reads them via `Get*(property, index, ...)`.
This is how table fetch, summary fetch, compute metric fetch, and
compute pivot fetch all receive their typed argument lists without
having to hand-roll a struct per call.

### 4.8 `StringTable`
File: `rocprofvis_controller_string_table.{h,cpp}`.

Process-global string interning, singleton via `StringTable::Get()`.
`AddString(str, store)` returns a stable `size_t` ID, `GetString(id)`
returns the canonical pointer. Used heavily in `Event` (name, category,
combined-top name) and in `Workload` metric definitions to avoid
duplicating millions of identical strings across events.

### 4.9 `IdGenerator<T>`
File: `rocprofvis_controller_id.h`. Per-type 64-bit monotonic counter
(`++m_id`, returns next ID). Used by trace/timeline/track/event/graph
to give every object a unique ID without colliding across types.

### 4.10 `Table` base class
File: `rocprofvis_controller_table.{h,cpp}`.

Shared base for both system-side and compute-side tables.

```cpp
class Table : public Handle
{
public:
    virtual rocprofvis_result_t Setup(rocprofvis_dm_trace_t,
                                      Arguments&, Future*) = 0;
    virtual rocprofvis_result_t Fetch(rocprofvis_dm_trace_t, uint64_t index,
                                      uint64_t count, Array&, Future*) = 0;
    virtual rocprofvis_result_t ExportCSV(rocprofvis_dm_trace_t,
                                          Arguments&, Future*,
                                          const char* path) const = 0;
protected:
    struct ColumnDefintion { std::string m_name; rocprofvis_controller_primitive_type_t m_type; };
    std::vector<ColumnDefintion>            m_columns;
    std::map<uint64_t, std::vector<Data>>   m_rows;
    std::string                             m_where;
    std::string                             m_filter;
    std::string                             m_group;
    std::string                             m_group_cols;
    uint64_t                                m_num_items;
    uint64_t                                m_id;
    uint64_t                                m_sort_column;
    rocprofvis_controller_sort_order_t      m_sort_order;
};
```

Subclasses are `SystemTable` (events/samples/search-results/kernel
instances), `ComputeTable` (catalog of pre-baked compute CSV tables),
and `ComputePivotTable` (dynamic pivoted metric matrix).

### 4.11 `Trace` base class
File: `rocprofvis_controller_trace.{h,cpp}`.

```cpp
class Trace : public Handle
{
public:
    Trace(uint32_t first_prop_index, uint32_t last_prop_index, const std::string& filename);
    virtual rocprofvis_result_t Init() = 0;
    virtual rocprofvis_result_t Load(Future& future) = 0;
    rocprofvis_dm_handle_t      GetDMHandle() const;
protected:
    uint64_t              m_id;
    rocprofvis_dm_trace_t m_dm_handle;
    std::mutex            m_mutex;
    std::string           m_trace_file;
};
```

`SystemTrace` and `ComputeTrace` derive from this. The DM handle is
the `src/model/` layer's typed reference; the controller never reaches
into SQLite directly, only through `rocprofvis_dm_*` calls.

## 5. System Trace Domain (`src/controller/src/system/`)

`SystemTrace` (`rocprofvis_controller_trace_system.{h,cpp}`) is the
root system-trace controller. It owns:

```cpp
std::vector<Track*>   m_tracks;
std::vector<Node*>    m_nodes;          // legacy flat node list
Timeline*             m_timeline;
SystemTable*          m_event_table;
SystemTable*          m_sample_table;
SystemTable*          m_search_table;
Summary*              m_summary;
MemoryManager*        m_mem_mgmt;
TopologyNode*         m_topology_root;
```

Five `AsyncFetch(...)` overloads cover everything the View asks for:

```cpp
rocprofvis_result_t AsyncFetch(Track&,    Future&, Array&, double s, double e);
rocprofvis_result_t AsyncFetch(Graph&,    Future&, Array&, double s, double e, uint32_t pixels);
rocprofvis_result_t AsyncFetch(Event&,    Future&, Array&, rocprofvis_property_t);
rocprofvis_result_t AsyncFetch(Table&,    Future&, Array&, uint64_t index, uint64_t count);
rocprofvis_result_t AsyncFetch(Table&,    Arguments&, Future&, Array&);
rocprofvis_result_t AsyncFetch(Summary&,  Arguments&, Future&, SummaryMetrics&);
rocprofvis_result_t AsyncFetch(rocprofvis_property_t, Future&, Array&,
                               uint64_t index, uint64_t count);
```

Each one creates a `Job` that, on a worker thread, walks the model
layer (`rocprofvis_dm_*`), fills the caller's `Array` /
`SummaryMetrics`, and returns. The View's `DataProvider` polls the
`Future` once per frame.

### 5.1 `Track` (`rocprofvis_controller_track.{h,cpp}`)

Represents a single horizontal lane in the timeline (queue, stream,
instrumented thread, sampled thread, counter). Carries:

- `m_type` (`rocprofvis_controller_track_type_t`: `Samples` or `Events`).
- `m_segments` (a `SegmentTimeline`, see section 7).
- `m_thread`, `m_queue`, `m_stream`, `m_counter` - the linked
  `TopologyNode*`s, may be null.
- `m_dm_handle` - the model-layer track reference.

`Fetch(start, end, array, index, future)` walks the segment cache:
segments inside `[start, end]` that are not yet loaded get a
`FetchFromDataModel` call, then segments emit their cached events /
samples into the output array. `FetchSegments(...)` is the lower-level
hook that lets `Graph` reuse the segment iteration logic.

Track properties are exposed under
`rocprofvis_controller_track_properties_t` (start at `0x30000000`):
`kRPVControllerTrackId`, `kRPVControllerTrackType`,
`kRPVControllerTrackMin/MaxTimestamp`, `kRPVControllerTrackNumberOfEntries`,
`kRPVControllerTrackEntry`, `kRPVControllerCategory`,
`kRPVControllerMainName`, `kRPVControllerSubName`,
`kRPVControllerTrackDescription`, `kRPVControllerTrackMin/MaxValue`,
`kRPVControllerTrackNode/Processor/Thread/Queue/Counter/Stream`,
`kRPVControllerTrackExtData*Indexed`, histogram buckets, and the
agent/queue id helpers.

### 5.2 `Event` / `Sample` / `SampleLOD`
Files: `rocprofvis_controller_event.h`, `rocprofvis_controller_sample.h`,
`rocprofvis_controller_sample_lod.h`.

`Event` (`kRPVControllerObjectTypeEvent`):

- Carries `m_id`, `m_start_timestamp`, `m_end_timestamp`,
  `m_name` / `m_category` / `m_combined_top_name` (all `size_t` IDs
  into `StringTable`), `m_level`, `m_retain_counter`.
- `m_children` (`Array*`) holds synthetic LOD children; only populated
  on summary events emitted by `Graph::GenerateLODEvent`.
- Static helpers `FetchDataModelFlowTraceProperty`,
  `FetchDataModelStackTraceProperty`,
  `FetchDataModelExtendedDataProperty` populate an output `Array` from
  the model layer. These are what
  `rocprofvis_controller_get_indexed_property_async` calls when the
  property is a flow / callstack / ext-data ID.
- Has `IsDeletable() / IncreaseRetainCounter()` overrides because the
  same `Event*` can be retained by both a `Track` segment cache and a
  `Graph` LOD; the count keeps it alive until both release.

`Sample` (`kRPVControllerObjectTypeSample`):

- A primitive value sample with a timestamp. Bound by
  `rocprofvis_controller_sample_properties_t` (`0x40000000`).

`SampleLOD : Sample` (`kRPVControllerObjectTypeSample` reused):

- Wraps a `std::vector<Sample*> m_children` and exposes the aggregate
  child statistics (`min`, `mean`, `median`, `max`,
  `min_timestamp`, `max_timestamp`) via the
  `kRPVControllerSampleChild*` family of properties.

### 5.3 `Graph` (`rocprofvis_controller_graph.{h,cpp}`)

A `Graph` represents a per-track LOD pyramid keyed by `m_lods`
(`map<uint32_t, SegmentTimeline>`). `Fetch(pixels, start, end, array,
index, future)` figures out which LOD level fits the requested pixel
density, calls `GenerateLOD(lod_to_generate, ...)` if the level is not
yet built, and emits the resulting `Event*` / `Sample*` into the
caller's `Array`.

`GenerateLODEvent` coalesces a vector of `Event*` into a single
synthetic event whose `m_combined_top_name` is the longest contained
event name. `CombineEventInfo` produces the shared display label and
the `max_duration_str_index`.

### 5.4 `Timeline` (`rocprofvis_controller_timeline.{h,cpp}`)

Wrapper around the trace-wide collection of `Graph*`s plus the global
`min_ts` / `max_ts`. Property bank:
`rocprofvis_controller_timeline_properties_t` (`0x10000000`). Fetch
delegates to `SystemTrace::AsyncFetch(Graph& ...)`.

### 5.5 `Summary` and `SummaryMetrics`
Files: `rocprofvis_controller_summary.{h,cpp}`,
`rocprofvis_controller_summary_metrics.{h,cpp}`.

`Summary` is the global summary controller hung off `SystemTrace`.
Its `Fetch(dm_handle, args, output, future)` walks every node and
processor, fills aggregate kernel exec time and GPU/CPU utilization,
and writes into the caller-allocated `SummaryMetrics`. The kernel
instance table for the Summary panel is owned here as
`m_kernel_instance_table` (a `SystemTable` at table type
`kRPVControllerTableTypeSummaryKernelInstances`).

`SummaryMetrics` is a recursive tree:

```
trace level -> per-node level -> per-processor level
```

Each `SummaryMetrics` carries:

- `m_aggregation_level` (`Trace | Node | Processor`).
- Optional `m_id`, `m_name`, `m_processor_type`,
  `m_processor_type_idx`.
- `m_gpu` (gfx_util, mem_util, top_kernels list, kernel_exec_time_total)
  or `m_cpu` (placeholder). Both `optional<...>` for sparse traces.
- `m_sub_metrics` - children at the next level down.

Property bank: `rocprofvis_controller_summary_metric_properties_t`
(`0xF0000000`). `AggregateSubMetrics()` rolls children into the
parent. `PadTopKernels()` makes sure every node has the same length
top-kernels list so the View's table renders cleanly.

### 5.6 `SystemTable` (`rocprofvis_controller_table_system.{h,cpp}`)

Single class implementing four logical tables, distinguished by
`rocprofvis_dm_table_use_case_enum_t m_use_case`:

- `kRPVControllerTableTypeEvents` - per-track event detail table.
- `kRPVControllerTableTypeSamples` - per-track sample detail table.
- `kRPVControllerTableTypeSearchResults` - global event search table.
- `kRPVControllerTableTypeSummaryKernelInstances` - kernel instance
  rollup used by the summary panel.

`Setup()` unpacks an `Arguments` (track IDs, start/end ns, sort
column, sort order, where/filter/group strings, op-type filters,
string-table filters) into a `QueryArguments` struct, then queries
the model. `Fetch(index, count, ...)` returns a row range from
`m_rows`. Argument property bank:
`rocprofvis_controller_table_arguments_t` (`0xE0000000`).

### 5.7 `TopologyNode` family
File: `rocprofvis_controller_topology.{h,cpp}`.

`TopologyNode` is the polymorphic base for the topology tree:
`TopologyRoot`, `Node`, `Process`, `Processor`, `Thread`, `Queue`,
`Stream`, `Counter`. Each one carries:

- `m_dm_topology_node` - the model-layer reference.
- `m_node_type` (`rocprofvis_controller_topology_node_type_t`).
- `m_name`.
- `m_children` (`map<type, vector<TopologyNode*>>`).
- `m_parent`.
- `m_track` - the optional `Track*` that this leaf node represents.

`GetParent(type)` walks up the chain to the nearest ancestor of a
given type. The View's `TrackTopology` mirrors this tree exactly when
populating the side-bar; do not add a new topology kind without also
extending `TopologyNode`.

### 5.8 `FlowControl`, `CallStack`, `ExtData`, `ArgumentData`
Files: `rocprofvis_controller_flow_control.h`,
`rocprofvis_controller_call_stack.h`,
`rocprofvis_controller_ext_data.h`.

- `FlowControl` - one outgoing/incoming arrow target on an event;
  property bank `kRPVControllerFlowControlPropertiesFirst` (`0x50000000`).
- `CallStack` - one call-stack frame attached to an event; property
  bank `kRPVControllerCallstackPropertiesFirst` (`0x60000000`).
- `ExtData` - one extended-data row (category / name / value / type),
  property bank `kRPVControllerExtDataPropertiesFirst` (`0xD0000000`).
- `ArgumentData : ExtData` - same shape plus position and arg type;
  carries event-argument metadata.

These are filled into an `Array` by the static
`Event::FetchDataModelFlowTraceProperty` /
`FetchDataModelStackTraceProperty` /
`FetchDataModelExtendedDataProperty` helpers and surfaced via
`rocprofvis_controller_get_indexed_property_async`.

### 5.9 Memory management & segments
See section 7. The other system-side files,
`rocprofvis_controller_segment.{h,cpp}` and
`rocprofvis_controller_mem_mgmt.{h,cpp}`, implement the LOD cache
and LRU eviction.

## 6. Compute Trace Domain (`src/controller/src/compute/`)

Compute-side public API is wrapped in `#ifdef COMPUTE_UI_SUPPORT`, and
`src/controller/CMakeLists.txt` only compiles compute sources when that
flag is enabled. New compute-only public symbols must follow the same
guarding pattern. The compute objects all share the same `Handle` base,
the same `Reference<>` validation, the same `JobSystem` / `Future`
plumbing.

Build note: `src/controller/CMakeLists.txt` currently compiles the
active compute controller set (`trace_compute`, `workload`, `kernel`,
`metrics_container`, `roofline`, and `table_compute_pivot`). Older /
experimental compute table and plot sources also exist in
`src/controller/src/compute/`; keep them documented for discoverability,
but check CMake before assuming a class is linked into
`roc-optiq-controller`.

### 6.1 `ComputeTrace` (`rocprofvis_controller_trace_compute.{h,cpp}`)

Root of the compute trace. Owns:

```cpp
std::vector<Workload*>      m_workloads;
QueryArgumentStore          m_query_arguments;
QueryDataStore              m_query_output;
std::atomic<uint64_t>       m_async_fetch_counter;
ComputePivotTable*          m_kernel_metric_table;
```

Two `AsyncFetch` overloads:

```cpp
rocprofvis_result_t AsyncFetch(Arguments&, Future&, MetricsContainer&);
rocprofvis_result_t AsyncFetch(Table&, Arguments&, Future&, Array&);
```

Internal helper `ExecuteQuery(...)` runs a database query through the
compute model layer and dispatches rows into a callback. The nested
`MetricID` class formats `"category.table.entry"` strings the View can
parse back into typed metric refs.

### 6.2 `Workload` (`rocprofvis_controller_workload.{h,cpp}`)

One profiling run / kernel session. Carries:

- `m_id`, `m_name`, `m_sub_name`.
- `JsonData m_system_info` and `m_profiling_config` - displayed in the
  Workload view.
- `std::vector<MetricDefinition> m_available_metrics` - catalog of the
  metrics that can be queried (each row carries category id, table
  id, name / description / unit string-table indices).
- `std::vector<MetricValueName> m_metric_value_names` - the value-name
  axis for each metric.
- `std::vector<Kernel*> m_kernels`.
- `Roofline* m_roofline`.

Property bank: `rocprofvis_controller_workload_properties_t`.

### 6.3 `Kernel` (`rocprofvis_controller_kernel.{h,cpp}`)

A kernel within a workload. Carries `m_id`, `m_name`,
`m_invocation_count`, and the duration set
(`total/min/max/median/mean`). Property bank:
`rocprofvis_controller_kernel_properties_t`.

### 6.4 `Roofline` (`rocprofvis_controller_roofline.{h,cpp}`)

Holds the roofline geometry: vectors of `CeilingBandwidth`,
`CeilingCompute`, `CeilingRidge`, `Intensity` (kernel point in 2D). The
public types
`rocprofvis_controller_roofline_ceiling_compute_type_t`,
`rocprofvis_controller_roofline_ceiling_bandwidth_type_t`,
`rocprofvis_controller_roofline_kernel_intensity_type_t` enumerate
every supported ceiling. Property bank:
`rocprofvis_controller_roofline_properties_t`.

### 6.5 `MetricsContainer` (`rocprofvis_controller_metrics_container.{h,cpp}`)

The output handle for `rocprofvis_controller_metric_fetch_async`. A
flat `vector<Metric>` where each row is:

```cpp
struct Metric {
    size_t                                 id_idx;        // StringTable id
    size_t                                 name_idx;      // StringTable id
    rocprofvis_controller_metric_source_type_t source_type; // Workload | Kernel
    uint32_t                               source_id;
    size_t                                 value_name_idx; // StringTable id
    double                                 value;
};
```

Property bank: `rocprofvis_controller_metrics_container_properties_t`.

### 6.6 `ComputeTable` and `ComputePivotTable`
Files: `rocprofvis_controller_table_compute.{h,cpp}`,
`rocprofvis_controller_table_compute_pivot.{h,cpp}`,
`rocprofvis_controller_compute_metrics.h`.

`ComputePivotTable` is part of the active controller target.
`ComputeTable` source exists but is not currently listed in
`src/controller/CMakeLists.txt`; treat it as older / auxiliary code
unless you wire it into the build.

`ComputeTable` is the catalog wrapper for the pre-baked compute CSV
tables. The mapping from CSV filename to logical table type lives in
`COMPUTE_TABLE_DEFINITIONS` inside
`rocprofvis_controller_compute_metrics.h` (top-kernels, sysinfo,
speed-of-light, memory chart, command processor, workgroup manager,
wavefront launch / runtime, instruction mixes, compute units, LDS,
caches, fabric, etc.). `Setup()` loads the CSV into `m_metrics_map`;
`Fetch(index, count, ...)` returns rows; `GetMetric(key, &out)` and
`GetMetricFuzzy(key, &out)` look metrics up by name.

`ComputePivotTable` is the dynamic pivot used by the
"Add Metric" workflow in the View. `Setup()` accepts:

- `kRPVControllerCPTArgsWorkloadId`,
- `kRPVControllerCPTArgsNumMetricSelectors` +
  `kRPVControllerCPTArgsMetricSelectorIndexed` (strings of the form
  `"category.table.entry:value_name"`, e.g. `"2.1.4:peak"`),
- `kRPVControllerCPTArgsSortColumnIndex`, `kRPVControllerCPTArgsSortOrder`,
- `kRPVControllerCPTArgsNumColumnFilters` plus column index +
  expression for each filter.

It pivots metric values into a `kernel x metric` matrix and exposes
the result like any other table.

### 6.7 `ComputePlot`, `Plot`, `PlotSeries`
Files: `rocprofvis_controller_plot.{h,cpp}`,
`rocprofvis_controller_plot_compute.{h,cpp}`,
`rocprofvis_controller_plot_series.{h,cpp}`.

These plot classes exist in source but are not currently listed in
`src/controller/CMakeLists.txt`; the current View-side compute roofline
path does not depend on these controller plot classes being linked.

`Plot` is the abstract base for any data plot (axes + named series).
`ComputePlot : Plot` consumes one or more `ComputeTable`s and
populates the `m_series` map keyed on series name. `PlotSeries` is the
concrete `(x, y)` value vector exposed to callers via the property
API. The static catalog of built-in compute plots is
`COMPUTE_PLOT_DEFINITIONS` in
`rocprofvis_controller_compute_metrics.h` (kernel duration pie,
SOL plots, instruction mix plots, cache stalls, etc.). Roofline plots
are configured by `ROOFLINE_DEFINITION` in the same header.

## 7. Memory Manager & Segment Timeline

This is the trickiest part of the controller; it is also the only
place that owns dedicated threads outside the `JobSystem`. Read
`rocprofvis_controller_segment.h` and
`rocprofvis_controller_mem_mgmt.h` together with this section.

### 7.1 `Segment` and `SegmentTimeline`

Every track owns a `SegmentTimeline` - a sparse `map<double,
unique_ptr<Segment>>` with two parallel `BitSet`s:

- `m_valid_segments` - which segments have been loaded from the model.
- `m_processed_segments` - which segments have been post-processed
  (e.g. LOD generated).

A `Segment` is a fixed time slice (`kSegmentDuration` ns by default
`1e9`, scalable to `kScalableSegmentDuration = 1e4` for high-density
tracks) holding `map<level, map<timestamp, Handle*>>`. `Insert(...)`
adds an event/sample, `Fetch(start, end, array, ...)` emits the
matching items into the caller's array.

`FetchSegments(start, end, user_ptr, future, func)` is the standard
"walk the cache, populate missing ones" loop. The callback `func`
gets called once per missing segment so the caller can fetch the
data from the model layer. This is reused by `Track::Fetch` and
`Graph::GenerateLOD` so the loading logic only lives in one place.

### 7.2 `MemoryManager`

Owns memory pools for `Event`, `Sample`, `SampleLOD` plus a
process-wide LRU eviction policy. Constructor:
`MemoryManager(uint64_t id)`; one is created per `SystemTrace`.

```cpp
void Init(size_t num_objects);
void Configure(double weight);
void AddLRUReference(SegmentTimeline* owner, Segment* reference,
                     uint32_t lod, void* array_ptr);
rocprofvis_result_t EnterArrayOwnership(void* array_ptr,
                                        rocprofvis_owner_type_t type);
rocprofvis_result_t CancelArrayOwnership(void* array_ptr,
                                         rocprofvis_owner_type_t type);
void Delete(Handle* handle, SegmentTimeline* owner);
Event*     NewEvent(uint64_t id, double s, double e, SegmentTimeline*);
Sample*    NewSample(rocprofvis_controller_primitive_type_t,
                     uint64_t id, double ts, SegmentTimeline*);
SampleLOD* NewSampleLOD(rocprofvis_controller_primitive_type_t,
                        uint64_t id, double ts,
                        std::vector<Sample*>& children, SegmentTimeline*);
```

Internals worth knowing:

- `MemoryPool` - one pool per `(object_size, object_type, num_objects)`
  triple. Allocations are slot-based with a `BitSet` mask so they are
  O(1) and contiguous.
- `m_lru_thread` - one background thread per manager that calls
  `ManageLRU()` on a condition variable. When total `m_lru_storage_memory_used`
  exceeds `m_lru_size_limit` (computed from `s_physical_memory_avail`,
  `kUseVailMemoryPercent`, and per-trace weight) it walks `m_lru_array`
  (sorted by oldest timestamp) and evicts segments by removing their
  `array_ptr`s, deleting the resident `Event*` / `Sample*` / `SampleLOD*`s
  through the pools, and clearing the `valid` bit on the segment.
- Static `s_memory_manager_instances` and `Configure(weight)` let
  multiple traces share the global memory budget proportionally.
- `kShortTracksMemoryPoolIdentifier = 1` partitions short, dense
  tracks into a separate pool so they are not evicted as easily as
  large sparse tracks.
- `Delete(handle, owner)` is the *only* legitimate destructor for
  pooled objects - it returns the slot to the pool and decrements LRU
  bookkeeping. Calling `delete` directly on a pooled `Event*` will
  corrupt the pool.

### 7.3 Array ownership and free-time eviction

When a `Graph` fills an `Array` with pooled events, it calls
`MemoryManager::EnterArrayOwnership(&array_vector, kRocProfVisOwnerTypeGraph)`
to mark every reachable segment as in-use so the LRU thread will not
evict them while the View holds the data. When the View later calls
`rocprofvis_controller_array_free`, the controller-side free path
checks whether the array was created by a `Trace` and, if so, calls
`CancelArrayOwnership(...)` to release the in-use marker. This is why
graph-output arrays carry a `Trace*` context and ordinary arrays do
not. **Do not skip this step** when adding a new fetch path that
returns pooled objects.

## 8. Request Lifecycle: View Call -> Future -> Pixels

This is the sequence the View and the controller follow for every
async fetch. It is the same shape for every request type.

```
View thread (frame N):
    DataProvider::FetchTrack(track_id, start, end, ...)
        -> rocprofvis_controller_track_fetch_async(
               controller, track, start, end, future, output_array)

extern "C" entry (rocprofvis_controller.cpp):
    SystemTraceRef trace(controller); TrackRef track_ref(track); ...
    if (all valid) trace->AsyncFetch(*track_ref, *future, *array, start, end);

SystemTrace::AsyncFetch:
    JobSystem::Get().IssueJob([=](Future* f) {
        // worker thread:
        track_ref.FetchSegments(start, end, ...) // hits Segment cache
        for each missing segment:
            Track::FetchFromDataModel(...) -> rocprofvis_dm_*
            MemoryManager::NewEvent(...) / NewSample(...) -> pooled
            segment->Insert(ts, level, handle);
        segment->Fetch(start, end, array, ...);
        MemoryManager::EnterArrayOwnership(array, Graph or Track);
        return kRocProfVisResultSuccess;
    }, future);

View thread (frame N+1..M):
    rocprofvis_controller_future_wait(future, 0.0f) until kRocProfVisResultSuccess
    (or read kRPVControllerFutureProgressPercentage to drive UI spinner)

View thread (frame M, success):
    Read array entries via rocprofvis_controller_get_object/uint64/...
    Render as pixels.

View thread on dispose:
    rocprofvis_controller_array_free(array)
        -> CancelArrayOwnership -> LRU may now evict the underlying segments.
```

When a request needs cancellation (e.g. tab close, view shutdown),
the View calls `rocprofvis_controller_future_cancel`. The
`JobFunction` is responsible for cooperatively checking
`Future::IsCancelled()` at safe checkpoints. Cleanup work runs through
`AppWindow::m_provider_cleanup_jobs` on the View side.

## 9. Property Model & Object Type Tags

Every controller object carries:

1. A `rocprofvis_controller_object_type_t` returned by
   `Handle::GetType()`. This is the runtime kind tag.
2. A property range `[m_first_prop_index, m_last_prop_index)` that
   every getter/setter call checks before dispatching.

Property bank starting points (`uint32_t` enum bases):

| Bank                                  | Base         |
|---------------------------------------|--------------|
| System controller                     | `0x00000000` |
| Timeline                              | `0x10000000` |
| View                                  | `0x20000000` |
| Track                                 | `0x30000000` |
| Sample                                | `0x40000000` |
| Flow Control                          | `0x50000000` |
| Callstack                             | `0x60000000` |
| Event                                 | `0x70000000` |
| Graph                                 | `0x80000000` |
| Array                                 | `0x90000000` |
| Table                                 | `0xA0000000` |
| Future                                | `0xB0000000` |
| Event Data (flow / callstack / ext)   | `0xC0000000` |
| Ext Data                              | `0xD0000000` |
| Table Arguments / Summary             | `0xE0000000` |
| Summary Metrics                       | `0xF0000000` |
| Common (memory usage, etc.)           | `0xFFFF0000` |

Compute-side banks (`COMPUTE_UI_SUPPORT`) start at the
`__kRPVControllerComputePropertiesFirst` family. The auto-incrementing
`__first / __last` brackets in each enum are an extension hint - if
you add a new property to an existing bank, declare it inside the
brackets.

When you add a new object type:

1. Add a value to `rocprofvis_controller_object_type_t` (after the
   existing range, never reorder).
2. Add a typedef in `rocprofvis_controller_types.h`.
3. Add a property bank enum in `rocprofvis_controller_types.h` with a
   fresh base.
4. Subclass `Handle` and implement only the getters/setters you need;
   pass `(first, last)` to the base ctor.
5. Add a `Reference<>` typedef in `rocprofvis_controller.cpp`.
6. Add `*_alloc` / `*_free` if it is caller-allocated; otherwise hang
   it off the parent and let `~ParentHandle()` delete it.

## 10. Coding Conventions Specific to the Controller

These supplement `CODING.md`. When the two disagree, `CODING.md` wins.

- **C ABI is C11.** No C++ types in the public headers crossing the
  ABI boundary. Use `typedef enum`, `typedef struct`, fixed-width
  integers from `<stdint.h>`, opaque `rocprofvis_handle_t*`.
- **Implementation is C++17.** Use `nullptr`, fixed-width integers,
  RAII, `std::unique_ptr` / `std::shared_ptr` for ownership. Prefer
  stack allocation when feasible.
- **Namespaces:** all C++ code lives in `RocProfVis::Controller`
  (close blocks with the trailing `// namespace Controller / //
  namespace RocProfVis` comments).
- **No exceptions** across the C ABI boundary (and avoid them
  internally). Return `rocprofvis_result_t`. The only existing
  `try/catch` is at the entry of `rocprofvis_controller_alloc` and it
  logs and falls through to `nullptr`.
- **Logging:** `spdlog::info/warn/error` only. Never `std::cout` /
  `printf` / `iostream`.
- **Asserts:** `ROCPROFVIS_ASSERT` for runtime invariants;
  `static_assert` for compile-time. Asserts must be side-effect free.
- **Memory:** controller-pooled `Event` / `Sample` / `SampleLOD` MUST
  go through `MemoryManager::New*`. Free with `MemoryManager::Delete`.
  Do not call `new`/`delete` on these classes directly.
- **Threading:** schedule async work via `JobSystem::Get().IssueJob`.
  The only sanctioned long-lived thread outside `JobSystem` is
  `MemoryManager::m_lru_thread`; if you must add another, follow the
  same shutdown / atomic-flag pattern.
- **String interning:** any high-cardinality string (event names,
  metric names, file paths in callstacks) goes through `StringTable`.
- **No globals.** Use `static T& Get()` singletons (matches
  `JobSystem`, `StringTable`, `Analysis`).

## 11. Reuse Catalog (controller edition)

| You want to...                                          | Use this                                                                |
|---------------------------------------------------------|-------------------------------------------------------------------------|
| Validate a `rocprofvis_handle_t*` cast                  | `Reference<HandleT, MyType, kRPVControllerObjectType...>`               |
| Schedule async work                                     | `JobSystem::Get().IssueJob(jobfn, future)`                              |
| Wait or poll on async work                              | `Future::Wait(timeout)` / `kRPVControllerFutureProgressPercentage`      |
| Cancel async work                                       | `Future::Cancel()` + check `Future::IsCancelled()` in your `JobFunction` |
| Compose with a model-layer DB future                    | `Future::AddDependentFuture` + `Future::ProgressCallback`               |
| Pass typed call arguments                               | `Arguments` (`Set*`/`Get*` per `property` bank)                         |
| Return a list of typed values                           | `Array` (heap-allocated via `rocprofvis_controller_array_alloc`)        |
| Return a primitive cell                                 | `Data` tagged union                                                     |
| Hold an interned string                                 | `StringTable::Get().AddString(s, store)`                                |
| Allocate an `Event` / `Sample` / `SampleLOD`            | `MemoryManager::NewEvent` / `NewSample` / `NewSampleLOD`                |
| Mark an array as in-use so segments survive eviction    | `MemoryManager::EnterArrayOwnership(arr, kRocProfVisOwnerTypeGraph)`    |
| Release an array's in-use grip                          | `MemoryManager::CancelArrayOwnership(arr, type)` (called by `array_free`) |
| Walk segments inside `[start, end]`                     | `SegmentTimeline::FetchSegments(start, end, user_ptr, future, func)`    |
| Identify a topology node's nearest typed ancestor       | `TopologyNode::GetParent(rocprofvis_controller_object_type_t)`          |
| Implement a new table                                   | Subclass `Table`, override `Setup` / `Fetch` / `ExportCSV`              |
| Implement a new system table use case                   | Add to `rocprofvis_dm_table_use_case_enum_t` and switch in `SystemTable` |
| Implement a new compute pre-baked table                 | Add a `ComputeTableDefinition` row in `COMPUTE_TABLE_DEFINITIONS`       |
| Implement a new compute plot                            | Add a `ComputeTablePlotDefinition` row in `COMPUTE_PLOT_DEFINITIONS`    |
| Implement a new analysis function                       | Extend `Analysis` and add a free function in `rocprofvis_controller_analysis.h` |
| Add a new object type                                   | See section 9 (six-step recipe)                                         |
| Add a new property to an existing object type           | Append to that bank's enum inside the `__first / __last` brackets       |
| Add a new compute-only feature                          | Wrap with `#ifdef COMPUTE_UI_SUPPORT` everywhere it appears             |
| Generate / consume a unique 64-bit per-type id          | `IdGenerator<MyType>` (see `rocprofvis_controller_id.h`)                |

## 12. Common Pitfalls

- **Calling `delete` on pooled objects.** `Event`, `Sample`,
  `SampleLOD` come from `MemoryManager`; deleting them with `delete`
  corrupts the underlying pool. Always go through
  `MemoryManager::Delete(handle, owner)`.
- **Forgetting `EnterArrayOwnership`.** A `Graph` that fills an array
  with pooled events but does not register ownership will see those
  events evicted under it the moment the LRU thread runs. Pair every
  pooled-fill with an `EnterArrayOwnership` and rely on
  `CancelArrayOwnership` in `array_free`.
- **`reinterpret_cast` from `rocprofvis_handle_t*`.** Always use a
  `Reference<>` so the type tag is checked.
- **Spawning `std::thread` from a fetch.** Use
  `JobSystem::Get().IssueJob`. The only legitimate non-job thread is
  `MemoryManager::m_lru_thread`.
- **Throwing exceptions.** Return `rocprofvis_result_t`. Do not let
  exceptions cross any `extern "C"` boundary.
- **Logging via `iostream` / `printf`.** Use `spdlog`.
- **Adding properties without minding the bank base.** Property
  values are bit-packed numerically, not symbolically; collisions
  between banks will surface as silent dispatch errors.
- **Forgetting the `COMPUTE_UI_SUPPORT` guard.** Compute-only headers
  in `inc/` are guarded; matching `src/compute/` and any callsite must
  also be guarded.
- **Holding a pointer returned by `GetString` past the next
  `Set*`.** The buffer is owned by the caller; if you read into a
  thread-local buffer and then call `SetString`, the pointer is stale.
  Copy into a `std::string` immediately if you need to retain it.
- **Calling `Future::Wait(FLT_MAX)` from the View thread.** That
  blocks the UI. The View should call `Wait(0.0f)` (poll) or read the
  progress percentage; it should never block the main loop.
- **Mixing `SystemTrace` and `ComputeTrace` in one validation.** Only
  one of `SystemTraceRef` / `ComputeTraceRef` is valid for a given
  controller handle - use `rocprofvis_controller_get_object_type` first
  if both paths are reachable (see
  `rocprofvis_controller_table_fetch_async` for the canonical
  example).

## 13. Testing

Catch2 tests live in `src/controller/tests/`:

- `rocprofvis_controller_system_tests.cpp` - run twice in CI: against
  `sample/trace_70b_1024_32.rpd` and `sample/rocpd-transpose.db`. Tests
  the public ABI (alloc, load, fetch tracks / graphs / tables /
  summary, save trimmed trace, cleanup, queue utilization), the
  `MemoryManager` LRU eviction logic, and segment timeline behaviour.
- `rocprofvis_controller_compute_tests.cpp` - runs against
  `sample/rocprof_compute_23ed6f36.db`. Tests the compute load,
  workload + kernel + roofline + metric-fetch + pivot-table flows.

Both binaries accept `--input_file <path>` (parsed by Catch2 + Clara).
Logs land in `Testing/Temporary/rocprofvis_controller_*_tests/`.

When you add a new public API or a new domain class, add a
corresponding test case here. Use the existing tests as templates -
they show the canonical "alloc, load_async, wait, fetch, validate,
free" sequence.

## 14. Quick Reference Index

### Public ABI

- `rocprofvis_controller.h` -> all C functions.
- `rocprofvis_controller_types.h` -> opaque handle typedefs and all
  property banks.
- `rocprofvis_controller_enums.h` -> `rocprofvis_result_t`,
  `rocprofvis_controller_object_type_t`,
  `rocprofvis_controller_primitive_type_t`, sort orders, table types,
  table arguments, etc.

### Core building blocks (`src/controller/src/`)

- `rocprofvis_controller.cpp` -> all `extern "C"` entry points.
- `rocprofvis_controller_handle.{h,cpp}` -> `Handle` base.
- `rocprofvis_controller_reference.h` -> typed cast wrapper.
- `rocprofvis_controller_id.h` -> `IdGenerator<T>`.
- `rocprofvis_controller_data.{h,cpp}` -> `Data` tagged union.
- `rocprofvis_controller_array.{h,cpp}` -> `Array`.
- `rocprofvis_controller_arguments.{h,cpp}` -> `Arguments`.
- `rocprofvis_controller_string_table.{h,cpp}` -> `StringTable`.
- `rocprofvis_controller_future.{h,cpp}` -> `Future`.
- `rocprofvis_controller_job_system.{h,cpp}` -> `Job`, `JobSystem`.
- `rocprofvis_controller_table.{h,cpp}` -> `Table` base.
- `rocprofvis_controller_trace.{h,cpp}` -> `Trace` base.
- `rocprofvis_controller_analysis.{h,cpp}` -> `Analysis` (queue
  utilization, room for more).

### System trace (`src/controller/src/system/`)

- `rocprofvis_controller_trace_system.{h,cpp}` -> `SystemTrace`.
- `rocprofvis_controller_track.{h,cpp}` -> `Track`.
- `rocprofvis_controller_event.{h,cpp}` -> `Event`.
- `rocprofvis_controller_sample.{h,cpp}` -> `Sample`.
- `rocprofvis_controller_sample_lod.{h,cpp}` -> `SampleLOD`.
- `rocprofvis_controller_segment.{h,cpp}` -> `Segment`,
  `SegmentTimeline`.
- `rocprofvis_controller_mem_mgmt.{h,cpp}` -> `MemoryManager`,
  `MemoryPool`, `BitSet`, LRU thread.
- `rocprofvis_controller_graph.{h,cpp}` -> `Graph` (LOD per track).
- `rocprofvis_controller_timeline.{h,cpp}` -> `Timeline`.
- `rocprofvis_controller_table_system.{h,cpp}` -> `SystemTable`.
- `rocprofvis_controller_summary.{h,cpp}` -> `Summary`.
- `rocprofvis_controller_summary_metrics.{h,cpp}` -> `SummaryMetrics`.
- `rocprofvis_controller_topology.{h,cpp}` -> `TopologyNode`,
  `TopologyRoot`, `Node`, `Process`, `Processor`, `Thread`, `Queue`,
  `Stream`, `Counter`.
- `rocprofvis_controller_flow_control.{h,cpp}` -> `FlowControl`.
- `rocprofvis_controller_call_stack.{h,cpp}` -> `CallStack`.
- `rocprofvis_controller_ext_data.{h,cpp}` -> `ExtData`,
  `ArgumentData`.

### Compute trace (`src/controller/src/compute/`,
`#ifdef COMPUTE_UI_SUPPORT`)

- `rocprofvis_controller_trace_compute.{h,cpp}` -> `ComputeTrace`.
- `rocprofvis_controller_workload.{h,cpp}` -> `Workload`.
- `rocprofvis_controller_kernel.{h,cpp}` -> `Kernel`.
- `rocprofvis_controller_roofline.{h,cpp}` -> `Roofline`.
- `rocprofvis_controller_metrics_container.{h,cpp}` -> `MetricsContainer`.
- `rocprofvis_controller_table_compute.{h,cpp}` -> `ComputeTable`
  (source present; not currently compiled by `src/controller/CMakeLists.txt`).
- `rocprofvis_controller_table_compute_pivot.{h,cpp}` -> `ComputePivotTable`.
- `rocprofvis_controller_plot.{h,cpp}` -> `Plot` base (source present;
  not currently compiled by `src/controller/CMakeLists.txt`).
- `rocprofvis_controller_plot_compute.{h,cpp}` -> `ComputePlot`
  (source present; not currently compiled by `src/controller/CMakeLists.txt`).
- `rocprofvis_controller_plot_series.{h,cpp}` -> `PlotSeries`
  (source present; not currently compiled by `src/controller/CMakeLists.txt`).
- `rocprofvis_controller_compute_metrics.h` -> static catalog
  (`COMPUTE_TABLE_DEFINITIONS`, `COMPUTE_PLOT_DEFINITIONS`,
  `COMPUTE_METRIC_DEFINITIONS`, `ROOFLINE_DEFINITION`).

### Tests (`src/controller/tests/`)

- `rocprofvis_controller_system_tests.cpp`
- `rocprofvis_controller_compute_tests.cpp`

---

**End of CONTROLLER.md.** When you change the controller, update the
section that covers the area you touched (especially section 14 and
the Reuse Catalog). Keep this file the single source of truth for
"where does X live in the controller" and "what should I reuse instead
of writing it." If you find a duplicate of something listed here,
prefer to delete the duplicate and route callers through the canonical
entry.

### 2.7 The "analysis" Sub-API

The controller currently exposes one analysis function on top of the
generic surface:
`rocprofvis_controller_analysis_fetch_queue_utilization`. It is built
on the same internal `Job + Future` plumbing as the data fetchers but
runs a SQL query against the trace's database directly instead of
serving from the segment cache. New cross-cutting analyses go in
`src/controller/src/rocprofvis_controller_analysis.{h,cpp}`.
