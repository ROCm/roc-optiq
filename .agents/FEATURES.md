# Features

Optional features that cross the View, controller, and model: two-trace compare, remote traces over SSH, in-app profiler launch, and the Ask Optiq assistant. Each builds only with its CMake flag, all default OFF; [Build and feature flags](ARCHITECTURE.md#build-and-feature-flags) explains the gating.

## Trace compare

Opens two system traces as one project: a combined timeline whose tracks carry an A/B source badge, and details tabs split into A/B panes. Flag: `ROCPROFVIS_ENABLE_TRACE_COMPARE`.

**Code:** `rocprofvis_compare_files_dialog.{h,cpp}`, `rocprofvis_compare_panes.{h,cpp}`, `rocprofvis_analysis_view.{h,cpp}`, `model/rocprofvis_trace_data_model.{h,cpp}`

**Related:** [App window and projects](UI.md#app-window-and-projects) (`File > Compare` and the `.rpv`), [Details panel](UI.md#details-panel) (the A/B panes), [Tables](UI.md#tables) (deferred table fetches), [C ABI and properties](CONTROLLER.md#c-abi-and-properties) (`rocprofvis_controller_alloc_compare`), [C ABI and Python bindings](DATABASE.md#c-abi-and-python-bindings) (`rocprofvis_db_open_database_multi`)

- The flag gates the entry points: `CompareFilesDialog`, `AppWindow::OpenCompare` / `MakeCompareId`, `Project::OpenCompare`, and `rocprofvis_controller_alloc_compare`. The pane helpers are always compiled because `AnalysisView`, `EventsView`, and `TrackDetails` call them; branch on `IsCompareTrace(model)` there, not on the flag.
- A compare project is keyed by the synthetic ID from `MakeCompareId`, not a trace path, and its `.rpv` stores both source paths relative to itself. A build without the flag refuses that `.rpv` with a rebuild message.
- Each file's tracks carry their source index, which the View reads as `TrackInfo::file_id` and resolves with `TraceDataModel::GetCompareSource()`. Never infer provenance from track IDs. `DataProvider::ApplyTrackOrderRanking` puts counterpart tracks next to each other.
- Both sources use the same controller table type, and the controller keeps one table per type. Each pane has its own `TablesModel` slot (`kCompareEventTableA`, `kCompareEventTableB`, and the sample pair) and client ID (`COMPARE_CLIENT_ID`). When `FetchTable` refuses because the other pane holds the table, `InfiniteScrollTable::m_fetch_data` stays set and `Update()` retries, so a refused fetch is not an error.
- **Reuse:** build every A/B layout from `rocprofvis_compare_panes.h`: `MakeCompareSplit`, `BeginCompareCard` / `EndCompareCard`, `RenderCompareCardTitle`, and `BuildCompareGroupByChoices` for a group-by list spanning both sources. Track badges are `RenderCompareSourceBadge` / `CompareSourceBadgeWidth`.

## Remote SSH

Opens a trace that lives on another machine: saved connection profiles, a non-blocking session that runs one SSH phase at a time through the controller's C API, and the dialogs on top. Flag: `ROCPROFVIS_ENABLE_REMOTE`; TLS comes from `CRYPTO_BACKEND` (vendored `mbedTLS`, the default, or system `OpenSSL`).

**Code:** `src/view/src/remote/`, `src/controller/src/remote/`, the SSH entry points in `src/controller/inc/rocprofvis_controller.h`

**Related:** [AppMonitor](UI.md#appmonitor) (polls each phase, defers teardown), [Profiler launch](#profiler-launch) (remote runs reuse the session), [Settings and persistence](UI.md#settings-and-persistence) (profiles.json), [Building blocks](CONTROLLER.md#building-blocks) (the `JobSystem` jobs behind each phase)

- One `SshSession` owns one connection and runs one `SshOperation` (`Connect`, `Authenticate`, `Execute`, `Download`, `Browse`) at a time; a second `Start*` asserts and returns 0. A failure is reported before its job lets go of the connection, so wait for `SshSession::IsIdle()` before the next phase. `IsConnected()` only means a handle is allocated.
- Listeners filter `kRemoteStatusChanged` by the operation ID their `Start*` call returned (`GetActiveOperationId()`); every session emits the same event.
- A pending auth prompt must be drawn every frame, or its blocked worker never returns. `AppWindow::Render` calls `RenderSshAuthModals()` for every live session. An owner that is itself a modal (`RemoteFileBrowser`, `SshSettingsDialog`) calls `SetAuthModalSelfManaged(true)` and draws `RenderSshAuthModal(session)` inside its own modal each frame, because ImGui modals are exclusive.
- Passwords and key passphrases live only in `SecretStore`, keyed `ssh/<connection id>/password` and `ssh/<connection id>/passphrase`. When `SecretStore::IsAvailable()` is false (Linux without libsecret), leave the secret unset and let the connection prompt. Never log credentials. `SecretStore` builds when this flag or `ROCPROFVIS_ENABLE_AGENTIC_PROFILING` is on; Ask Optiq keeps its API keys there too.
- Keep persisted identity apart from per-operation state: `SshConnectionStore` saves `SshConnectionConfig`s in the `ssh_connections` section of profiles.json through `ProfilesDocument`; `RemoteUri` holds the command, result path, browse path, and cache key.
- How it works: each `Start*` clears the connection's latched status (`rocprofvis_controller_remote_reset`), calls one `rocprofvis_controller_remote_*_async` function, and registers the phase with `AppMonitor`, which polls `SshSession::Poll()` and emits `RemoteStatusEvent`s. The controller's `SshBridge` contract: workers own every status change, a terminal Completed or Failed stays latched until reset, and a worker waits on the UI only at the prompt and host-key rendezvous. An execute result is the transport outcome; the remote exit code comes back separately.
- Destroying a session while its last job may still run hands the connection to `AppMonitor::AppendTeardown`, so a worker never sees it freed.
- `RemoteTraceOrchestrator` chains connect, authenticate, optional execute, and optional download, then passes the local path to its open callback. `BrowsePath()` and `DownloadPath()` reuse an authenticated idle session; a failed listing keeps it, any other failure drops it. Downloads cache under `{config}/remote_cache/<connection-and-path-hash>/<filename>`; trusted host keys go to the user's `.ssh/known_hosts`.
- Payloads in `rocprofvis_ssh_fetch.h` (`PromptRequest`, `HostKeyRequest`, `ExecutionOutput`, `FileStat`, `RemoteDir`) are mutex-guarded; read them with `ConsumeIfUpdated()` or `Get()` and never hold a lock across ImGui calls.
- **Reuse:** `SshTestDialog` (`File > Open Remote...`) and the launcher's remote Browse buttons share `RemoteFileBrowser`; adopt its live session with `RemoteFileBrowser::TakeSession` instead of reconnecting.

**Recipe: add a remote operation.**

1. Declare a `rocprofvis_controller_remote_*_async` function in `rocprofvis_controller.h` under `#ifdef ROCPROFVIS_ENABLE_REMOTE`. In `rocprofvis_controller.cpp`, validate the handles and call a new static on `Remote`.
2. In `Remote`, read inputs from `Arguments` and issue a `JobSystem::Get().IssueJob` job that calls an `SshClient` method (pass the `Future*` so it stops on cancel) and ends by setting `kRPVControllerSshCompleted` or `kRPVControllerSshFailed` on `connection.GetSshBridge()`.
3. In `SshSession`, add an `SshOperation` value, a `Check*` case in `Poll()`, and a `Start*` method that calls `BeginOperation` with a `MonitorOperationType`.
4. If it returns data, add a snapshot class to `rocprofvis_ssh_fetch.h` and fill it from the `Check*` method.
5. Start it from an orchestrator once `IsIdle()` holds, and advance on its `kRemoteStatusChanged`.

## Profiler launch

Configures and runs a profiler from inside Optiq, locally or over SSH, and opens the trace it produces. Flag: `ROCPROFVIS_ENABLE_PROFILER`; remote runs also need `ROCPROFVIS_ENABLE_REMOTE`, since `RemoteProfilerSession` and `SshProfilerExecutor` build only with both.

**Code:** `src/view/src/profiler/`, `src/controller/inc/rocprofvis_profiler.h`, `src/controller/src/profiler/`

**Related:** [AppMonitor](UI.md#appmonitor) (session polling and deferred teardown), [Remote SSH](#remote-ssh) (the session remote runs reuse), [Settings and persistence](UI.md#settings-and-persistence) (`ProfilerSettings`, profiles.json), [Overview](CONTROLLER.md#overview) (controller threading rules)

### Rules

- No profiler binary path comes from the UI, a settings file, or a profile. A tool is a `rocprofvis_profiler_tool_t`, and the controller's tool table (`ProfilerTool::GetBinaryName`) owns every file name. A profile may set `LaunchConfig::tool_directory`, a directory only; when set, it is the only place searched, with no fallback to `$ROCM_PATH` or `$PATH`.
- `rocprofvis_profiler_tool_t` is append-only: profiles store its integer, so renumbering silently points saved profiles at another tool. `ToolFromInt` bounds stored values with `__kRPVProfilerToolLast`.
- `m_config.tool` is the only copy of the selected tool. Call `SyncToolWithBackend()` after anything that switches the backend or loads a profile, or the combo can show one tool while the launch runs another.
- Every control in a backend tab ORs its ImGui return value into the tab's `render_fn` result. That bool is the only signal that the command preview is stale; `ImGui::IsAnyItemActive()` cannot replace it.
- `FlattenToExecution` emits the complete argv after `argv[0]`: backend flags, `extra_argv`, any output flag, and the target with its arguments split by `SplitArguments`, wherever that tool expects them. The controller never re-splits it, and the preview shows exactly this argv. Never assume `--output`; `ProfilerLaunchSpec::output_directory` reaches no command line, and the controller deliberately has no reader for it.
- The trace path comes only from `IProfilerBackend::ParseTraceOutputPath` on stdout, for local and remote runs. There is no newest-file fallback; locally, an empty result means a warning toast and a manual open.
- Scrape patterns live only in `rocprofvis_controller_profiler_scrape_rules.{h,cpp}`, keyed by tool, operation, and version, so no preset, project, or remote host can supply one.
- The launcher's remote preview, `ToPosixShellCommand` quoting in `SshProfilerExecutor`, and `ProfilerConfig::ResolveToolPathRemote` change together. `ResolveToolPath` answers for the local machine only, so the launcher skips it in SSH mode.

### How it works

- `ProfilerLauncherDialog` authors the `LaunchConfig` and renders. `ProfilerLaunchOrchestrator` is the run engine (`Launch`, `Cancel`, `Close`, `Update`) and hides the local/remote difference. `ProfilerSession` and `RemoteProfilerSession` derive from `ProfilerSessionBase`, which owns the controller config, session, and future handles and registers a `MonitorOperationType::ProfilerSession` operation. A produced trace becomes a project only through `AppWindow::OpenFile`.
- `AppWindow::ShowProfilerLauncher()` (`File > Launch Profiler...`) creates the dialog lazily. Closing it only hides it: `AppWindow` keeps calling `Update()`, so a run settles, auto-loads, and stays viewable. A new launch replaces the session; app shutdown destroys it.
- Local: `OnLaunchClicked` rebuilds the `ExecutionCache` (`FlattenToExecution` plus `extra_env`), `ProfilerSession` calls `rocprofvis_profiler_launch_async`, and on Completed the orchestrator parses the trace path and opens it when `auto_load_trace` is set.
- Remote: `RemoteProfilerSession` steps through `Connecting`, `Authenticating`, `Profiling`, `Downloading`, and `Done` on one `SshSession`. After authentication it passes the idle connection to `rocprofvis_profiler_launch_remote_async`; after the run it parses the remote path and downloads over the same connection. It filters `kRemoteStatusChanged` by the SSH operation ID and `kProfilerStatusChanged` by its profiler operation ID, and the badge reads Completed only at `Done`. Its destructor moves the `SshSession` into `m_extra_teardown`, so the connection is freed after the profiler worker joins.
- Controller: a config handle (`rocprofvis_profiler_config_*`) collects the tool, directory, environment, and one argv entry per call. A session handle (`rocprofvis_profiler_alloc`) runs it through `ProfilerProcessController` with `LocalProfilerExecutor` or `SshProfilerExecutor`; status is read from the session, not the future. `SshProfilerExecutor` owns a dedicated joined thread because `SshClient::ExecuteCommand` blocks until the channel closes. `ProfilerStage` and the scrape rules are groundwork for multi-stage pipelines whose stage C ABI does not exist yet. Tests: `roc-optiq-controller-profiler-tests`.
- **Reuse:** `rocprofvis_launch_shared_tabs.h` holds the launcher UI pieces (`BeginLaunchCard` / `EndLaunchCard`, `EditablePill`, `RenderConfigChips`, `RenderTargetSection`, `RenderCommandPreview`, `RenderOutputConsole`). Launch profiles are `LaunchPresetManager` entries in the `launch_profiles` section of profiles.json, not Compute `PresetManager` presets; `RocprofSysSettings::rocprof_preset` is a rocprof-sys `--preset=` name, a third thing.

### rocprof-sys backend

`RocprofSysBackend` is the only registered backend. It offers `kRPVProfilerToolRocprofSysRun`, `kRPVProfilerToolRocprofSysSample`, and `kRPVProfilerToolRocprofSysInstrument`, and flattens per tool:

- Run and Sample (`FlattenRunOrSample`): `--preset=`, `--trace=`, `extra_argv`, `--output <dir>`, then `--` and the target.
- Instrument (`FlattenInstrumentRuntime`) is one-shot runtime instrumentation. `--preset` and `--trace` are parse errors there, and `--output` names a rewritten binary and exits without running the app, so the output folder goes in `ROCPROFSYS_OUTPUT_PATH` and Perfetto in `ROCPROFSYS_TRACE`. A power user may still put `-o` in `extra_argv` to opt into rewriting; do not strip it.

### Recipes

**Add a profiler backend.**

1. Implement `IProfilerBackend` in `src/view/src/profiler/`. `GetTools` returns `ToolOption`s that name enum values, never paths; settings persist only through `SaveSettings` / `LoadSettings` (`LaunchConfig::backend_payload`); override `ParseTraceOutputPath`, whose default returns empty and disables auto-open.
2. Push it onto `m_backends` in the `ProfilerLauncherDialog` constructor beside `RocprofSysBackend`.
3. For a binary the enum lacks, append a `rocprofvis_profiler_tool_t` value before `__kRPVProfilerToolLast`, add its case to `ProfilerTool::GetBinaryName`, and add scrape rules if its output must be read. rocprofv3 and rocprof-compute already have values (`kRPVProfilerToolRocprofV3`, `kRPVProfilerToolRocprofCompute`).
4. Add the new files to the `ROCPROFVIS_ENABLE_PROFILER` list in `CMakeLists.txt`.

## Ask Optiq

An in-app LLM analyst that reads the open system trace through the normal View APIs and drives the UI the way a user would. Built only with `ROCPROFVIS_ENABLE_AGENTIC_PROFILING` (default OFF).

**Code:** `src/view/src/agenticprofiling/`

**Related:** [Editor and approval](SCRIPTING.md#editor-and-approval) (`run_analysis_script` proposes scripts into the Script tab), [Data provider](UI.md#data-provider) (the request IDs the data tools share with the normal UI), [Trace view](UI.md#trace-view) and [Details panel](UI.md#details-panel) (the accessors `OptiqActions` calls), [Settings and persistence](UI.md#settings-and-persistence) (`AssistantSettings`; the API key lives in `SecretStore`)

### Rules

- **Guard every call site outside `agenticprofiling/`** with `#ifdef ROCPROFVIS_ENABLE_AGENTIC_PROFILING`. They live in `AppWindow`, the `TraceView` toolbar, and `SettingsPanel`. `AssistantProvider` / `AssistantSettings` and their JSON stay compiled either way, so settings files round-trip through builds without the assistant; only the token methods (`HasAssistantToken`, `GetAssistantToken`, `SetAssistantToken`, `ClearAssistantToken`, `MigrateLegacyAssistantToken`) are guarded.
- **Only `OptiqActions` mutates the UI.** Each method reproduces one real interaction, including the events the rest of the app listens for. Tools never touch widgets directly.
- **Tools run only from `Update()`,** never `Render()`: they toggle panels and rebuild layout.
- **A tool formats only rows from a fetch it started.** Table request IDs are shared with the normal UI: check `IsRequestPending`, wait for the other owner, and retry against the original deadline rather than reading someone else's rows.
- **Every tool call goes through `BeginToolQueue`** and answers on the next round, including UI-only tools. The final answer is a separate round with tools off (`BeginFinalAnswer`).
- **HTTP runs on `std::async`**, the sanctioned exception to the no-threads rule. It must stay cancellable: `AssistantChatCall::Cancel()` closes the socket so the panel can be destroyed without waiting out a read timeout.
- **The API key lives only in `SecretStore`,** one entry per endpoint, never in settings JSON or a log line.
- **System traces only.** `StartAssistantTool` rejects compute traces once, for every tool.

### How it works

Transport at the bottom, panel at the top:

| File | Role |
|---|---|
| `rocprofvis_ai_client.*` | `AssistantChatCall`: one POST to an OpenAI-style chat endpoint plus the reply parser. Knows nothing about traces. |
| `rocprofvis_ai_tool_schema.*` | `BuildAssistantToolsJson` and `ASSISTANT_TOOL_LABELS`. Touches no View state, so it is safe on the HTTP thread. Tool descriptions here are the model's only instructions for each tool. |
| `rocprofvis_ai_tool_query.*` | Model arguments to table filters and grouping. Treats input as hostile: column and operator allowlists, quoted literals, escaped `LIKE`. Columns must be names the rows carry (`node`, not `nodeId`). |
| `rocprofvis_ai_tools.*` | `StartAssistantTool`: parses arguments, refuses work with no trace, dispatches to the handler tables. |
| `rocprofvis_ai_ui_tools.cpp` | Tools that change Optiq (`goto`, `show_panel`, `annotate`, `measure`, `offer_next_steps`, ...). They finish synchronously and never start a fetch. |
| `rocprofvis_ai_data_tools.cpp` | Tools that read the trace: queue `DataProvider` fetches, then format rows in `FinishAssistantFetch`. |
| `rocprofvis_ai_script_tools.cpp` | `run_analysis_script`, compiled only with `ROCPROFVIS_ENABLE_SCRIPTING`. Offers the script through `OptiqActions::ProposeScript`; the user runs or rejects it. |
| `rocprofvis_ai_actions.*` | `OptiqActions`. |
| `rocprofvis_ai_assistant.*`, `rocprofvis_ai_assistant_render.cpp` | `AssistantPanel` singleton: transcript and turn loop, with drawing split into the render file. |
| `rocprofvis_ai_prompts.*` | `ASSISTANT_SYSTEM_PROMPT`, plus `ASSISTANT_SCRIPT_PROMPT` appended under the scripting flag. |
| `rocprofvis_ai_settings.cpp` | The Settings page and per-endpoint key storage. |
| `rocprofvis_ai_trace_view_actions.cpp` | `TraceView` methods only the assistant calls. Move one back to `rocprofvis_trace_view.cpp` once anything else needs it. |

The tool context is rebuilt on every call (`MakeToolContext()`), so it never holds a dead provider; `m_turn_project_id` detects the front trace changing mid-turn. Endpoints are stock OpenAI or Azure-style, decided once by `EndpointFlavour` from the URL. The request body never sets `temperature` or `reasoning_effort`, so any chat model works.

### Prompt rules

- The base prompt must not name a tool the build might lack; scripting text lives only in `ASSISTANT_SCRIPT_PROMPT`.
- Every `WHAT TO LOOK FOR` entry must be answerable with the existing tools and column allowlist, or the model invents arguments. The list is re-sent every round, so keep it one line per check; move anything needing arithmetic into a C++ tool.
- `LIMITS` must match what the data model records. Extend it when a tool's reach changes.

### Recipes

**Add a tool.**

1. Add a schema entry and a label to `ASSISTANT_TOOL_LABELS` in `rocprofvis_ai_tool_schema.cpp`.
2. Write the body in the UI, data, or script tools file, matching what it touches, and register it in that file's handler table.
3. If it changes the UI, add one `OptiqActions` method for the interaction. If that needs a new `TraceView` accessor, put it in `rocprofvis_ai_trace_view_actions.cpp`.
4. If the prompt should mention it, keep the mention true for every build configuration.
