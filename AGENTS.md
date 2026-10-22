# AGENTS.md - ROCm Optiq

ROCm Optiq (`roc-optiq`) is AMD's desktop viewer for ROCm profiler traces: C++17 and Dear ImGui, layered app -> view -> controller (C ABI) -> model (SQLite).

This file is always loaded; the guides in [`.agents/`](.agents/) are not. Check **Already exists** before writing anything, then open the guide section for the component you are changing and follow its **Related** links only as far as your change reaches. [`CODING.md`](CODING.md) wins on style and naming. The source wins over the guides; fix a wrong guide in the same change.

## Rules

1. **Never invent a style value.** Colors, padding, spacing, rounding, and font sizes come from `SettingsManager` and `FontManager`. A missing reusable value is added there; geometry private to one drawing is a named local `constexpr`. See [Styling](.agents/STYLING.md).
2. **Reuse what fits, and keep general code general.** Check the table below, then search the owning folder and its guide section. Use a helper only if it does the job as written; otherwise extend it cleanly or write the right thing, never a workaround. General-purpose code goes in a shared home, not a feature file: move a general file-local function there instead of copying it. Details in [`.cursor/rules/reuse-before-write.mdc`](.cursor/rules/reuse-before-write.mdc).
3. **Keep the layers apart.** The View calls only the controller's public C ABI and gets trace data only through `DataProvider`. It may include the shared type headers `rocprofvis_c_interface_types.h` and `rocprofvis_shared_types.h`, never model or controller internals.
4. **Never block a frame.** Long work is a controller future, a `DataProvider` request, or an `AppMonitor` operation. No `std::thread` or blocking wait in UI code.
5. **Log with `spdlog`, return error codes.** No `printf` or `std::cout` in library code, and no exception crosses a C ABI.
6. **Gate optional features** (remote, profiler, compare, Ask Optiq, scripting, multi-window, developer mode) behind their `ROCPROFVIS_*` flags. See [Build and feature flags](.agents/ARCHITECTURE.md#build-and-feature-flags).
7. **Comments say why.** No narration of the change and no commented-out code.
8. **Keep the guides true.** A change that makes a guide wrong fixes it in the same change. See [Maintaining the book](.agents/ARCHITECTURE.md#maintaining-the-book).

## Already exists

| Need | Use |
|---|---|
| Color | `SettingsManager::GetColor(Colors::k...)`; a new role goes in both theme tables |
| Padding, spacing, rounding | `SettingsManager::GetDefaultStyle().<field>` |
| Font or text size | `FontManager::GetFont(FontType::...)`, `GetFontSize(FontSize::...)`; size boxes from `GetTextLineHeight` / `GetFrameHeight` |
| Confirm or message dialog | `AppWindow::ShowConfirmationDialog`, `ShowMessageDialog` |
| File or folder picker | `AppWindow::ShowOpenFileDialog`, `ShowSaveFileDialog`, `ShowPathPickerDialog`; never NFD or `ImGuiFileDialog` directly |
| Toast or progress | `NotificationManager::GetInstance().Show`, `ShowPersistent` |
| Button, tooltip, card, spinner, popup | `IconButton`, `XButton`, `AccentButton`, `BeginTooltipStyled`, `BeginPanelCard`, `RenderLoadingIndicator`, `PopUpStyle` |
| A widget | Subclass `RocWidget` with `m_widget_name = GenUniqueName(...)`; `RocCustomWidget` for a callback |
| Split, tabs, wrapping row | `HSplitContainer`, `VSplitContainer`, `VFixedContainer`, `TabContainer`, `FlexContainer` |
| `std::string` input | `InputTextString`, `InputTextStringWithHint`, `InputTextMultilineString` |
| Elided or inline-editable text | `ElidedText`, `ElideWithEllipsis`, `EditableTextField` |
| Copyable cell, row menu | `CopyableTextUnformatted`, `RenderRowHitbox`, `BeginCellContextMenu` |
| Event or sample table | Subclass `InfiniteScrollTable` ([Tables](.agents/UI.md#tables)) |
| Compute metric picker or table | `QueryBuilder`, `MetricTable`, `PinnedMetricTable`, `MetricTableWidget` |
| Time or number text | `nanosecond_to_formatted_str`, `compact_number_format`, `parse_compact_number` |
| Timeline math, measurement, click layers | `TimePixelTransform`, `MeasurementController`, `TimelineFocusManager` |
| One widget reacting to another | `EventManager` and a `RocEvents` value; unsubscribe the stored token in the destructor |
| Trace data in the View | `DataProvider`; widgets never call the controller |
| Current selection | `TimelineSelection`, `ComputeSelection`; no private copies |
| Request or client ID | `RequestIdBuilder::MakeRequestId` and its siblings; `IdGenerator` |
| Persisted View state | `ProjectSetting` (`.rpv`), `PresetManager` (Compute layouts) |
| Keep rendering | `RenderScheduler::GetInstance().RequestRender()`, `RequestRenderForSeconds` |
| App paths, URLs, axis ticks | `get_application_config_path`, `open_url`, `calculate_nice_interval` |
| Controller async work | `JobSystem::Get().IssueJob` with a `Future` that checks `IsCancelled()` |
| Controller handle cast | `Reference<>` for typed entries; generic property calls dispatch through `Handle*` |
| Pooled event or sample | `MemoryManager::NewEvent`, `NewSample`, `NewSampleLOD`; free with `MemoryManager::Delete` |
| SQL | `Builder` from `QueryFactory` or `ComputeQueryFactory`; never a hand-built string |
| Timed model lock | `TimedLock<std::unique_lock<std::shared_mutex>>` or the shared-lock variant |
| String trim, case, prefix | `RocProfVis::Core::String` |

## Table of contents

**[Architecture](.agents/ARCHITECTURE.md)** - cross-layer orientation

- [Layers and boundaries](.agents/ARCHITECTURE.md#layers-and-boundaries) - what each layer owns and may include
- [App shell and core](.agents/ARCHITECTURE.md#app-shell-and-core) - window, frame loop, CLI, `src/core`
- [Data flow](.agents/ARCHITECTURE.md#data-flow) - one request from click to pixels
- [Build and feature flags](.agents/ARCHITECTURE.md#build-and-feature-flags) - presets, `ROCPROFVIS_*` flags, new source files
- [Tests](.agents/ARCHITECTURE.md#tests) - test targets and writing UI tests
- [Packaging and CI](.agents/ARCHITECTURE.md#packaging-and-ci) - installers, workflows, changelog
- [Maintaining the book](.agents/ARCHITECTURE.md#maintaining-the-book) - section template and budgets

**[Styling](.agents/STYLING.md)** - colors, sizes, fonts, icons, and adding a missing one

**[UI](.agents/UI.md)** - the View, `src/view/`

- [App window and projects](.agents/UI.md#app-window-and-projects) - menus, tabs, dialogs, `.rpv` projects
- [Widgets](.agents/UI.md#widgets) - `RocWidget`, containers, dialogs, notifications, GUI helpers
- [Tables](.agents/UI.md#tables) - `InfiniteScrollTable` paging, filters, sort, export
- [Settings and persistence](.agents/UI.md#settings-and-persistence) - preferences, hotkeys, project settings, presets
- [Events](.agents/UI.md#events) - `EventManager` and adding a `RocEvents` value
- [Data provider](.agents/UI.md#data-provider) - fetches, request IDs, callbacks, cancellation
- [View models](.agents/UI.md#view-models) - View-side trace, timeline, table, and track data
- [Trace view](.agents/UI.md#trace-view) - system-trace layout, toolbar, sidebar, minimap
- [Timeline](.agents/UI.md#timeline) - zoom, selection, flow arrows, measurement, notes
- [Tracks](.agents/UI.md#tracks) - flame and line tracks; adding a track type
- [Details panel](.agents/UI.md#details-panel) - event and sample tables, details, compare panes
- [Compute view](.agents/UI.md#compute-view) - compute tabs, roofline, metric tables
- [Compute memory chart](.agents/UI.md#compute-memory-chart) - per-architecture memory diagram
- [Compute ISA view](.agents/UI.md#compute-isa-view) - ISA and source correlation, PC sampling
- [AppMonitor](.agents/UI.md#appmonitor) - polled background operations
- [Utilities](.agents/UI.md#utilities) - formatting, paths, URLs, axis intervals

**[Features](.agents/FEATURES.md)** - optional features that span layers

- [Trace compare](.agents/FEATURES.md#trace-compare) - A/B compare projects
- [Remote SSH](.agents/FEATURES.md#remote-ssh) - SSH profiles, sessions, remote files
- [Profiler launch](.agents/FEATURES.md#profiler-launch) - local and remote profiler runs
- [Ask Optiq](.agents/FEATURES.md#ask-optiq) - the LLM assistant and its tools

**[Controller](.agents/CONTROLLER.md)** - `src/controller/`, the C ABI the View calls

- [Overview](.agents/CONTROLLER.md#overview) - ownership, threading, tests
- [C ABI and properties](.agents/CONTROLLER.md#c-abi-and-properties) - handles, properties, futures, adding API
- [Building blocks](.agents/CONTROLLER.md#building-blocks) - `Handle`, `Reference<>`, `Future`, `JobSystem`
- [System traces](.agents/CONTROLLER.md#system-traces) - tracks, events, samples, LOD graphs, tables
- [Compute traces](.agents/CONTROLLER.md#compute-traces) - workloads, kernels, metrics, roofline
- [Memory and segments](.agents/CONTROLLER.md#memory-and-segments) - segment cache, pools, eviction

**[Database](.agents/DATABASE.md)** - `src/model/`, SQLite and the data model

- [Overview](.agents/DATABASE.md#overview) - ownership, threading, locks, tests
- [C ABI and Python bindings](.agents/DATABASE.md#c-abi-and-python-bindings) - `rocprofvis_db_*` and `rocprofvis_dm_*`
- [Database adapters and trace formats](.agents/DATABASE.md#database-adapters-and-trace-formats) - rocpd, rocprof, multinode
- [Compute database](.agents/DATABASE.md#compute-database) - compute schema and queries
- [Data model](.agents/DATABASE.md#data-model) - in-memory trace objects and binding
- [Query pipeline](.agents/DATABASE.md#query-pipeline) - `Builder`, `QueryFactory`, packed tables
- [Versioning and cleanup](.agents/DATABASE.md#versioning-and-cleanup) - `roc_optiq_*` tables, rebuild, trim

**[Scripting](.agents/SCRIPTING.md)** - in-app Python analysis

- [Overview and status](.agents/SCRIPTING.md#overview-and-status) - goals, architecture, phases
- [Runtime and sandbox](.agents/SCRIPTING.md#runtime-and-sandbox) - interpreter thread, allowlist, threat model
- [Controller ABI and bindings](.agents/SCRIPTING.md#controller-abi-and-bindings) - `rocprofvis_script_*` and the `optiq` module
- [Editor and approval](.agents/SCRIPTING.md#editor-and-approval) - Script tab and Ask Optiq offers
- [Known gaps](.agents/SCRIPTING.md#known-gaps) - open bugs and missing tests
