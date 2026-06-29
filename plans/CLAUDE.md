# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ROCm Optiq (roc-optiq) is a cross-platform visualizer for ROCm Profiler Tools. It provides visualization and analysis capabilities for:
- ROCm Systems Profiler trace data (*.db*, *.rpd* files)
- ROCm Compute Profiler analysis data
- Project files (*.rpv*) containing customizations, bookmarks, and annotations

The application is built with C++17 using Dear ImGui for the UI and Vulkan for rendering, with fallback to OpenGL and software rendering modes.

## Build System

### CMake Presets

Use CMake presets for configuration and building:

**Windows:**
```powershell
# Configure
cmake --preset "x64-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
# Build
cmake --build build/x64-release --preset "Windows Release Build" --parallel 4
```

Available Windows presets: `x64-release`, `x64-release-symbols`, `x64-debug`

**Linux:**
```bash
# Configure
cmake -B build/linux-release --preset "linux-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
# Build
cmake --build build/linux-release --preset "Linux Release Build" --parallel 4 --target package
```

Available Linux presets: `linux-release`, `linux-release-symbols`, `linux-debug`

**macOS:**
```bash
# Configure
cmake -B build/macos-release --preset "macos-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
# Build
cmake --build build/macos-release --preset "macOS Release Build" --parallel 4
```

Available macOS presets: `macos-release`, `macos-release-symbols`, `macos-debug`

### Build Options

- `COMPUTE_UI_SUPPORT=ON` (default): Build with ROCprof Compute analysis support
- `ROCPROFVIS_DEVELOPER_MODE=OFF` (default): Enable developer mode features
- `ROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF`: Show diagonal internal build banner
- `USE_NATIVE_FILE_DIALOG=ON` (default): Use OS native file dialog (automatically disabled on macOS)
- `GLFW_LINK_STATIC=ON` (default): Link GLFW statically

## Architecture

The codebase follows a modular MVC-like architecture with clear separation of concerns:

### Core Components

**src/app/** - Application entry point and main window initialization

**src/core/** - Core library with fundamental data structures and utilities
- Contains low-level utilities used across all modules
- Public API headers in `inc/`

**src/model/** - Data model layer
- SQLite-based database access layer for trace file parsing
- Supports two schema types: `kRocpdSqlite` and `kRocprofSqlite`
- Layered data storage with interface to access data properties
- Asynchronous database operations using future/promise pattern
- Public API in `inc/`
- Can be tested standalone (see `src/model/README.md`)
- Provides memory management for time slices, event properties, and tables

**src/controller/** - Business logic layer (Controller pattern)
- Bridges the model and view layers
- Handles trace loading, timeline management, event processing
- Data transformations and aggregations
- Systems profiler features: events, samples, tracks, timelines, flow control, call stacks
- Compute profiler features (when `COMPUTE_UI_SUPPORT=ON`): workloads, kernels, metrics, rooflines
- Public API in `inc/`
- Uses job system for asynchronous operations

**src/view/** - User interface layer (ImGui-based)
- Timeline visualization with tracks, events, and samples
- Event tables, sample tables, analysis views
- Annotations, bookmarks, and search functionality
- Compute analysis views: summary, kernel details, rooflines, memory charts
- Rendering backends: Vulkan (primary), OpenGL (fallback), software rendering

**src/qt_view/** - Legacy Qt-based view (appears to be deprecated)

### Data Flow

1. **Model layer** reads trace files from disk using SQLite queries
2. **Database interface** provides asynchronous API with future objects for long-running operations
3. **Controller layer** transforms raw data into application-specific structures (tracks, events, timelines)
4. **View layer** renders the data using ImGui widgets and custom rendering

### Architecture Details

**Controller-View Interface (C ABI):**
- The Controller exposes a pure C API defined in `src/controller/inc/rocprofvis_controller.h`
- Functions follow C11 standard with `extern "C"` linkage (e.g., `rocprofvis_controller_alloc()`, `rocprofvis_controller_load_async()`)
- This ABI boundary allows the Controller to be a separate module that could be replaced or tested independently
- The View layer is NOT allowed to directly instantiate Controller C++ classes - all interaction goes through the C API

**DataProvider Pattern:**
- The `DataProvider` class (`src/view/src/rocprofvis_data_provider.h`) is the abstraction layer in the View that wraps Controller C API calls
- DataProvider manages the lifetime of controller objects (`rocprofvis_controller_t*`, futures, arrays, etc.)
- It provides C++ convenience methods and callbacks to decouple View widgets from raw C API calls
- All async operations use the Controller's future-based pattern (`rocprofvis_controller_future_t*`)
- Callbacks registered with DataProvider notify View widgets when data is ready (e.g., `SetTrackDataReadyCallback()`, `SetTableDataReadyCallback()`)

**EventManager Scope:**
- `EventManager` and `RocEvent` classes (`src/view/src/rocprofvis_event_manager.h`, `rocprofvis_events.h`) are **only for inter-view messaging**
- Events are used for communication between View widgets (e.g., tab selection, timeline events, kernel selection changes)
- Events do NOT cross the View-Controller boundary
- Controller callbacks to View use `std::function<>` callbacks, not events

**Correct Communication Pattern:**
```
View Widget → DataProvider → Controller C API → Async Job → Controller Callback → DataProvider Callback → View Widget Update
```

**Example Flow:**
1. User clicks "Load Trace" in AppWindow
2. AppWindow calls `DataProvider::FetchTrace(controller, file_path)`
3. DataProvider calls `rocprofvis_controller_load_async(controller, future)` (C API)
4. Controller submits job to JobSystem, returns immediately
5. When load completes, Controller invokes completion callback
6. DataProvider receives callback, updates internal state, invokes `m_trace_data_ready_callback`
7. AppWindow's registered callback is invoked, updates UI

### Key Patterns

**Asynchronous Database Access:**
- Database operations use futures: `rocprofvis_db_future_alloc()`, `rocprofvis_db_future_wait()`
- Metadata loading: `rocprofvis_db_read_metadata_async()`
- Time slice loading: `rocprofvis_db_read_trace_slice_async()`
- Event property loading: `rocprofvis_db_read_event_property_async()`

**Memory Management:**
- Time slices, event properties, and tables stay in trace memory until explicitly deleted
- Use `rocprofvis_dm_delete_time_slice()` or `rocprofvis_dm_delete_all_time_slices()`
- Use `rocprofvis_dm_delete_event_property_for()` or `rocprofvis_dm_delete_all_event_properties_for()`

**Property System:**
- Universal property getters: `rocprofvis_dm_get_property_as_uint64()`, `_int64()`, `_double()`, `_charptr()`, `_handle()`
- Type-specific properties accessed via enumerations

## Testing

Tests use Catch2 framework and are located in:
- `src/controller/tests/basic_tests.cpp`
- `src/model/src/tests/basic_tests.cpp`

To run tests, build with `-DTEST=ON`:
```bash
cmake -DTEST=ON ..
make
```

## Coding Standards

The project follows strict coding standards detailed in [CODING.md](CODING.md). Key points:

### Naming Conventions
- **Files**: `rocprofvis_<module>_<lower_case_with_underscores>` (e.g., `rocprofvis_controller.h`)
- **Namespaces**: `RocProfVis::ModuleName`
- **Classes**: `UpperCamelCase`
- **Class members**: `m_lower_case_with_underscores`
- **Static class members**: `s_lower_case_with_underscores`
- **Public structs/typedefs/enums**: `rocprofvis_lower_case_with_underscores_t`
- **Enum class values**: `kUpperCamelCase`
- **Functions**: `lower_case_with_underscores`

### Code Style
- C++17 standard
- No exceptions, avoid heap allocations
- Fixed-width integers
- `nullptr` instead of NULL/0
- Braces on new lines, always use braces for control flow
- Use `.clang-format` file in `src/` for automatic formatting
- Public APIs must be C11 (not C++)
- Forward declare in headers to minimize includes
- Use `#pragma once` for header guards

### Memory Management
- Prefer stack allocation
- Use smart pointers (`std::unique_ptr`, `std::shared_ptr`) for heap allocations
- Functions returning raw pointers that transfer ownership must start with `alloc_`
- Matching `free_` functions for cleanup

### Documentation
- All files in `src/` must have AMD copyright and license header
- Document type declarations and function definitions for auto-generated API docs

## Third-Party Dependencies

Located in `thirdparty/`:
- **Dear ImGui**: Immediate mode GUI toolkit
- **ImPlot**: GPU-accelerated plotting for ImGui
- **ImGuiFileDialog**: File selection dialog
- **GLFW**: Window and event handling
- **Vulkan SDK**: Graphics API (required)
- **SQLite3**: Database engine
- **jsoncpp**: JSON parsing
- **spdlog**: Logging
- **Catch2**: Testing framework
- **CLI11**: Command-line parsing
- **yaml-cpp**: YAML parsing
- **csv-parser**: CSV parsing
- **nativefiledialog-extended**: Native file dialogs

## Platform-Specific Notes

**Windows:**
- Requires Visual Studio 2022 with "Desktop development with C++"
- Requires Vulkan SDK from LunarG
- Executables in `build/<preset>/<config>/roc-optiq.exe`

**Linux:**
- Requires Wayland/X11 development libraries
- Requires Vulkan SDK (via package manager or manual install)
- Packages emitted as `.deb`, `.rpm`, `.tar.gz`

**macOS:**
- Requires Xcode Command Line Tools
- Requires Vulkan SDK and MoltenVK (via Homebrew)
- Native file dialog disabled (must run on main thread)
- Executables in `build/<preset>/`

## Common Tasks

### Adding New Features

When adding systems profiler features:
1. Update model layer if new database queries needed (`src/model/`)
2. Add controller logic to transform data (`src/controller/src/system/`)
3. Implement UI components (`src/view/src/`)

When adding compute profiler features (requires `COMPUTE_UI_SUPPORT=ON`):
1. Add controller logic in `src/controller/src/compute/`
2. Implement UI in `src/view/src/compute/`

### File Organization

Each module follows the pattern:
```
src/<module>/
  inc/           - Public API headers
  src/           - Implementation files
  tests/         - Unit tests (if applicable)
  CMakeLists.txt - Build configuration
```

### Debugging

Use debug presets for detailed symbols and checks:
- Windows: `x64-debug` (enables `ROCPROFVIS_DEVELOPER_MODE`)
- Linux: `linux-debug`
- macOS: `macos-debug`

Enable logging with: `rocprofvis_core_enable_log("log.txt", spdlog::level::debug)`
