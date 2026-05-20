# AGENTS.md - ROCm Optiq Codebase Guide

This document is the repo-level reference for AI assistants working on
ROCm Optiq (binary name: `roc-optiq`). It is written as plain,
tool-agnostic Markdown so any LLM-based coding agent can inspect and
reuse it as project context.

**Scope:** class-level architecture, where things live, and a reuse
catalog so contributors do not duplicate existing widgets, events, or
utilities. For per-method API details, read the headers under
`src/view/src/`. When this file and the source disagree, the source is
authoritative; please update this file in the same change.

It pairs with the human-facing docs:

- `README.md` - User-facing usage instructions and UI tour.
- `BUILDING.md` - Build instructions per platform.
- `CODING.md` - Hard rules on style, naming, formatting (READ THIS FIRST
  before changing any C++).
- `.github/CONTRIBUTING.md` - Contribution workflow.

When humans and `CODING.md` disagree with this file, `CODING.md` wins.

---

## Table of Contents

1. Project Identity
2. Build & Run
3. Repository Layout
4. Architectural Pillars
5. Module Boundaries: app / core / model / controller / view
6. The View Layer (UI) - top-down tour
7. Widget Library Reference (`src/view/src/widgets/`)
8. Track Item Hierarchy (Timeline rendering primitives)
9. Trace View Internals (System Profiler UI)
10. Compute View Internals (Compute Profiler UI)
11. UI Models (`src/view/src/model/`)
12. Cross-cutting Services (Events, Settings, Hotkeys, Notifications, Fonts, Icons)
13. Data Flow: How a click becomes a request becomes pixels
14. Coding Conventions in this Repo
15. Comment & Documentation Style
16. Reuse Catalog: "Before you write a new X, check if Y exists"
17. Common Pitfalls
18. Quick Reference Index of Every UI Class

---

## 1. Project Identity

- **Name:** ROCm Optiq (Beta) - `roc-optiq`
- **Owner:** AMD / ROCm
- **License:** MIT (`LICENSE.md`)
- **Language:** C++17. Public C library headers use C11.
- **GUI toolkit:** Dear ImGui (immediate-mode), `thirdparty/imgui/`. The repo
  does **not** use Qt, GTK, wxWidgets, or any retained-mode toolkit.
- **Plotting:** ImPlot (`thirdparty/implot/`).
- **Window/input:** GLFW (`thirdparty/glfw/`).
- **Render backend:** Vulkan (preferred) with OpenGL fallback. Selected by
  `src/app/src/rocprofvis_imgui_backend.cpp`.
- **Persistence / parsing:** SQLite (`thirdparty/sqlite3/`), jsoncpp, yaml-cpp.
- **Logging:** spdlog (`thirdparty/spdlog/`). Use `spdlog::info/warn/error`,
  never `std::cout` / `printf` / `iostream`.
- **File dialog:** Native via `nativefiledialog-extended` on most platforms,
  with `ImGuiFileDialog` as a built-in fallback (used automatically over
  SSH / remote display sessions). See `BUILDING.md` -> "File dialog
  behavior on Linux".
- **Trace formats consumed:** `*.db`, `*.rpd`, and `*.rpv` (project files).

## 2. Build & Run

Use CMake presets (`CMakePresets.json`). Don't hand-write CMake commands.

- Windows: `cmake --preset x64-debug` then build with the matching build preset.
- Linux:   `cmake --preset linux-release` -> `cmake --build build/linux-release`.
- macOS:   `cmake --preset macos-release` -> `cmake --build build/macos-release`.

Output executable:

- Windows debug: `build/x64-debug/Debug/roc-optiq.exe`
- Linux:         `build/<preset>/roc-optiq`
- macOS:         `build/<preset>/roc-optiq`

CMake options worth knowing:

- `ROCPROFVIS_ENABLE_INTERNAL_BANNER` - draws a watermark on internal builds.
- `ROCPROFVIS_DEVELOPER_MODE` - enables extra menus, the Debug Window, and
  `ComputeTester`. Guarded with `#ifdef ROCPROFVIS_DEVELOPER_MODE` in code.
- `COMPUTE_UI_SUPPORT` - guards the entire Compute analysis surface.
  Code touching compute features is wrapped with `#ifdef COMPUTE_UI_SUPPORT`.
- `USE_NATIVE_FILE_DIALOG` - off disables `nativefiledialog-extended`.

The CLI flag `--file-dialog={auto|imgui|native}` overrides dialog selection
at runtime (see `src/app/src/rocprofvis_cli_parser.h`).

## 3. Repository Layout

```
.
+-- src/                  # All first-party code
|   +-- app/              # Entry point: window, GLFW, ImGui backend, CLI
|   |   +-- inc/          # rocprofvis_imgui_backend.h, rocprofvis_version.h
|   |   +-- src/          # main.cpp, glfw_util, imgui_(opengl|vulkan), cli_parser
|   +-- core/             # Tiny core lib: assert macros, profile markers
|   |   +-- inc/          # rocprofvis_core.h, rocprofvis_core_assert.h
|   |   +-- src/
|   +-- model/            # Trace I/O & data model. Builds the C ABI.
|   |   +-- inc/          # rocprofvis_c_interface.h, rocprofvis_interface.h
|   |   +-- python/       # CFFI bindings
|   |   +-- src/common/   # Error handling, C interface impl
|   |   +-- src/database/ # SQLite, RocPD/RocProf adapters, query factories
|   |   +-- src/datamodel # In-memory dm_* types (events, tracks, samples)
|   |   +-- src/test*/    # Unit tests (Catch2)
|   +-- controller/       # Coordinates model + view, owns request queue
|   |   +-- inc/          # rocprofvis_controller.h (PUBLIC C API)
|   |   +-- src/          # Generic controller plumbing
|   |   +-- src/system/   # System-profile-specific (events, samples, tracks...)
|   |   +-- src/compute/  # Compute-profile-specific (kernels, roofline...)
|   |   +-- tests/        # System & compute Catch2 tests
|   \-- view/             # << This is the UI. Read sections 6-12. >>
|       +-- inc/          # rocprofvis_view_module.h (entry: init/render/destroy)
|       +-- src/          # All UI classes
|       |   +-- compute/  # Compute-only views (#ifdef COMPUTE_UI_SUPPORT)
|       |   +-- icons/    # Icon font glyph constants & ranges
|       |   +-- model/    # UI-side data models (cached projections)
|       |   |   +-- compute/
|       |   +-- widgets/  # Reusable widget library (use these first!)
+-- thirdparty/           # Vendored libs (imgui, glfw, sqlite3, spdlog, ...)
+-- resources/            # Icons, fonts, AMD logo, embedded assets
+-- sample/               # Sample traces (.db, .rpd, .rpv)
+-- docs/                 # ReadTheDocs sources, screenshots
+-- Installer/            # Windows installer (NSIS) config
+-- docker/               # Container build helpers
+-- .github/workflows/    # CI: ubuntu, oracle, rocky, windows, macos
+-- CMakeLists.txt        # Root build
+-- CMakePresets.json     # Build presets (preferred entrypoint)
+-- build.cmd             # Windows convenience wrapper
+-- pkgbuild.cmd          # Windows packaging
+-- CODING.md             # Coding standards (HARD RULES)
+-- BUILDING.md           # Per-platform build steps
+-- README.md             # Public README + UI tour
\-- AGENTS.md             # This file
```

## 4. Architectural Pillars

ROCm Optiq is layered model -> controller -> view, with a thin app shell
on top. The same trace can drive multiple parallel projects (one per tab).

```
                   +-------------------------+
                   |  app/ (main, GLFW,      |
                   |  Vulkan/GL backend, CLI)|
                   +------------+------------+
                                | calls rocprofvis_view_init/render/destroy
                                v
                   +-------------------------+
                   |  view/  (ImGui UI)      |
                   |  AppWindow singleton    |
                   |  N projects in tabs     |
                   +------------+------------+
                                | DataProvider per project
                                v
                   +-------------------------+
                   |  controller/  (C ABI)   |
                   |  rocprofvis_controller* |
                   |  request/future queues  |
                   +------------+------------+
                                |
                                v
                   +-------------------------+
                   |  model/ (SQLite, parse) |
                   |  rocprofvis_dm_* types  |
                   +-------------------------+
```

Key invariants:

1. **The view never touches SQLite or trace files directly.** It calls
   `controller`'s C API through `DataProvider`.
2. **The controller exposes a stable C ABI** (`src/controller/inc/`) so it
   can be called from C/C++/Python (CFFI). All long-running operations are
   asynchronous via `rocprofvis_controller_future_t`.
3. **The view is single-threaded immediate mode.** All ImGui calls happen on
   the main thread inside the per-frame `Render()` traversal. Heavy work
   (SQL queries, file save, cleanup) is offloaded to controller futures or
   to `std::future<...>` jobs owned by `AppWindow`.
4. **Ownership is explicit.** Every `Project` owns one `DataProvider`. Every
   widget that subscribes to events stores its `EventManager::SubscriptionToken`
   and unsubscribes in its destructor.
5. **Per-project state is keyed by trace path.** `Project::GetID()` is the
   trace file path; the `EventManager` source ID and `ProjectSetting`
   serialization use the same key.
6. **The app supports both system traces (`TraceView`) and compute traces
   (`ComputeView`)**, both deriving from `RootView` and gated by
   `Project::TraceType`. Compute is conditional behind `COMPUTE_UI_SUPPORT`.

## 5. Module Boundaries

### `src/app/`

Owns the OS-level shell. Specifically:

- `main.cpp` - GLFW window, fullscreen state, drag-and-drop callback, and
  the per-frame loop that calls `rocprofvis_view_render()`.
- `glfw_util.{h,cpp}` - `FullscreenState`, `toggle_fullscreen`,
  `sync_fullscreen_state`. Use these instead of touching GLFW directly.
- `rocprofvis_imgui_backend.{h,cpp}` plus `rocprofvis_imgui_opengl.cpp` /
  `rocprofvis_imgui_vulkan.cpp` - selects renderer at runtime, sets up
  ImGui's backend, and exposes the texture-creation callback that
  `GuiTexture::SetBackend()` plugs into.
- `rocprofvis_cli_parser.{h,cpp}` - generic short/long flag parser
  (`CLIParser::AddOption`). Flags currently registered in `main.cpp`:
  `-v/--version`, `-f/--file <path>`, `-b/--backend {auto|vulkan|opengl}`,
  `-d/--file-dialog {auto|native|imgui}`, `-h/--help`. Add new flags by
  calling `AddOption` in `main.cpp::parse_command_line_args`.

### `src/core/`

Tiny library reused by every other module. Its job is to be header-light:

- `rocprofvis_core_assert.h` - `ROCPROFVIS_ASSERT(cond)` and related
  assert/return helpers. Use language `static_assert` for compile-time
  invariants. Do not use bare `assert()`.
- `rocprofvis_core_profile.h` - lightweight profiling markers used to
  instrument hot paths. Wrap costly work in these instead of adding
  ad-hoc timers.
- `rocprofvis_core.h` - umbrella header.

### `src/model/`

Owns the on-disk trace and produces typed in-memory records. The view
should not include anything from this directory directly.

- `inc/rocprofvis_c_interface.h` - the C ABI surface (used by Python CFFI).
- `inc/rocprofvis_interface.h` - C++ wrappers and helpers.
- `src/database/` - SQLite adapters (`rocprofvis_db_sqlite`), source-format
  adapters (`rocprofvis_db_rocpd`, `rocprofvis_db_rocprof`), packed storage,
  query builders/factories, expression filter, schema versioning.
- `src/datamodel/` - in-memory data structures with the prefix `dm_`:
  `dm_event_record`, `dm_event_track_slice`, `dm_pmc_record`,
  `dm_pmc_track_slice`, `dm_flow_trace`, `dm_stack_trace`, `dm_table`,
  `dm_table_row`, `dm_topology`, `dm_track`, `dm_track_slice`, `dm_trace`,
  `dm_ext_data`. These are the canonical model types.
- `src/common/rocprofvis_error_handling.{h,cpp}` - error propagation
  primitives (`rocprofvis_result_t` codes).
- `src/tests/` - Catch2 unit tests for the model.
- `python/` - CFFI build script (`rocprofvis_cffi_build.py`) and tests.

### `src/controller/`

The bridge between model and view. **Public** API in `inc/`:

- `rocprofvis_controller.h` - the full C function set. Highlights:
  - `rocprofvis_controller_alloc(filename)` / `rocprofvis_controller_load_async`.
  - `rocprofvis_controller_future_alloc/free` for async fences.
  - `rocprofvis_controller_array_alloc` and `_arguments_alloc` for batched
    calls.
  - `rocprofvis_controller_get_uint64/double/object/string/...` - generic
    property accessor (see `rocprofvis_property_t`).
  - Specialized fetchers per category (events, samples, summary, kernels,
    roofline, metrics, queue utilization, cleanup, save trim, etc).
- `rocprofvis_controller_enums.h` / `_types.h` - all opaque handles
  (`rocprofvis_handle_t`, `rocprofvis_controller_t`, ...) and result codes.

Internal source layout under `src/controller/src/`:

- `rocprofvis_controller.cpp` - lifecycle, dispatch.
- `rocprofvis_controller_future.{h,cpp}` - the async future type.
- `rocprofvis_controller_array.{h,cpp}` - growable array used in args.
- `rocprofvis_controller_arguments.{h,cpp}` - typed argument lists for
  controller calls.
- `rocprofvis_controller_data.{h,cpp}` - shared "result data" wrapper.
- `rocprofvis_controller_handle.{h,cpp}` - ref-counted handle base class.
- `rocprofvis_controller_id.h` - 64-bit ID encoding helpers.
- `rocprofvis_controller_job_system.{h,cpp}` - small thread pool for
  async work. Prefer scheduling controller async work here; if a
  dedicated thread is necessary, follow the ownership/lifetime pattern
  used by `MemoryManager`.
- `rocprofvis_controller_string_table.{h,cpp}` - intern-style table.
- `rocprofvis_controller_table.{h,cpp}` - generic table support.
- `rocprofvis_controller_trace.{h,cpp}` - trace-file lifecycle.
- `rocprofvis_controller_analysis.{h,cpp}` - cross-cutting analytics
  (e.g. queue utilization).
- `system/` - per-domain modules covering events, ext_data, flow
  control, graphs, memory management, samples (and sample LOD),
  segments, summary (+metrics), tables, timeline, topology, traces,
  tracks, call stack.
- `compute/` - per-domain modules covering kernel, metrics container,
  plots (+compute, +series), roofline, table compute (+pivot), trace
  compute, workload.
- `tests/` - Catch2 system + compute tests.

### `src/view/`

This is the entire UI and the largest target. The remainder of this
document focuses on it. The single public entry surface is
`src/view/inc/rocprofvis_view_module.h`:

```cpp
bool rocprofvis_view_init(std::function<void(int)> notification_callback,
                          rocprofvis_view_file_dialog_preference_t pref);
void rocprofvis_view_render(const rocprofvis_view_render_options_t& opts);
void rocprofvis_view_destroy();
void rocprofvis_view_set_dpi(float dpi);
void rocprofvis_view_open_files(const std::vector<std::string>& paths);
void rocprofvis_view_set_fullscreen_state(bool is_fullscreen);
void rocprofvis_view_set_texture_backend(...);
std::string rocprofvis_get_application_config_path();
bool rocprofvis_view_is_remote_display_session();
```

These are the only symbols `app/` should call from `view/`. Internally
they all forward to `RocProfVis::View::AppWindow`.

## 6. The View Layer - Top-Down Tour

Everything in the view lives in `namespace RocProfVis::View`. The widget
hierarchy is rooted at `AppWindow` and looks like this for a typical
"opened a system trace" session:

```
AppWindow (singleton, RocWidget)
|
|  ImGui menu bar (rendered inline before m_main_view->Render():
|    File / Edit / View / Help [+ Developer in dev mode])
|
+-- VFixedContainer m_main_view  (vertical fixed layout, 3 slots)
|   +-- [0] toolbar slot (RootView::GetToolbar(), per active project)
|   +-- [1] main_area_item : RocCustomWidget
|   |       renders m_tab_container OR RenderEmptyState() if no tabs
|   |       Per-tab widget = Project::GetView() = RootView
|   |         |
|   |         +-- TraceView : RootView                 (system trace)
|   |         |   +-- m_tool_bar      : RocCustomWidget (slotted into [0])
|   |         |   +-- m_horizontal_split_container : HSplitContainer
|   |         |   |   +-- left  : SideBar (topology tree)
|   |         |   |   +-- right : VSplitContainer
|   |         |   |              +-- top    : VFixedContainer
|   |         |   |              |           +-- TimelineView (+ Minimap below)
|   |         |   |              +-- bottom : AnalysisView
|   |         |   +-- m_event_search   : EventSearch (toolbar pop-down)
|   |         |   +-- m_summary_view   : SummaryView (separate sub-window)
|   |         |   +-- AnnotationsManager (per-project, drives StickyNote items)
|   |         |   +-- TimelineSelection (per-project, owns selection state)
|   |         |   +-- TrackTopology    (per-project, owns sidebar tree)
|   |         |
|   |         \-- ComputeView : RootView                 (compute trace)
|   |             +-- m_tool_bar       : RocCustomWidget (slotted into [0])
|   |             +-- m_tab_container  : TabContainer with sub-tabs:
|   |                 +-- ComputeSummaryView
|   |                 +-- ComputeKernelDetailsView
|   |                 +-- ComputeTableView
|   |                 +-- ComputeComparisonView
|   |                 +-- ComputeWorkloadView
|   |                 +-- (ComputeTester, dev mode only)
|   +-- [2] status bar (RocCustomWidget calling AppWindow::RenderStatusBar)
+-- m_settings_panel        : SettingsPanel (modal, opened via menu)
+-- m_confirmation_dialog   : ConfirmationDialog
+-- m_message_dialog        : MessageDialog
+-- NotificationManager     : singleton, drawn last (toasts overlay)
+-- DebugWindow             : singleton, dev mode only
```

### `AppWindow` - the root singleton

File: `src/view/src/rocprofvis_appwindow.{h,cpp}`. Owns global UI state:

- `static AppWindow* GetInstance() / DestroyInstance()` - lifetime is
  managed via these. **Never `new` an `AppWindow` directly.**
- `bool Init()` - one-time setup (settings, fonts, event manager, image
  helpers). Called by `rocprofvis_view_init`.
- `void Render()` / `void Update()` - per-frame entry points.
- `void OpenFile(std::string file_path)` - opens a trace or `.rpv`
  project. Routes via `Project::Open()` and adds a tab.
- `void ShowConfirmationDialog(title, message, on_confirm)` /
  `ShowMessageDialog` - centralized modal dialogs. Always go through
  these, do not create your own popups for ok/cancel flows.
- `void ShowSaveFileDialog(...)` / `ShowOpenFileDialog(...)` - file
  pickers. Auto-routes between native and ImGui implementations based on
  `m_file_dialog_preference`.
- `Project* GetCurrentProject() / GetProject(id)` - lookup helpers.
- `BeginAppShutdown()` - graceful shutdown that drains async
  `DataProvider` cleanup work via `m_provider_cleanup_jobs`. Use this
  rather than terminating the process.
- `SetTabLabel(label, id)` - pushed to the `TabContainer` via the
  `Project` system; useful when a child view wants to indicate dirty
  state.

State of note:
- `m_main_view` (`shared_ptr<VFixedContainer>`) - 3-slot vertical layout
  for the main window: `[0]` toolbar slot (filled from
  `RootView::GetToolbar()` of the active tab via the `m_tool_bar_index`
  index), `[1]` `RocCustomWidget` that renders `m_tab_container` or
  `RenderEmptyState()`, `[2]` status bar. The ImGui menu bar is rendered
  inline by `AppWindow::Render()` before this view, not slotted in.
- `m_tab_container` (`shared_ptr<TabContainer>`) - the project tabs;
  source name is `TAB_CONTAINER_SRC_NAME` (`"MainTabContainer"`).
- `m_projects` (`unordered_map<string, unique_ptr<Project>>`) - one
  entry per opened trace.
- `m_provider_cleanup_jobs` - async cleanup of in-flight `DataProvider`
  requests when a tab closes or the app exits.
- `m_status_message`, `m_status_show_busy_indicator` -
  `UpdateStatusBar()` populates these from the active project's
  `DataProvider::GetProgressMessage()`.
- Subscribed events: `kTabClosed`, `kTabSelected`, `kFontSizeChanged`.

### `Project` - per-trace bundle

File: `rocprofvis_project.{h,cpp}`. Wraps everything tied to a single
trace file:

- `OpenResult Open(std::string& file_path)` - opens a `.rpv`/trace and
  attaches a `RootView` (either `TraceView` or `ComputeView`). Returns
  `Success`, `Duplicate`, or `Failed`.
- `void Save()` / `void SaveAs(file_path)` - serializes registered
  `ProjectSetting`s into a `.rpv`.
- `void RegisterSetting(ProjectSetting*)` - any per-project state that
  must persist participates by deriving from `ProjectSetting` and
  registering itself in its constructor. Examples:
  `TimelineViewProjectSettings`, `SystemTraceProjectSettings`,
  `TrackProjectSettings`, `LineTrackProjectSettings`,
  `FlameTrackProjectSettings`, `AnnotationsManagerProjectSettings`.
- `jt::Json& GetSettingsJson()` - the in-memory JSON tree. JSON keys
  used in `.rpv` files are the `JSON_KEY_*` constants in this header.
- `TraceType GetTraceType()` - `Undefined | System | Compute`.
- `GetID()` - the trace file path; used as event source ID and as the
  registration key in `PresetManager`.

### `RootView` - polymorphic per-trace view

File: `rocprofvis_root_view.{h,cpp}`. Tiny base class that the two
concrete views derive from:

```cpp
class RootView : public RocWidget {
public:
    virtual std::shared_ptr<RocWidget>          GetToolbar();
    virtual void                                RenderEditMenuOptions();
    virtual std::optional<DataProviderCleanupWork> DetachProviderCleanup();
    virtual DataProvider* GetDataProvider();
protected:
    void                 RenderLoadingScreen(const char* progress_label);
    SettingsManager&     m_settings_manager;
};
```

Implementations: `TraceView` (system) and `ComputeView` (compute, gated
by `COMPUTE_UI_SUPPORT`). When you add a new project type, you derive
from `RootView`, fill `GetToolbar`, `RenderEditMenuOptions`, and
`DetachProviderCleanup`, then teach `Project::Open` to instantiate it.

## 7. Widget Library Reference (`src/view/src/widgets/`)

**Always look here before writing a new piece of UI.** These are the
shared, theme-aware building blocks. Headers are listed with the
reusable types they expose.

### 7.1 `rocprofvis_widget.{h,cpp}` - base classes

- `class RocWidget` - base for every drawable. Has `Render()`, `Update()`,
  `GenUniqueName(prefix)` (collision-safe ImGui IDs), `GetWidgetName()`,
  and a `m_widget_name` slot. **Every widget must own a unique
  `m_widget_name`** (set via `GenUniqueName(...)`). It is used as the
  ImGui scope ID and as the event source ID.
- `class LayoutItem` - struct describing one slot in a container. Carries
  width/height, visibility, background color, child/window flags,
  spacing, and the wrapped `RocWidget`. Use `LayoutItem::CreateFromWidget`.
- `class RocCustomWidget : public RocWidget` - wraps a free-standing
  `std::function<void()>` callback. Used to host inline custom rendering
  inside a layout slot (toolbars, ad-hoc panels). Prefer this over
  defining a one-off `RocWidget` subclass for trivial content.
- `struct TabItem` - `{ label, id, widget, can_close }`, the shape of a
  tab.
- Free functions:
  - `WithPadding(left, right, top, bottom, content_fn)` - RAII padding.
  - `CopyableTextUnformatted(text, unique_id, ...)` - clickable copy-to-
    clipboard text. Use this for any cell whose value the user may want
    to copy (events, args, paths).
  - `COPY_DATA_NOTIFICATION` - canonical "data was copied" toast text.
- `class PopUpStyle` - RAII helper that pushes consistent popup colors,
  borders, and centering. Use this around `BeginPopupModal` instead of
  hand-rolling style pushes.

### 7.2 `rocprofvis_dialog.{h,cpp}` - confirm/message dialogs

- `class ConfirmationDialog` - title/message, OK/Cancel callbacks,
  optional "don't ask again" checkbox bound to a `bool&` setting.
  Instantiate inside the owning widget; call `Show(...)` to request
  open and `Render()` from inside the owner's `Render()`.
- `class MessageDialog` - one-button info popup.

### 7.3 `rocprofvis_split_containers.{h,cpp}` - layout

- `class VFixedContainer : public RocWidget` - vertical fixed slots,
  used as the app skeleton.
- `class SplitContainerBase : public RocWidget` - shared logic for
  splitters with grip drag, min sizes, and ratio.
- `class HSplitContainer : public SplitContainerBase` - left/right
  splitter. Used for sidebar+timeline, kernel-details flex, etc.
- `class VSplitContainer : public SplitContainerBase` - top/bottom
  splitter. Used between Timeline and Analysis panels.

When you need a resizable two-pane layout, use these. Don't reinvent
splitter dragging.

### 7.4 `rocprofvis_flex_container.{h,cpp}` - flex layout

- `struct FlexItem` - `{ id, widget, min_width, height, flex_grow,
  full_row }`.
- `class FlexContainer : public RocWidget` - flexbox-style row layout
  that wraps when items don't fit. Set `subcomponent_layout = true` to
  paint a single shared card with separators between items.

### 7.5 `rocprofvis_tab_container.{h,cpp}` - tabs

- `class TabContainer : public RocWidget` - drives tabs and emits
  `kTabClosed` / `kTabSelected` `RocEvent`s. Set the event source name
  with `SetEventSourceName(...)`. Toggle close/change events via
  `EnableSendCloseEvent` / `EnableSendChangeEvent`. Used in `AppWindow`
  for the project tabs and in `ComputeView` for sub-tabs.

### 7.6 `rocprofvis_gui_helpers.{h,cpp}` - low-level UI helpers

Use these instead of inlining their logic anywhere new.

- `RenderLoadingIndicator(color, window_id, centering, dot_radius,
  num_dots, dot_spacing, anim_speed)` and the lower-level
  `RenderLoadingIndicatorDots`. Use this any time you need the standard
  bouncing-dots spinner. `LoadingIndicatorCentering` enum =
  `kCenterNone | kCenterHorizontal | kCenterVertical | kCenterBoth`.
- `ApplyAlpha(color, alpha)` - mix alpha into an `ImU32`.
- `ThemeColor(settings, Colors color, alpha)` - fetch a themed color
  with alpha applied. Use this rather than reading
  `settings.GetColor(...)` then composing alpha by hand.
- `GetResponsiveWindowSize(desired, min, viewport_margin)` - clamp a
  modal size to a fraction of the viewport.
- `PushComboStyles()` / `PopComboStyles()` - paired style pushes that
  make ImGui combo boxes visually distinct from plain text inputs.
- `IconButton(icon, icon_font, size, tooltip, frameless, ...)` - the
  canonical glyph button. Must use the icon font from `FontManager`.
- `IsMouseReleasedWithDragCheck(button, drag_threshold)` - "click
  vs drag" disambiguator.
- `InputTextWithClear(id, hint, buf, buf_size, icon_font, bg_color,
  style, width)` - standard text input with an X-button to clear.
- `SetTooltipStyled / BeginTooltipStyled / BeginItemTooltipStyled /
  EndTooltipStyled` - themed tooltip helpers; use these instead of
  `ImGui::SetTooltip` so font/color/padding stay consistent.
- `enum Alignment { Left, Center, Right }` and `ElidedText(text,
  available_width, tooltip_width, alignment, align_to_frame)` -
  ellipsizes text and shows the full text in a tooltip on hover.
- `CenterNextTextItem` / `CenterNextItem` - centers the next ImGui
  draw within the current row.
- `XButton(id, tool_tip, settings)` - the small "x" close button
  reused everywhere (tabs, event details, search box).
- `SectionTitle(text, large, settings)` - the standard heading style.
- `VerticalSeparator(settings)` - themed vertical rule.
- `TableRowHeight()` - canonical row height; align tables to this.
- `DrawInternalBuildBanner(text)` - watermark, gated by
  `ROCPROFVIS_ENABLE_INTERNAL_BANNER`.

### 7.7 `rocprofvis_image_helpers.{h,cpp}` - GPU textures

- `class GuiTexture` - owns one renderer-side texture. `CreateRGBA32`,
  `Destroy`, `Render(top_left, target_width)`. Move-only.
  `SetBackend(create, destroy, user_data)` is called by the app shell
  exactly once at startup.
- `class EmbeddedImage` - decodes a PNG/JPG byte buffer (via
  `stb_image`) into RGBA8, retains CPU pixels for callers like
  `glfwSetWindowIcon`, and lazily uploads on first `Render()`.
  Used for the AMD logo, app icon, and other compiled-in assets.

### 7.8 `rocprofvis_notification_manager.{h,cpp}` - toast/notifications

- `enum NotificationLevel { Info, Success, Warning, Error }`.
- `struct Notification` - includes id, message, level, progress
  (`progress_pct`, `progress_message`), timing, `is_persistent`,
  `is_hiding`. `GetOpacity(now)` and `IsExpired(now)` drive the fade.
- `class NotificationManager` - singleton (`GetInstance()`). Show
  ephemeral toasts with `Show(message, level)` /
  `Show(message, level, duration)`. Show progress/sticky toasts with
  `ShowPersistent(id, message, level)` and update them with
  `UpdateProgress(id, pct, message)`. Hide persistent ones via
  `Hide(id)`. **Never use `ImGui::OpenPopup` for status messages -
  use this.** Drawn last in the frame.

### 7.9 `rocprofvis_infinite_scroll_table.{h,cpp}` - virtualized table

`class InfiniteScrollTable : public RocWidget`. Base class for any
table that wants:

- on-demand row fetching (chunked from controller),
- `where`/group-by filters and column hide/show,
- sort by clicking a header,
- right-click context menu, multi-row selection, "go to event" routing,
- export-to-CSV.

Subclasses must override:
- `void FormatData() const` - format raw cells for display.
- `void IndexColumns()` - record column indices for the
  `ImportantColumns` enum (`kUUId`, `kDbEventId`, `kName`, `kTrackId`,
  `kStreamId`).
- `void RowSelected(ImGuiMouseButton)` - what happens on row click.

Concrete subclasses already in the tree: `MultiTrackTable`,
`EventSearch`, `KernelInstanceTable`. **Add new event/sample tables by
subclassing this; don't roll your own.**

### 7.10 `rocprofvis_query_builder.{h,cpp}` - 4-level metric picker

- `class QueryBuilder : public RocWidget` - guided picker for a
  Compute metric expression (Category -> Table -> Entry -> ValueName).
  `Show(on_confirm, on_cancel)`, `SetWorkload(WorkloadInfo*)`. Used by
  `KernelMetricTable` (Add Metric flow) and `ComputeTester`.

### 7.11 `rocprofvis_compute_widget.{h,cpp}` - metric tables

(Defined here, shared between Compute views.)

- `class MetricTableBase : public RocWidget` - rows of `MetricId`->`Row`
  with optional pin checkbox column. `RenderTooltip`, `RenderRowValues`,
  `RenderUnitValue`, `FillDefaultColumns`, `AddMetricToKernelDetails`.
- `class MetricTable : public MetricTableBase` - normal "all metrics in
  this table" view; `Populate(table, get_value_lambda)`.
- `class PinnedMetricTable : public MetricTableBase` - the cross-table
  "pinned" view, populated incrementally via `AddRow(metric_id)` and
  `RefillTable(pinned_ids)`.
- `class MetricTableWidget : public RocWidget` and
  `class WorkloadMetricTableWidget : public MetricTableWidget` -
  fetch-and-render wrappers used by `ComputeKernelDetailsView` (System
  Speed-of-Light) and `ComputeSummaryView`.

### 7.12 `rocprofvis_editable_textfield.{h,cpp}`

- `class EditableTextField` - inline "click to edit" text field with a
  reset-to-default button. Use this for editable labels (e.g.
  `LineTrackItem::VerticalLimits`).

### 7.13 `rocprofvis_debug_window.{h,cpp}` (developer mode only)

- `class DebugWindow` - singleton. Aggregates spdlog output into a
  scrollable panel. `AddDebugMessage`, `AddPersitentDebugMessage`
  (sic), max-message limit, transient/persistent split panes.

## 8. Track Item Hierarchy

Tracks are the horizontal lanes in the timeline. They live in
`src/view/src/`:

- `rocprofvis_track_item.{h,cpp}` - `class TrackItem` (base).
- `rocprofvis_flame_track_item.{h,cpp}` - `class FlameTrackItem :
  public TrackItem` for event/duration data.
- `rocprofvis_line_track_item.{h,cpp}` - `class LineTrackItem :
  public TrackItem` for counter/sample data.

### `TrackItem` (base) - what every track has

Constructor: `TrackItem(DataProvider& dp, uint64_t id,
shared_ptr<TimePixelTransform> tpt)`.

Public:
- `Render(width)` / `Update()` - per-frame work.
- `GetID() / SetID(id)` - the controller's track id.
- `GetTrackHeight()` and `bool TrackHeightChanged()` - height
  bookkeeping that the parent timeline reads.
- `IsInViewVertical() / SetInViewVertical(bool)` - culling hint.
- `GetDistanceToView() / SetDistanceToView(float)` - drives data
  release for off-screen tracks.
- `GetReorderGripWidth()` - width of the drag handle.
- `IsSelected() / SetSelected(bool)` - selection state (mirrored to
  `TimelineSelection`).
- `IsMetaAreaClicked()` - did the user click the description column.
- `GetRequestState()` - `kIdle | kRequesting | kTimeout | kError`.
- `GetMetaAreaScaleWidth() / UpdateMaxMetaAreaSize(new_size)` -
  contributes to the global meta-area width sync.
- Data-handling overrides: `HasData`, `ReleaseData`, `RequestData`,
  `RequestAnalysis`, `HandleTrackDataChanged`, `HasPendingRequests`,
  `IsCompactMode`. Override `ExtractPointsFromData()` to convert raw
  data into your renderable cache.

Protected hooks every subclass implements:
- `RenderMetaArea()` - left "description" column (default impl is
  reused).
- `RenderMetaAreaScale()` - the in-meta numeric scale (per type).
- `RenderMetaAreaOptions()` - the gear-icon menu (per type).
- `RenderMetaAreaExpand()` - expand/collapse affordance.
- `RenderChart(graph_width)` - the actual graph drawing.
- `RenderResizeBar(parent_size)` - drag handle for height.

State of note: `m_track_metadata` (`const TrackInfo*` from
`TrackTopology`), `m_track_height`, `m_pill` (`Pill`, the small label
pill in the meta area), `m_request_queue`, `m_pending_requests`.

`class Pill` - the rounded label badge used inside meta areas.
`Activate/Deactivate`, `Show/Hide`, `SetLabel/SetTooltipLabel`. Renders
through `RenderPillLabel(container_size, settings, reorder_grip_width)`.

### `FlameTrackItem` - flame-chart for events

Used for queues, streams, and instrumented threads. It holds:

- `enum class EventColorMode { kNone, kByEventName, kByTimeLevel }`.
- `class FlameTrackProjectSettings : ProjectSetting` - persists
  `ColorEvents()` and `CompactMode()`.
- A cache of `ChartItem` (`TraceEvent` + selected/highlight + child
  info), refilled via `ExtractPointsFromData()`.
- Selection / highlight bookkeeping that listens to
  `kTimelineEventSelectionChanged` and
  `kTimelineEventHighlightChanged`.
- A static `s_max_event_label_width` and helper
  `CalculateMaxEventLabelWidth()` invoked when fonts change.

### `LineTrackItem` - line/box-plot for counters

Holds `m_data` (`vector<TraceCounter>`) and:

- `class LineTrackProjectSettings : ProjectSetting` - persists box-plot
  toggle, stripes, highlight band, and Y limits.
- `class LineTrackItem::VerticalLimits` - inline editable Y-min/Y-max
  fields backed by `EditableTextField`.
- `BoxPlotRender(width)` and `RenderHighlightBand(...)`.
- `MapToUI` helper that converts `(x, y)` data to screen.

When you add a new track type:

1. Subclass `TrackItem`.
2. Implement `RenderChart`, `RenderMetaAreaScale`,
   `RenderMetaAreaOptions`, `ExtractPointsFromData`, `ReleaseData`.
3. Add a `TrackProjectSettings` subclass if any state must persist.
4. Construct from `TimelineView::MakeGraphView()` based on
   `TrackInfo::TrackType` / `rocprofvis_controller_track_type_t`.

## 9. Trace View Internals (System Profiler UI)

This section covers the `TraceView` workspace - the system-profile
view shown when a `.db` / `.rpd` / `.rpv` (system trace) is opened.

### `TraceView` (`rocprofvis_trace_view.{h,cpp}`)

Composition (members):

- `m_data_provider` (`DataProvider`) - the controller bridge.
- `m_timeline_selection` (`shared_ptr<TimelineSelection>`) - holds the
  currently selected tracks/events/time-range. Always pass this around
  to children rather than reinventing selection state.
- `m_track_topology` (`shared_ptr<TrackTopology>`) - rebuilds the
  hierarchical sidebar tree on metadata changes.
- `m_annotations` (`shared_ptr<AnnotationsManager>`) - sticky-note
  annotations.
- `m_timeline_view` (`shared_ptr<TimelineView>`) - the grid + tracks +
  scrubber surface (described below).
- `m_horizontal_split_container` / `m_vertical_split_container` -
  layout. The left side is a `SideBar` (topology). The right side is
  vertically split into the timeline+minimap and the `AnalysisView`.
- `m_event_search` (`shared_ptr<EventSearch>`) - search popup for
  finding events by name/regex.
- `m_summary_view` (`shared_ptr<SummaryView>`) - independent panel
  that visualizes top-kernel summary metrics.
- `m_minimap` (`shared_ptr<Minimap>`) - density mini-map below the
  timeline.
- `m_tool_bar` (`RocCustomWidget`) - toolbar with bookmark/flow/search
  controls.
- `m_bookmarks` - 10 saved view positions; `RenderBookmarkControls()`,
  `HandleHotKeys()` keyed via `HotkeyManager`.
- `SystemTraceProjectSettings` - persists bookmarks via `Project`.

Public surface:
- `LoadTrace(controller, file_path)` / `CreateView()` / `DestroyView()`.
- `SaveSelection(file_path)`, `CleanupDatabase(rebuild, on_complete)`,
  `IsCleanupPending()`, `HasTrimActiveTrimSelection()`,
  `IsTrimSaveAllowed()`.
- `SetAnalysisViewVisibility / SetSidebarViewVisibility /
  SetHistogramVisibility` - toggled from the View menu.
- `GetTimelineSelection()` / `GetToolbar()` / `GetDataProvider()`.

### `TimelineView` (`rocprofvis_timeline_view.{h,cpp}`)

The big one. Owns the timeline rendering surface. Members worth knowing:

- `m_graphs` (`shared_ptr<vector<TrackGraph>>`) - the ordered set of
  visible tracks. Each `TrackGraph` has `{ graph_type, display, chart
  (TrackItem*), selected }`.
- `m_tpt` (`shared_ptr<TimePixelTransform>`) - **the** time<->pixel
  conversion. All time math goes through this.
- `m_arrow_layer` (`TimelineArrow`) - draws fan/chain arrows for event
  flows.
- `m_loading_timer` - debounces showing the loading spinner.
- `m_grid_interval_ns / m_grid_interval_count` - cached "nice"
  intervals from `calculate_nice_interval` /
  `fit_graph_axis_interval` (in `rocprofvis_utils.h`).
- `m_track_position_y / m_track_height_total` - per-track layout
  caches; recomputed when heights change.
- `m_reorder_request` - drag-and-drop to reorder tracks.
- `m_dragging_selection_start / m_dragging_selection_end /
  m_is_selecting_region` - region (time-range) selection drag state.
- `m_pseudo_focus / m_histogram_pseudo_focus` -
  `TimelineFocusManager` integration so keyboard hotkeys only fire
  when the timeline (or histogram) "owns" focus.

Public surface:
- `MakeGraphView()` - rebuilds `m_graphs` from current track metadata.
- `ResetView()` - restores zoom/pan defaults.
- `RenderInteractiveUI`, `RenderGraphPoints`, `RenderHistogram`,
  `RenderTraceView`, `RenderGrid`, `RenderScrubber`, `RenderSplitter`,
  `RenderGraphView`, `RenderAnnotations` - composable render passes.
- `ScrollToTrack(track_id)`, `MoveToPosition(start_ns, end_ns,
  y_position, center)`, `SetViewableRangeNS(start, end)`,
  `GetViewCoords()`, `GetTransform()`, `GetGraphSize()`,
  `GetTotalTrackHeight() / GetTrackViewportHeight() /
  GetVisibleTrackFractions(...)` - everything needed to bookmark,
  navigate, or query the current view.
- `GetArrowLayer()` returns the `TimelineArrow` for flow visuals.

Subscribed events: `kNewTrackData`, `kHandleUserGraphNavigationEvent`
(carries `ScrollToTrackEvent`), `kSetViewRange`, `kFontSizeChanged`,
`kGoToTimelineSpot` (carries `NavigationEvent`),
`kTimelineTimeRangeChanged`. Always keep these tokens released in the
destructor.

Note: there is no `kScrollToTrack` enum value. The `ScrollToTrackEvent`
payload class is dispatched on `kHandleUserGraphNavigationEvent` and
disambiguated by `dynamic_pointer_cast`.

### `TimePixelTransform` (`rocprofvis_time_to_pixel.{h,cpp}`)

Owns the affine mapping `time_ns <-> pixel_x` for the timeline. **All**
time math should funnel through this object - never multiply
`pixels_per_ns` by hand in feature code.

Public:
- `SetMinMaxX(min, max)`, `SetViewTimeOffsetNs(v)`, `SetZoom(z)`,
  `SetGraphSize(x, y)`.
- `ZoomAtPixel(pixel_x, zoom_delta)` - the scroll-wheel zoom anchor.
- `Reset()`, `ValidateAndFixState()`.
- `TimeToPixel(time_ns)` / `RawTimeToPixel(time_ns)` /
  `PixelToTime(x)` / `NormalizeTime(t)` / `DenormalizeTime(t)`.
- Read accessors for everything.

### `TrackTopology` (`rocprofvis_track_topology.{h,cpp}`)

Builds the hierarchical sidebar model and maps tracks back to nodes.
Run inside `Update()` when track metadata changes
(`kTrackMetadataChanged`). Exposes:

- `Update()`, `Dirty()`, `FormatCells()`.
- `GetTopology()` returning the full `TopologyModel` (Node ->
  Process/Processor -> Stream/Queue/Counter/Thread...).
- `GetSidebarTree()` returning a `SidebarTree` made of `TreeNode` and
  `LeafNode` (defined in `rocprofvis_tree_node.h`,
  `enum class NodeType` for the kind).

### `SideBar` (`rocprofvis_sidebar.{h,cpp}`)

Renders the topology tree in the left pane:

- Tracks are leaves; branches are nodes/processes/devices.
- `EyeButtonState` (`kAllVisible|kAllHidden|kMixed`) is computed
  recursively to drive the show/hide eye icon at each level.
- `ApplyVisibility(node, visible)` toggles every leaf below `node` and
  pushes the change through `m_graphs` and `DataProvider`.

### `Minimap` (`rocprofvis_minimap.{h,cpp}`)

Density heatmap below the timeline. Bins per-track samples into
`MINIMAP_SIZE = 500` columns, supports event/counter color cache, and
exposes a draggable viewport (`HandleNavigation`) that pans the main
timeline. Drives navigation via `RangeEvent` -> `kSetViewRange`.

### `EventSearch` (`rocprofvis_event_search.{h,cpp}`)

A toolbar-attached `InfiniteScrollTable` subclass. `Show()` opens it;
`Search()` issues a fetch using
`DataProvider::EVENT_SEARCH_REQUEST_ID`. Right-click row -> "Go to
event" routes through `TimelineSelection::NavigateToEvent`.

### `SummaryView`, `HWUtilization`, `TopKernels`, `KernelInstanceTable` (`rocprofvis_summary_view.{h,cpp}`)

The top-bar Summary panel. `SummaryView` hosts an `HSplitContainer`
with:
- `HWUtilization` - aggregated GPU/CPU bars.
- `TopKernels` - pie/bar/table view of top kernels with node/GPU
  filter combos and a callback that notifies the rest of the UI when
  a kernel is selected.
- `KernelInstanceTable` (subclass of `InfiniteScrollTable`) - lists
  kernel instances and supports `ToggleSelectKernel`.

### `AnalysisView` (`rocprofvis_analysis_view.{h,cpp}`)

The bottom-right tabbed panel. Hosts:
- `m_tab_container` (`TabContainer`) with sub-tabs:
  - `EventsView` - per-event detail tab (basic info, ext data, flow,
    callstack, args).
  - `TrackDetails` - selected-track summary tab.
  - `MultiTrackTable` (event table) - cross-track event listing.
  - `MultiTrackTable` (sample table) - cross-track sample listing.
  - `AnnotationView` - sticky-note list.
- Listens to track / range / event selection events to keep tabs in
  sync.

### `EventsView` (`rocprofvis_events_view.{h,cpp}`)

Renders the currently-selected event(s). Subsections rendered via
`RenderBasicData / RenderEventExtData / RenderEventFlowInfo /
RenderCallStackData / RenderArgumentData`. Tracks a
`FlowHighlightState` so hovering an arg row in the flow table
highlights the linked event.

### `MultiTrackTable` (`rocprofvis_multi_track_table.{h,cpp}`)

`InfiniteScrollTable` subclass that aggregates rows across multiple
selected tracks. Supports `group_by_choices`, custom filter store, and
context menu copy.

### `TrackDetails` (`rocprofvis_track_details.{h,cpp}`)

Shows aggregated info per selected track, sourced from
`TrackTopology::GetTopology()`. `DetailItem` collects pointers into
the topology models (node, process, processor, queue, thread, stream,
counter) for the track, then renders an `InfoTable`.

### `Annotations` and `StickyNote`

- `AnnotationsManager` (`rocprofvis_annotations.{h,cpp}`) - holds the
  list of `StickyNote`s, the popup state for adding/editing, and the
  visibility flag. Persisted via `AnnotationsManagerProjectSettings`.
- `StickyNote` (`rocprofvis_stickynote.{h,cpp}`) - one note. Carries
  position (time + y_offset), size, text/title, and view-range
  metadata. `Render(draw_list, window_pos, tpt)`,
  `HandleResize(...)`, `HandleDrag(...)`. Edit handled via
  `kStickyNoteEdited` event.
- `AnnotationView` (`rocprofvis_annotation_view.{h,cpp}`) - the
  sub-tab in `AnalysisView` that lists notes and lets the user
  select/hide them.

### `TimelineSelection` (`rocprofvis_timeline_selection.{h,cpp}`)

Single source of truth for tracks/events/time-range selection plus
"highlighted" event state. Sends `kTimelineTrackSelectionChanged`,
`kTimelineTimeRangeChanged`, `kTimelineEventSelectionChanged`,
`kTimelineEventHighlightChanged` `RocEvent`s. `NavigateToEvent` is the
canonical "scroll to and highlight an event" entry point - call this
instead of poking the timeline yourself.

### `TimelineArrow` (`rocprofvis_timeline_arrow.{h,cpp}`)

Renders the flow arrows between events on different tracks. Two
`RenderStyle`s: `kFan` and `kChain`. Two `FlowDisplayMode`s:
`kShowAll` and `kHide`. `Render(draw_list, window, track_position_y,
graphs, tpt)` is called from `TimelineView::RenderTraceView`.

### Click / focus arbitration: `TimelineFocusManager`

`rocprofvis_click_manager.{h,cpp}` (named "click_manager" for
historical reasons; the class is `TimelineFocusManager`).

- `enum class Layer { kNone, kGraphLayer, kInteractiveLayer,
  kScrubberLayer, kCount }`.
- Layered widgets (graph, interactive overlay, scrubber) each call
  `RequestLayerFocus(layer)` when they detect a hover/click; the
  manager picks the topmost (`EvaluateFocusedLayer`) and other layers
  defer their click handling. **Use this for layered hit-testing; do
  not invent new "who got the click" logic.**
- A separate "right click" channel
  (`SetRightClickLayer/GetRightClickLayer/ClearRightClickLayer`)
  persists across frames so the right context menu wins when several
  layers overlap a right-click.

### `LoadingTimer`

Defined in `rocprofvis_timeline_view.h`. Tiny timer that delays
showing the loading indicator until a request takes longer than a
threshold. Use this pattern (don't show spinners immediately) for any
new long-running fetch.

## 10. Compute View Internals

All compute UI lives under `src/view/src/compute/` and is gated by
`#ifdef COMPUTE_UI_SUPPORT`. Always wrap new compute-only code with
this guard.

### `ComputeView : RootView` (`rocprofvis_compute_view.{h,cpp}`)

The compute analogue of `TraceView`. Owns:

- `m_compute_selection` (`shared_ptr<ComputeSelection>`) - the
  currently selected workload + kernel, and the only place that emits
  `kComputeWorkloadSelectionChanged` /
  `kComputeKernelSelectionChanged`.
- `m_preset_browser` (`unique_ptr<PresetBrowser>`) - load/save preset
  browser shared across compute sub-views.
- `m_tab_container` - sub-tabs:
  - `ComputeSummaryView` - workload-wide overview (top kernels,
    workload SOL, workload roofline).
  - `ComputeKernelDetailsView` - per-kernel deep-dive.
  - `ComputeTableView` - hierarchical metric tables.
  - `ComputeComparisonView` - baseline vs target comparison.
  - `ComputeWorkloadView` - system info + profiling config tables.
  - `ComputeTester` - dev-mode scratchpad
    (`#ifdef ROCPROFVIS_DEVELOPER_MODE`).
- `m_data_provider` - same `DataProvider` type as `TraceView`, but its
  `ComputeModel()` accessor exposes the compute data model.

`LoadTrace`, `CreateView`, `DestroyView`, `GetToolbar`,
`DetachProviderCleanup` mirror `TraceView`.

### `ComputeSelection` (`rocprofvis_compute_selection.{h,cpp}`)

Singleton-ish per-project. `SelectKernel(id)` and `SelectWorkload(id)`
are the only entry points. Always call these instead of mutating local
state - they emit the proper events. `INVALID_SELECTION_ID` is the
sentinel.

### `ComputeWorkloadView` (`rocprofvis_compute_workload_view.{h,cpp}`)

Shows the two static tables for a workload:
`RenderSystemInfo(WorkloadInfo)` and
`RenderProfilingConfig(WorkloadInfo)`. Layout uses an `HSplitContainer`.

### `ComputeKernelDetailsView` (`rocprofvis_compute_kernel_details.{h,cpp}`)

Per-kernel page. Composes:
- `m_memory_chart` (`ComputeMemoryChartView`) - the cache-hierarchy
  schematic with per-block metric overlays.
- `m_roofline` (`Roofline`, single-kernel mode).
- `m_kernel_metric_table` (`KernelMetricTable`) - the kernel listing
  with "add metric" workflow.
- `m_sol_table` (`MetricTableWidget`) - System Speed-of-Light (table
  ID = `METRIC_TABLE_SOL`).
- `m_flex_container` (`FlexContainer`) - row-flex layout that wraps
  when the window is narrow.

Handles `kComputeWorkloadSelectionChanged`,
`kComputeKernelSelectionChanged`, `kComputeMetricsFetched`,
`kNewTableData`, `kComputeShowMetricInKernelDetails` events.

### `Roofline` (`rocprofvis_compute_roofline.{h,cpp}`)

ImPlot-based roofline chart. Two modes:
`SingleKernel | AllKernels`. Internal models:
- `ItemModel` - one ceiling or intensity entry; carries `Type`,
  `SubType` union, `Info` union, `ParentInfo` union, label, weight.
- `PresetModel` - FP4/FP6/FP8/FP16/FP32/FP64 presets that toggle
  groups of items.
- `ApplyPreset(type)` switches the active preset.
- `RenderMenus(...)` draws the legend / options panels in
  `InsideTopLeft|TopRight|BottomLeft|BottomRight|Outside` placements.

### `ComputeMemoryChartView` (`rocprofvis_compute_memory_chart.{h,cpp}`)

Hand-laid block diagram of the GPU memory hierarchy. Each block
(LDS, VL1, SL1D, IL1, L2, Fabric, HBM, ...) is a `ChartBlock` with
`x/y/w/h` and helpers `Right/Bottom/MidX/MidY`. Renders metric values
inline via `DrawMetricRow`. The catalog of supported chart-only
metrics is `enum MemChartMetric` (maps 1:1 to entries in compute
metric table 3.1).

If you need to add a new memory-hierarchy block:
1. Add a `MemChartMetric` enum value (before `MEMCHART_METRIC_NA`).
2. Add a `Draw*` method.
3. Wire it into `ComputeLayout()` and `Render()`.

### `KernelMetricTable` (`rocprofvis_compute_kernel_metric_table.{h,cpp}`)

The interactive kernel list with sortable, filterable, optionally
bar-chart columns. Public:
- `FetchData(workload_id)`, `HandleNewData()`, `ClearData()`.
- `SetQuery(query_string)` and
  `SetExternalQuery(metric_id, value_name)` for the "Add Metric" flow
  (driven by `QueryBuilder`).
- Per-column filters (`ColumnFilter`) with `ApplyFilters` /
  `ClearAllFilters` / `ValidateFilterExpression`.
- Bar-chart columns (`m_bar_chart_columns`) computed via
  `ComputeColumnMaxValues`.
- Persistent state via the nested `Preset : PresetComponent`.

### `ComputeTableView` (`rocprofvis_compute_table_view.{h,cpp}`)

The hierarchical category-tab view. `RebuildTabs()` fills sub-tabs
from `AvailableMetrics::Category`/`Table`/`Entry`. Pinning is
delegated to `PinnedMetricTable`. Persistent via nested `Preset`.

### `ComputeComparisonView` (`rocprofvis_compute_comparison.{h,cpp}`)

Cross-workload / cross-kernel diff view. Notable nested types:
- `Table` - bespoke comparison table (`Row { id, entry, values_map,
  cells, display_props, tags, selected }`, `Column { Selection |
  MetricID | MetricName | Unit | Value }`, freeze rows/columns,
  filtering by tag bitset).
- `CategoryModel`, `PinnedModel` - state for the tabbed layout and
  the pinned-metrics table.
- `DifferenceGroup` - holds value+pct value pair for threshold
  highlighting. `UpdateDifferenceGroups` /
  `UpdateDifferenceHighlight` apply the user's threshold to flag
  cells red/green.
- Persistent via nested `Preset`.

### `ComputeSummaryView` and `ComputeTopKernels` (`rocprofvis_compute_summary.{h,cpp}`)

Workload-wide summary. `ComputeTopKernels` supports `Pie | Bar`
display modes, several `KernelInfo::DispatchMetric`s
(`InvocationCount`, `DurationTotal/Min/Max/Mean/Median`), and uses
`FlexContainer` for layout.

### `ComputeTester` (`rocprofvis_compute_tester.{h,cpp}`) - dev only

Internal scratchpad UI for exercising the metric / roofline APIs.
Behind `#ifdef ROCPROFVIS_DEVELOPER_MODE`. Not user-facing - keep
production code from depending on it.

### Compute data plumbing

- `ComputeDataProvider` (`rocprofvis_compute_data_provider.{h,cpp}`)
  is a separate provider used by the **older** dedicated compute UI
  flows. New code should use the unified `DataProvider` (which has
  `ComputeModel()`, `FetchMetrics`, `FetchMetricPivotTable`, and
  `SetFetchMetricsCallback`).
- `ComputeDataModel` (`model/compute/rocprofvis_compute_data_model.{h,cpp}`)
  holds `WorkloadInfo`, `KernelInfo`, `MetricValue` per
  `(store_id, kernel_id|workload_id)`.
- `compute_model_types.h` is the core type vocabulary:
  `AvailableMetrics::Entry/Table/Category`, `KernelInfo`,
  `WorkloadInfo`, `MetricValue`, `MetricId`, `MetricIdHash`,
  `ComputeTableInfo`, `Point`. Reuse these types whenever you handle
  metric IDs or roofline geometry - **do not reinvent metric
  identifiers**; use `MetricId::ToString()` etc.

## 11. UI Models (`src/view/src/model/`)

These are **view-side** caches/projections (not the canonical model in
`src/model/`). Do not include `src/view/src/model` headers from
`src/model` - it's a one-way dependency: view-models read from
controller results.

- `rocprofvis_common_defs.h` - `INVALID_UINT64_INDEX`. Use this
  constant instead of writing `std::numeric_limits<uint64_t>::max()`.
- `rocprofvis_model_types.h` - the **shared vocabulary** for the
  view layer. Defines:
  - `enum class GraphType { TYPE_LINECHART, TYPE_FLAMECHART }`,
    `struct TrackGraph`.
  - `union TopologyId` - 54-bit `id` + 10-bit `instance` packing.
  - `struct TrackInfo` (`id`, `track_type`, `min_ts/max_ts`,
    `num_entries`, names, `Topology` substruct, `graph_handle`).
  - `union TraceEventId` - 52/8/4-bit packing for `(event_id,
    event_node, event_op)`.
  - `struct EventInfo` and its parts: `BasicEventData`, `EventArg`,
    `EventExtData`, `EventFlowData`, `CallStackData`.
  - Topology types: `NodeInfo`, `DeviceInfo`, `ProcessInfo`,
    `IterableInfo`, `ThreadInfo`, `QueueInfo`, `StreamDeviceInfo`,
    `StreamInfo`, `CounterInfo`.
  - `struct SummaryInfo` with `KernelMetrics`, `GPUMetrics`,
    `CPUMetrics`, `AggregateMetrics`.
  - `struct TableInfo`, `FormattedColumnInfo`,
    `AnalysisQueueUtilization` (with state machine `kStale ->
    kPending -> kRequested -> kReady`).

- `rocprofvis_trace_data_model.{h,cpp}` - the **`TraceDataModel`
  facade**: aggregates `TopologyDataModel`, `TimelineModel`,
  `TablesModel`, `SummaryModel`, `EventModel`, `AnalysisModel`. Use
  `DataProvider::DataModel()` to access it.
- `rocprofvis_topology_model.{h,cpp}` - holds the eight
  `unordered_map<uint64_t, *Info>` (nodes, devices, processes,
  instrumented threads, sampled threads, queues, streams, counters)
  and helpers like `GetDeviceByInfoId`, `GetDeviceTypeLabel`,
  `TopologyToString` (debug).
- `rocprofvis_timeline_model.{h,cpp}` - `TimelineModel`: track
  metadata + raw track data + histogram + minimap. Use the typed
  raw-data helpers (`GetTrackData`, `FreeTrackData`,
  `FreeAllTrackData`) rather than touching the underlying maps.
- `rocprofvis_event_model.{h,cpp}` - `EventModel`: per-event
  `EventInfo` cache.
- `rocprofvis_summary_model.{h,cpp}` - `SummaryModel`: holds the
  computed `SummaryInfo::AggregateMetrics`.
- `rocprofvis_tables_model.{h,cpp}` - `TablesModel`: `enum class
  TableType { kSampleTable, kEventTable, kEventSearchTable,
  kSummaryKernelTable }` and the table cache addressed by it.
- `rocprofvis_analysis_model.{h,cpp}` - `AnalysisModel`: per-track
  queue-utilization cache, with the `AnalysisQueueUtilization` state
  machine.
- `rocprofvis_compute_data_model.{h,cpp}` and
  `rocprofvis_compute_model_types.h` (compute) - described above.

`TraceEvent` and `TraceCounter` (defined in
`rocprofvis_raw_track_data.h`) are the in-memory record types for
tracks. The chunked storage class is `TemplatedRawTrackData<T>` with
typedefs `RawTrackEventData` / `RawTrackSampleData`. **Re-use these,
do not invent new "list of events" structures.**

## 12. Cross-cutting Services

### `EventManager` (`rocprofvis_event_manager.{h,cpp}`)

In-process publish/subscribe bus for the view layer. Singleton via
`GetInstance()`. Two operations matter:

```cpp
SubscriptionToken token = EventManager::GetInstance()->Subscribe(
    static_cast<int>(RocEvents::kSomething),
    [this](shared_ptr<RocEvent> e){ HandleSomething(e); });

EventManager::GetInstance()->AddEvent(
    make_shared<TimeRangeSelectionChangedEvent>(start, end, source_id));
```

- **Always store `SubscriptionToken`s in members** named
  `m_<event>_token`, and unsubscribe in your destructor:
  `EventManager::GetInstance()->Unsubscribe(event_id, m_xxx_token)`.
- The bus uses **deferred dispatch**: `AddEvent` queues, then
  `DispatchEvents()` (called once per frame) drains. Don't rely on
  same-frame ordering with the dispatcher.
- Use the typed `RocEvent` subclasses in `rocprofvis_events.h`:
  `NavigationEvent`, `TrackDataEvent`, `TableDataEvent`,
  `StickyNoteEvent`, `ScrollToTrackEvent`, `TabEvent`,
  `TrackSelectionChangedEvent`, `TimeRangeSelectionChangedEvent`,
  `EventSelectionChangedEvent`, `EventHighlightChangedEvent`,
  `RangeEvent`, `RequestProgressUpdateEvent`,
  `ComputeSelectionChangedEvent`, `ComputeMetricsFetchedEvent`,
  `ComputeAddMetricToKernelDetailsEvent`. Add new event types here;
  do not pass payloads through the base `RocEvent`.
- Set `SetSourceId()` on every event you emit (typically the project
  ID or widget name) so subscribers can filter by source.

### `enum class RocEvents` - the canonical event IDs

The full list is in `rocprofvis_events.h`. Examples used widely:
`kNewTrackData`, `kNewTableData`, `kTabClosed`, `kTabSelected`,
`kTimelineTrackSelectionChanged`, `kTimelineTimeRangeChanged`,
`kTimelineEventSelectionChanged`, `kTimelineEventHighlightChanged`,
`kHandleUserGraphNavigationEvent`, `kTrackMetadataChanged`,
`kStickyNoteEdited`, `kFontSizeChanged`, `kSetViewRange`,
`kGoToTimelineSpot`, `kTimeFormatChanged`, `kTopologyChanged`,
`kRequestProgressUpdate`. Compute-only:
`kComputeWorkloadSelectionChanged`,
`kComputeKernelSelectionChanged`, `kComputeMetricsFetched`,
`kComputeShowMetricInKernelDetails`.

When adding a new event:
1. Add a new value at the end of `RocEvents` (don't reorder).
2. If it carries data, add a `RocEvent` subclass and a new
   `RocEventType` value.
3. Document the source ID convention in this file.

### `SettingsManager` (`rocprofvis_settings_manager.{h,cpp}`)

Application settings + theme. Singleton. **Read colors and fonts only
through this** - never hardcode `IM_COL32(...)` in feature code.

- `GetUserSettings()` -> `UserSettings` (display, units, "don't ask"
  flags). `ApplyUserSettings(old, save_json)` writes JSON to disk.
- `GetInternalSettings()` -> recent files (`MAX_RECENT_FILES = 5`).
  Use `AddRecentFile / RemoveRecentFile / ClearRecentFiles`.
- `GetAppWindowSettings()` -> show/hide flags (`show_toolbar`,
  `show_details_panel`, `show_sidebar`, `show_histogram`,
  `show_summary`).
- `GetColor(Colors)` returns an `ImU32`. The `Colors` enum lists
  every themed color (background tiers, accents, table borders,
  flame chart, line chart, ruler, scrubber, comparison highlight,
  minimap bins, sticky note, etc.). **If you need a new color, add
  it to the `Colors` enum and `ApplyColorStyling`** rather than
  hardcoding.
- `GetFontManager()` -> `FontManager`. Get fonts via
  `fonts.GetFont(FontType::kMainText | kIcon)` and sizes via
  `fonts.GetFontSize(FontSize::kSmall|kMedium|kMedLarge|kLarge)`.
- `GetEventLevelHeight()` / `GetEventLevelCompactHeight()` - the
  pixel height of one flame-chart row in normal / compact mode.
  **Reuse these instead of magic numbers.**

JSON keys are constants in this header
(`JSON_KEY_SETTINGS_DISPLAY_DARK_MODE` etc.).

### `FontManager` (`rocprofvis_font_manager.{h,cpp}`)

Owns the text font and the icon font. `Init()` runs after the ImGui
context is created. `GetDPIScaledFontIndex()` picks the right size
when DPI scaling is enabled.

### `HotkeyManager` (`rocprofvis_hotkey_manager.{h,cpp}`)

Singleton owning all rebindable shortcuts. Reads bindings from
settings JSON, writes them back via
`SettingsManager::SaveHotkeySettings`.

- `enum class HotkeyActionId` - every action has an ID
  (`kPanLeft`, `kZoomIn`, `kBookmarkSave0..9`,
  `kBookmarkRestore0..9`, `kMultiSelect`, `kRegionSelect`,
  `kSpeedBoost`, `kClearSelection`, `kToggleMark`, ...).
- `WasActionTriggered(action)` for press semantics,
  `IsActionHeld(action)` for hold semantics.
- `GetActionInfo(action)` returns metadata
  (`HotkeyActionInfo { display_name, category, key,
  default_binding, type, allow_repeat, active_during_text_input }`).
- Static helpers: `BookmarkSaveAction(i)`,
  `BookmarkRestoreAction(i)`, `KeyChordToString`,
  `StringToKeyChord`, `IsRebindableKey`.
- The Settings panel rebinding flow goes through
  `SetBinding/ResetBinding/FindConflictingAction/StealChord`.

**To add a new hotkey:**
1. Append to `HotkeyActionId` (before `kCount`).
2. Add `HotkeyActionInfo` with default key + category to the static
   table in the cpp.
3. Read it in your `Update()` via `WasActionTriggered`.

### `NotificationManager`

See section 7.8.

### `SettingsPanel` (`rocprofvis_settings_panel.{h,cpp}`)

The modal Settings UI. Categories: `Display`, `Units`, `Other`,
`Hotkeys`. Each category has a `Render*Options()` and
`Reset*Options()` pair. Reuse this rather than building per-feature
preferences UI.

### `PresetManager` and `PresetComponent` (`rocprofvis_presets.{h,cpp}`)

Save/load named on-disk presets per project. Used by Compute views
(comparison, table view, kernel metric table). To make a widget
preset-aware:

1. Define a nested `class Preset : public PresetComponent` and
   implement `ToJson(json) / FromJson(json) / Reset()`.
2. Construct it in your widget's ctor passing `(component_type,
   project_id)` and pass the project ID through. The base ctor
   registers with `PresetManager::GetInstance()`.
3. `~Preset()` (via the base) unregisters.

`PresetBrowser` is the floating popup that lists presets and binds
into the Compute toolbar.

### `Project` settings serialization

`Project::RegisterSetting(ProjectSetting*)` participates in `.rpv`
save/load. Implement `ToJson()` and `Valid()` on your subclass,
construct passing `project_id`, and the base ctor registers with
the owning `Project`. JSON keys for the system trace are listed in
`rocprofvis_project.h` (`JSON_KEY_GENERAL_*`,
`JSON_KEY_TIMELINE_*`, `JSON_KEY_ANNOTATION_*`).

### `rocprofvis_utils.{h,cpp}` - shared utilities

Use these instead of writing your own.

- `template<class T> class CircularBuffer<T>` - bounded FIFO with
  iterators.
- Time-string conversions:
  - `nanosecond_to_timecode_str(ns, condensed, round)`
  - `nanosecond_to_us_str / _ms_str / _s_str / _str(ns, include_units)`
  - `nanosecond_to_formatted_str(ns, TimeFormat, include_units)`
  - `nanosecond_str_to_formatted_str(str, offset_ns, fmt, units)`
  - `timeformat_sufix(fmt)`.
- `enum class TimeFormat { kTimecode, kTimecodeCondensed, kSeconds,
  kMilliseconds, kMicroseconds, kNanoseconds }`. Drives display
  formatting everywhere; do not hand-format ns values.
- `calculate_nice_interval(view_range, target_divisions)` and
  `fit_graph_axis_interval(...)` -> `FittedGraphAxisInterval` for
  axis ticks. Use these when building any new ruler/axis.
- `calculate_adaptive_view_range(item_start_ns, item_duration_ns)`
  -> `ViewRangeNS`. Use when navigating to an event so the chosen
  zoom looks right.
- `TimeConstants::ns_per_us / ns_per_ms / ns_per_s / minute_in_s /
  minute_in_ns`.
- `compact_number_format(double)` - SI-style "1.2K", "3.4M"
  formatting.
- `get_application_config_path(create_dirs)` -> the per-user config
  dir.
- `open_url(url)` - opens a browser. Reuse for "more info" links.
- `get_executable_name(full_path)`.
- `is_remote_display_session()` - detects SSH/X-forwarding for the
  file-dialog auto-selection logic.

### Icons (`src/view/src/icons/`)

- `rocprovfis_icon_defines.h` (note the typo - kept for stability)
  - `ICON_CHEVRON_DOWN/LEFT/RIGHT`, `ICON_X_CIRCLED`, `ICON_EYE`,
    `ICON_EYE_SLASH`, `ICON_EYE_THIN`, `ICON_GEAR`, `ICON_CHAIN`,
    `ICON_ADD_NOTE`, `ICON_ARROWS_SHRINK`, `ICON_COMPASS`,
    `ICON_CHART_BAR`, `ICON_CHART_PIE`, `ICON_ARCHIVE`,
    `ICON_DELETE`, `ICON_TREE`, `ICON_GRID`, `ICON_ARROW_UP/DOWN`,
    `ICON_EDIT`, `ICON_TRASH_CAN`, `ICON_OPEN`,
    `ICON_ARROWS_CYCLE`, `ICON_LIST`, `ICON_STICKY_NOTE`.
  - `icon_ranges[]` - the codepoint ranges added to the icon font
    glyph set.
- `rocprofvis_icon_data.h` - the embedded icon font binary blob.
- **Always reuse an existing icon constant** before adding a new
  glyph. Adding a new icon requires updating both files and the
  ranges array.

## 13. Data Flow: How a click becomes pixels

This is the canonical request lifecycle. Match this when adding new
data-driven UI.

```
User clicks on a track meta area
  -> TrackItem::RenderMetaArea sets m_meta_area_clicked
  -> TimelineSelection::SelectTrack(track_graph)
  -> TimelineSelection::SendTrackSelectionChanged(...)
        emits TrackSelectionChangedEvent (kTimelineTrackSelectionChanged)

EventManager dispatches next frame
  -> AnalysisView::HandleTimelineSelectionChanged subscribed -> reacts
  -> MultiTrackTable::HandleTrackSelectionChanged
        -> issues DataProvider::FetchMultiTrackEventTable(track_ids, ...)
              -> internally builds TableRequestParams with
                 RequestIdBuilder::MakeRequestId(...) and calls
                 controller's async fetch via rocprofvis_controller_*
              -> stores RequestInfo in m_requests

Controller worker thread fulfills the request
  -> Calls back via DataProvider's table_data_ready_callback
  -> DataProvider posts a TableDataEvent (kNewTableData)

EventManager dispatches next frame
  -> InfiniteScrollTable::HandleNewTableData picks it up
  -> FormatData / IndexColumns -> rows visible.
```

Things to honor in any new data path:

1. **Request IDs are packed.** Use `RequestIdBuilder::MakeRequestId`
   or `MakeTrackDataRequestId` / `MakeClientRequestId`. Do not invent
   raw IDs - they collide with the encoded fields.
2. **Long-running fetches must update progress.** Wire your widget
   into `SetRequestProgressUpdateCallback` and emit
   `RequestProgressUpdateEvent`s if you want progress in the UI.
3. **Do not block the render thread on a fetch.** Issue the request,
   show a `RenderLoadingIndicator`, return; respond when the
   completion event arrives.
4. **Handle navigation-away cleanup.** If a request can outlive the
   view that issued it, make it cancellable via
   `DataProvider::CancelRequest` or drain it through the provider
   cleanup path.
5. **Tab close and shutdown require asynchronous cleanup.** Override
   `RootView::DetachProviderCleanup()` to return any in-flight work;
   `AppWindow` will drain it asynchronously
   (`StartProviderCleanup` / `UpdateProviderCleanups`).

## 14. Coding Conventions

`CODING.md` is authoritative. Highlights you must follow:

### Source layout

- File names: `rocprofvis_<module>_<lower_snake_case>.h/.cpp`. Public
  C headers use the same `rocprofvis_` prefix.
- Header / source pair per class (small classes can share files when
  they form a cohesive unit, like dialogs).
- `#pragma once` in every header. No include guards.
- Every header forward-declares or includes its own deps. No header
  should require a particular include order.
- Include order: matching header, system, non-system external, local.
  Alphabetical within each group.
- All `src/` files start with the AMD copyright header:

```cpp
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
```

### Namespace

```cpp
namespace RocProfVis
{
namespace View
{
// ...
}  // namespace View
}  // namespace RocProfVis
```

Always close with the trailing comment. No `using namespace` in
headers. No anonymous namespaces in code intended to be linked
across translation units.

### Naming (cheat sheet, see `CODING.md` for the full table)

| Construct           | Form                                       |
|---------------------|--------------------------------------------|
| Class / Method      | `UpperCamelCase`                           |
| Interface class     | `IUpperCamelCase`                          |
| Member variable     | `m_lower_with_underscores`                 |
| Static member       | `s_lower_with_underscores`                 |
| Global              | `g_lower_with_underscores`                 |
| TLS                 | `tls_lower_with_underscores`               |
| Public C struct     | `rocprofvis_<thing>_t`                     |
| Public typedef/enum | `rocprofvis_<thing>_t`                     |
| Enum class          | `UpperCamelCase`                           |
| Enum class value    | `kUpperCamelCase`                          |
| Const variable      | `UPPER_WITH_UNDERSCORES`                   |
| Free function       | `lower_with_underscores`                   |
| Function parameter  | `lower_with_underscores`                   |
| Macro / `#define`   | `ROCPROFVIS_UPPER_WITH_UNDERSCORES`        |

### Language rules (non-exhaustive)

- C++17. Public C surfaces must be C11 (no C++ in headers crossing
  the ABI).
- Use `nullptr`, never `0` or `NULL` for pointers.
- Use fixed-width integers (`uint64_t`, `int32_t`, ...). Don't rely
  on `int`/`long` width.
- Replace magic numbers with named `inline constexpr` constants.
- Single-statement blocks must still use braces:
  ```cpp
  if(x)
  {
      do_thing();
  }
  ```
  Braces go on their own line for blocks.
- Prefer single `return` per function unless it complicates control
  flow.
- Don't throw exceptions. Return error codes, `optional`, or
  `pair<bool, T>`.
- Don't use `auto` except for iterators or RHS-typed initializers.
- Don't use `goto`.
- Prefer stack allocation. When you must heap-allocate, use
  `std::unique_ptr` or `std::shared_ptr`. If a function returns an
  owning raw pointer (rare), name it `alloc_*` and provide a
  `free_*` counterpart.
- `static_assert` for compile-time invariants. `ROCPROFVIS_ASSERT`
  for runtime invariants. Assertions must be side-effect free.
- Specify the storage class on enums when forward-declaring.
- Minimize template usage outside of containers. Avoid
  code-bloat-prone templates.
- Avoid preprocessor macros; prefer `inline constexpr` /
  `constexpr` functions.
- Don't make member variables public; provide accessors.

### Format and tooling

- `.clang-format` is the source of truth. Run it before submitting.
- `.clang-tidy` validates additional rules.
- Trailing newline at EOF. No other trailing whitespace.

## 15. Comment & Documentation Style

The codebase mixes plain `//` comments with Doxygen-style. Both are
acceptable; consistency within a class is the only requirement.

### Doxygen `@brief` blocks for public APIs

Used widely on `RocEvent` accessors, utilities, and model facades.
Example shape:

```cpp
/**
 * @brief Get the event ID, see the RocEvents enum for standard
 * event IDs.
 * @return int Event ID
 */
int GetId() const;
```

`@param`, `@return`, and `@brief` are the supported tags.

### Plain `/* ... */` blocks

Used for internal helpers and on `Project::Open` / controller C
APIs. Example:

```cpp
/*
* Loads the file into the controller or returns an error.
* @param controller The controller to load into.
* @param future The object that tells you when the file has been loaded.
* @returns kRocProfVisResultSuccess or an error code.
*/
rocprofvis_result_t rocprofvis_controller_load_async(
    rocprofvis_controller_t* controller, rocprofvis_controller_future_t* future);
```

### Inline comments

- Comment **why**, not **what**. The CODING.md rule is firm:
  no narrating obvious code.
- Document non-trivial state lifecycles (e.g. cleanup work, request
  ID encoding, focus arbitration).
- For each member variable in a complex struct, prefer a short
  trailing comment if the meaning isn't obvious. See
  `TimePixelTransform` and `TrackInfo` for examples.

### Don't add for AI

Do not add `// AI-generated` markers, edit-narration, or "TODO"
comments without context. Do not check in commented-out code blocks
- delete them.

### File-level header

Every new `src/` file starts with:

```cpp
// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT
```

There is no other required preamble. Do not duplicate the file path
or class name in a header banner.

## 16. Reuse Catalog: Before You Write a New X

The user's #1 rule for this repo is "do not duplicate code." Before
adding **anything** new, check this list and reuse if at all possible.

| You want to...                                | Use this                                                                                       |
|-----------------------------------------------|------------------------------------------------------------------------------------------------|
| Make a widget                                 | Subclass `RocWidget`, set `m_widget_name = GenUniqueName("Foo")`                              |
| Render a free-form callback inside a layout   | `RocCustomWidget(callback)`                                                                    |
| Lay out two panes with a draggable splitter   | `HSplitContainer` / `VSplitContainer`                                                          |
| Pin N items in a row that wrap                | `FlexContainer` with `FlexItem`s                                                               |
| Stack widgets vertically with fixed slots     | `VFixedContainer` with `LayoutItem`s                                                           |
| Build a tabbed area                           | `TabContainer`, with `EnableSendCloseEvent / SendChangeEvent` if children need to react        |
| Show OK/cancel modal                          | `AppWindow::ShowConfirmationDialog(title, message, on_confirm)`                                |
| Show an info modal                            | `AppWindow::ShowMessageDialog(title, message)`                                                 |
| Open a save/open file dialog                  | `AppWindow::ShowSaveFileDialog / ShowOpenFileDialog`                                           |
| Show a transient toast                        | `NotificationManager::GetInstance().Show(message, level)`                                      |
| Show progress for a long-running task         | `NotificationManager::GetInstance().ShowPersistent(id, msg, level)` + `UpdateProgress(id, ...)`|
| Show a spinner while loading                  | `RenderLoadingIndicator(color, ...)` and gate behind `LoadingTimer`                            |
| Render a glyph button                         | `IconButton(ICON_FOO, fonts.GetFont(FontType::kIcon), ...)`                                    |
| Render an X close button                      | `XButton(id, tooltip, settings)`                                                               |
| Render a heading                              | `SectionTitle(text, large, settings)`                                                          |
| Render a vertical separator                   | `VerticalSeparator(settings)`                                                                  |
| Get a themed color                            | `settings.GetColor(Colors::kSomething)` / `ThemeColor(settings, Colors::..., alpha)`           |
| Add a copyable text cell                      | `CopyableTextUnformatted(text, unique_id, COPY_DATA_NOTIFICATION)`                             |
| Truncate text with tooltip                    | `ElidedText(text, available_width, tooltip_width, alignment)`                                  |
| Themed tooltip                                | `BeginTooltipStyled / EndTooltipStyled` or `SetTooltipStyled(fmt, ...)`                        |
| Inline-editable label                         | `EditableTextField`                                                                            |
| Drive a virtualized table                     | Subclass `InfiniteScrollTable`                                                                 |
| Pick a metric (Compute)                       | `QueryBuilder` + `KernelMetricTable::SetExternalQuery`                                         |
| Display SOL / pinned compute metrics          | `MetricTable` / `PinnedMetricTable` / `MetricTableWidget`                                      |
| Add a bookmark or save view state             | The `Project` + `ProjectSetting` system, JSON keys in `rocprofvis_project.h`                   |
| Save user-customizable layouts (Compute)      | `PresetComponent` + `PresetManager` + `PresetBrowser`                                          |
| Convert ns to a display string                | `nanosecond_to_formatted_str(ns, settings_time_format, include_units)`                         |
| Compute axis ticks                            | `calculate_nice_interval` / `fit_graph_axis_interval`                                          |
| Map screen <-> time on the timeline           | `TimePixelTransform` (never recompute pixels-per-ns by hand)                                   |
| Get the application config dir                | `get_application_config_path(create_dirs)`                                                     |
| Open a URL in the user's browser              | `open_url(url)`                                                                                |
| Detect SSH / remote display                   | `is_remote_display_session()`                                                                  |
| Track selection state (system trace)          | `TimelineSelection` (call its setters, listen for the events it emits)                         |
| Track selection state (compute)               | `ComputeSelection`                                                                             |
| Find / scroll to a track                      | `TimelineView::ScrollToTrack(track_id)` or emit `ScrollToTrackEvent`                           |
| Move/zoom the timeline                        | `TimelineView::MoveToPosition(start, end, y, center)` or `SetViewableRangeNS`                  |
| Navigate to and highlight an event            | `TimelineSelection::NavigateToEvent(track_id, event_uuid, start_ns, dur_ns)`                   |
| Pub/sub between widgets                       | `EventManager::Subscribe(id, handler)` / `AddEvent(make_shared<XxxEvent>(...))`                |
| Add a hotkey                                  | Add to `HotkeyActionId`, declare info, read with `WasActionTriggered / IsActionHeld`           |
| Disambiguate clicks across timeline layers    | `TimelineFocusManager::RequestLayerFocus / EvaluateFocusedLayer`                               |
| Encode a request ID                           | `RequestIdBuilder::MakeRequestId(...)` or `MakeTrackDataRequestId(...)`                        |
| Generate a unique client ID                   | `IdGenerator::GetInstance().GenerateId()`                                                      |
| Get an index sentinel                         | `INVALID_UINT64_INDEX` (`view/src/model/rocprofvis_common_defs.h`)                             |
| Get a selection sentinel                      | `TimelineSelection::INVALID_SELECTION_ID` / `ComputeSelection::INVALID_SELECTION_ID`           |
| Reuse a glyph                                 | One of the `ICON_*` macros in `icons/rocprovfis_icon_defines.h`                                |
| Add a keyboard-driven label edit              | `EditableTextField` (used in `LineTrackItem::VerticalLimits`)                                  |
| Persist a per-project flag/value              | Subclass `ProjectSetting`, register in ctor                                                    |
| Render an embedded PNG/JPG                    | `EmbeddedImage(data, len)` then `Render(top_left, target_width)`                               |
| Allocate a renderer texture                   | `GuiTexture::CreateRGBA32(pixels, w, h)`                                                       |
| Wrap a constructor in compute UI guards       | `#ifdef COMPUTE_UI_SUPPORT` / `#endif`                                                         |
| Wrap dev-only UI                              | `#ifdef ROCPROFVIS_DEVELOPER_MODE` / `#endif`                                                  |

If a row above doesn't apply, search the source tree before writing
new code. The `widgets/` and the existing track items have wrappers
for nearly every common pattern.

## 17. Common Pitfalls

- **Forgetting to unsubscribe.** If you `Subscribe` to an event, store
  the token and `Unsubscribe` in your destructor. Otherwise the
  EventManager will dispatch into a dead `this`.
- **Hardcoded colors.** Always go through `SettingsManager::GetColor`
  / `ThemeColor`. Theme switching, contrast, and dark mode rely on it.
- **Hand-rolled hit testing on the timeline.** Use
  `TimelineFocusManager` (a.k.a. `rocprofvis_click_manager.h`).
- **Bypassing `TimelineSelection` / `ComputeSelection`.** They emit
  the events the rest of the UI relies on. Don't keep a private copy
  of "selected track" anywhere.
- **Inventing your own request ID.** Use `RequestIdBuilder` /
  `IdGenerator`. The encoded layout is parsed by the controller.
- **Adding a new modal popup.** Reuse `ConfirmationDialog`,
  `MessageDialog`, or `AppWindow::ShowConfirmationDialog`.
- **Adding a new toast.** Use `NotificationManager` (single source of
  truth for status messages).
- **Touching ImGui style by hand.** Use `PopUpStyle`,
  `PushComboStyles/PopComboStyles`, and the existing helpers, so the
  defaults survive in `SettingsManager::GetDefaultStyle()`.
- **Magic-number tracks heights / row heights.** Use
  `DEFAULT_TRACK_HEIGHT`, `SettingsManager::GetEventLevelHeight()`,
  `TableRowHeight()`.
- **Leaking native dialog state.** All file dialogs go through
  `AppWindow::ShowOpenFileDialog/ShowSaveFileDialog`. Don't call
  `nativefiledialog` or `ImGuiFileDialog` directly.
- **Spawning std::thread.** Long-running work goes through controller
  futures (preferred) or `std::async` futures held in
  `AppWindow::m_provider_cleanup_jobs`-style storage. Threads owned by
  random widgets are forbidden.
- **Including model headers from a widget.** The view depends on
  controller and view-side models only.

## 18. Quick Reference Index of Every UI Class

For fast lookup. Each entry: class -> file -> one-line role.

### Application skeleton

- `AppWindow` -> `rocprofvis_appwindow.h` -> Singleton; menus, tabs,
  dialogs, status bar, project lifecycle.
- `Project` / `ProjectSetting` -> `rocprofvis_project.h` -> Per-trace
  bundle and serialization protocol.
- `RootView` -> `rocprofvis_root_view.h` -> Base for `TraceView` /
  `ComputeView`.
- `FileFilter` (struct) -> `rocprofvis_appwindow.h` -> File-dialog
  filter spec.

### Top-level views

- `TraceView` -> `rocprofvis_trace_view.h` -> System-profile workspace.
- `SystemTraceProjectSettings` -> same -> Persists bookmarks.
- `ComputeView` -> `compute/rocprofvis_compute_view.h` -> Compute
  workspace.

### Timeline surface

- `TimelineView` -> `rocprofvis_timeline_view.h` -> Timeline grid +
  tracks + scrubber + interaction.
- `TimelineViewProjectSettings` -> same -> Persists per-track display
  and order.
- `LoadingTimer` -> same -> Debounce for the loading indicator.
- `ViewCoords` (struct) -> same -> `{ y, z, v_min_x, v_max_x }` for
  bookmarks.
- `TimePixelTransform` -> `rocprofvis_time_to_pixel.h` -> Time<->pixel
  affine mapping.
- `TimelineFocusManager` -> `rocprofvis_click_manager.h` -> Layered
  click/focus arbitration.
- `TimelineSelection` -> `rocprofvis_timeline_selection.h` ->
  Tracks/events/range selection state.
- `TimelineArrow` -> `rocprofvis_timeline_arrow.h` -> Flow arrow
  rendering, fan/chain styles.
- `Minimap` -> `rocprofvis_minimap.h` -> Density mini-map and viewport
  navigator.
- `TrackTopology` -> `rocprofvis_track_topology.h` -> Hierarchical
  topology builder + sidebar tree.
- `SideBar` -> `rocprofvis_sidebar.h` -> Topology tree renderer.

### Track items

- `TrackItem` -> `rocprofvis_track_item.h` -> Base for any timeline
  track (description + chart + resize handle).
- `Pill` -> same -> The small badge label inside a track meta area.
- `TrackProjectSettings` -> same -> Persists track height per project.
- `FlameTrackItem` -> `rocprofvis_flame_track_item.h` -> Flame chart
  for events; supports color modes and compact mode.
- `FlameTrackProjectSettings` -> same.
- `LineTrackItem` -> `rocprofvis_line_track_item.h` -> Line/box-plot
  for counter samples.
- `LineTrackProjectSettings` -> same.
- `LineTrackItem::VerticalLimits` -> same -> Editable Y-axis bounds.
- `HighlightYRange` -> same -> `{ min_limit, max_limit }`.
- `RawTrackData` / `TemplatedRawTrackData<T>` /
  `RawTrackEventData` / `RawTrackSampleData` ->
  `rocprofvis_raw_track_data.h` -> Chunked typed track storage.
- `TraceEvent` / `TraceCounter` -> same -> Record types.

### Analysis pane (system view)

- `AnalysisView` -> `rocprofvis_analysis_view.h` -> Tabbed bottom
  pane.
- `EventsView` -> `rocprofvis_events_view.h` -> Selected-event detail.
- `EventItem` (struct) -> same -> Cached state per visible event.
- `FlowHighlightState` (struct) -> same -> Hover state for flow
  arrows.
- `TrackDetails` -> `rocprofvis_track_details.h` -> Selected-track
  detail tab.
- `MultiTrackTable` -> `rocprofvis_multi_track_table.h` -> Cross-track
  event/sample table.
- `EventSearch` -> `rocprofvis_event_search.h` -> Toolbar event search.
- `AnnotationView` -> `rocprofvis_annotation_view.h` -> Sticky-note
  list tab.
- `AnnotationsManager` -> `rocprofvis_annotations.h` -> Sticky-note
  store + popups.
- `StickyNote` -> `rocprofvis_stickynote.h` -> One sticky note widget.

### Summary surface

- `SummaryView` -> `rocprofvis_summary_view.h` -> The "Summary" panel.
- `HWUtilization` -> same -> GPU/CPU utilization bars.
- `TopKernels` -> same -> Top-N kernels chart (pie/bar/table).
- `KernelInstanceTable` -> same -> Specialized
  `InfiniteScrollTable` for kernel instances.

### Settings, fonts, hotkeys, notifications

- `SettingsManager` -> `rocprofvis_settings_manager.h`.
- `UserSettings`, `DisplaySettings`, `UnitSettings`,
  `InternalSettings`, `AppWindowSettings`, `Colors` -> same.
- `FontManager`, `FontType`, `FontSize` -> `rocprofvis_font_manager.h`.
- `HotkeyManager`, `HotkeyActionId`, `HotkeyActionInfo`,
  `HotkeyBinding`, `ActionType` -> `rocprofvis_hotkey_manager.h`.
- `NotificationManager`, `Notification`, `NotificationLevel` ->
  `widgets/rocprofvis_notification_manager.h`.
- `SettingsPanel` -> `rocprofvis_settings_panel.h` -> Settings modal.
- `PresetManager`, `PresetComponent`, `PresetBrowser` ->
  `rocprofvis_presets.h`.

### Events / pubsub

- `EventManager` -> `rocprofvis_event_manager.h`.
- `RocEvent`, `RocEvents` (enum), `RocEventType` (enum) ->
  `rocprofvis_events.h`.
- Typed events in same file: `NavigationEvent`, `TrackDataEvent`,
  `TableDataEvent`, `StickyNoteEvent`, `ScrollToTrackEvent`,
  `TabEvent`, `TrackSelectionChangedEvent`,
  `TimeRangeSelectionChangedEvent`, `EventSelectionChangedEvent`,
  `EventHighlightChangedEvent`, `RangeEvent`,
  `RequestProgressUpdateEvent`, `ComputeSelectionChangedEvent`,
  `ComputeMetricsFetchedEvent`,
  `ComputeAddMetricToKernelDetailsEvent`.

### Data plumbing

- `DataProvider` -> `rocprofvis_data_provider.h` -> The view's
  controller bridge.
- `ProviderState` -> same.
- `DataProviderCleanupWork`, `DataProviderCleanupResult` -> same.
- `RequestType`, `RequestState`, `RequestInfo`, `RequestIdBuilder`,
  `IdGenerator`, `RequestParamsBase`, `TrackRequestParams`,
  `TableRequestParams`, `EventRequestParams`,
  `AnalysisQueueUtilizationRequestParams`,
  `MetricsRequestParams`, `ComputeTableRequestParams` ->
  `rocprofvis_requests.h`.

### View-side models

- `TraceDataModel` -> `model/rocprofvis_trace_data_model.h` -> Facade.
- `TopologyDataModel` -> `model/rocprofvis_topology_model.h`.
- `TimelineModel` -> `model/rocprofvis_timeline_model.h`.
- `EventModel` -> `model/rocprofvis_event_model.h`.
- `SummaryModel` -> `model/rocprofvis_summary_model.h`.
- `TablesModel`, `TableType` -> `model/rocprofvis_tables_model.h`.
- `AnalysisModel` -> `model/rocprofvis_analysis_model.h`.
- `INVALID_UINT64_INDEX` -> `model/rocprofvis_common_defs.h`.
- `TrackInfo`, `EventInfo`, `BasicEventData`, `EventArg`,
  `EventExtData`, `EventFlowData`, `CallStackData`, `NodeInfo`,
  `DeviceInfo`, `ProcessInfo`, `IterableInfo`, `ThreadInfo`,
  `QueueInfo`, `StreamInfo`, `StreamDeviceInfo`, `CounterInfo`,
  `SummaryInfo` (with `KernelMetrics`/`GPUMetrics`/`CPUMetrics`/
  `AggregateMetrics`), `TableInfo`, `FormattedColumnInfo`,
  `TraceEventId`, `TopologyId`, `TrackGraph`, `GraphType`,
  `AnalysisQueueUtilization` -> `model/rocprofvis_model_types.h`.

### Compute UI (`#ifdef COMPUTE_UI_SUPPORT`)

- `ComputeSelection` -> `compute/rocprofvis_compute_selection.h`.
- `ComputeWorkloadView` -> `compute/rocprofvis_compute_workload_view.h`.
- `ComputeKernelDetailsView` -> `compute/rocprofvis_compute_kernel_details.h`.
- `Roofline` -> `compute/rocprofvis_compute_roofline.h`.
- `ComputeMemoryChartView`, `ChartBlock`, `MemChartMetric` ->
  `compute/rocprofvis_compute_memory_chart.h`.
- `KernelMetricTable` (+ nested `Preset`, `MetricInfo`,
  `ColumnFilter`) -> `compute/rocprofvis_compute_kernel_metric_table.h`.
- `ComputeTableView` (+ nested `Preset`) ->
  `compute/rocprofvis_compute_table_view.h`.
- `ComputeComparisonView` (+ nested `Table`, `Preset`,
  `CategoryModel`, `PinnedModel`, `DifferenceGroup`) ->
  `compute/rocprofvis_compute_comparison.h`.
- `ComputeSummaryView`, `ComputeTopKernels` ->
  `compute/rocprofvis_compute_summary.h`.
- `ComputeTester` (dev only) ->
  `compute/rocprofvis_compute_tester.h`.
- `ComputeDataProvider`, `ComputeTableModel`, `ComputeTableCellModel`,
  `ComputePlotModel`, `ComputePlotAxisModel`, `ComputePlotSeriesModel`,
  `ComputeMetricModel` -> `compute/rocprofvis_compute_data_provider.h`.
- `ComputeDataModel`, `ComputeKernelSelectionTable` ->
  `model/compute/rocprofvis_compute_data_model.h`.
- `AvailableMetrics`, `KernelInfo`, `WorkloadInfo`, `MetricValue`,
  `MetricId`, `MetricIdHash`, `Point`, `ComputeTableInfo` ->
  `model/compute/rocprofvis_compute_model_types.h`.

### Widget library (`src/view/src/widgets/`)

- `RocWidget`, `LayoutItem`, `RocCustomWidget`, `TabItem`,
  `PopUpStyle`, `WithPadding`, `CopyableTextUnformatted`,
  `COPY_DATA_NOTIFICATION` -> `rocprofvis_widget.h`.
- `ConfirmationDialog`, `MessageDialog` -> `rocprofvis_dialog.h`.
- `VFixedContainer`, `SplitContainerBase`, `HSplitContainer`,
  `VSplitContainer` -> `rocprofvis_split_containers.h`.
- `FlexItem`, `FlexContainer` -> `rocprofvis_flex_container.h`.
- `TabContainer` -> `rocprofvis_tab_container.h`.
- `RenderLoadingIndicator`, `LoadingIndicatorCentering`,
  `IconButton`, `XButton`, `SectionTitle`, `VerticalSeparator`,
  `ElidedText`, `Alignment`, `CenterNextItem`, `InputTextWithClear`,
  `BeginTooltipStyled`, `BeginItemTooltipStyled`,
  `EndTooltipStyled`, `SetTooltipStyled`, `ApplyAlpha`, `ThemeColor`,
  `GetResponsiveWindowSize`, `PushComboStyles/PopComboStyles`,
  `TableRowHeight`, `IsMouseReleasedWithDragCheck`,
  `DrawInternalBuildBanner` -> `rocprofvis_gui_helpers.h`.
- `GuiTexture`, `EmbeddedImage` -> `rocprofvis_image_helpers.h`.
- `Notification`, `NotificationLevel`, `NotificationManager` ->
  `rocprofvis_notification_manager.h`.
- `InfiniteScrollTable` -> `rocprofvis_infinite_scroll_table.h`.
- `EditableTextField` -> `rocprofvis_editable_textfield.h`.
- `QueryBuilder` -> `rocprofvis_query_builder.h`.
- `MetricTableBase`, `MetricTable`, `PinnedMetricTable`,
  `MetricTableWidget`, `WorkloadMetricTableWidget` ->
  `rocprofvis_compute_widget.h`.
- `DebugWindow` (dev only) -> `rocprofvis_debug_window.h`.

### Tree types and constants

- `TreeNode`, `LeafNode`, `NodeType`, `SidebarTree` ->
  `rocprofvis_tree_node.h`.
- `INVALID_TIME_NS` -> `rocprofvis_annotations.h`.
- `INVALID_UINT64_INDEX` -> `model/rocprofvis_common_defs.h`.

### Utilities

- `CircularBuffer<T>`, time conversions, `TimeFormat`,
  `calculate_nice_interval`, `fit_graph_axis_interval`,
  `calculate_adaptive_view_range`, `TimeConstants`,
  `compact_number_format`, `get_application_config_path`,
  `open_url`, `get_executable_name`, `is_remote_display_session` ->
  `rocprofvis_utils.h`.
- Icon constants and `icon_ranges[]` ->
  `icons/rocprovfis_icon_defines.h`, `icons/rocprofvis_icon_data.h`.

---

**End of AGENTS.md.** When you change the UI, update the section that
covers the area you touched (especially the index in section 18).
Keep this file the single source of truth for "where does X live"
and "what should I reuse instead of writing it." If you find a
duplicate of something listed here, prefer to delete the duplicate
and route callers through the canonical entry.
