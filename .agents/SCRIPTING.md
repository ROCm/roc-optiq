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
tree. The `run_analysis_script` half of Phase 3 is also in tree; the
vendored CPython half is not.

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
void rocprofvis_python_interrupt(void);   // raise into interpreter thread
void rocprofvis_python_shutdown(void);
```

`prepare_globals` is provided by the controller: it creates the
`optiq` module and stuffs `optiq.trace` / `optiq.selection` into the
exec dict. The runtime never includes `rocprofvis_controller.h`.

**Thread:** one long-lived interpreter thread, owned by this lib (not
by `JobSystem`). Controller posts `{source, prepare, user}` to that
thread and completes the script `Future` when `exec` returns.

**Deadline:** a second thread in the same lib arms a wall-clock
deadline around each `PyRun_String`. `rocprofvis_python_exec` takes the
budget; 0 means the built-in default. Nothing else bounds a script: one
loop that never ends holds the only interpreter thread forever, and the
caller would just see a future that never completes.

**Do not use `PyErr_SetInterrupt` here, and do not add it back.** It
trips the SIGINT flag for `PyErr_CheckSignals` to act on, but the
isolated config sets `install_signal_handlers` to 0, so nothing is
registered to handle it. CPython refuses the signal - `OSError: Signal
2 ignored due to race condition`, printed once per eval-loop tick - and
that ignored-handler path **clears the error indicator on its way
out**, wiping any exception already raised. Pairing it with the raise
below is strictly worse than the raise alone; that combination was
tried and the timeout tests caught it. Delivery is
`PyThreadState_SetAsyncExc` against the interpreter thread's Python id,
which does not involve signal handling and lands the next time that
thread runs a bytecode.

Three more details are load-bearing. The raise is **repeated** on an
interval rather than sent once, because a bare `except:` swallows the
first one - which model-written code does. It is delivered from the
watchdog, under the GIL, so whoever asked to cancel (usually the UI
thread) never blocks on it. And `Shutdown` brings the deadline forward
instead of letting the watchdog exit on the stop flag, since it joins
the interpreter thread and a hung script would otherwise hold that join
open.

A run stopped by the deadline reports an **error**, not
`kRocProfVisPythonCancelled`: a timeout is a script to fix, and only an
explicit cancel is a cancellation.

One gap worth knowing: a script parked inside `wait_inner` has released
the GIL, so the raise only lands once that fetch returns and control is
back in bytecode. The fetch bounds itself, so this has not needed
solving, but it is why a timeout is not instant on a query-heavy
script.

**Errors carry the traceback.** `FormatPythonError` runs the failing
exception through the stdlib `traceback` module, so the message names
the line that raised. The script's import allowlist does not apply to
it, because that `__import__` only exists in the script globals, not to
C callers. This is what lets a failed script be *fixed* rather than
abandoned - by a user reading the editor, and by the assistant, which
gets the same text back as its tool result and can correct the line and
call again.

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

`Event` is a **copy**, not a live handle: `id`, `start`, `end`, `level`,
`name`, `category`, `value`. `copy_event` fills it from the event
property bank for event tracks and the sample bank for sample tracks,
so `level` / `category` are inert on samples and `value` is `None` on
interval events. `PyObject_New` does not zero the payload — null every
owned reference before a path that can decref a partially built event.
Extend `copy_event` (not the fetch loop) when adding fields; user-facing
docs live in [`PYTHON.md`](../PYTHON.md).

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
- **Script editor** (`widgets/rocprofvis_script_editor.*`): the
  **Script tab** of the details panel, built and owned by `AnalysisView`
  beside Event Table / Top Events / Annotations. It is a plain
  `RocWidget` - no `ImGui::Begin`, no singleton, no visibility flag -
  because a docked tab is what the rest of the panel is and a floating
  window was one more thing to find and manage. Run / Reject or Cancel /
  Load / Save; Load/Save use `AppWindow` file dialogs with a `.py`
  filter. Source is `InputTextMultiline` (via `InputTextMultilineString`);
  the result pane is `optiq.result.text` / error text. No syntax
  highlighting in v1.
- **One editor per trace**, because `AnalysisView` is per trace. That is
  what lets `Run` go straight to its own `DataProvider` and
  `TimelineSelection` instead of asking which tab is in front, and it
  means source and output belong to the trace the user is looking at.
  Empty track selection means all tracks; time range is the timeline
  selection or the full trace.
- **Compute traces have no Script tab.** They have no `AnalysisView`,
  and `Track.events()` / `optiq.table()` are system-oriented anyway -
  `run_analysis_script` already refuses a compute trace. Give
  `ComputeView` its own tab when the compute bindings land, not before.
- Phase 2: page a script-owned table handle with `InfiniteScrollTable`
  using a **unique** request id (not `EVENT_TABLE_REQUEST_ID`).

Ask Optiq: the tool `run_analysis_script { "script": "..." }` lives in
`view/src/agenticprofiling/rocprofvis_ai_script_tools.cpp` and calls
the same `ExecuteScript`. There is no second interpreter path, and UI
mutation still belongs to `OptiqActions`. Four things it relies on:

- **Nothing the model writes runs unattended.** The tool *offers* a
  script; it never executes one. `OptiqActions::ProposeScript` fills the
  Script tab, selects it - which opens the details panel through the
  same `SelectAnalysisTab` the model uses for any other tab - and parks.
  The user reads the source and presses **Run** or **Reject**, and the
  tab drives the run through exactly the path a hand-written script
  takes, so there is one execution path rather than two. Selecting the
  tab is part of offering: a question the user cannot see is one the
  assistant would wait on until the deadline. Rejection is reported as a
  decision rather than a failure, and the prompt tells the model not to
  offer the same script back.
- **The assistant never holds the widget.** It reaches the editor
  through `OptiqActions` -> `TraceView` -> `AnalysisView`, the same
  chain as every other UI action, so nothing in `agenticprofiling/`
  keeps a pointer to a view that a closing tab could take away.
- **The wait is on a person, so it gets its own deadline.**
  `AssistantToolStartResult::timeout_seconds` overrides the 45s a fetch
  runs under; the script tool asks for 300. The panel also routes a
  timed-out `kScript` fetch back through
  `FinishAssistantScriptFetch` rather than reporting a generic timeout,
  because only that side knows whether the user never answered or the
  run was abandoned, and it has an outstanding offer to clear.
- **`ScriptApproval` is the whole state machine.** `kPending` ->
  `kRunning` -> `kFinished` on approval, `kRejected` on refusal, and
  `kFailedToStart` when an approved run could not begin - which exists
  so a script that never started still answers the assistant instead of
  waiting out the full five minutes. `AssistantScriptFetchPending` is
  true for `kPending` and `kRunning` only.
- **An offer is pinned to its trace.** `Run` refuses when the tab in
  front is not the trace the script was written against, the same
  mistake `m_turn_project_id` guards elsewhere.
- **Events are filtered by source id.** `ScriptExecuteCompleteEvent` is
  posted from tab-close cleanup as well as from a real run, so the
  editor answers only events carrying the trace it is tracking.
  Without that, closing any other tab drops the editor out of Running
  and wipes its output.
- **`DataProvider` keeps the last result.** The editor listens for the
  completion event, but a caller that polls the request id instead -
  which is how every assistant tool waits - finds the result handle
  already freed. `GetLastScriptResult` outlives the request and is
  cleared when the next script starts, so a poller can never read the
  previous run's text as this one's answer.
- **One script per trace.** `EXECUTE_SCRIPT_REQUEST_ID` is a single
  slot, and the user's own run owns it just as much as the
  assistant's. The tool reports that rather than queueing.

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

**Keep the builtin set coherent with the allowlist.** A `class`
statement compiles to `__build_class__`, so leaving that out of
`kSafeBuiltinNames` made every class fail with a bare `NameError` - and
took `dataclasses` and `enum` down with it, since both are used by
declaring a class, while still being advertised as importable. The
class machinery (`__build_class__`, `type`, `object`, `super`,
`property`, `staticmethod`, `classmethod`) is in the set for that
reason. `print` is not a builtin here: the bindings put one in the
script globals that appends to the result, because there is no stdout
and a bare `NameError` on the first line anyone writes is not a useful
guardrail. When adding a module to the allowlist, check what its normal
use actually needs - `rocprofvis_controller_script_tests.cpp` has a
case per promise, and the tool description is written from it.

Also: exec deadline (see 3); fresh exec dict every run;
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

- ~~Tool `run_analysis_script` on the Phase 1/2 execute path.~~ In
  tree, along with the exec deadline and tracebacks that make an
  unattended script safe to run and possible to fix.
- ~~Editor shows the source before or as it runs.~~ In tree via
  `ShowGeneratedScript`.
- Vendor embeddable CPython into the package; CI builds against it.
- Tighten restriction (optional RestrictedPython, scratch-dir `open`).
- Decide about raw `where` / `group`. `Table.fetch` passes those
  strings to the table args untouched, while the assistant's own
  `BuildAssistantWhereClause` whitelists columns, quotes literals, and
  escapes `LIKE` wildcards precisely because model input is hostile. A
  script is now a second way for a model to reach the same query
  layer, and it skips all of that. The blast radius looks small - trace
  data the user already opened, read-only - but confirm what the DB
  layer does with a hostile fragment and either accept it in writing or
  route these through the existing query builder.

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
