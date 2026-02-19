# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# ROCm Optiq - ROCm Profiler Visualizer

## Tech Stack
- Language: C++17
- Build System: CMake 3.21+ with presets
- Graphics: Vulkan, ImGui/ImPlot, GLFW
- Database: SQLite3
- Key Libraries: spdlog (logging), nlohmann/json, yaml-cpp
- Platforms: Windows (MSVC), Linux (GCC/Clang), macOS (Clang + MoltenVK)

## Common Commands

### Building

#### Windows (Visual Studio 2022)
```powershell
# Configure (Release)
cmake --preset "x64-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF

# Build (Release)
cmake --build build/x64-release --preset "Windows Release Build" --parallel 4

# Configure & Build (Debug)
cmake --preset "x64-debug" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/x64-debug --preset "Windows Debug Build" --parallel 4

# Configure & Build (Release with Symbols)
cmake --preset "x64-release-symbols" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/x64-release-symbols --preset "Windows Release Build with Symbols" --parallel 4
```

Executable location: `build/<preset>/<config>/roc-optiq.exe`

#### Linux
```bash
# Configure (Release)
cmake -B build/linux-release --preset "linux-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF

# Build & Package (Release)
cmake --build build/linux-release --preset "Linux Release Build" --parallel 4 --target package

# Configure & Build (Debug)
cmake -B build/linux-debug --preset "linux-debug" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/linux-debug --preset "Linux Debug Build" --parallel 4
```

Packages created in: `build/linux-release/` (.deb, .rpm, .tar.gz)

#### macOS
```bash
# Configure (Release)
cmake -B build/macos-release --preset "macos-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF

# Build (Release)
cmake --build build/macos-release --preset "macOS Release Build" --parallel 4
```

Executable location: `build/macos-release/`

### Build Options
- `COMPUTE_UI_SUPPORT=ON`: Enable compute kernel profiling UI (CSV support)
- `ROCPROFVIS_DEVELOPER_MODE=ON`: Enable developer-only features
- `USE_NATIVE_FILE_DIALOG=ON/OFF`: Use OS native file dialog vs. ImGui
- `GLFW_LINK_STATIC=ON/OFF`: Static vs. dynamic GLFW linking

### Running
- Launch the executable and use `File -> Open` to load trace files (.db, .rpd)
- Drag-and-drop trace files onto the application window
- Open project files (.rpv) to restore saved sessions

## Architecture Overview

ROCm Optiq uses a **layered MVC architecture** with clear separation of concerns:

```
Application (GLFW + Vulkan + ImGui)
          ↓
   View (UI Components)
          ↓
Controller (Orchestration + Job System)
          ↓
   Model (Database + Data Structures)
          ↓
    Core (Logging + Utils)
```

### Key Components

#### 1. Application Layer (`src/app/`)
- Entry point with GLFW window management
- Vulkan rendering backend for ImGui
- CLI argument parsing
- File drag-and-drop handling

#### 2. View Layer (`src/view/`)
**Widget-based UI composition:**
- `AppWindow`: Main window manager (singleton)
- `TraceView`: System trace visualization (timeline, tables, analysis)
- `ComputeView`: Compute profiling visualization (optional with COMPUTE_UI_SUPPORT)
- `Project`: Project state persistence (.rpv files)

**Core UI Components:**
- `TimelineView`: Main timeline rendering with LOD
- Track items: `FlameTrackItem`, `LineTrackItem`
- `InfiniteScrollTable`: Large dataset tables with filtering
- `SummaryView`, `AnalysisView`: Statistical views
- `Minimap`, `EventSearch`, `AnnotationView`

**Data Models (View ↔ Controller bridge):**
- `TimelineModel`, `EventModel`, `TopologyModel`, `TablesModel`
- `DataProvider`: Async data fetching from Controller

**User State:**
- `SettingsManager`: Application settings
- `ProjectSetting`: Project-specific settings (JSON)

#### 3. Controller Layer (`src/controller/`)
**Handle-based type system:**
- `Handle`: Opaque pointer with runtime type checking
- `Reference<T>`: Type-safe access wrapper
- Properties accessed via enumeration system

**Core managers:**
- `SystemTrace`/`ComputeTrace`: Main trace containers
- `Timeline`, `Track`, `Graph`: Time-based organization with LOD sampling
- `Table`: Query results with filtering/sorting
- `Event`, `Sample`, `FlowControl`, `ExtData`: Data objects
- `Summary`/`SummaryMetrics`: Aggregated statistics

**Job System:**
- Async task execution with Future-based completion tracking
- Multi-threaded batch processing

**Memory Management:**
- `Array`, `Arguments`, `Future`: Result containers
- `StringTable`: Deduplicated string storage

#### 4. Model Layer (`src/model/`)
**Database Component (`src/model/src/database/`):**
- SQLite wrapper with query builder
- Supports multiple trace formats: Rocpd (.db), Rocprof (.db), Compute
- Auto-detection of database type
- Thread-safe multi-DB access with `OrderedMutex`
- Query builders: time-slice queries, event properties, table queries

**Data Model Component (`src/model/src/datamodel/`):**
- `Trace`: Root container (tracks, events, tables, strings)
- `Track`/`TrackSlice`: Event/sample containers with time-bounded slices
- Record types: `EventTrackSlice`, `PmcTrackSlice`, `StackTrace`, `FlowTrace`, `ExtData`
- `Table`/`TableRow`: SQL query results
- String deduplication and async loading support

**C API Interface:**
- `rocprofvis_c_interface.h`: C API for Python bindings
- `rocprofvis_interface.h`: C++ interface

#### 5. Core Layer (`src/core/`)
- Logging infrastructure (spdlog wrapper)
- Application assertion framework
- Performance profiling support

### Data Flow Patterns

**File Loading:**
```
User → AppWindow.OpenFile()
  → Controller.Load()
    → DataModel.CreateTrace()
      → Database.Open()
        → Database.ReadMetadataAsync()
        → Database.ReadTraceSliceAsync()
    → View Models populate → UI renders
```

**Interactive Data Request (pan/zoom):**
```
User interacts with Timeline
  → View calculates visible range
  → DataProvider.FetchData()
    → Controller.TrackFetchAsync()/GraphFetchAsync()
      → Database.QueryData() with LOD
      → View Models updated → ImGui re-renders
```

**Query/Filter:**
```
User applies filter
  → EventTable receives expression
  → Controller.TableFetchAsync()
    → Database.BuildTableQuery() with WHERE/GROUP/ORDER
    → Database.ExecuteQueryAsync()
    → Table results cached → View displays
```

### Key Design Patterns

1. **Handle/Reference Pattern**: Opaque pointers with runtime type checking for C API compatibility
2. **Async/Future Pattern**: Non-blocking database operations for UI responsiveness
3. **Lazy Loading with LOD**: Data loaded on-demand; level-of-detail sampling for visualization efficiency
4. **String Interning**: Deduplicated string storage to minimize memory footprint
5. **Thread-Safe Caching**: Multi-threaded database access with ordered synchronization
6. **Widget Composition**: Hierarchical ImGui-based UI building
7. **Observer Pattern**: Event/notification callbacks for async completion

## Important Development Notes

### Trace File Formats
- `.db`: SQLite database (rocpd or rocprof format)
- `.rpd`: ROCm profiler data files
- `.rpv`: Project files (JSON) - saves bookmarks, annotations, customizations

### Threading Model
- Main thread: UI rendering (ImGui), event handling
- Job system threads: Database queries, data processing
- Synchronization via `Future` objects and callbacks

### Memory Considerations
- Large trace files: Use LOD and time-sliced queries to avoid loading entire dataset
- String deduplication: All strings go through `StringTable`
- Event stacking: Events grouped by level for flame graph visualization

### Vulkan Dependency
- Requires Vulkan SDK installed
- Linux: Install from LunarG repos
- Windows: Install LunarG SDK
- macOS: Requires MoltenVK (via Homebrew)

### CMake Preset System
All builds use CMake presets (see `CMakePresets.json`):
- Configure presets: `x64-release`, `linux-release`, `macos-release`, etc.
- Build presets: Platform-specific parallelization and packaging

### Optional Features
Controlled via CMake cache variables:
- `COMPUTE_UI_SUPPORT`: Adds compute kernel profiling visualization
- `ROCPROFVIS_DEVELOPER_MODE`: Enables developer-only UI features
- `ROCPROFVIS_ENABLE_INTERNAL_BANNER`: Shows internal build watermark
