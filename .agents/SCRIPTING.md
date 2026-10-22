# Python scripting

In-app Python analysis of the open system trace: the embedded interpreter, the controller's script session and `optiq` bindings, and the Script tab. Read this before changing any of them; the API reference for script authors is [PYTHON.md](../PYTHON.md).

## Overview and status

A user, or Ask Optiq once the user approves, runs Python against the open trace and gets text back.

**Code:** `src/python/`, `src/controller/src/python/`, `src/controller/inc/rocprofvis_controller_script.h`, `src/view/src/widgets/rocprofvis_script_editor.{h,cpp}`

**Related:** [Ask Optiq](FEATURES.md#ask-optiq) (offers scripts), [Details panel](UI.md#details-panel) (hosts the Script tab), [Build and feature flags](ARCHITECTURE.md#build-and-feature-flags) (`ROCPROFVIS_ENABLE_SCRIPTING`, `ROCPROFVIS_ENABLE_VENDOR_PYTHON`)

- In development behind `ROCPROFVIS_ENABLE_SCRIPTING` (default OFF). In tree: the runtime, the read path (`optiq.trace`, `optiq.selection`, `optiq.table().fetch()`, `Track.events()`), the Script tab, and Ask Optiq's `run_analysis_script`. Output is text only; next is a result table the View pages with an `InfiniteScrollTable` under its own request id, never `EVENT_TABLE_REQUEST_ID`. Compute traces cannot be scripted yet.
- The editor calls `DataProvider::ExecuteScript`, which calls `rocprofvis_script_execute_async`. `ScriptEngine` queues the run on the interpreter thread of `roc-optiq-python`, `optiq_prepare_globals` injects `optiq`, and the script's fetches go through the public controller C ABI to `JobSystem` while the interpreter thread waits.
- The controller links `roc-optiq-python` privately. The runtime includes no controller header and sees the controller only through hooks it is handed; the View includes no runtime or Python header. `src/model/python/` is the data model's unrelated CFFI binding.
- Scripts query a private `SystemTable` from `rocprofvis_controller_table_alloc`, never the UI singletons `kRPVControllerSystemEventTable` / `kRPVControllerSystemSampleTable`, whose rows and arguments a second fetch would overwrite. Fetches of one use case still serialize on `SystemTrace::GetTableMutex`, so a heavy script query can stall the UI table but never shows in it.
- Non-goals: scripts never mutate the UI or call `OptiqActions`, and never reach SQLite or `src/model/`. `rocprofvis_controller_create_analysis_view_async` is an unimplemented placeholder, not a second entry point.

The acceptance script below checks whether events on the first selected event track are evenly spaced. It is also the editor's `DEFAULT_SCRIPT` and an example in PYTHON.md; change all three together.

```python
track = None
for t in optiq.selection.tracks:
    if t.type == optiq.TRACK_TYPE_EVENTS and t.num_entries > 0:
        track = t
        break
if track is None:
    optiq.result.text('No event track in the selection')
else:
    events = track.events(start=optiq.selection.start, end=optiq.selection.end)
    if len(events) < 2:
        optiq.result.text('Need at least two events on ' + track.name)
    else:
        gaps = [events[i].start - events[i - 1].end for i in range(1, len(events))]
        mean = sum(gaps) / len(gaps)
        max_dev = max(abs(g - mean) for g in gaps)
        even = max_dev <= (abs(mean) * 0.1)
        optiq.result.text(track.name)
        optiq.result.text('events=' + str(len(events)))
        optiq.result.text('mean_gap=' + str(mean))
        optiq.result.text('max_dev=' + str(max_dev))
        optiq.result.text('even' if even else 'uneven')
```

## Runtime and sandbox

`roc-optiq-python` embeds CPython: one long-lived interpreter thread that runs queued scripts one at a time, a watchdog thread for deadlines and cancels, and the limits on what a script reaches.

**Code:** `src/python/inc/rocprofvis_python_runtime.h`, `src/python/src/rocprofvis_python_runtime.cpp`, `src/python/inc/rocprofvis_python.h`, `cmake/rocprofvis_python_vendor.cmake`

**Related:** [Controller ABI and bindings](#controller-abi-and-bindings) (the controller side of the hooks), [Known gaps](#known-gaps) (open sandbox holes)

**Threat model.** The interpreter runs in-process and shares memory with the UI, the controller, and the open trace. It is not a sandbox: a determined script can reach arbitrary code in the process. The security boundary is user approval before a run: nothing executes until a person presses Run on source they can read, and Ask Optiq can only offer a script. The measures below are defence in depth against mistakes and casual escapes, never containment; describe them that way everywhere, PYTHON.md included. If a generated script ever runs unattended, this model is void and process isolation has to be reopened.

- Import allowlist: `restricted_import` refuses any module whose top-level name is not in `ALLOWLISTED_MODULES` (stdlib math, data, and text modules, plus `optiq`).
- Reduced builtins: `SAFE_BUILTIN_NAMES` omits `open`, `eval`, `exec`, `compile`, `getattr`, `vars`, and `globals`.
- Source screen: `ScreenSource` walks the script's `ast` and refuses interpreter-internal names (`__globals__`, `__class__`, `__subclasses__`, ...) in attribute or name position, closing escapes such as `json.dumps.__globals__`. It screens names, not values, so a function that takes an attribute name as a string gets past it. It fails closed: init fails if the screen cannot compile.
- Isolated `PyConfig`: no environment, `site`, user site, or signal handlers, and `sys.path` is the runtime's own stdlib.
- Deadline: `SCRIPT_TIMEOUT_MS` (30 s, used when the caller passes 0, as the controller does) bounds every run, so a runaway loop cannot hold the only interpreter thread forever.

Rules:

- Never run a script on a `JobSystem` worker: scripts wait on futures whose jobs run on that pool.
- Include `rocprofvis_python.h`, never CPython's header directly, after the standard headers. It undefines `_DEBUG` around the include because stock Windows CPython ships no `python3xx_d.lib`.
- Do not add `PyErr_SetInterrupt`: with `install_signal_handlers` off, CPython ignores the signal and clears any exception already raised. The watchdog delivers `KeyboardInterrupt` with `PyThreadState_SetAsyncExc` under the GIL, so `rocprofvis_python_interrupt` never blocks its caller, and repeats it every `INTERRUPT_RETRY_MS` because a bare `except:` swallows one.
- A deadline stop reports `kRocProfVisPythonError` ("timed out"); only an explicit interrupt reports `kRocProfVisPythonCancelled`. Decide against `m_exec_deadline`, not the retry time `m_next_interrupt` or how the wait ended, or every cancel reads as a timeout.
- Keep the builtins coherent with the allowlist: a `class` statement needs `__build_class__`, and `dataclasses` and `enum` need classes. Never add `getattr` or a module that reads attributes by string, and never screen ordinary dunders such as `__init__`.

A run (`Runtime::ExecWork`) clears stale async exceptions, screens the source, builds fresh globals (`MakeScriptGlobals`), and calls `prepare_globals`; anything but success skips the run and is reported as-is. `teardown_globals` follows whenever prepare ran, with the GIL held, and `done` gets the result without it. Errors carry the traceback from `FormatPythonError`, trimmed to the last `MAX_ERROR_CHARS`, so a user or the assistant can fix the failing line.

`resolve_layout` uses an explicit `runtime_root`, else `<exe dir>/python`, else `ROCPROFVIS_PYTHON_RELDIR`. `ROCPROFVIS_ENABLE_VENDOR_PYTHON` (default ON with scripting) fetches a pinned CPython 3.12 archive by SHA256 and ships a trimmed stdlib through `rocprofvis_python_vendor_deploy` and `rocprofvis_python_vendor_install`. Headers, `libpython`, and stdlib must come from that one archive because the embed uses the full C API. With vendoring OFF the build machine's Python paths are baked in, so the binary runs only where it was built.

## Controller ABI and bindings

The controller's script session API, and the `optiq` module compiled into the controller, which reaches trace data only through the public C ABI.

**Code:** `src/controller/inc/rocprofvis_controller_script.h`, `src/controller/src/rocprofvis_controller_script.cpp`, `src/controller/src/rocprofvis_controller_script_engine.h`, `src/controller/src/python/rocprofvis_controller_python.{h,cpp}`

**Related:** [C ABI and properties](CONTROLLER.md#c-abi-and-properties) (handles and property banks the bindings call), [Runtime and sandbox](#runtime-and-sandbox) (hooks and interrupt), [Tests](ARCHITECTURE.md#tests) (`rocprofvis_controller_script_tests.cpp` runs against a sample trace)

- `ScriptEngine::ExecuteAsync` sets a `Job` on the caller's future that never enters `JobSystem`; `on_python_done` completes it when the run ends. So `rocprofvis_controller_future_cancel` never reaches a script; `rocprofvis_script_cancel` does, marking the session and interrupting only if that session is running, because the interrupt is process-global and other traces may have scripts queued.
- Context has its own property bank, `kRPVControllerScriptContext*`; results are `kRPVControllerScriptResultText` and `kRPVControllerScriptResultErrorMessage`. Never put context in the timeline or `kRPVControllerTableArgs*` banks.

Rules:

- Bindings call only `rocprofvis_controller_*`, never C++ `Handle` subclasses. In the controller, only `rocprofvis_controller_python.cpp` includes `rocprofvis_python.h`.
- Any wrapper entry point that reads a controller, a session, or a handle borrowed from either calls `check_generation(self->generation)` first, including helpers that take wrappers from the script (`append_tracks_from_sequence`). A script can keep a wrapper past its run, on an allowlisted module (alive for the whole process) or in a cycle collected later, after its session and perhaps its controller are freed. Never test the pointer instead: `Reference<>` validates by calling a virtual through it. Python-only state (`Table.rows`, `Trace.tracks`) and value copies (`Event`, `Selection`) need no check.
- `optiq_prepare_globals` fails closed, returning an error with a Python exception set, because a half-built environment lets `import optiq` find the previous run's module in `sys.modules`. Only the default time window (`timeline_range`) is soft.
- A script cancelled while queued is refused by `ScriptEngine::BeginSession` returning false and reports `kRocProfVisPythonCancelled`. Do not interrupt there: the interrupt is a no-op until exec starts. Prepare checks `Session::cancelled` again before returning success.
- `optiq_teardown_globals` removes `optiq` from `sys.modules` and empties the module, since a script may keep it (`json.keep = optiq`). It must run in teardown, with the GIL held and the session alive; `done` has neither.
- Wait on inner fetches only through `wait_inner`. It slice-waits `SLICE_WAIT_SECONDS` without the GIL and copies progress to the session future. On a cancel or signal it cancels the inner future once but waits for it to finish, because the caller then frees the future and `~Future` deletes a job a worker may still run. One inner fetch at a time.
- A cancel cannot leak because every binding either returns Python objects that own their C handles or frees them on its error path. A progress callback would run script code inside `wait_inner` while `array`, `future`, and `fetch_args` are owned only by the C frame; make those exception-safe before adding one.
- `Event` is a copy built by `copy_event` from the event or sample property bank. Extend it there, nulling owned fields first, since `PyObject_New` does not zero.
- `Table.fetch` passes `filter`, `group`, and `group_columns` through unvalidated, deliberately: a person reads every script, and whitelisting would take the query language away from users. The assistant's own tools validate with `BuildAssistantFilterExpression` because nobody reads a tool call. Revisit if scripts ever run unattended.

**Recipe: extend the `optiq` API.**

1. Add the getter or method in `rocprofvis_controller_python.cpp` on the public C ABI only, calling `check_generation` first if it touches controller or session state. Reuse `set_fetch_args`, `wait_inner`, and `copy_event` / `copy_table_rows`.
2. Return copies; no live handle leaves a binding.
3. Add a case to `rocprofvis_controller_script_tests.cpp`.
4. Update [PYTHON.md](../PYTHON.md) and the `run_analysis_script` description in `rocprofvis_ai_tool_schema.cpp`, the model's only account of the API. Allowlist, builtin, and timeout changes need the same two updates.

## Editor and approval

The View runs scripts through `DataProvider` and the details panel's Script tab; Ask Optiq's `run_analysis_script` only offers a script there to run or reject.

**Code:** `src/view/src/widgets/rocprofvis_script_editor.{h,cpp}`, `src/view/src/rocprofvis_data_provider_script.cpp`, `src/view/src/agenticprofiling/rocprofvis_ai_script_tools.cpp`, `OptiqActions::ProposeScript`

**Related:** [Details panel](UI.md#details-panel) (hosts the Script tab), [Data provider](UI.md#data-provider) (request plumbing), [Ask Optiq](FEATURES.md#ask-optiq) (tool dispatch and fetch waits)

- `ScriptEditor` is a plain `RocWidget`, the last tab of `AnalysisView`, so each trace has one editor wired to its own `DataProvider` and `TimelineSelection`. Run sends the selected tracks and time range, else all tracks and the full timeline.
- `DataProvider::ExecuteScript` builds the context and starts a `RequestType::kExecuteScript` request that reports the session future's progress. `ProcessExecuteScriptRequest` frees the result handle, keeps text and error for `GetLastScriptResult`, and posts `ScriptExecuteCompleteEvent`.
- `ScriptApproval` tracks an offer: `kPending`, then `kRunning` and `kFinished` on Run, `kRejected` on Reject, or `kFailedToStart`, so an approved run that cannot start still answers the assistant. The tool parks an `AssistantFetchKind::kScript` fetch with its own `ASSISTANT_SCRIPT_APPROVAL_TIMEOUT_SECONDS` (300 s). Whether the wait completes or times out, `FinishAssistantScriptFetch` answers: a rejection as the user's decision so the model does not re-offer, a failure with the traceback so it can fix the line.

Rules:

- Model-written scripts are offered, never run. `OptiqActions::ProposeScript` fills and selects the Script tab, which opens the details panel, because an offer the user cannot see only waits out its deadline. Run then takes the same path as a hand-written script.
- The assistant reaches the editor only through `OptiqActions`, `TraceView`, and `AnalysisView`; nothing in `agenticprofiling/` keeps the widget.
- One script per trace: `EXECUTE_SCRIPT_REQUEST_ID` is one slot shared by the user and the assistant. A second run is refused, not queued.
- Cancel through `DataProvider::CancelScript`. Trace-close cleanup also calls `rocprofvis_script_cancel`, waits `CLEANUP_WAIT_SECONDS`, then abandons the handles and controller of a script that will not stop rather than free memory under it: a deliberate leak.
- Every editor hears every trace's completion and progress events; act only on those whose source id matches `m_running_source_id`.
- Code that does not listen for the completion event, such as `FinishAssistantScriptFetch`, reads `GetLastScriptResult`; the request and its result handle are gone by then. `ExecuteScript` clears it so nobody reads the previous run.
- Compute traces have no Script tab (they have no `AnalysisView`), and `optiq.table()` refuses them. Add a `ComputeView` tab only with compute bindings.

## Known gaps

- `ScriptEditor::ProposeScript` during a run leaves `m_running_source_id` and the request live, so the old run's completion overwrites the offer's output while approval stays `kPending`. It also drops an unsaved draft without asking.
- `LoadFromFile` and `SaveToFile` give `AppWindow` file dialogs a raw `[this]`; closing the trace with a dialog open leaves `m_file_dialog_callback` calling a destroyed editor.
- A cancelled run reaches the assistant as "The script failed": `GetLastScriptResult` cannot tell cancel from error.
- `operator.attrgetter` and `operator.methodcaller` take attribute names as strings and walk past the source screen. Drop them or screen their arguments, and keep PYTHON.md in step.
- A failed interpreter init is permanent: `Runtime::Init` replays the stored error, and the app never calls `rocprofvis_python_shutdown`.
- Shipping: no macOS scripting bundle has been signed or notarized, and only CPython's own license ships, not the notices of libraries linked statically into `libpython`.
- PYTHON.md says Script opens from a toolbar (it is a details-panel tab) and, in one place, that `print` is unavailable (it works).
- Untested: cancelling a running script, cancel or timeout during a fetch, `import optiq`, the `attrgetter` route, interrupt retry against a bare `except:` (the test catches `Exception`), a wrapper freed late by the collector, and the editor cases above.
