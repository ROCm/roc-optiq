# Architecture

How the layers fit together, and how Optiq is built, tested, shipped, and documented. Read the matching section before a cross-layer, build, test, CI, or packaging change.

## Layers and boundaries

Each layer is a CMake target that calls only the layer below it.

**Code:** `src/`, `CMakeLists.txt`

**Related:** [Data flow](#data-flow) (a request through every layer), [Controller overview](CONTROLLER.md#overview) and [Database overview](DATABASE.md#overview) (ownership, threads, locks)

| Layer | Target | Owns | May include |
|---|---|---|---|
| `src/app/` | `roc-optiq` | window, frame loop, backend, CLI | `src/view/inc/`, `src/core/` |
| `src/view/` | built into each executable | ImGui UI, view models | controller `inc/`, `src/core/`, the shared type headers |
| `src/controller/` | `roc-optiq-controller` | C ABI, jobs, caches | `rocprofvis_c_interface.h`, `src/core/` |
| `src/model/` | `datamodel` | trace I/O, SQL, in-memory trace | `src/core/`, `rocprofvis_controller_enums.h` |
| `src/core/` | `roc-optiq-core` | asserts, profiling, logs, strings | spdlog |
| `src/python/` | `roc-optiq-python` | embedded CPython | `src/core/`; only the controller links it |

- One stack serves both trace kinds: `rocprofvis_db_identify_type` picks the adapter by probing the file, the controller builds a `SystemTrace` or `ComputeTrace`, and the view a `TraceView` or `ComputeView`.
- Two unrelated Python stacks: CFFI bindings in `src/model/python/` drive the model ABI from outside the app; in-app scripting (`src/controller/src/python/`, `src/python/`) wraps the controller ABI.
- Elsewhere: `thirdparty/` (vendored libraries), `resources/` (icons, memory-chart layouts), `sample/` (test traces), `docs/` (ReadTheDocs), and build, installer, and CI files in `cmake/`, `wix/`, `docker/`, `.github/`.

Trace formats ([detection rules](DATABASE.md#database-adapters-and-trace-formats)):

- `.rpd`: legacy rocpd, read by `RocpdDatabase`.
- `.db`: a rocprofiler-sdk system trace for `RocprofDatabase`, or a rocprof-compute trace for `ComputeDatabase`.
- `.yaml`: a multinode manifest of rocpd files, one trace in `RocprofDatabase`.
- `.json`, `.proto`, `.pftrace`: Chrome or Perfetto via `GoogleTraceProcessor`, only with `ROCPROFVIS_PERFETTO_ENABLED`.
- `.rpv`: an Optiq project (trace path, view state), read only by the view's `Project`.

## App shell and core

The executable around the UI, and the small library every layer links.

**Code:** `src/app/`, `src/core/`, `src/view/inc/rocprofvis_view_module.h`

**Related:** [App window and projects](UI.md#app-window-and-projects) (what `rocprofvis_view_render` drives), [Tests](#tests) (the harness shares this shell)

- `main.cpp` and `src/app/test/test_main.cpp` each own a frame loop; shared startup goes in `rocprofvis_app_shell.{h,cpp}`. New app-to-view entry points go in `rocprofvis_view_module.h`, and `RocProfVis::Platform` helpers never call the view.
- The loop is lazy: after input it renders `RENDER_FRAMES_AFTER_INPUT` frames, then sleeps unless `rocprofvis_view_wants_continuous_render()` is true. Work that advances without input must ask `RenderScheduler` for frames, or it stalls until the mouse moves.
- `--backend auto` tries Vulkan and falls back to OpenGL; the fallback can recreate the window, so callbacks are installed after init. macOS uses bundled MoltenVK; Linux forces GLFW onto X11 so viewports can be positioned.
- `add_common_cli_options` registers the flags both executables take: `-f/--file`, `-b/--backend auto|vulkan|opengl`, `-d/--file-dialog auto|native|imgui` (`auto` picks ImGui over SSH or a forwarded display), `-v/--version [hash]` (exits only when alone), and `-h/--help`. `main.cpp` adds `-r/--drag-repair on|off` in Linux multi-window builds, and the harness `-t/--run-tests`.
- The `ROCPROFVIS_ASSERT*` macros (`rocprofvis_core_assert.h`) expand to several statements, so brace the `if` around one. With asserts compiled out, the `_RETURN` and `_BREAK` forms still return or break.
- `PROFILE` timers need CMake `-DTEST=ON`, which reaches only core and model.

## Data flow

One request end to end: the View asks and returns, a controller job completes a future, and an event brings the result back a frame later.

**Code:** `src/view/src/rocprofvis_data_provider.{h,cpp}`, `src/controller/src/rocprofvis_controller.cpp`

**Related:** [Data provider](UI.md#data-provider) (callbacks, cleanup), [Events](UI.md#events) (source IDs), [C ABI and properties](CONTROLLER.md#c-abi-and-properties) (wait, cancel), [Memory and segments](CONTROLLER.md#memory-and-segments) (array ownership), [AppMonitor](UI.md#appmonitor) (SSH and profiler work)

1. A track click calls `TimelineSelection::SelectTrack`, which queues a `TrackSelectionChangedEvent` that `AppWindow::Update` dispatches next frame.
2. `MultiTrackTable::HandleTrackSelectionChanged` flags a fetch, and the table's next `Update` calls `DataProvider::FetchTable`. The provider allocates a future and an array, calls `rocprofvis_controller_table_fetch_async`, records a `RequestInfo`, and returns.
3. The C entry point validates handles with `Reference<>` typedefs and issues a `JobSystem` job. The job has the model build and run the SQL (`rocprofvis_db_build_table_query`, `rocprofvis_db_execute_query_async`) on a model thread, waits, and copies the rows into the array.
4. On the UI thread, `DataProvider::Update` polls pending futures each frame with a zero timeout. The finished request is copied into `TablesModel`, its array freed, and the ready callback posts `kNewTableData`.
5. Next frame, `InfiniteScrollTable::HandleNewTableData` picks it up and the rows draw.

- Timeline tracks take the same shape through `DataProvider::FetchTrack` and `rocprofvis_controller_graph_fetch_async`: the job picks a level of detail, `Track::FetchSegments` loads only uncached segments with `rocprofvis_db_read_trace_slice_async` into pooled `MemoryManager` objects, and `kNewTrackData` reaches `TimelineView::HandleNewTrackData`.
- A cancel runs down the same path: `DataProvider::CancelRequest` cancels the controller future, which cancels the model futures its job registered, and the result must come back as `kRocProfVisResultCancelled`, never as empty data, so the view asks again.

## Build and feature flags

Presets, the `ROCPROFVIS_*` options that switch features on, and where a new source file is registered.

**Code:** `CMakeLists.txt`, `CMakePresets.json`, `src/controller/CMakeLists.txt`, `thirdparty/CMakeLists.txt`

**Related:** [Tests](#tests) (targets behind a flag), [BUILDING.md](../BUILDING.md) (prerequisites, submodules)

- Configure presets build into `build/<preset>`: `x64-debug`, `x64-release`, `x64-release-symbols`, `linux-debug`, `linux-release`, `linux-release-symbols`, `macos-debug`, `macos-release`, `macos-release-symbols`. Build presets have display names such as `Windows Release Build`, and Visual Studio puts binaries in a per-configuration subfolder. `x64-debug` and `macos-debug` also set `ROCPROFVIS_DEVELOPER_MODE`; `linux-debug` does not.

| Option, default OFF | Turns on |
|---|---|
| `ROCPROFVIS_DEVELOPER_MODE` | Developer Options menu, debug output, `ComputeTester` tab |
| `ROCPROFVIS_ENABLE_INTERNAL_BANNER` | the internal-build banner |
| `ROCPROFVIS_ENABLE_PROFILER` | [Profiler launch](FEATURES.md#profiler-launch) |
| `ROCPROFVIS_ENABLE_REMOTE` | [Remote SSH](FEATURES.md#remote-ssh), libssh2 |
| `ROCPROFVIS_ENABLE_AGENTIC_PROFILING` | [Ask Optiq](FEATURES.md#ask-optiq), cpp-httplib |
| `ROCPROFVIS_ENABLE_SCRIPTING` | [Embedded Python](SCRIPTING.md#overview-and-status) |
| `ROCPROFVIS_ENABLE_VENDOR_PYTHON` | ON with scripting: a pinned CPython, else the build machine's |
| `ROCPROFVIS_ENABLE_TRACE_COMPARE` | [Trace compare](FEATURES.md#trace-compare) |
| `ROCPROFVIS_MULTI_WINDOW` | ImGui platform viewports, handled only in `main.cpp` |
| `ROCPROFVIS_ENABLE_UI_TESTS` | `roc-optiq-tests` |

- Each feature option appends its `-D` define to `CMAKE_CXX_FLAGS` in the root `CMakeLists.txt` before any subdirectory, so every layer and the controller's public headers see it. `ROCPROFVIS_ENABLE_VENDOR_PYTHON` and `ROCPROFVIS_ENABLE_UI_TESTS` stay in CMake.
- Feature sources sit only in `if()` blocks (`list(APPEND VIEW_FILES ...)`, `list(APPEND rocprofvis_controller_source_files ...)`), and other code reaches them under `#ifdef`. Remote profiling needs both `ROCPROFVIS_ENABLE_PROFILER` and `ROCPROFVIS_ENABLE_REMOTE`.
- Developer-mode files always compile; only their call sites sit under `#ifdef ROCPROFVIS_DEVELOPER_MODE`.
- A new flag also gets an `exp_enable_*` input in `ci-controller.yml`, passed to the platform workflows, and a `CHANGES.md` entry under Experimental.
- Outside the `ROCPROFVIS_*` options: `USE_NATIVE_FILE_DIALOG` (ON) adds nfd beside the always-linked `ImGuiFileDialog`, and `PERFETTO_AVAILABLE`, set when prebuilt trace-processor libraries exist, defines `ROCPROFVIS_PERFETTO_ENABLED`.

| New `.cpp` in | Register it in |
|---|---|
| `src/view/src/` | `VIEW_FILES` in the root `CMakeLists.txt`, or its feature block |
| `src/app/src/` | `APP_FILES` in the root; `main.cpp` and `test_main.cpp` are per executable |
| `src/controller/src/` | `rocprofvis_controller_source_files`, or its feature block |
| `src/model/src/{common,datamodel,database}/`, `src/core/src/` | nothing: globbed with `CONFIGURE_DEPENDS`; subfolders are not |
| `src/python/src/` | `src/python/CMakeLists.txt` |

## Tests

Catch2 suites for the model and controller, a layout parser test, and a Dear ImGui Test Engine harness.

**Code:** `src/controller/tests/`, `src/model/src/tests/`, `src/app/test/`, `src/view/test/`

**Related:** [Controller overview](CONTROLLER.md#overview) and [Database overview](DATABASE.md#overview) (what the suites cover)

- `roc-optiq-controller-system-tests` and `datamodel-system-tests` open `trace_70b_1024_32.rpd` and `rocpd-transpose.db`; `roc-optiq-controller-compute-tests` and `datamodel-compute-tests` open `rocprof_compute_23ed6f36.db`; `roc-optiq-memory-chart-tests` parses every embedded layout. `roc-optiq-controller-profiler-tests` needs `ROCPROFVIS_ENABLE_PROFILER`, `roc-optiq-controller-script-tests` needs `ROCPROFVIS_ENABLE_SCRIPTING`, and `roc-optiq-tests` needs `ROCPROFVIS_ENABLE_UI_TESTS`.
- All need `BUILD_TESTING` (ON). Run `ctest --output-on-failure` in `build/<preset>` (plus `-C` and the configuration on Visual Studio); trace suites run serially from the source root.
- The system suites run database cleanup on the tracked `sample/` trace in place; check `git status` before committing.
- `roc-optiq-tests` recompiles `APP_FILES` and `VIEW_FILES` with `IMGUI_ENABLE_TEST_ENGINE` and enters through `src/app/test/test_main.cpp`, so production `main.cpp` never sees the engine.
- Register UI tests in `RegisterAppTests` (`src/app/test/app_tests.cpp`) as `sys_*`, `compute_*`, or `common_*`. Each CTest entry runs them all on one sample, so start with `GetTraceViewOrSkip` or `GetComputeViewOrSkip`, which pass on the wrong trace type. `--run-tests` fails if any test fails or never runs.
- Read private state through the peers in `src/view/test/rocprofvis_view_test_access.h` (such as `TraceViewTestPeer`), never test-only accessors in production classes. Canvas-drawn items need `IMGUI_TEST_ENGINE_ITEM_ADD` under `#ifdef IMGUI_ENABLE_TEST_ENGINE`, as in `FlameTrackItem`. CI runs UI tests on Ubuntu only, under `xvfb-run -a`.

## Packaging and CI

Installers, CI, and the user-facing files that change with code.

**Code:** `CMakeLists.txt` (install and CPack), `cmake/`, `wix/`, `resources/mac/`, `.github/workflows/`, `docs/`, `CHANGES.md`

**Related:** [Build and feature flags](#build-and-feature-flags) (what experimental CI turns on)

- Linux: CPack `TGZ` plus DEB or RPM by host distro, from the `RV_RUNTIME` component (package name `ROCM_PACKAGE_NAME`); releases are ManyLinux builds from the manual `release-controller.yml`.
- Windows: the flat install, wrapped in an MSI from `wix/roc-optiq.wxs` by the `PACKAGE_WIX` target when `wix.exe` is found.
- macOS: a `MACOSX_BUNDLE` app with the Vulkan loader and MoltenVK (`ROCPROFVIS_BUNDLE_VULKAN`), signed when `ROCPROFVIS_MACOS_CODESIGN_IDENTITY` is set.
- A package must not depend on the build machine: CMake stages runtime libraries (OpenSSL, the vendored CPython) into the install tree or bundle, and so must a new one.
- `ci-controller.yml` (push, pull request, nightly, manual) calls `ci-ubuntu.yml`, `ci-redhat.yml`, `ci-manylinux.yml`, `ci-mac.yml`, `ci-windows.yml`, and `ci-asan.yml`; nightly and opted-in manual runs add `-experimental` variants with every in-development flag on.
- With the code, update `docs/reference/keyboard-shortcuts.rst` (hotkeys), `docs/reference/cli-support.rst` (CLI flags), `docs/how-to/` (workflows), and `CHANGES.md` (user-visible changes, experimental ones under their flags).

## Maintaining the book

How the agent docs are laid out and kept small and true.

**Code:** `AGENTS.md`, `.agents/`, `.cursor/rules/reuse-before-write.mdc`

**Related:** [CODING.md](../CODING.md) (style rules the book never repeats)

- `AGENTS.md` is always loaded: rules, the **Already exists** table, and a table of contents linking to sections. Seven guides in `.agents/` are read on demand, one `##` section per component: this file, [STYLING](STYLING.md), [UI](UI.md), [FEATURES](FEATURES.md), [CONTROLLER](CONTROLLER.md), [DATABASE](DATABASE.md), [SCRIPTING](SCRIPTING.md). `.cursor/rules/reuse-before-write.mdc` is an always-applied Cursor rule for search-first.
- Section template: scope sentence, `**Code:**`, `**Related:**` with reasons, checkable rules, how it works, `**Reuse:**`, and `**Recipe:**` for common changes; `###` only inside large sections, tables only for short enumerable facts.
- Budgets: `AGENTS.md` near 1,300 words, a guide under about 4,500, a section 150 to 800. Past that, split the file and add TOC lines to `AGENTS.md`.
- Fix the owning section in the same change. A new reusable helper gets a **Reuse** bullet there, and a root row only if agents would otherwise reinvent it. After renaming or deleting a symbol, file, or flag, run `rg -n OldName AGENTS.md .agents` and fix every hit.
