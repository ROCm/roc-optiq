# ROCm Optiq (roc-optiq) - Claude Context

## Project Overview

**ROCm Optiq** (formerly rocprofiler-visualizer) is a cross-platform desktop application for visualizing AMD ROCm profiler traces. It provides an interactive timeline view, advanced event analysis, and performance profiling capabilities for GPU applications.

- **Product Name**: ROCm Optiq (Beta)
- **Repository**: https://github.com/ROCm/roc-optiq
- **Purpose**: Visualizer for ROCm Profiler Tools
- **Supported Platforms**: Windows, Linux (Ubuntu, RHEL, Rocky), macOS (Apple Silicon & Intel)
- **License**: MIT

## Architecture

### Components

The project follows a **Model-View-Controller (MVC)** architecture with the following main components:

1. **app/** - Application skeleton and infrastructure
2. **core/** - Core functionality and data structures
3. **model/** - Data models for traces, events, and metrics
4. **controller/** - Business logic and coordination layer
5. **view/** - UI implementation using Dear ImGui
6. **qt_view/** - Qt-based view (alternative implementation)
7. **thirdparty/** - External dependencies

### Technology Stack

- **Language**: C++17 (required standard)
- **Build System**: CMake 3.15+ with presets
- **UI Framework**: Dear ImGui (immediate mode GPU-accelerated UI)
- **Graphics API**: Vulkan (via Vulkan SDK 1.3.296.0+)
- **Windowing**: GLFW (platform abstraction)
- **Plotting**: ImPlot (GPU-accelerated plotting)
- **File Dialogs**: ImGuiFileDialog or native OS dialogs
- **Logging**: spdlog
- **Data Parsing**: CSV parser, jsoncpp, yaml-cpp, SQLite3
- **Testing**: Catch2

### Supported File Formats

- **Trace Files**: `.db` (SQLite) and `.rpd` (ROCProfiler)
- **Project Files**: `.rpv` (ROCm Optiq project with customizations)
- **Export**: CSV format

### Build Configurations

Use CMake presets defined in `CMakePresets.json`:
- **Windows**: `x64-release`, `x64-release-symbols`, `x64-debug`
- **Linux**: `linux-release`, `linux-release-symbols`, `linux-debug`
- **macOS**: `macos-release`, `macos-release-symbols`, `macos-debug`

### Key CMake Options

- `COMPUTE_UI_SUPPORT` - Build with support for ROCProfiler Compute .CSVs (OFF by default)
- `ROCPROFVIS_DEVELOPER_MODE` - Enable developer mode features (OFF by default)
- `ROCPROFVIS_ENABLE_INTERNAL_BANNER` - Show diagonal internal build banner (OFF by default)
- `USE_NATIVE_FILE_DIALOG` - Use OS native file dialog instead of ImGui (ON by default, forced OFF on macOS)
- `GLFW_LINK_STATIC` - Link GLFW statically instead of dynamically (OFF by default)

## Coding Standards

### General Principles

- **Standard**: C++17 (strictly enforced)
- **Code Style**: ROCm SDK coding style (see [CODING.md](CODING.md))
- **Formatting**: `.clang-format` and `.clang-tidy` in `src/` directory
- **Philosophy**: KISS principle for APIs, avoid over-engineering
- **Error Handling**: NO C++ exceptions (return error codes)
- **Memory**: Prefer stack allocation over heap; use smart pointers when heap is necessary

### Naming Conventions

| Construct | Format | Example |
|-----------|--------|---------|
| File Names | `rocprofvis_` + module + `lower_case_with_underscores` | `rocprofvis_controller.h` |
| Macros/Defines | `ROCPROFVIS_` + `UPPER_CASE_WITH_UNDERSCORES` | `ROCPROFVIS_MY_CONST_VALUE` |
| Namespace Names | `UpperCamelCase` | `RocProfVis::Controller` |
| Class Names | `UpperCamelCase` | `MyClass` |
| Interface Classes | `I` + `UpperCamelCase` | `IMyInterface` |
| Class Methods | `UpperCamelCase` | `MyMemberFunction` |
| Static Members | `s_` + `lower_case_with_underscores` | `s_my_static_variable` |
| Non-static Members | `m_` + `lower_case_with_underscores` | `m_my_member_variable` |
| Public Structs | `rocprofvis_` + `lower_case_with_underscores` + `_t` | `rocprofvis_my_struct_type_t` |
| Enum Classes | `UpperCamelCase` | `MyEnum` |
| Enum Values | `k` + `UpperCamelCase` | `kMyEnumValue` |
| Global Variables | `g_` + `lower_case_with_underscores` | `g_my_global_variable` |
| Functions | `lower_case_with_underscores` | `my_free_function` |

### Important Rules

- **Braces**: Always on new lines for blocks
- **Control Flow**: Always use braces (no brace-less if/for/while)
- **No `goto`**: Use formal flow control statements
- **No `auto`**: Except for iterators, anonymous structures, or when type is explicit in RHS
- **nullptr**: Use `nullptr` instead of `0` or `NULL`
- **Magic Numbers**: Replace with named constants
- **Assertions**: Use aggressively (prefer `static_assert` for compile-time)
- **Fixed-Width Integers**: Always use (`int32_t`, `uint64_t`, etc.)
- **Header Guards**: Use `#pragma once`
- **Namespace Usage**: Format as `namespace RocProfVis { namespace ModuleName {} }`
- **No `using` in Headers**: Keep namespaces explicit in header files

### Memory Management

- Avoid heap allocations where possible
- Use `std::unique_ptr` or `std::shared_ptr` for ownership clarity
- Functions returning raw pointers with ownership should start with `alloc_`
- Provide matching `free_` functions for `alloc_` functions

### Documentation

- All files in `src/` must have AMD copyright & license header
- Document type declarations and function definitions for auto-generated API docs

## Project-Specific Context

### Trace Types

The application supports two main trace types:

1. **SystemTrace** - Traditional system-level GPU profiling traces
2. **ComputeTrace** - Compute workload profiling (enabled with `COMPUTE_UI_SUPPORT`)

Both trace types are handled through the controller layer with type-specific implementations.

### Key Features

- **Timeline View**: Interactive event visualization with zoom/pan controls
- **System Topology Tree**: Hierarchical view of GPU/CPU resources
- **Event Analysis**: Detailed event inspection and filtering
- **Bookmarks**: Save and restore view positions (Ctrl+0-9 to save, 0-9 to recall)
- **Annotations**: User-created markers and notes on timeline
- **Time Range Filters**: Select and analyze specific time ranges
- **Flow Visualization**: Dependency and data flow rendering (fan or chain mode)
- **CSV Export**: Export filtered event data

### Recent Work (Based on Git Status)

The following files are untracked and may contain work in progress:
- `CONTROLLER_INTEGRATION_SUMMARY.md` - Controller integration documentation
- `DATAPROVIDER_PIVOT_TABLE_IMPLEMENTATION.md` - Pivot table implementation notes
- `KERNEL_METRICS_MATRIX_USAGE.md` - Kernel metrics documentation
- `Testing/` - Test-related files
- `thirdparty/CLI11/` - CLI argument parsing library
- `thirdparty/imgui-flame-graph/` - Flame graph visualization

## Building from Source

See [BUILDING.md](BUILDING.md) for detailed instructions.

**Quick Start (Linux):**
```bash
git clone --recursive <repo-url>
cmake -B build/linux-release --preset "linux-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/linux-release --preset "Linux Release Build" --parallel 4 --target package
```

**Quick Start (Windows):**
```powershell
cmake --preset "x64-release" -DROCPROFVIS_ENABLE_INTERNAL_BANNER=OFF
cmake --build build/x64-release --preset "Windows Release Build" --parallel 4
```

## Development Workflow

1. Ensure Vulkan SDK is installed and `VULKAN_SDK` environment variable is set
2. Clone with `--recursive` to get all submodules
3. Use appropriate CMake preset for your platform
4. Follow coding standards in [CODING.md](CODING.md)
5. Format code with `.clang-format` before committing
6. Run clang-tidy validation

## Important Files

- [README.md](README.md) - User documentation and usage guide
- [BUILDING.md](BUILDING.md) - Build instructions for all platforms
- [CODING.md](CODING.md) - Complete coding standards and style guide
- [CHANGES.md](CHANGES.md) - Changelog
- [LICENSE.md](LICENSE.md) - MIT License
- [CMakePresets.json](CMakePresets.json) - CMake build presets
- [.clang-format](src/.clang-format) - Code formatting rules
- [.clang-tidy](src/.clang-tidy) - Static analysis rules

## Documentation

Official documentation: https://rocm.docs.amd.com/projects/roc-optiq/en/latest/

## Tips for AI Assistance

1. **When modifying code**: Always follow the naming conventions and coding standards in CODING.md
2. **When adding features**: Consider both SystemTrace and ComputeTrace if `COMPUTE_UI_SUPPORT` is relevant
3. **When working with UI**: The project uses Dear ImGui immediate mode paradigm
4. **When handling files**: Support both `.db` and `.rpd` trace formats, and `.rpv` project files
5. **When writing headers**: Include AMD copyright, use `#pragma once`, minimize includes with forward declarations
6. **When naming files**: Use `rocprofvis_<module>_<description>` pattern
7. **Error handling**: Return error codes, never throw exceptions
8. **Cross-platform**: Consider Windows, Linux, and macOS compatibility
