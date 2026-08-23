# SCRIPTING.md - Custom Python Analysis

Design for in-app Python analysis scripts. Read
[`.agents/CONTROLLER.md`](./CONTROLLER.md) and [`.agents/UI.md`](./UI.md)
alongside this file. When `CODING.md` disagrees with this file,
`CODING.md` wins. When this file disagrees with source, the source
wins; update this file in the same change.

This is a planned feature. Enable with
`ROCPROFVIS_ENABLE_SCRIPTING=ON`. Phase 0 (runtime skeleton), the
Phase 1 **read path** (query-table alloc, `optiq.trace` / `selection` /
`table().fetch()` / `Track.events()`, Catch2 against a sample trace),
and Phase 1b (DataProvider execute + floating script editor) are in
tree.

---

## 1. Goals

A user (or the Ask Optiq assistant) writes a Python script that
analyzes the open trace and presents results in the view. Examples:

- Loop events on a track and test whether they are spaced evenly.
- Run a custom table query (filter / group / time range) without
  overwriting the Event Table / Sample Table tabs.
- Let the assistant generate the same kind of script, show the source,
  and run it through the same pipeline.

Constraints agreed in design:

- The view sends script **source strings** plus selection context. It
  does not own the interpreter.
- Scripts call into the controller through the existing **C ABI**
  (bindings wrap `src/controller/inc/` only).
- The **Python interpreter lives in a separate library** in this repo.
- The ABI grows on demand. Phase 1 finishes `table_alloc` so scripts
  can fetch through a private table handle.

---

## 2. Architecture

```
View (ImGui)
  ScriptEditor  --source string + context-->  DataProvider
  ScriptResult  <-- poll Future / request -->  DataProvider
  Ask Optiq     -- same ExecuteScript path --
                         |
                         | C ABI only (inc/)
                         v
Controller
  ScriptEngine  (execute_async, session Future, context inject)
  python/optiq bindings  (Python.h in this TU only; calls C ABI)
                         |
                         | runtime ABI (no controller types)
                         v
roc-optiq-python
  embed, isolated PyConfig, import allowlist, exec, interrupt
  dedicated interpreter thread
                         |
                         v
  script may call controller C ABI (track_fetch, table_fetch, ...)
  JobSystem workers run those fetches; Python thread waits/polls
```

### 2.1 Ownership (no circular link)

| Target | Owns | Must not own |
|--------|------|----------------|
| `roc-optiq-python` | CPython embed, `exec`, cancel/interrupt, isolated `sys.path`, import allowlist | controller headers, SQLite, ImGui |
| `roc-optiq-controller` | `ScriptEngine`, `optiq` bindings, inject live controller handle, marshal script Future | `Py_Initialize` / vendor layout internals |
| View | editor, run/cancel, render results | Python.h, interpreter, ABI fetches |

CMake: controller **links** `roc-optiq-python`. The python lib **does
not** link the controller. Bindings compiled into the controller call
the public C ABI (`rocprofvis_controller_*`), not C++ `Handle`
subclasses.

Do not reuse `src/model/python/` (external datamodel CFFI). That is a
different Python stack.

### 2.2 Two table roles

Do **not** fetch `kRPVControllerSystemEventTable` /
`kRPVControllerSystemSampleTable` from scripts. Those handles are the
UI singletons; a second fetch overwrites their row cache and args.

| Role | Handle | Purpose |
|------|--------|---------|
| **Query table** (Phase 1) | Extra `SystemTable` from `rocprofvis_controller_table_alloc` | Script runs the same (or custom) SQL-shaped query as the UI, then analyzes rows in Python. UI tables stay untouched. |
| **Result table** (Phase 2) | In-memory table, not `SystemTable` | Script publishes columns/rows for the view to page/export. |

`table_alloc(track)` was declared in `rocprofvis_controller.h` and
never defined. It is now **query-table alloc** with no track argument
— tracks belong in fetch args. Pair with
`rocprofvis_controller_table_free`.

Analysis already uses the query-table pattern:
`rocprofvis_analysis_get_*_events_table` allocates separate
`SystemTable` subclasses so Top Events does not clobber the Event
Table tab.

---

## 3. Interpreter library (`src/python/`)

C ABI sketch (owned by `roc-optiq-python`):

```c
rocprofvis_result_t rocprofvis_python_init(char const* runtime_root);
rocprofvis_result_t rocprofvis_python_exec(
    char const* source,
    void (*prepare_globals)(void* py_dict, void* user),
    void* user);
void rocprofvis_python_interrupt(void);   // PyErr_SetInterrupt
void rocprofvis_python_shutdown(void);
```

`prepare_globals` is provided by the controller: it creates the
`optiq` module and stuffs `optiq.trace` / `optiq.selection` into the
exec dict. The runtime never includes `rocprofvis_controller.h`.

**Thread:** one long-lived interpreter thread, owned by this lib (not
by `JobSystem`). Controller posts `{source, prepare, user}` to that
thread and completes the script `Future` when `exec` returns.

**Hard rule:** never `exec` a user script on a `JobSystem` worker.
`track_fetch_async` / `table_fetch_async` already `IssueJob`. Waiting
on those futures from another worker can deadlock the pool. The Python
thread waiting with `future_wait` is the same pattern as tests, and is
safe.

---

## 4. Controller ABI (script session)

New header: `src/controller/inc/rocprofvis_controller_script.h`
(family name `rocprofvis_script_*`, like `rocprofvis_analysis_*`).

```c
rocprofvis_result_t rocprofvis_script_execute_async(
    rocprofvis_controller_t*           controller,
    char const*                        source,
    rocprofvis_controller_arguments_t* context,  // selection, time range
    rocprofvis_controller_future_t*    future,
    rocprofvis_controller_script_result_t** result);

rocprofvis_result_t rocprofvis_script_cancel(
    rocprofvis_controller_future_t* future);
```

Do **not** implement `rocprofvis_controller_create_analysis_view_async`
as a second script entry point. That placeholder stays unused; scripts
go through `rocprofvis_script_*`.

Context properties live in a **dedicated bank at `0x15000000`**
(`kRPVControllerScriptContext*`: selected track handles, time range
start/end). Script **result** text uses `0x14000000`. Do not reuse
timeline (`0x10000000`) or table-args (`0xE0000000`) for context.

Query-table ABI (Phase 1), finishing existing declarations:

```c
rocprofvis_controller_table_t* rocprofvis_controller_table_alloc(void);
void rocprofvis_controller_table_free(rocprofvis_controller_table_t* table);
```

Implementation: `new SystemTable`. Fetch remains
`rocprofvis_controller_table_fetch_async` with the existing
`kRPVControllerTableArgs*` (type, tracks, start/end, where, filter,
group, sort, page).

Per-`use_case` mutex on `SystemTrace` **serializes** a script query
with the UI event-table query; it does not merge row caches. The UI
may hitch during a heavy script query; it must not display the
script's rows.

---

## 5. Bindings (`src/controller/src/python/`)

One translation unit may include `Python.h`. Every `rocprofvis_handle_t*`
is a capsule (`owns=0` borrowed, `owns=1` for alloc'd future/array/table).
Bindings call only the C ABI.

User-facing surface (injected as `optiq`, not imported from disk):

```python
optiq.trace                 # current rocprofvis_controller_t*
optiq.selection             # tracks + time range from context args
optiq.result.text(str)
optiq.on_progress(cb)       # optional; default is none

t = optiq.table()           # table_alloc; NOT the UI singleton
t.fetch(tracks=..., start=..., end=..., where=..., group=...)
for row in t.rows():
    ...

for e in optiq.selection.tracks[0].events():
    gap = e.end - e.start
```

Property getters wrap `get_uint64` / `get_double` / `get_string` /
`get_object` for a small set of names (`track.id`, `event.start`,
…). Raw property ids can remain an escape hatch.

### 5.1 Async from Python (looks sync, polls internally)

The ABI is async. The Python thread is the waiter, like the UI thread.
User scripts should not write poll loops.

Inside `track.events()` / `table.fetch()`:

1. Issue `*_async` (work runs on `JobSystem`).
2. Slice-wait (`future_wait(inner, ~0.05s)`).
3. Copy inner progress onto the **script session** Future so the view
   bar updates without Python running a callback.
4. Check cancel / `PyErr_CheckSignals`.
5. On completion, copy rows/events into Python objects (or keep the
   array alive for the list). Do not leave live handles after
   `CloseController`.

`wait(FLT_MAX)` is correct but silent: the script cannot react until
the fetch ends. Slice-wait is the default so cancel and UI progress
work. An optional `optiq.on_progress` runs between slices.

One outstanding inner wait per script in v1. Sequential fetches are
enough.

---

## 6. View integration

Follow `DataProvider`'s existing request/poll path (same as table
fetch), not a parallel-only event channel.

- `RequestType::kExecuteScript`.
- `DataProvider::ExecuteScript(source, track_ids, start_ts, end_ts)`
  builds script-context `arguments_t`, calls
  `rocprofvis_script_execute_async`, polls the Future each frame,
  forwards `kRPVControllerFutureProgressPercentage`. Cancel calls
  `rocprofvis_script_cancel` **then** `future_cancel` (script jobs are
  not on JobSystem). Closing a tab posts `ScriptExecuteCompleteEvent`
  from cleanup so the editor does not stay on Running.
- **Script editor** (`widgets/rocprofvis_script_editor.*`): floating
  `ImGui::Begin` overlay like `LogViewer`, not a docked column. Open
  from `View > Show Script Editor` (checkable) and a **Script** toolbar
  button next to Ask Optiq on TraceView and ComputeView. Run / Cancel /
  Load / Save; Load/Save use `AppWindow` file dialogs with a `.py`
  filter. Source is `InputTextMultiline` (via `InputTextMultilineString`);
  the result pane is `optiq.result.text` / error text. No syntax
  highlighting in v1.
- Run uses the **current tab's** ready `DataProvider`. Empty track
  selection means all tracks; time range is the timeline selection or
  the full trace. Compute traces can open the editor; `Track.events()` /
  `optiq.table()` are system-oriented and may error in Python.
- Phase 2: page a script-owned table handle with `InfiniteScrollTable`
  using a **unique** request id (not `EVENT_TABLE_REQUEST_ID`).

Ask Optiq (Phase 3): tool `run_analysis_script { "script": "..." }`
calls the same `ExecuteScript`. Show the generated source in the
editor so the user can audit it. Do not add a second interpreter path.
UI mutation stays in `OptiqActions`.

---

## 7. Embedding and restriction

In-process CPython is **not a jail**. The goal is to stop mistakes and
a prompt-injected agent, not a hostile local user.

**No system Python at runtime.** Vendor a known CPython (Windows
embeddable zip; `libpython` + stdlib tree/zip on Linux/macOS). Build
against those headers. Isolated `PyConfig`:

- `PyConfig_InitIsolatedConfig` (ignore env, user site)
- `sys.path` = bundled stdlib + `optiq` only
- no `pip` / `ensurepip`

Until the vendor package is wired (Phase 3), a **build-machine**
Python is allowed to link the embed, but startup must still use
isolated config and a pinned `sys.path` so a user's `PYTHONPATH`
cannot inject modules.

**Import allowlist** (deny-by-default), not a denylist. Phase 1 set:

- `math`, `statistics`, `decimal`, `fractions`
- `itertools`, `functools`, `operator`, `collections`, `heapq`
- `dataclasses`, `typing`, `enum`
- `json`, `re`, `datetime`, `textwrap`, `string`
- `optiq`

Do not allow `os`, `subprocess`, `shutil`, `pathlib`, `ctypes`,
`socket`, `importlib`, `multiprocessing`, `pip`. Omit `open` or
replace with a scratch-dir helper later. No numpy in v1 (stdlib +
`optiq` is enough). Vendoring numpy is an explicit later choice: ship
a matching wheel, still no pip.

Also: timeout + `PyErr_SetInterrupt`; fresh exec dict every run;
document that scripts have **trace read** access in-process.

A real sandbox (Wasm or a locked-down child) would require RPC for
the controller ABI. Out of scope until the threat model changes.

---

## 8. Phases

CMake: `option(ROCPROFVIS_ENABLE_SCRIPTING ...)` default **OFF**, same
spirit as profiler/remote.

### Phase 0 — Runtime skeleton

**Done when:** a Catch2 test executes a source string and reads back
text, with the interpreter on its own thread.

- `src/python/` lib: init / exec / interrupt / shutdown, isolated
  config, import allowlist (even if stdlib still comes from the
  build-tree Python).
- Controller `ScriptEngine` + `rocprofvis_script_execute_async`.
- Bindings: inject `optiq`, `optiq.result.text` only.
- No view code. No trace reads.

### Phase 1 — Custom analysis read path

**Done when:** a script can query events without touching the UI event
table (Catch2 + sample trace). Even-spacing is the acceptance script.

- Implement `table_alloc` / `table_free` as a private `SystemTable`.
- Bindings: `optiq.trace`, `optiq.selection`, `optiq.table().fetch()`,
  `Track.events()` with slice-wait and session-Future progress.
- Copy fetch results into Python (do not share UI table handles).
- Controller tests against a sample trace.

### Phase 1b — Minimal UI

**Done when:** a floating editor can run a script and show text (including the
even-spacing example).

- `DataProvider` request type + poll + progress callback.
- Floating `ScriptEditor` (`InputTextMultiline` + text result, Load/Save,
  View menu + toolbar).

### Phase 2 — Present analysis in the view

**Done when:** script output can appear as a table the UI pages,
without using the Event Table singleton.

- Result payload: text + table handle (in-memory result table, or
  display of a query table the script kept).
- `InfiniteScrollTable` (or equivalent) keyed by the script table
  handle / client request id.
- Optional CSV via existing `table_export_csv` once the handle is a
  real `Table`.
- Annotations / plots only if they drop out cheaply; otherwise Phase 4.

### Phase 3 — Assistant + shipped runtime

**Done when:** Ask Optiq can generate a script, the user can see it,
and a release build does not require a system Python.

- Tool `run_analysis_script` on the Phase 1/2 execute path.
- Composer / editor shows the source before or as it runs.
- Vendor embeddable CPython into the package; CI builds against it.
- Tighten restriction (optional RestrictedPython, scratch-dir `open`).

### Phase 4 — Widen the analysis surface

- Compute-trace wrappers (kernels, metrics) on demand.
- Optional vendored numpy (matching ABI, still no pip).
- Syntax-highlighted editor, saved snippets.
- Only then consider Wasm/process isolation if required.

---

## 9. Non-goals (v1)

- Scripts mutating ImGui or calling `OptiqActions` directly.
- SQLite / `src/model/` access from Python.
- Concurrent fetches inside one script.
- Implementing `create_analysis_view_async` as a competing API.
- Claiming RestrictedPython or an import allowlist is a security VM.

---

## 10. Suggested first implementation slice

Phases 0, 1, and 1b are in tree. Next is Phase 2 (result tables in
the view). Do not start Ask Optiq or vendored CPython until that
presentation path is stable.
