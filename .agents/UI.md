# View layer

The Dear ImGui front end in `src/view/`: the app window, per-trace views, timeline, tables, and the view-side caches they draw from. Read the section for the component you are changing; colors, sizes, and fonts are in [Styling](STYLING.md), and the flags that gate optional UI are in [Build and feature flags](ARCHITECTURE.md#build-and-feature-flags). **Code:** paths are relative to `src/view/src/` unless they start with `resources/` or `cmake/`.

## App window and projects

`AppWindow` is the root singleton that owns the menu bar, project tabs, shared dialogs and file pickers, status bar, and shutdown. Each tab is a `Project` that wraps one `RootView`: `TraceView` or `ComputeView`.

**Code:** `rocprofvis_appwindow.{h,cpp}`, `rocprofvis_project.{h,cpp}`, `rocprofvis_root_view.{h,cpp}`, `rocprofvis_render_scheduler.h`, `welcome/`, `rocprofvis_compare_files_dialog.{h,cpp}`

**Related:** [App shell and core](ARCHITECTURE.md#app-shell-and-core) (the frame loop that calls `AppWindow`), [Settings and persistence](#settings-and-persistence) (what a `.rpv` stores), [Data provider](#data-provider) (the cleanup work a closing tab hands back), [Trace compare](FEATURES.md#trace-compare) (compare projects and their dialog).

- A project ID is the `weakly_canonical` trace path, or `compare://a|b` from `MakeCompareId`. It keys `m_projects` and the tab, and it equals `DataProvider::GetTraceFilePath()`, the source ID of per-trace events. Never open it as a file.
- `AppWindow::Update()` runs on the main thread in a fixed order: provider-cleanup polling, hotkeys, `AppMonitor`, `EventManager::DispatchEvents()`, `LogViewer::Poll()`, then the tab container, which updates every enabled tab so background projects keep loading.
- Close a project with `CloseProjectTab(id)` or the tab's close button. The `kTabClosed` handler calls `Project::Close()`, takes `RootView::DetachProviderCleanup()`, and finishes it on a `std::async` job in `m_provider_cleanup_jobs`, so closing never blocks a frame. Both views' load-error dialogs close their tab this way.
- Exit goes through the private `BeginAppShutdown()`, which detaches every project's cleanup and waits for those jobs and, up to a time bound, for `AppMonitor` operations.
- Panel visibility changes go through `ApplyPanelVisibilitySettings()`, from the View menu and from code alike.
- The loop renders lazily. `WantsContinuousRender()` covers scheduler requests, pending events, cleanup, and loading providers; anything else that animates or polls asks `RenderScheduler` itself. Do not add per-feature branches there.
- A new project type derives from `RootView`, owns a `DataProvider`, overrides `GetToolbar`, `RenderEditMenuOptions`, `DetachProviderCleanup`, and `GetDataProvider`, shows `RenderLoadingScreen` until the provider is `kReady` or `kError`, and is constructed in `Project::OpenTrace` by controller object type.

## Widgets

The shared, theme-aware controls in `widgets/`. The root **Already exists** table names the everyday ones; this section covers their behavior and the helpers it leaves out.

**Code:** `widgets/`, except `rocprofvis_script_editor.*` (scripting) and `rocprofvis_query_builder.*` / `rocprofvis_compute_widget.*` (compute)

**Related:** [Styling](STYLING.md) (the theme these controls apply), [Tables](#tables) (`InfiniteScrollTable`, also in `widgets/`), [Events](#events) (tab events).

- `m_widget_name` is both the ImGui ID scope and the default event source name, which is why it must come from `GenUniqueName` and never be shared.
- `TabContainer` posts `kTabClosed` / `kTabSelected` only after `EnableSendCloseEvent` / `EnableSendChangeEvent`, with the `SetEventSourceName` value (else `m_widget_name`) as source ID. It updates every enabled tab each frame, not just the active one. A tab disabled with `SetTabEnabled(id, enabled, tooltip)` skips `Update()` and programmatic selection. `FindTabByLabel` returns null for an ambiguous label.
- `ConfirmationDialog` can bind a `bool&` "don't ask again" setting. `MessageDialog` queues a request that arrives while one is showing, so callers never overwrite each other.
- `NotificationManager::UpdateProgress` and `Hide` drive persistent toasts; the manager draws last in the frame.
- `LogViewer` is the user-facing log window: `AppWindow::Update()` calls `Poll()`, which reads `rocprofvis_core_get_log_entries_ex`, and its preferences live in `UserSettings::log_viewer`. `DebugWindow` exists only under `ROCPROFVIS_DEVELOPER_MODE`.
- **Reuse:** `LayoutItem::CreateFromWidget` for container slots; `FlexItem` (`min_width`, `flex_grow`, `full_row`) and `FlexContainer::subcomponent_layout` for one shared card with separators; `IconMenuItem` / `IconBeginMenu`; `InputTextWithClear`; `SetTooltipStyled` / `BeginItemTooltipStyled`; `SectionTitle`, `VerticalSeparator`, `CenterNextItem`; `IsMouseReleasedWithDragCheck` to tell a click from a drag; `CellMenuTarget`, `CaptureCellRightClick`, `AddCopyRowCellMenuItems`, and `COPY_DATA_NOTIFICATION` / `COPY_ROW_DATA_NOTIFICATION` for table copy menus; `LoadingTimer` (in `rocprofvis_timeline_view.h`) to show a spinner only once a request is slow; `GuiTexture` (move-only; the app shell calls `SetBackend` once) and `EmbeddedImage` for compiled-in images; `DrawInternalBuildBanner` under `ROCPROFVIS_ENABLE_INTERNAL_BANNER`.

## Tables

Every event, sample, search, and kernel-instance table subclasses `InfiniteScrollTable`, which pages rows from a controller table and provides sort, filters, group-by, column fitting, copy menus, go-to-event, and CSV export.

**Code:** `widgets/rocprofvis_infinite_scroll_table.{h,cpp}`, `rocprofvis_multi_track_table.{h,cpp}`, `rocprofvis_event_search.{h,cpp}`, `model/rocprofvis_tables_model.{h,cpp}`

**Related:** [Data provider](#data-provider) (`FetchTable` and request IDs), [Details panel](#details-panel) (the A/B compare tables), [System traces](CONTROLLER.md#system-traces) (the controller table behind each one), [Query pipeline](DATABASE.md#query-pipeline) (how filters and group-by become SQL).

- The controller keeps one table per table type, so `DataProvider` refuses a fetch while a request of that type is pending. `FetchData()` then leaves `m_fetch_data` set, requests a frame, and retries from `Update()`.
- Views that share a controller table type (compare A/B, Ask Optiq) each need their own `TableType` slot and request ID; the base `UpdateFetchParams` sends `m_view_table_type` and `m_request_id` explicitly for that reason.
- A table accepts `kNewTableData` only when the source is its trace and the request ID is its own, and it reformats on `kTimeFormatChanged`.
- `MultiTrackTable` lists rows for the selected tracks of its kind (one source in compare mode) over the selected time range or the whole trace. It caches group-by candidates from the last ungrouped header (`EligibleGroupByColumns`); its Advanced mode edits raw `FilterOptions`, and `CommitAdvancedFilter` never sends group-by and filter together.
- `EventSearch` carries its options (substring, category, partial match, time range) in `EventSearchRequestParams`. `KernelInstanceTable` and `TopEventsView::TopEventsTable` (built on `MultiTrackTable`) are the other subclasses.

**Recipe: add a table.**

1. Subclass `InfiniteScrollTable`, or `MultiTrackTable` for selected-track rows, passing a `TableType` slot (append one before `__kTableTypeCount` if none fits), the controller table type, a request ID, and `TablesModel` accessors.
2. Override `UpdateFetchParams`: create `TrackTableRequestParams` or `EventSearchRequestParams` when the pointer is null, call the base, which asserts on null, then fill your fields.
3. Override `IndexColumns` (base first, then `ColumnIndexOf`) and `FormatData` (start with `ResetFormattedData()`) only for extra columns.
4. Call `RequestFetch()` when inputs change, and `CancelFetch(true)` when nothing will replace the pending request.

## Settings and persistence

Where View state lives, all in the config directory: preferences and hotkeys in settings_application.json (`SettingsManager`, `HotkeyManager`), per-project state in `.rpv` files (`ProjectSetting`), named Compute layouts in presets.json (`PresetManager`), and SSH and launch profiles in profiles.json (`ProfilesDocument`).

**Code:** `rocprofvis_settings_manager.{h,cpp}`, `rocprofvis_settings_panel.{h,cpp}`, `rocprofvis_hotkey_manager.{h,cpp}`, `rocprofvis_project.{h,cpp}`, `rocprofvis_presets.{h,cpp}`, `rocprofvis_profiles_document.{h,cpp}`, `rocprofvis_json_utils.{h,cpp}`

**Related:** [Styling](STYLING.md) (the theme half of `SettingsManager`), [Remote SSH](FEATURES.md#remote-ssh) and [Profiler launch](FEATURES.md#profiler-launch) (the profiles.json writers and the credential store).

- `ApplyUserSettings(old, save_json)` diffs against `old` and posts `kThemeChanged` or `kTimeFormatChanged`. The Settings panel edits the live `UserSettings` and applies each change at once (font size waits for OK); OK saves, Cancel restores the copy taken when it opened.
- `ProfilerSettings` deliberately has no profiler binary path: a tool enum picks the binary, so an edited or copied settings file cannot choose what runs.
- `ProjectSetting`'s constructor looks its project up through `AppWindow` and registers, so never create one in a `RootView` constructor or `LoadTrace()`, which run before `AppWindow` registers the project; views create them from `Update()`. Its destructor does not unregister, so a setting must live as long as its project. `Project::Save()` rebuilds the tree from every `ToJson()`; read fields only after `Valid()`.
- Saved key strings are file format: never change a `JSON_KEY_*` value, not even the trailing space in `JSON_KEY_TIMELINE_BOOKMARK_V_MIN_X`.
- Preset-aware Compute widgets own a nested `PresetComponent` (`ToJson`, `FromJson`, `Reset`) registered under their project ID, one per `ComponentType`. This is unrelated to `LaunchPresetManager`.
- Write profiles.json only through `SshConnectionStore` or `LaunchPresetManager` so one section never clobbers the other; secrets stay in the OS credential store. `JsonUtils` has the typed, defaulted readers.

**Recipe: add a user setting** (`compact_sidebar` is a small example).

1. Add the field to `DisplaySettings`, `UnitSettings`, or `UserSettings`, and a `JSON_KEY_SETTINGS_*` key beside its neighbors.
2. Put its default at the same position in the positional `m_usersettings_default` initializer.
3. Write it in the matching `Serialize*Settings` and read it, type-checked, in `Deserialize*Settings`.
4. Add the control to `RenderDisplayOptions`, `RenderUnitOptions`, or `RenderOtherSettings`, and restore it in the matching `Reset*Options` where one exists.
5. If live UI must react, handle it in `ApplyUserSettings` and post an event.

**Recipe: add a hotkey.**

1. Append a `HotkeyActionId` before `kCount`.
2. Add its `HotkeyActionInfo` at the same position in `kActionTable`: name, category, a stable `key` (its JSON key), default chord, and `ActionType`.
3. Read it with `WasActionTriggered` or `IsActionHeld`; the Hotkeys page lists it automatically.
4. Add it to [keyboard-shortcuts.rst](../docs/reference/keyboard-shortcuts.rst).

## Events

`EventManager` is the View's deferred publish/subscribe bus: a widget posts a typed `RocEvent`, and subscribers handle it on the main thread at the next dispatch.

**Code:** `rocprofvis_event_manager.{h,cpp}`, `rocprofvis_events.{h,cpp}`

**Related:** [Timeline](#timeline) (`TimelineSelection` posts the selection events), [Data provider](#data-provider) (callbacks the views turn into `kNewTrackData` / `kNewTableData`), [AppMonitor](#appmonitor) (operation status events).

- `AddEvent` and `HasPendingEvents` are thread-safe; `Subscribe`, `Unsubscribe`, and `DispatchEvents` are main-thread only. `AppWindow::Update()` drains the queue once per frame, so never rely on same-frame ordering.
- Per-trace events carry the project ID (`DataProvider::GetTraceFilePath()`) as source ID, and handlers check it, because every open project subscribes to the same IDs. App-wide events (`kThemeChanged`, `kFontSizeChanged`, `kTimeFormatChanged`) carry none.
- Known gap: the minimap, sticky notes, and annotation list post `NavigationEvent` (`kGoToTimelineSpot`) without a source ID, and every `TimelineView` acts on it.
- Handlers receive `shared_ptr<RocEvent>` and `dynamic_pointer_cast` to the payload class, checking the result. `ScrollToTrackEvent` rides on `kHandleUserGraphNavigationEvent` (scroll the timeline to a track) and `kRevealTrackInTopology` (reveal it in the sidebar). Selection and highlight events may be batches (`IsBatch()`).
- `kTrackVisibilityChanged` is the only show/hide signal; the sidebar, histogram, and minimap all listen, so none of them call each other.

**Recipe: add an event.**

1. Append a value to `RocEvents`.
2. For a payload, add a `RocEvent` subclass in `rocprofvis_events.h` whose constructor sets a new `RocEventType` value; never pass data through the base class.
3. Post it with `EventManager::GetInstance()->AddEvent(...)` and set its source ID.
4. Have handlers check the source ID and the cast result before acting.

## Data provider

Each `RootView` owns one `DataProvider`, which wraps the trace's controller and the view models. Its `Fetch*` methods start controller futures and return at once; `Update()` polls them, copies results into the models, and fires callbacks that the owning view turns into events.

**Code:** `rocprofvis_data_provider.{h,cpp}`, `rocprofvis_data_provider_script.cpp`, `rocprofvis_requests.h`

**Related:** [Data flow](ARCHITECTURE.md#data-flow) (one request through every layer), [View models](#view-models) (where results land), [C ABI and properties](CONTROLLER.md#c-abi-and-properties) (the controller calls behind each fetch), [Ask Optiq](FEATURES.md#ask-optiq) (the background reader with its own client ID).

- `GetState()` moves `kInit`, `kLoading`, then `kReady` or `kError`. Views build widgets only after `kReady` or `kError`, showing `GetProgressMessage()` meanwhile.
- A refused `Fetch*` returns false; check `IsRequestPending` / `IsTableRequestPending` and retry from a later `Update()`, never in a loop.
- Request IDs pack the `RequestType` above the track, chunk, or client bits. They are View-side keys only; the controller never sees them.
- The static request IDs on `DataProvider` (such as `EVENT_TABLE_REQUEST_ID`) are dynamically initialized; do not read them in another file's namespace-scope initializer.
- A background reader needs its own client ID. `ASSISTANT_CLIENT_ID` gives Ask Optiq separate request IDs, controller tables, and `TablesModel` slots, so it never overwrites rows a tab shows.
- `FetchTrack` is the pixel-LOD fetch (`kFetchGraph`); only `FetchWholeTrack` uses `kFetchTrack`.
- Callbacks fire from `Update()` on the UI thread. Each `Set*Callback` is one slot that replaces the previous owner, and whoever sets a callback capturing `this` clears it in its destructor, since the provider can outlive it.
- `CancelRequest` is best-effort; still handle the completion. A closing tab hands `DetachCleanupWork()` (controller, requests, client tables) to `AppWindow`; `FreeRequests()` blocks and is only for callers that may block.

**Recipe: add a request type.**

1. Append a `RequestType` value.
2. Add a params class deriving from `RequestParamsBase` in `rocprofvis_requests.h` if it needs inputs.
3. Add a public `Fetch*` that builds controller arguments, allocates the future and output, starts the controller's async call, and records a `RequestInfo` in `m_requests` under a `RequestIdBuilder` ID.
4. Add a `ProcessRequest` case and a `Process*Request` that copies results into the model and fires a callback.
5. Have the owning view turn the callback into a `RocEvent`; widgets subscribe to that.

## View models

View-side caches and projections in `model/` that `DataProvider` writes and widgets read. They are not the canonical `src/model/`, which never includes them.

**Code:** `model/`, `rocprofvis_raw_track_data.{h,cpp}`

**Related:** [Data provider](#data-provider) (their only writer), [Trace view](#trace-view) (the sidebar's projection of `TopologyTree`), [Data model](DATABASE.md#data-model) (the canonical model).

- `DataProvider::DataModel()` returns the `TraceDataModel` facade over `TopologyTree`, `TimelineModel`, `TablesModel`, `SummaryModel`, `EventModel`, and `AnalysisModel`; compute traces use `ComputeModel()`.
- `rocprofvis_model_types.h` is the shared vocabulary (`TrackInfo`, `TopologyId`, `TraceEventId`, `EventInfo`, `SummaryInfo`, `AnalysisTrackStatistics`, `CompareSourceInfo`); `INVALID_UINT64_INDEX` is the index sentinel.
- `TopologyTree` owns every node in one arena; edges are non-owning pointers, and the typed lookups (`GetQueue`, `FindByTrackId`, ...) are shortcuts. A stream's processors and queues are linked (`GetLinkedChildren`), not copied.
- `Finalize()` ranks nodes by ascending id (`GetNodeDisplayIndex`, behind node labels and colors), builds `GetTrackOrder()`, and bumps `GetRevision()`. A new revision invalidates every node pointer; rebuild whatever you derived.
- `TablesModel` slots are keyed by `TableType`: compare A/B slots (`kCompareEventTableA`, the B twins of `kAnalysisTop*`) and Ask Optiq's `kAssistant*` slots. Append new types.
- `AnalysisTrackStatistics` moves `kStale`, `kPending`, `kRequested`, `kReady`, and each stat composes its own labels (`CompactLabel()`, `FullLabel()`), so pills and Track Details read one cache.
- Read raw track data through `TimelineModel` (`GetTrackData`, `FreeTrackData`) as chunked `RawTrackEventData` / `RawTrackSampleData` of `TraceEvent` / `TraceCounter`; do not add another event-list type.
- Compare provenance comes from `SetCompareSources()` and `GetCompareSource()`, never from parsing IDs.

## Trace view

`TraceView` is the system-trace `RootView`: topology sidebar on the left, timeline and minimap on the right, details panel below, and a toolbar with search, bookmarks, measurement, and the Summary panel.

**Code:** `rocprofvis_trace_view.{h,cpp}`, `rocprofvis_sidebar.{h,cpp}`, `rocprofvis_tree_node.h`, `rocprofvis_minimap.{h,cpp}`, `rocprofvis_summary_view.{h,cpp}`

**Related:** [Timeline](#timeline) (the main surface), [Details panel](#details-panel) (the panel below), [Versioning and cleanup](DATABASE.md#versioning-and-cleanup) (trimmed save and database cleanup behind its menu items).

- `TraceView` owns the project's `TimelineSelection`, `AnnotationsManager`, and `MeasurementController` and passes them down; never create a second copy.
- On reaching `kReady` it calls `TimelineView::MakeGraphView()` and restores bookmarks through `SystemTraceProjectSettings`; the ten slots map to `BookmarkSaveAction(i)` / `BookmarkRestoreAction(i)`.
- A load failure shows the shared message dialog, whose close calls `AppWindow::CloseProjectTab`; save and cleanup errors leave the tab open.
- `SideBar` draws a projection of `TopologyTree` (`BuildTree()` into `TreeNode` / `LeafNode`), rebuilt on a new `GetRevision()` or a metadata change. Group headers, eye states (`EyeButtonState`), and node swatches belong to the sidebar, not the tree.
- A `LeafNode` stores a `track_id`, never a position, because drag-reorder moves positions; rows resolve to a `TrackItem*` each frame. Every track gets a row: those the walk missed go under Uncategorized.
- `ApplyVisibility(node, visible)` toggles every leaf below a node, and every visibility change posts `kTrackVisibilityChanged`. "Reveal in topology" posts `kRevealTrackInTopology`; `HandleRevealTrack` opens, highlights, and scrolls to each row for the track.
- `CompactSidebar()` drops the per-row eye and scroll buttons; the right-click menus still do both.
- `Minimap` bins density into `MINIMAP_SIZE` columns and navigates with a `NavigationEvent`. `SummaryView` hosts `HWUtilization`, `TopKernels` (from `FetchSummary`), and `KernelInstanceTable`.

## Timeline

`TimelineView` lays out and draws the tracks against time, with one `TimePixelTransform` for time math, one `TimelineSelection` for selection, `TimelineFocusManager` for layered clicks, and the measurement and sticky-note state.

**Code:** `rocprofvis_timeline_view.{h,cpp}`, `rocprofvis_time_to_pixel.{h,cpp}`, `rocprofvis_timeline_selection.{h,cpp}`, `rocprofvis_click_manager.{h,cpp}`, `rocprofvis_measurement_controller.{h,cpp}`, `rocprofvis_annotations.{h,cpp}`, `rocprofvis_stickynote.{h,cpp}`, `rocprofvis_annotation_view.{h,cpp}`, `rocprofvis_timeline_arrow.{h,cpp}`

**Related:** [Tracks](#tracks) (the lanes it draws), [Events](#events) (selection and navigation events), [Utilities](#utilities) (axis intervals and adaptive zoom).

- Cull and hit-test rows against `ImGui::GetScrollY()` inside `Graph View Main`, not `m_scroll_position_y`: `SetScrollY` lands on the next `Begin`, so the member can lead the layout by a frame. ImGui keeps whole-pixel scroll, so `m_previous_scroll_position` is a `float` and a sub-pixel gap counts as applied; otherwise a leftover fraction overwrites a scrollbar drag.
- All time math goes through the shared `TimePixelTransform` (`TimeToPixel`, `PixelToTime`, `ZoomAtPixel`).
- `TimelineSelection` alone owns selected tracks, events, time range, and highlight, and posts the `kTimeline*Changed` events. `NavigateToEvent(track_id, event_uuid, start_ns, duration_ns)` is the way to scroll to and highlight an event.
- Move the view with `MoveToPosition`, `SetViewableRangeNS`, or `ScrollToTrack`, or post `kSetViewRange` (`RangeEvent`) or `kGoToTimelineSpot` (`NavigationEvent`).
- The graph, interactive overlay, and scrubber each call `RequestLayerFocus`; `EvaluateFocusedLayer` picks the topmost, and the others skip their click handling.
- Track order is view-only. `TrackSortMode` is `kTopology` (sidebar order, applied once the tree loads), `kDefault` (load order), or `kCustom` (drag-reordered). `ApplyTrackOrder` reindexes through `DataProvider::SetTrackIndex` without touching controller order. The mode is saved as an int under `JSON_KEY_TIMELINE_SORT_MODE`, so keep the enum values.
- `MeasurementController` holds the two-point measurement, event-anchored or freehand; `TraceView::RenderMeasurementControls()` draws its toolbar and `TimelineView::RenderMeasurement()` the overlay.
- `AnnotationsManager` owns the `StickyNote`s and persists them through `AnnotationsManagerProjectSettings`. A note may bind to a track (`INVALID_TRACK_ID` when free) with a track-relative y; `AnnotationView` is their list tab.
- `TimelineArrow` draws flow arrows (`kFan` or `kChain`). `LoadingTimer` advances only through `Tick()`, so call it every frame while it runs.

## Tracks

Each lane is a `TrackItem`, not a `RocWidget`, that requests its own data, caches renderable points, and draws a meta area and a chart. `FlameTrackItem` draws events and `LineTrackItem` counters.

**Code:** `rocprofvis_track_item.{h,cpp}`, `rocprofvis_flame_track_item.{h,cpp}`, `rocprofvis_line_track_item.{h,cpp}`, `rocprofvis_timeline_track_options.{h,cpp}`

**Related:** [Timeline](#timeline) (owner and time transform), [Data provider](#data-provider) (`FetchTrack`), [System traces](CONTROLLER.md#system-traces) (the controller track and LOD graph behind each lane).

- `TimelineView::MakeGraphView()` creates one track per `TrackInfo` by `track_type` and owns them in `m_tracks`; everyone else uses `GetTracks()`.
- Selection has no setter: a track follows `kTimelineTrackSelectionChanged`, so select through `TimelineSelection`.
- The timeline requests data for visible and nearby tracks and calls `ReleaseData()` on those farther than `m_unload_track_distance`. The base frees the raw data and cancels pending chunks; overrides call it, then clear their own cache. `ExtractPointsFromData()` must cope with missing raw data, since a track can unload before its response arrives.
- Per-track options are `TrackOptions` subclasses (`EventTrackOptions`, `QueueTrackOptions`, `CounterTrackOptions`) chosen by topology type in `TimelineTrackOptions::InitTrack` and edited from the meta-area right-click menu. They combine through `operator&=` so one edit can apply to selected or sibling tracks, and they persist per project via `TrackOptions::TrackProjectSetting`.
- Flame rows use `GetEventLevelHeight()` or `GetEventLevelCompactHeight()` plus `GetEventLevelSpacing()`. Line tracks show interior Y ticks only above `2 * DEFAULT_TRACK_HEIGHT`, and ticks, grid, and chart share `CalculatePlotHeight()` so whole-pixel child sizing cannot misalign them.
- `Pill` is the meta-area badge; statistic pills read `AnalysisTrackStatistics` via `RequestAnalysis()`. `RenderCompareSourceBadge` draws the A/B badge for the timeline and the sidebar.

**Recipe: add a track type.**

1. Subclass `TrackItem` and implement `RenderChart` and `ExtractPointsFromData`, plus `RenderMetaAreaScale` if it has a scale; do all time math through `m_tpt`.
2. Override `ReleaseData`: call the base, then clear your cache.
3. For persisted options, add a `TrackOptions` subclass with a `TrackOptions::Type` value after its base, `operator&=`, `ToJson`, `Valid`, `FromJson`, and `Render`, created in `TimelineTrackOptions::InitTrack`.
4. Construct it in `MakeGraphView()` for its `rocprofvis_controller_track_type_t`; a new data type needs controller support too.

## Details panel

`AnalysisView` is the tabbed panel under the timeline: Event Table, Sample Table, Event Details, Track Details, Top Events, Annotations, and Script under `ROCPROFVIS_ENABLE_SCRIPTING`. Tabs follow `TimelineSelection` events and split into A/B panes in compare projects.

**Code:** `rocprofvis_analysis_view.{h,cpp}`, `rocprofvis_events_view.{h,cpp}`, `rocprofvis_track_details.{h,cpp}`, `rocprofvis_top_events_view.{h,cpp}`, `rocprofvis_compare_panes.{h,cpp}`

**Related:** [Tables](#tables) (the event and sample tables), [Trace compare](FEATURES.md#trace-compare) (the rest of compare mode), [Editor and approval](SCRIPTING.md#editor-and-approval) (the Script tab).

- Script stays the last tab because it is the only one that writes. Ask Optiq lists and selects tabs by visible label (`ListTabs`, `SelectTab`), so labels are part of its interface.
- In compare mode (`IsCompareTrace`) every tab except Annotations splits into source-filtered A/B panes built from `MakeCompareSplit`, `BeginCompareCard` / `EndCompareCard`, and `RenderCompareCardTitle`. `COMPARE_SOURCE_A` / `COMPARE_SOURCE_B` are also what `TrackInfo::file_id` counts; `EventsView` and `TrackDetails` split their cards by it.
- A/B tables share a controller table type, so each side has its own `TableType` slot and `MakeClientRequestId(type, COMPARE_CLIENT_ID[source])`. The pair shares one filter form and sort but never pools results; the group-by combo is the union from `BuildCompareGroupByChoices`, and `AdjustFilterForRequest` drops a group-by the sending side lacks.
- `TrackDetails` resolves the selected tracks in `TopologyTree`, reformats raw cells on `kTimeFormatChanged`, and re-resolves on a new `GetRevision()`.
- `TopEventsView` shows six category tables, each filtered by `TrackInfo::operation_types` and hidden until a matching track is selected. Each `CategoryDescription` pairs a `kAnalysisTop*` slot and its B twin with a `kFetchAnalysisTop*` request type.

**Recipe: add a details-panel tab.**

1. Write a `RocWidget` that follows `TimelineSelection` events and reads through `DataProvider`.
2. Construct it in the `AnalysisView` constructor and `AddTab` a `TabItem` with a stable label, before Script.
3. For per-source data, use the compare-pane helpers; for tables, give each source its own `TableType` slot and client request ID.

## Compute view

`ComputeView` is the compute-trace `RootView`: a `TabContainer` of Summary View, Kernel Details, Table View, Baseline Comparison, Profile Details, ISA View, and the developer-only Compute Tester, all driven by one `ComputeSelection`.

**Code:** `compute/`, `model/compute/`, `widgets/rocprofvis_compute_widget.{h,cpp}`, `widgets/rocprofvis_query_builder.{h,cpp}`

**Related:** [Compute memory chart](#compute-memory-chart) and [Compute ISA view](#compute-isa-view) (two of its tabs), [Compute traces](CONTROLLER.md#compute-traces) (workloads, kernels, metrics, roofline), [Compute database](DATABASE.md#compute-database) (the SQL behind every metric).

- Only `ComputeSelection::SelectWorkload` / `SelectKernel` post `kComputeWorkloadSelectionChanged` / `kComputeKernelSelectionChanged`; change selection through them.
- `CreateView()` validates first: a provider error, no workloads, or no workload with a kernel queues one message dialog per load, whose close removes the tab, instead of building tabs.
- Each sub-view owns a `TAB_ID` and a static `CreateTabItem(...)`. Without the data (no metric tables, no ISA lines) it returns a disabled tab with a tooltip and no widget; availability is computed once after load.
- Metric consumers take a client ID from `IdGenerator`, pass it to `FetchMetrics`, and filter `kComputeMetricsFetched` on `ComputeMetricsFetchedEvent::GetClientId()`. A workload with no metric tables is skipped, never fetched with zero selectors.
- Identify metrics with `MetricId` (`ToString()`, `MetricIdHash`) and the types in `rocprofvis_compute_model_types.h`; do not invent metric IDs or roofline geometry.
- `ComputeComparisonView`, `ComputeTableView`, and `KernelMetricTable` persist layouts through a nested `PresetComponent`; `ComputeView` owns the shared `PresetBrowser`.
- Add Metric is `QueryBuilder` (category, table, entry, value) feeding `KernelMetricTable::SetExternalQuery`. `Roofline` runs in `SingleKernel`, `AllKernels`, or `Compare` mode (`SetCompareTarget`).

## Compute memory chart

`ComputeMemoryChartView` draws the GPU memory hierarchy on the Kernel Details tab from a JSON layout of blocks and arrows, so most chart changes are data changes.

**Code:** `compute/rocprofvis_compute_memory_chart.{h,cpp}`, `model/compute/rocprofvis_memory_chart_model.{h,cpp}`, `resources/memory_chart/`, `cmake/embed_memory_chart_layouts.cmake`

**Related:** [Compute view](#compute-view) (the tab that hosts it), [Styling](STYLING.md) (the `kMemChart*` palette), [Tests](ARCHITECTURE.md#tests) (where `roc-optiq-memory-chart-tests` runs).

- Change blocks, arrows, and metric references in the JSON under `resources/memory_chart/`. At build time `embed_memory_chart_layouts.cmake` compiles every non-schema JSON there into a generated header (`kMemChartEmbeddedLayouts`), so layouts ship inside the binary. C++ changes only for new routing or drawing behavior.
- `LoadWorkloadLayout()` resolves in order: a developer override (memory_chart.json in the config directory), the workload's `compute_workload.memory_chart_extdata` blob, the embedded layout for its `gpu_arch` (exact key, then the gfx94x or gfx950 family), then the embedded default.
- A block id is a readable string, unique across the layout including nested `children`; an arrow names `from` / `to` block ids and a `direction`. `ParseFromString()` rejects missing, numeric, or duplicate ids and unknown arrow ends, and the loader falls back to the next source.
- `OnLayoutLoaded()` sorts the blocks, then resolves arrow ends to block pointers once; nothing may move or reallocate blocks afterwards, or arrows dangle.
- Metric refs are dotted `category.table.entry` ids. `FetchMemChartMetrics()` fetches every category the layout references under the chart's own client ID; unresolved refs draw as N/A.
- `ComputeLayout()` stacks blocks by column, `BuildArrowRoutes()` routes adjacent, same-column, and column-skipping arrows (the last on shared lanes below the blocks), and `ResolveLabelOverlaps()` nudges labels. The chart caches palette colors and rebuilds them on `kThemeChanged`.

**Recipe: add a layout for a GPU architecture.**

1. Add `resources/memory_chart/<arch>.json` following `memory_chart.schema.json`; the file stem is the key matched against `gpu_arch`.
2. Add a branch to `EmbeddedLayoutForArch` only if one file should serve a family of architectures.
3. Run `roc-optiq-memory-chart-tests`, which parses every embedded layout.

## Compute ISA view

`ComputeIsaView` correlates a kernel's ISA and source with PC-sampling counts and stall reasons, fetched in three independent layers so the ISA pane never waits on optional data.

**Code:** `compute/rocprofvis_compute_isa_view.{h,cpp}`

**Related:** [Data provider](#data-provider) (`FetchPcSampling`), [Compute traces](CONTROLLER.md#compute-traces) (the kernel-owned `PcSampling` handle), [Compute database](DATABASE.md#compute-database) (the PC-sampling queries, which need compute schema 2.2 or newer).

- All data comes through `DataProvider::FetchPcSampling(PcSamplingRequestParams)`; extend `PcSamplingData` for a new rendered field instead of querying the controller.
- `PcSamplingLayer::kIsa` loads code object, symbols, instructions, and offsets when the view opens or the kernel changes. `kSource` loads files, correlations, and one file's lines when the source pane is shown or the file changes (file ID 0 lets the controller pick). `kStalls` loads sample states and stall reasons when sampling details are shown or retried.
- Layers use distinct request IDs and may be in flight together; a newer request for a layer replaces the older one. The view accepts a callback only if layer, kernel, selection generation (`m_generation`), request token, and, for source, the selected file still match. A failed source or stalls layer never clears a loaded ISA pane.
- Issued and stall counts are nullable: the controller returns `kRocProfVisResultNotLoaded` for an unmeasured count, and the provider keeps it as an empty `std::optional`, distinct from a measured zero. Stall % shows only when every sample state of the instruction has both counts; otherwise the cell is a disabled N/A, and an all-host-trap capture omits the column.
- Stochastic captures report the irrelevant stall reason of issued samples as `OTHER_WAIT`; the view subtracts that before showing reason shares.
- The Offset column is the byte offset inside the selected code object, not a runtime address.
- Selecting a row in `IsaCodeWidget` or `SourceCodeWidget` (both `BaseCodeWidget`, sharing one `LineSelection`) scrolls the other pane to the first correlated row, switching source file first if needed. ISA rows without a source line stay visible but cannot be selected for correlation.

## AppMonitor

`AppMonitor` polls long-running controller operations that are not `DataProvider` requests (SSH connection and authentication, profiler sessions, file transfers, directory listings), turns status changes into events, and frees their resources only after their futures resolve.

**Code:** `rocprofvis_appmonitor.{h,cpp}`

**Related:** [Remote SSH](FEATURES.md#remote-ssh) and [Profiler launch](FEATURES.md#profiler-launch) (the operations registered here), [Events](#events) (`RemoteStatusEvent`, `ProfilerStatusEvent`), [App window and projects](#app-window-and-projects) (polling, status bar, and the exit gate).

- `AddOperation` takes a status function polled every frame, an event factory called only when the status changes, a terminal test, an optional cancel function, the bound future, and a first teardown. It returns the operation ID; subscribers filter status events by it, because several operations post the same event type.
- `RemoveOperation` never blocks: if the future has resolved it runs the teardowns at once, otherwise it calls the cancel function once and reaps the operation in a later `Update()`.
- Teardowns run in registration order after the future resolves. They capture raw handles and `shared_ptr`s by value, never a session's `this`, which may already be gone.
- `AppendTeardown(id, fn)` hands one more resource to a live operation and returns false if it was already reaped, in which case the caller frees it directly. `AddTeardownOp` registers a teardown-only operation for an owner destroyed while its future is in flight.
- `ProfilerSessionBase::FreeProfilerObjects()`, `RemoteProfilerSession`, and `SshSession` use these on destruction, so the SSH connection is freed last, after the remote worker is done.
- While operations exist the monitor requests frames itself; `HasPendingOperations()` gates app exit and `GetActiveOperationCount()` feeds the status bar.

## Utilities

`rocprofvis_utils.{h,cpp}` holds small shared helpers for time and number text, axis and zoom math, application paths, and POSIX paths.

**Code:** `rocprofvis_utils.{h,cpp}`

**Related:** [Timeline](#timeline) (the main user of the interval and zoom helpers), [Settings and persistence](#settings-and-persistence) (the time-format preference).

- Format times in the user's `UnitSettings::time_format` (a `TimeFormat`) and refresh on `kTimeFormatChanged`; never hand-format nanoseconds.
- Check a range with `is_usable_time_range` before driving the view; NaN slips past a plain `end <= start` test.
- `is_remote_display_session()` only picks a file-dialog backend under SSH or X11 forwarding; it says nothing about an `SshSession`.
- `get_application_config_path` resolves the per-user directory that holds every settings file: `LOCALAPPDATA` on Windows, `~/Library/Application Support` on macOS, `XDG_CONFIG_HOME` or `~/.config` on Linux. It rejects relative or `..` environment values. Logs share it except on macOS, which uses `~/Library/Logs`.
- **Reuse:** `nanosecond_to_timecode_str`, `nanosecond_str_to_formatted_str`, `timeformat_sufix`, and `TimeConstants`; `full_number_format` (full precision matching `compact_number_format`); `format_byte_size`; `calculate_adaptive_view_range` for the zoom around an event; `get_application_log_path`; `posix_base_name`, `posix_parent_path`, `join_posix_path`, and `normalize_posix_path` for remote paths, never host filesystem rules; `strip_ansi_for_display` for tool output; `CircularBuffer` for a bounded FIFO.
