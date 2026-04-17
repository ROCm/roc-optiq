# Profiler Launcher Feature - Implementation Plan

## Implementation Status (April 2026)

**Phase 1A — completed:**
- Controller: `LocalProcessExecutor` (Windows + Linux), `ProfilerProcessController`, `ProfilerConfig` — all in `src/controller/src/rocprofvis_controller_profiler_process.{h,cpp}`.
- Controller C API: config alloc/free/setters, `rocprofvis_profiler_launch_async`, `rocprofvis_profiler_get_state/output/trace_path`, `rocprofvis_profiler_cancel` — in `src/controller/inc/rocprofvis_controller.h` and `src/controller/src/rocprofvis_controller.cpp`.
- Controller lifetime: `ProfilerProcessController` stored as user data on `Future` via `SetUserData` with a deleter; deleted when `Future` is freed.
- Cooperative cancel: `ExecuteJob` polls `Future::IsCancelled()`; `rocprofvis_profiler_cancel` both sets the cancel flag and directly terminates the child process.
- `CloseProfiler` cancels and waits (5 s timeout) before freeing the future to avoid use-after-free.
- View: `ProfilerLauncherDialog` with fields, browse dialogs, Launch/Cancel/Close buttons, live output, state indicator — in `src/view/src/widgets/rocprofvis_profiler_launcher_dialog.{h,cpp}`.
- DataProvider: `LaunchProfiler`, `GetProfilerState/Output/TracePath`, `CancelProfiler`, `CloseProfiler` — in `src/view/src/rocprofvis_data_provider.{h,cpp}`.
- AppWindow: "Launch Profiler..." menu item, `Update`/`Render` hooks, `ShowProfilerLauncher`.
- Settings: `ProfilerSettings` with `profiler_path`, `profiler_output_directory`, `auto_load_trace` — persisted in JSON.
- Auto-load: checkbox in dialog, calls `AppWindow::OpenFile` on completion.
- CLI correctness: `BuildCommandArgs` inserts `--` separator; `DetermineTracePath` picks newest matching file.

**Phase 1B (multi-stage workflows) — not started.** Architecture supports it; `ProfilerLaunchConfig` in the original plan includes stage fields. Requires UI for two-stage configuration and sequential stage execution in `ExecuteJob`.

**Phase 2 (remote profiling via SSH) — not started.** `IProcessExecutor` abstraction in the plan supports adding `SSHProcessExecutor`.

## References

**ROCm Systems Profiler Documentation:**  
See [plan_resources/systems_profiler_inst.md](plan_resources/systems_profiler_inst.md) for detailed command-line usage of `rocprof-sys` tools including:
- `rocprof-sys-sample` (single-stage sampling workflow)
- `rocprof-sys-instrument` + `rocprof-sys-run` (two-stage instrumentation workflow)
- Command syntax, flags, and output file patterns

## Context

The rocprofiler-visualizer is currently a **visualization-only** application that reads pre-collected profiling data from `.db`, `.rpd`, and `.yaml` files. Users must separately run ROCm profiling tools (like `rocprof-sys` or `rocprof`) via command-line before loading traces into the visualizer.

This feature adds the ability to **launch profilers directly from the GUI**, with a modal dialog for configuration, live output display, and automatic trace loading upon completion. This eliminates the context-switching between terminal and GUI, streamlining the profiling workflow.

### User Requirements

**Phase 1A (Initial Implementation - Option B):**
- Modal dialog to configure profiler launch options
- Controller executes the profiler process locally (Windows/Linux)
- Monitor process progress and capture stdout/stderr
- Display live output to user in the dialog
- Auto-load resulting trace file when profiling completes
- **Initial support:** Single-stage profilers (`rocprof-sys-sample`)
- Architecture designed to support multi-stage workflows (future)

**Phase 1B (Enhancement - Option A):**
- Multi-stage workflow support (stage 1 → stage 2)
- **Systems Profiler:** `rocprof-sys-instrument` → `rocprof-sys-run` (instrument binary, then execute)
- **Compute Profiler:** `rocprof-compute` (capture) → `rocprof-compute --analysis` (analyze and generate DB)
- Automatic intermediate file management (`.inst` binaries, capture data)
- Support for sampling-only mode (`-M sampling`)

**Phase 2 (Future):**
- Remote profiling via SSH (Windows/Linux client → Linux server)
- Transfer trace files back to local machine for visualization

### Architecture Context

The application uses:
- **ImGui** for UI rendering (immediate-mode GUI)
- **Layered architecture**: App → View → Controller (C ABI) → Model
- **Controller C ABI**: Controller exposes pure C API (`rocprofvis_controller.h`) for all View interactions
- **DataProvider pattern**: View uses `DataProvider` class to wrap Controller C API calls and manage callbacks
- **EventManager scope**: Events are **only for inter-view messaging** (between UI widgets), NOT for View-Controller communication
- **Async pattern**: `Future`-based async operations via `JobSystem` (Controller) with callbacks to View
- **Dialog pattern**: `Show()` + `Render()` lifecycle with callback-based rendering

---

## Implementation Plan

### 1. Controller Layer: Process Execution (C++ Implementation)

**Important:** The Controller layer will have internal C++ implementation classes, but these are NOT exposed to the View. The View only sees the C API functions defined in `rocprofvis_controller.h`.

#### 1.1 New File (Internal): `rocprofvis_controller_profiler_process.h`

**Location:** `src/controller/src/rocprofvis_controller_profiler_process.h`

**Purpose:** Internal C++ implementation for process execution. This is private to the Controller module.

**Key Classes (Internal C++ - not exposed to View):**

```cpp
namespace RocProfVis {
namespace Controller {

enum class ProcessState {
    kIdle,
    kLaunching,
    kRunning,
    kCompleted,
    kFailed,
    kCancelled
};

enum class ProfilerType {
    kROCmSystems,    // rocprof-sys
    kROCmCompute,    // rocprof
    kROCprofV3,      // rocprofv3 (optional)
    kCustom          // User-specified
};

struct ProfilerLaunchConfig {
    ProfilerType profiler_type;
    
    // Multi-stage workflow configuration (Phase 1B - future, but included from day 1 for future-proofing)
    bool requires_two_stage_workflow = false;     // Enable multi-stage workflow
    
    // Stage 1: Preparation stage (Phase 1B)
    // - Systems Profiler: Instrumentation (rocprof-sys-instrument)
    // - Compute Profiler: Capture (rocprof-compute with capture flags)
    std::string stage1_command;                   // e.g., "rocprof-sys-instrument" or "rocprof-compute"
    std::vector<std::string> stage1_args;         // e.g., ["-o", "./app.inst"] or capture flags
    std::string stage1_output;                    // e.g., "./app.inst" or "./capture_data.csv"
    
    // Stage 2: Execution/Analysis stage (always used)
    // - Systems Profiler: Run instrumented binary (rocprof-sys-run with .inst)
    // - Compute Profiler: Analysis (rocprof-compute --analysis with captured data)
    // - Single-stage: Direct execution (rocprof-sys-sample)
    std::string stage2_command;                   // Path to stage 2 command
    std::vector<std::string> stage2_args;         // Arguments for stage 2
    
    // Common configuration
    std::string target_executable;                // Application to profile
    std::vector<std::string> target_args;         // Arguments for target app
    std::string output_directory;                 // Where to save trace files
    bool auto_load_on_complete;                   // Auto-open trace when done
};

// Abstract base for Phase 2 extensibility (SSH)
class IProcessExecutor {
public:
    virtual ~IProcessExecutor() = default;
    virtual rocprofvis_result_t Launch(const ProfilerLaunchConfig& config) = 0;
    virtual rocprofvis_result_t Terminate(bool force = false) = 0;
    virtual ProcessState GetState() const = 0;
    virtual std::string ReadOutput() = 0;  // Non-blocking read
    virtual int GetExitCode() const = 0;
};

// Phase 1: Local process execution (Windows/Linux)
class LocalProcessExecutor : public IProcessExecutor {
    // Platform-specific implementation using CreateProcess (Windows) or fork+execvp (Linux)
};

class ProfilerProcessController {
public:
    using ProgressCallback = std::function<void(uint64_t percent, const std::string& message)>;
    using OutputCallback = std::function<void(const std::string& output)>;
    using CompletionCallback = std::function<void(rocprofvis_result_t result, const std::string& trace_path)>;

    rocprofvis_result_t LaunchProfilerAsync(
        const ProfilerLaunchConfig& config,
        rocprofvis_controller_future_t* future,
        ProgressCallback progress_cb,
        OutputCallback output_cb,
        CompletionCallback completion_cb);

    rocprofvis_result_t CancelProfiler(rocprofvis_controller_future_t* future);
};

} // namespace Controller
} // namespace RocProfVis
```

**Implementation Strategy:**

- **Windows**: Use `CreateProcess()` with `STARTUPINFO` pipe redirection for stdout/stderr
- **Linux**: Use `fork()` + `execvp()` with `pipe()` for stdout/stderr redirection
- **Monitoring**: Background thread polls process state every 100ms using `GetExitCodeProcess()` (Windows) or `waitpid(WNOHANG)` (Linux)
- **Output Capture**: Non-blocking reads from pipes using `PeekNamedPipe()` (Windows) or `select()/poll()` (Linux)
- **Cancellation**: `TerminateProcess()` (Windows) or `kill(SIGTERM)` followed by `kill(SIGKILL)` if needed (Linux)
- **Trace Detection**: Parse profiler stdout for output file path, or scan `output_directory` for newly created `.db`/`.rpd` files
- **Multi-stage Support (Phase 1B)**: Execute stages sequentially within same job:
  - **Systems Profiler:**
    - Stage 1: `rocprof-sys-instrument` (create `.inst` binary), verify intermediate file created
    - Stage 2: `rocprof-sys-run` with `.inst` binary as target
  - **Compute Profiler:**
    - Stage 1: `rocprof-compute` with capture flags (collect profiling data)
    - Stage 2: `rocprof-compute --analysis` to analyze captured data and generate output DB
  - Monitor each stage until completion and capture trace file

**File Pattern:**
```cpp
// Windows platform
#ifdef _WIN32
    HANDLE m_process_handle;
    HANDLE m_stdout_read, m_stderr_read;
    rocprofvis_result_t LaunchWindows(const ProfilerLaunchConfig& config);
#else
    pid_t m_pid;
    int m_stdout_fd, m_stderr_fd;
    rocprofvis_result_t LaunchUnix(const ProfilerLaunchConfig& config);
#endif
```

#### 1.2 New C API Functions in Controller Header

**Modified File:** `src/controller/inc/rocprofvis_controller.h`

**Purpose:** Add public C API functions for profiler launching (exposed to View layer).

**New C API Functions:**

```cpp
#ifdef __cplusplus
extern "C" {
#endif

/* Profiler launch configuration structure */
typedef struct rocprofvis_profiler_config_t rocprofvis_profiler_config_t;

/* Profiler type enumeration */
typedef enum {
    kROCmSystemsProfiler = 0,
    kROCmComputeProfiler = 1,
    kROCprofV3Profiler = 2,
    kCustomProfiler = 3
} rocprofvis_profiler_type_t;

/* Profiler process state */
typedef enum {
    kProfilerStateIdle = 0,
    kProfilerStateLaunching = 1,
    kProfilerStateRunning = 2,
    kProfilerStateCompleted = 3,
    kProfilerStateFailed = 4,
    kProfilerStateCancelled = 5
} rocprofvis_profiler_state_t;

/*
 * Allocate a profiler configuration object.
 * @returns A valid config object, or nullptr.
 */
rocprofvis_profiler_config_t* rocprofvis_profiler_config_alloc(void);

/*
 * Free a profiler configuration object.
 * @param config The config to free.
 */
void rocprofvis_profiler_config_free(rocprofvis_profiler_config_t* config);

/*
 * Set properties on a profiler configuration.
 * Uses existing property system pattern (rocprofvis_controller_set_string, etc.)
 */
rocprofvis_result_t rocprofvis_profiler_config_set_string(
    rocprofvis_profiler_config_t* config,
    rocprofvis_property_t property,
    uint64_t index,
    const char* value);

/*
 * Launch a profiler process asynchronously.
 * @param config The profiler configuration.
 * @param future The future object to track completion.
 * @returns kRocProfVisResultSuccess or an error code.
 */
rocprofvis_result_t rocprofvis_profiler_launch_async(
    rocprofvis_profiler_config_t* config,
    rocprofvis_controller_future_t* future);

/*
 * Cancel a running profiler process.
 * @param future The future associated with the profiler launch.
 * @returns kRocProfVisResultSuccess or an error code.
 */
rocprofvis_result_t rocprofvis_profiler_cancel(
    rocprofvis_controller_future_t* future);

/*
 * Get profiler state from a future.
 * @param future The future associated with the profiler launch.
 * @param state Output parameter for state.
 * @returns kRocProfVisResultSuccess or an error code.
 */
rocprofvis_result_t rocprofvis_profiler_get_state(
    rocprofvis_controller_future_t* future,
    rocprofvis_profiler_state_t* state);

/*
 * Get profiler output (stdout/stderr) from a future.
 * @param future The future associated with the profiler launch.
 * @param buffer Buffer to write output to, or null to query length.
 * @param length Pointer to length of buffer, or to receive required length.
 * @returns kRocProfVisResultSuccess or an error code.
 */
rocprofvis_result_t rocprofvis_profiler_get_output(
    rocprofvis_controller_future_t* future,
    char* buffer,
    uint32_t* length);

/*
 * Get resulting trace file path from completed profiler run.
 * @param future The future associated with the profiler launch.
 * @param buffer Buffer to write path to, or null to query length.
 * @param length Pointer to length of buffer, or to receive required length.
 * @returns kRocProfVisResultSuccess or an error code.
 */
rocprofvis_result_t rocprofvis_profiler_get_trace_path(
    rocprofvis_controller_future_t* future,
    char* buffer,
    uint32_t* length);

#ifdef __cplusplus
}
#endif
```

**New Properties for Configuration:**
```cpp
// Add to rocprofvis_property_t enum in rocprofvis_controller_enums.h

// Basic profiler configuration (Phase 1A)
kRPVProfilerConfigType,              // rocprofvis_profiler_type_t
kRPVProfilerConfigPath,              // string: path to profiler executable
kRPVProfilerConfigTargetExecutable,  // string: app to profile
kRPVProfilerConfigTargetArgs,        // string: args for target app
kRPVProfilerConfigOutputDirectory,   // string: where to save traces
kRPVProfilerConfigProfilerArgs,      // string: additional profiler flags

// Multi-stage workflow configuration (Phase 1B - included from day 1 for future-proofing)
kRPVProfilerConfigRequiresTwoStage,             // bool: enable multi-stage workflow
kRPVProfilerConfigStage1Command,                // string: stage 1 command (instrument/capture)
kRPVProfilerConfigStage1Args,                   // string: args for stage 1
kRPVProfilerConfigStage1Output,                 // string: stage 1 output file (.inst or capture data)
kRPVProfilerConfigStage2Command,                // string: stage 2 command (run/analyze)
kRPVProfilerConfigStage2Args,                   // string: args for stage 2
```

#### 1.3 New File (Internal): `rocprofvis_controller_profiler_process.cpp`

**Location:** `src/controller/src/rocprofvis_controller_profiler_process.cpp`

**Key Implementation Details:**

**Stage Execution Helper (Phase 1A with Phase 1B support):**
```cpp
// Reusable for both single-stage and multi-stage workflows
bool ProfilerProcessController::ExecuteStage(
    const std::string& stage_name,
    const std::string& command,
    const std::vector<std::string>& args,
    OutputCallback output_cb)
{
    auto executor = std::make_unique<LocalProcessExecutor>();
    
    auto result = executor->Launch(command, args);
    if (result != kRocProfVisResultSuccess) {
        return false;
    }
    
    // Monitor until completion
    while (executor->GetState() == ProcessState::kRunning) {
        std::string new_output = executor->ReadOutput();
        if (!new_output.empty()) {
            output_cb("[" + stage_name + "] " + new_output);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return (executor->GetExitCode() == 0);
}
```

**Monitor Thread Pattern (Multi-stage aware):**
```cpp
void ProfilerProcessController::MonitorProcess(
    ProgressCallback progress_cb,
    OutputCallback output_cb,
    CompletionCallback completion_cb)
{
    // Stage 1: Preparation (Phase 1B - skipped in Phase 1A)
    // - Systems: Instrumentation (create .inst binary)
    // - Compute: Capture (collect profiling data)
    if (m_config.requires_two_stage_workflow) {
        std::string stage1_label = GetStage1Label(m_config.profiler_type);
        progress_cb(0, stage1_label + "...");
        
        if (!ExecuteStage(stage1_label, 
                         m_config.stage1_command,
                         m_config.stage1_args,
                         output_cb)) {
            completion_cb(kRocProfVisResultFailed, stage1_label + " failed");
            return;
        }
        
        // Verify stage 1 output file created
        if (!FileExists(m_config.stage1_output)) {
            completion_cb(kRocProfVisResultFailed, stage1_label + " output not found: " + m_config.stage1_output);
            return;
        }
        
        progress_cb(50, stage1_label + " complete");
    }
    
    // Stage 2: Execution/Analysis (always runs)
    // - Systems: Run instrumented binary (rocprof-sys-run)
    // - Compute: Analyze captured data (rocprof-compute --analysis)
    // - Single-stage: Direct execution (rocprof-sys-sample)
    std::string stage2_label = GetStage2Label(m_config.profiler_type, m_config.requires_two_stage_workflow);
    progress_cb(m_config.requires_two_stage_workflow ? 50 : 0, stage2_label + "...");
    
    if (!ExecuteStage(stage2_label,
                     m_config.stage2_command,
                     m_config.stage2_args,
                     output_cb)) {
        completion_cb(kRocProfVisResultFailed, stage2_label + " failed");
        return;
    }
    
    // Detect trace file
    std::string trace_path = DetectOutputFile(m_config);
    
    progress_cb(100, "Profiling complete");
    completion_cb(kRocProfVisResultSuccess, trace_path);
}

std::string ProfilerProcessController::GetStage1Label(ProfilerType type) {
    switch (type) {
        case kROCmSystemsProfiler: return "Instrument";
        case kROCmComputeProfiler: return "Capture";
        default: return "Stage 1";
    }
}

std::string ProfilerProcessController::GetStage2Label(ProfilerType type, bool two_stage) {
    if (!two_stage) return "Profile";
    
    switch (type) {
        case kROCmSystemsProfiler: return "Run";
        case kROCmComputeProfiler: return "Analyze";
        default: return "Stage 2";
    }
}
```

**Integration with JobSystem:**
```cpp
rocprofvis_result_t ProfilerProcessController::LaunchProfilerAsync(
    const ProfilerLaunchConfig& config,
    rocprofvis_controller_future_t* future,
    ProgressCallback progress_cb,
    OutputCallback output_cb,
    CompletionCallback completion_cb)
{
    auto job_function = [=](rocprofvis_controller_future_t* f) -> rocprofvis_result_t {
        m_executor = std::make_unique<LocalProcessExecutor>();
        
        auto result = m_executor->Launch(config);
        if (result != kRocProfVisResultSuccess) {
            completion_cb(result, "");
            return result;
        }
        
        // Start monitor thread (detached, will notify via callbacks)
        m_monitor_thread = std::thread(&ProfilerProcessController::MonitorProcess,
                                       this, progress_cb, output_cb, completion_cb);
        m_monitor_thread.detach();
        
        return kRocProfVisResultSuccess;
    };
    
    // Submit to JobSystem (same pattern as rocprofvis_controller_load_async)
    // Implementation follows existing controller async patterns
}
```

---

### 2. View Layer: UI Dialog and DataProvider Integration

#### 2.1 New File: `rocprofvis_profiler_launcher_dialog.h`

**Location:** `src/view/src/widgets/rocprofvis_profiler_launcher_dialog.h`

**Purpose:** Modal dialog for configuring and launching profiler sessions with live output display.

**Class Design:**

```cpp
namespace RocProfVis {
namespace View {

enum class DialogState {
    kIdle,           // Not shown
    kConfiguring,    // User filling out form
    kLaunching,      // Waiting for process to start
    kRunning,        // Process running, showing output
    kCompleted,      // Process finished successfully
    kFailed          // Process failed or cancelled
};

class ProfilerLauncherDialog {
public:
    ProfilerLauncherDialog(AppWindow* app_window, DataProvider* data_provider);
    ~ProfilerLauncherDialog();
    
    void Show();
    void Render();
    void Update();  // Called each frame to poll profiler state
    
private:
    void RenderConfigurationUI();
    void RenderOutputUI();
    void RenderButtons();
    
    void OnLaunchClicked();
    void OnCancelClicked();
    void OnCloseClicked();
    
    void LoadSettings();
    void SaveSettings();
    bool ValidateConfig();
    
    // Polling/update methods
    void PollProfilerState();
    void UpdateOutput();
    void OnProfilerStateChanged(rocprofvis_profiler_state_t new_state);
    
    // State
    bool m_should_open;
    DialogState m_state;
    
    // Configuration UI buffers
    int m_profiler_type_index;  // For ImGui combo box
    char m_profiler_path_buffer[512];
    char m_target_executable_buffer[512];
    char m_target_args_buffer[1024];
    char m_output_directory_buffer[512];
    char m_profiler_args_buffer[1024];
    bool m_auto_load_on_complete;
    
    // Output display
    std::string m_output_text;
    bool m_auto_scroll;
    rocprofvis_profiler_state_t m_profiler_state;
    std::string m_status_message;
    std::string m_trace_path;
    
    // References (not owned)
    AppWindow* m_app_window;        // For opening trace files
    DataProvider* m_data_provider;  // For Controller communication
};

} // namespace View
} // namespace RocProfVis
```

**UI Layout:**

```
┌──────────────────────────────────────────────────────┐
│  Launch Profiler Session                     [X]     │
├──────────────────────────────────────────────────────┤
│  Profiler Type:  [ROCm Systems ▼]                    │
│  Profiler Path:  [/opt/rocm/bin/rocprof-sys    ] [📁]│
│  Target Exec:    [/path/to/app                 ] [📁]│
│  Target Args:    [--arg1 --arg2                    ] │
│  Output Dir:     [/tmp/profiler_output         ] [📁]│
│  Profiler Flags: [-o trace.db                     ] │
│                                                       │
│  ☑ Auto-load trace file when profiling completes     │
│                                                       │
│  ┌─────────────────────────────────────────────────┐ │
│  │ Process Output:                                 │ │
│  │ [rocprof-sys] Starting profiler...              │ │
│  │ [rocprof-sys] Profiling PID 12345               │ │
│  │ [rocprof-sys] Writing trace to trace.db         │ │
│  │ ...                                             │ │
│  │                                                 │ │
│  └─────────────────────────────────────────────────┘ │
│  Progress: [████████░░░░░░░░░░] 40%                  │
│                                                       │
│                         [Launch]  [Cancel]  [Close]  │
└──────────────────────────────────────────────────────┘
```

**State Machine:**

- **kIdle**: Dialog not shown
- **kConfiguring**: User entering configuration, "Launch" button enabled if paths valid
- **kLaunching**: Waiting for profiler process to start (brief)
- **kRunning**: Process executing, output scrolling, "Cancel" button available
- **kCompleted**: Process finished, "Close" button or auto-close if auto-load enabled
- **kFailed**: Error message shown, "Close" button available

#### 2.2 New File: `rocprofvis_profiler_launcher_dialog.cpp`

**Location:** `src/view/src/widgets/rocprofvis_profiler_launcher_dialog.cpp`

**Key Implementation:**

**Render Pattern (follows existing dialog pattern):**
```cpp
void ProfilerLauncherDialog::Render() {
    if (m_should_open) {
        ImGui::OpenPopup("Launch Profiler Session");
        m_should_open = false;
    }
    
    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();
    
    ImGuiWindowFlags flags = ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings;
    
    if (ImGui::BeginPopupModal("Launch Profiler Session", nullptr, flags)) {
        switch (m_state) {
            case DialogState::kConfiguring:
            case DialogState::kLaunching:
            case DialogState::kRunning:
            case DialogState::kCompleted:
            case DialogState::kFailed:
                RenderConfigurationUI();
                RenderOutputUI();
                RenderButtons();
                break;
        }
        
        ImGui::EndPopup();
    }
    
    popup_style.PopStyles();
}
```

**Configuration UI (Phase 1A with Phase 1B support):**
```cpp
void ProfilerLauncherDialog::RenderConfigurationUI() {
    // Profiler type selection
    const char* profiler_types[] = {"ROCm Systems (rocprof-sys)", "ROCm Compute (rocprof-compute)", "ROCprofv3", "Custom"};
    ImGui::Combo("Profiler Type", &m_profiler_type_index, profiler_types, 4);
    
    // Target executable
    ImGui::Text("Target Executable:");
    ImGui::InputText("##target_exe", m_target_executable_buffer, sizeof(m_target_executable_buffer));
    ImGui::SameLine();
    if (ImGui::Button("📁##pick_target")) { /* file picker */ }
    
    ImGui::Text("Target Arguments:");
    ImGui::InputText("##target_args", m_target_args_buffer, sizeof(m_target_args_buffer));
    
    // Advanced: Multi-stage Workflow (Phase 1B - collapsible section, hidden by default in Phase 1A)
    if (ImGui::CollapsingHeader("Advanced: Multi-Stage Workflow", ImGuiTreeNodeFlags_None)) {
        ImGui::Indent();
        
        ImGui::Checkbox("Enable two-stage workflow", &m_requires_two_stage_workflow);
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("Enable for profilers requiring two stages:\n"
                             "• Systems: rocprof-sys-instrument → rocprof-sys-run\n"
                             "• Compute: rocprof-compute (capture) → rocprof-compute --analysis");
        }
        
        if (m_requires_two_stage_workflow) {
            ImGui::Separator();
            ImGui::Text("Stage 1 (Preparation):");
            
            ImGui::Text("Command:");
            ImGui::InputText("##stage1_cmd", m_stage1_command_buffer, sizeof(m_stage1_command_buffer));
            ImGui::SameLine();
            if (ImGui::Button("📁##pick_stage1_cmd")) { /* file picker */ }
            ImGui::TextDisabled("e.g., rocprof-sys-instrument or rocprof-compute");
            
            ImGui::Text("Arguments:");
            ImGui::InputText("##stage1_args", m_stage1_args_buffer, sizeof(m_stage1_args_buffer));
            ImGui::TextDisabled("Systems: -o ./app.inst -M sampling  |  Compute: capture flags");
            
            ImGui::Text("Output File:");
            ImGui::InputText("##stage1_output", m_stage1_output_buffer, sizeof(m_stage1_output_buffer));
            ImGui::TextDisabled("Systems: ./app.inst  |  Compute: ./capture_data.csv");
            
            ImGui::Separator();
            ImGui::Text("Stage 2 (Execution/Analysis):");
            
            ImGui::Text("Command:");
            ImGui::InputText("##stage2_cmd", m_stage2_command_buffer, sizeof(m_stage2_command_buffer));
            ImGui::SameLine();
            if (ImGui::Button("📁##pick_stage2_cmd")) { /* file picker */ }
            ImGui::TextDisabled("e.g., rocprof-sys-run or rocprof-compute");
            
            ImGui::Text("Arguments:");
            ImGui::InputText("##stage2_args", m_stage2_args_buffer, sizeof(m_stage2_args_buffer));
            ImGui::TextDisabled("Systems: <empty>  |  Compute: --analysis <capture_data>");
        }
        
        ImGui::Unindent();
    }
    
    // Profiler command/path
    ImGui::Text("Profiler Command:");
    ImGui::InputText("##profiler_path", m_profiler_path_buffer, sizeof(m_profiler_path_buffer));
    ImGui::SameLine();
    if (ImGui::Button("📁##pick_profiler")) { /* file picker */ }
    ImGui::TextDisabled("e.g., rocprof-sys-sample (single-stage) or rocprof-sys-run (after instrumentation)");
    
    ImGui::Text("Profiler Arguments:");
    ImGui::InputText("##profiler_args", m_profiler_args_buffer, sizeof(m_profiler_args_buffer));
    
    ImGui::Text("Output Directory:");
    ImGui::InputText("##output_dir", m_output_directory_buffer, sizeof(m_output_directory_buffer));
    ImGui::SameLine();
    if (ImGui::Button("📁##pick_output_dir")) { /* directory picker */ }
    
    ImGui::Checkbox("Auto-load trace file when profiling completes", &m_auto_load_on_complete);
}
```

**Launch Handler (via DataProvider):**
```cpp
void ProfilerLauncherDialog::OnLaunchClicked() {
    // Validate configuration
    if (!ValidateConfig()) {
        // Show error message
        return;
    }
    
    // Convert profiler type index to enum
    rocprofvis_profiler_type_t profiler_type = 
        static_cast<rocprofvis_profiler_type_t>(m_profiler_type_index);
    
    // Launch via DataProvider (abstracts Controller C API)
    bool success = m_data_provider->LaunchProfiler(
        profiler_type,
        std::string(m_profiler_path_buffer),
        std::string(m_target_executable_buffer),
        std::string(m_target_args_buffer),
        std::string(m_output_directory_buffer),
        std::string(m_profiler_args_buffer));
    
    if (success) {
        m_state = DialogState::kLaunching;
        m_status_message = "Starting profiler...";
        
        // Register callback for state changes (optional)
        m_data_provider->SetProfilerStateChangedCallback(
            [this](rocprofvis_profiler_state_t new_state) {
                OnProfilerStateChanged(new_state);
            });
    } else {
        m_state = DialogState::kFailed;
        m_status_message = "Failed to launch profiler";
    }
}
```

**Polling Pattern (called from Update):**
```cpp
void ProfilerLauncherDialog::Update() {
    if (m_state == DialogState::kLaunching || m_state == DialogState::kRunning) {
        PollProfilerState();
        UpdateOutput();
    }
}

void ProfilerLauncherDialog::PollProfilerState() {
    // Get state via DataProvider
    rocprofvis_profiler_state_t current_state = m_data_provider->GetProfilerState();
    
    if (current_state != m_profiler_state) {
        m_profiler_state = current_state;
        OnProfilerStateChanged(current_state);
    }
}

void ProfilerLauncherDialog::OnProfilerStateChanged(rocprofvis_profiler_state_t new_state) {
    switch (new_state) {
        case kProfilerStateRunning:
            m_state = DialogState::kRunning;
            m_status_message = "Profiling in progress...";
            break;
            
        case kProfilerStateCompleted:
            m_state = DialogState::kCompleted;
            m_status_message = "Profiling completed";
            
            // Get trace path via DataProvider
            m_trace_path = m_data_provider->GetProfilerTracePath();
            
            // Auto-load if enabled
            if (m_auto_load_on_complete && !m_trace_path.empty()) {
                m_app_window->OpenFile(m_trace_path);
            }
            break;
            
        case kProfilerStateFailed:
        case kProfilerStateCancelled:
            m_state = DialogState::kFailed;
            m_status_message = (new_state == kProfilerStateFailed) 
                ? "Profiling failed" 
                : "Profiling cancelled";
            break;
    }
}

void ProfilerLauncherDialog::UpdateOutput() {
    // Get new output via DataProvider
    std::string new_output = m_data_provider->GetProfilerOutput();
    
    if (new_output.size() > m_output_text.size()) {
        m_output_text = std::move(new_output);
        m_auto_scroll = true;  // Trigger auto-scroll in Render()
    }
}
```

**Cancel Handler:**
```cpp
void ProfilerLauncherDialog::OnCancelClicked() {
    if (m_state == DialogState::kLaunching || m_state == DialogState::kRunning) {
        m_data_provider->CancelProfiler();
    }
}
```

---

### 3. DataProvider Integration (Required for Future JSON Interface)

**Important:** DataProvider will be the abstraction layer for all Controller communication. In the future, this will enable JSON-based RPC communication, allowing Controller and View to run on different systems.

#### 3.1 Modified File: `rocprofvis_data_provider.h`

**Location:** `src/view/src/rocprofvis_data_provider.h`

**Add profiler management methods:**

```cpp
class DataProvider {
public:
    // ... existing methods ...
    
    // Profiler launching (wraps Controller C API)
    bool LaunchProfiler(rocprofvis_profiler_type_t profiler_type,
                       const std::string& profiler_path,
                       const std::string& target_executable,
                       const std::string& target_args,
                       const std::string& output_directory,
                       const std::string& profiler_args);
    
    rocprofvis_profiler_state_t GetProfilerState() const;
    std::string GetProfilerOutput();
    std::string GetProfilerTracePath();
    bool CancelProfiler();
    void CloseProfiler();  // Cleanup profiler resources
    
    // Callback for profiler state changes (optional, for async updates)
    void SetProfilerStateChangedCallback(
        const std::function<void(rocprofvis_profiler_state_t)>& callback);
    
private:
    // ... existing members ...
    
    // Profiler session management
    rocprofvis_profiler_config_t* m_profiler_config;
    rocprofvis_controller_future_t* m_profiler_future;
    rocprofvis_profiler_state_t m_profiler_state;
    std::string m_profiler_output_cache;
    std::function<void(rocprofvis_profiler_state_t)> m_profiler_state_changed_callback;
};
```

#### 3.2 Modified File: `rocprofvis_data_provider.cpp`

**Location:** `src/view/src/rocprofvis_data_provider.cpp`

**Implementation:**

```cpp
bool DataProvider::LaunchProfiler(
    rocprofvis_profiler_type_t profiler_type,
    const std::string& profiler_path,
    const std::string& target_executable,
    const std::string& target_args,
    const std::string& output_directory,
    const std::string& profiler_args)
{
    // Cleanup any existing profiler session
    CloseProfiler();
    
    // Allocate config via C API
    m_profiler_config = rocprofvis_profiler_config_alloc();
    if (!m_profiler_config) {
        return false;
    }
    
    // Set config properties via C API
    rocprofvis_profiler_config_set_uint64(m_profiler_config, kRPVProfilerConfigType, 0, profiler_type);
    rocprofvis_profiler_config_set_string(m_profiler_config, kRPVProfilerConfigPath, 0, profiler_path.c_str());
    rocprofvis_profiler_config_set_string(m_profiler_config, kRPVProfilerConfigTargetExecutable, 0, target_executable.c_str());
    rocprofvis_profiler_config_set_string(m_profiler_config, kRPVProfilerConfigTargetArgs, 0, target_args.c_str());
    rocprofvis_profiler_config_set_string(m_profiler_config, kRPVProfilerConfigOutputDirectory, 0, output_directory.c_str());
    rocprofvis_profiler_config_set_string(m_profiler_config, kRPVProfilerConfigProfilerArgs, 0, profiler_args.c_str());
    
    // Allocate future
    m_profiler_future = rocprofvis_controller_future_alloc();
    if (!m_profiler_future) {
        rocprofvis_profiler_config_free(m_profiler_config);
        m_profiler_config = nullptr;
        return false;
    }
    
    // Launch async via C API
    rocprofvis_result_t result = rocprofvis_profiler_launch_async(m_profiler_config, m_profiler_future);
    
    if (result == kRocProfVisResultSuccess) {
        m_profiler_state = kProfilerStateLaunching;
        return true;
    } else {
        rocprofvis_controller_future_free(m_profiler_future);
        rocprofvis_profiler_config_free(m_profiler_config);
        m_profiler_future = nullptr;
        m_profiler_config = nullptr;
        return false;
    }
}

rocprofvis_profiler_state_t DataProvider::GetProfilerState() const
{
    if (!m_profiler_future) {
        return kProfilerStateIdle;
    }
    
    rocprofvis_profiler_state_t state = kProfilerStateIdle;
    rocprofvis_profiler_get_state(m_profiler_future, &state);
    return state;
}

std::string DataProvider::GetProfilerOutput()
{
    if (!m_profiler_future) {
        return "";
    }
    
    // Get output length
    uint32_t output_length = 0;
    rocprofvis_profiler_get_output(m_profiler_future, nullptr, &output_length);
    
    if (output_length > m_profiler_output_cache.size()) {
        // Resize and fetch new output
        m_profiler_output_cache.resize(output_length);
        rocprofvis_profiler_get_output(m_profiler_future, &m_profiler_output_cache[0], &output_length);
    }
    
    return m_profiler_output_cache;
}

std::string DataProvider::GetProfilerTracePath()
{
    if (!m_profiler_future) {
        return "";
    }
    
    uint32_t path_length = 0;
    rocprofvis_profiler_get_trace_path(m_profiler_future, nullptr, &path_length);
    
    if (path_length == 0) {
        return "";
    }
    
    std::string trace_path;
    trace_path.resize(path_length);
    rocprofvis_profiler_get_trace_path(m_profiler_future, &trace_path[0], &path_length);
    
    return trace_path;
}

bool DataProvider::CancelProfiler()
{
    if (!m_profiler_future) {
        return false;
    }
    
    rocprofvis_result_t result = rocprofvis_profiler_cancel(m_profiler_future);
    return (result == kRocProfVisResultSuccess);
}

void DataProvider::CloseProfiler()
{
    if (m_profiler_future) {
        // Cancel if still running
        rocprofvis_profiler_state_t state;
        rocprofvis_profiler_get_state(m_profiler_future, &state);
        if (state == kProfilerStateRunning || state == kProfilerStateLaunching) {
            rocprofvis_profiler_cancel(m_profiler_future);
        }
        
        rocprofvis_controller_future_free(m_profiler_future);
        m_profiler_future = nullptr;
    }
    
    if (m_profiler_config) {
        rocprofvis_profiler_config_free(m_profiler_config);
        m_profiler_config = nullptr;
    }
    
    m_profiler_state = kProfilerStateIdle;
    m_profiler_output_cache.clear();
}

void DataProvider::SetProfilerStateChangedCallback(
    const std::function<void(rocprofvis_profiler_state_t)>& callback)
{
    m_profiler_state_changed_callback = callback;
}
```

**Update in DataProvider::Update():**

```cpp
void DataProvider::Update()
{
    // ... existing update logic ...
    
    // Poll profiler state if active
    if (m_profiler_future) {
        rocprofvis_profiler_state_t current_state = GetProfilerState();
        
        // Notify if state changed
        if (current_state != m_profiler_state) {
            m_profiler_state = current_state;
            if (m_profiler_state_changed_callback) {
                m_profiler_state_changed_callback(m_profiler_state);
            }
        }
    }
}
```

**Future JSON Interface:**

When implementing JSON-based RPC, the DataProvider methods will serialize to JSON and send to remote Controller:

```cpp
// Future implementation example:
bool DataProvider::LaunchProfiler(/* ... */) {
    if (m_use_json_rpc) {
        // Serialize to JSON
        json request = {
            {"method", "profiler.launch"},
            {"params", {
                {"type", profiler_type},
                {"path", profiler_path},
                {"target", target_executable},
                // ...
            }}
        };
        
        // Send to remote Controller via network
        SendJsonRpcRequest(request);
        return true;
    } else {
        // Current implementation: direct C API calls
        // ...
    }
}
```

**Location:** `src/view/src/rocprofiler_session_manager.cpp`

**Key Implementation:**

**Initialization:**
```cpp
void ProfilerSessionManager::Init() {
    // Subscribe to launch events
    m_launch_event_token = EventManager::GetInstance().Subscribe(
        static_cast<int>(RocEvents::kProfilerLaunchRequested),
        [this](std::shared_ptr<RocEvent> event) {
            HandleLaunchEvent(event);
        });
}
```

**Launch Handler:**
```cpp
void ProfilerSessionManager::HandleLaunchEvent(std::shared_ptr<RocEvent> event) {
    auto launch_event = std::static_pointer_cast<ProfilerLaunchEvent>(event);
    std::string session_id = launch_event->GetSessionId();
    
    // Create session
    Session session;
    session.session_id = session_id;
    session.config = launch_event->GetConfig();
    session.controller = std::make_unique<Controller::ProfilerProcessController>();
    session.future = rocprofvis_controller_future_alloc();
    
    // Launch profiler with callbacks
    session.controller->LaunchProfilerAsync(
        session.config,
        session.future,
        [this, session_id](uint64_t percent, const std::string& msg) {
            OnProfilerProgress(session_id, percent, msg);
        },
        [this, session_id](const std::string& output) {
            OnProfilerOutput(session_id, output);
        },
        [this, session_id](rocprofvis_result_t result, const std::string& path) {
            OnProfilerCompleted(session_id, result, path);
        });
    
    m_sessions[session_id] = std::move(session);
}
```

**Callback Routing:**
```cpp
void ProfilerSessionManager::OnProfilerOutput(const std::string& session_id, const std::string& output) {
    // Route back to dialog via event
    auto event = std::make_shared<ProfilerOutputEvent>(session_id, output);
    EventManager::GetInstance().AddEvent(event);
}

void ProfilerSessionManager::OnProfilerCompleted(const std::string& session_id, 
                                                 rocprofvis_result_t result,
                                                 const std::string& trace_path) {
    auto event = std::make_shared<ProfilerCompletedEvent>(session_id, result, trace_path);
    EventManager::GetInstance().AddEvent(event);
    
    // If auto-load enabled and successful, trigger file open
    auto it = m_sessions.find(session_id);
    if (it != m_sessions.end() && it->second.config.auto_load_on_complete && 
        result == kRocProfVisResultSuccess && !trace_path.empty()) {
        
        // Trigger AppWindow to open the trace file
        // (via another event or direct call to AppWindow::OpenFile)
    }
    
    // Cleanup session
    rocprofvis_controller_future_free(it->second.future);
    m_sessions.erase(it);
}
```

---

### 4. AppWindow Integration

#### 4.1 Modified File: `rocprofvis_appwindow.h`

**Location:** `src/view/src/rocprofvis_appwindow.h`

**Changes:**

Add member variable:
```cpp
class AppWindow : public RocWidget {
public:
    // ... existing methods ...
    
    // Public method for dialog to trigger file opening
    void OpenFile(const std::string& file_path);
    
private:
    // ... existing members ...
    std::unique_ptr<ProfilerLauncherDialog> m_profiler_launcher_dialog;
};
```

#### 4.2 Modified File: `rocprofvis_appwindow.cpp`

**Location:** `src/view/src/rocprofvis_appwindow.cpp`

**Changes:**

**Constructor initialization (pass AppWindow* and DataProvider* to dialog):**
```cpp
AppWindow::AppWindow(/* ... */) 
    : m_profiler_launcher_dialog(std::make_unique<ProfilerLauncherDialog>(this, &m_data_provider))
{
    // ... existing initialization ...
}

// Note: AppWindow already has DataProvider member (m_data_provider or GetDataProvider())
// Check existing AppWindow implementation for the exact member name
```

**Add menu item in File menu:**
```cpp
void AppWindow::RenderFileMenu() {
    if (ImGui::BeginMenu("File")) {
        // ... existing menu items ...
        
        if (ImGui::MenuItem("Launch Profiler Session...")) {
            m_profiler_launcher_dialog->Show();
        }
        
        // ... rest of menu ...
        ImGui::EndMenu();
    }
}
```

**Render and Update dialog:**
```cpp
void AppWindow::Render() {
    // ... existing rendering ...
    
    m_profiler_launcher_dialog->Render();
    
    // ... rest of rendering ...
}

void AppWindow::Update() {
    // ... existing update logic ...
    
    // Update profiler dialog state (polls Controller C API)
    m_profiler_launcher_dialog->Update();
    
    // ... rest of update ...
}
```

---

### 6. Settings Persistence

#### 6.1 Modified File: `rocprofvis_settings_manager.h`

**Location:** `src/view/src/rocprofvis_settings_manager.h`

**Changes:**

Add new settings structure:
```cpp
typedef struct ProfilerLauncherSettings {
    std::string last_profiler_type;        // "ROCmSystems", "ROCmCompute", etc.
    std::string last_profiler_path;
    std::string last_output_directory;
    bool auto_load_on_complete;
    std::vector<std::string> recent_targets;  // Recent target executables
} ProfilerLauncherSettings;

typedef struct UserSettings {
    // ... existing fields ...
    ProfilerLauncherSettings profiler_launcher_settings;
} UserSettings;
```

Add JSON keys:
```cpp
constexpr const char* JSON_KEY_PROFILER_LAUNCHER = "profiler_launcher";
constexpr const char* JSON_KEY_PROFILER_TYPE = "last_profiler_type";
constexpr const char* JSON_KEY_PROFILER_PATH = "last_profiler_path";
constexpr const char* JSON_KEY_PROFILER_OUTPUT_DIR = "last_output_directory";
constexpr const char* JSON_KEY_PROFILER_AUTO_LOAD = "auto_load_on_complete";
constexpr const char* JSON_KEY_PROFILER_RECENT_TARGETS = "recent_targets";
```

Add serialization methods:
```cpp
class SettingsManager {
private:
    void SerializeProfilerLauncherSettings(jt::Json& json);
    void DeserializeProfilerLauncherSettings(jt::Json& json);
};
```

#### 6.2 Modified File: `rocprofvis_settings_manager.cpp`

**Location:** `src/view/src/rocprofvis_settings_manager.cpp`

Implement serialization/deserialization following existing patterns for DisplaySettings/UnitSettings.

---

### 7. Build System Changes

#### 7.1 Modified File: `CMakeLists.txt`

**Location:** `CMakeLists.txt` (root or `src/view/CMakeLists.txt` if separate)

**Changes:**

Add new source files to VIEW_FILES list:
```cmake
set(VIEW_FILES
    # ... existing files ...
    src/view/src/widgets/rocprofvis_profiler_launcher_dialog.h
    src/view/src/widgets/rocprofvis_profiler_launcher_dialog.cpp
    src/view/src/rocprofvis_profiler_session_manager.h
    src/view/src/rocprofvis_profiler_session_manager.cpp
)
```

Add new source files to CONTROLLER_FILES list:
```cmake
set(CONTROLLER_FILES
    # ... existing files ...
    src/controller/src/rocprofvis_controller_profiler_process.h
    src/controller/src/rocprofvis_controller_profiler_process.cpp
)
```

---

### 5. Progress Notification Integration (Optional Enhancement)

**Strategy:** Optionally use existing `NotificationManager` for system-wide progress notifications.

**Implementation in Dialog (if notifications desired):**
```cpp
void ProfilerLauncherDialog::PollProfilerState() {
    rocprofvis_profiler_state_t old_state = m_profiler_state;
    rocprofvis_profiler_get_state(m_profiler_future, &m_profiler_state);
    
    // State changed - update notification
    if (old_state != m_profiler_state) {
        switch (m_profiler_state) {
            case kProfilerStateRunning:
                m_state = DialogState::kRunning;
                NotificationManager::GetInstance().Show(
                    "Profiler started",
                    NotificationLevel::Info);
                break;
                
            case kProfilerStateCompleted:
                m_state = DialogState::kCompleted;
                NotificationManager::GetInstance().Show(
                    "Profiling completed successfully!",
                    NotificationLevel::Success);
                
                // Get trace path and auto-load
                uint32_t path_length = 0;
                rocprofvis_profiler_get_trace_path(m_profiler_future, nullptr, &path_length);
                m_trace_path.resize(path_length);
                rocprofvis_profiler_get_trace_path(m_profiler_future, &m_trace_path[0], &path_length);
                
                if (m_auto_load_on_complete && !m_trace_path.empty()) {
                    m_app_window->OpenFile(m_trace_path);
                }
                break;
                
            case kProfilerStateFailed:
            case kProfilerStateCancelled:
                m_state = DialogState::kFailed;
                NotificationManager::GetInstance().Show(
                    "Profiling failed. Check output for details.",
                    NotificationLevel::Error);
                break;
        }
    }
}
```

**Note:** Notifications are optional - the dialog itself shows status, so system notifications are redundant unless user needs background operation alerts.

---

## Communication Flow Diagram

**Correct Architecture (DataProvider abstracts Controller C API):**

```
User clicks "Launch Profiler Session..." in File menu
    ↓
AppWindow::RenderFileMenu() calls ProfilerLauncherDialog::Show()
    ↓
Dialog displays configuration UI (kConfiguring state)
    ↓
User fills in paths, clicks "Launch" button
    ↓
ProfilerLauncherDialog::OnLaunchClicked()
  - Validates configuration
  - Calls m_data_provider->LaunchProfiler(type, path, target, ...) [DataProvider method]
  - Changes state to kLaunching
    ↓
DataProvider::LaunchProfiler()
  - Calls rocprofvis_profiler_config_alloc() [Controller C API]
  - Sets config properties via rocprofvis_profiler_config_set_string() [Controller C API]
  - Calls rocprofvis_profiler_launch_async(config, future) [Controller C API]
  - Stores future in m_profiler_future
  - Returns true if successful
    ↓
Controller (C++ implementation):
  - Receives C API call
  - Creates ProfilerProcessController internally
  - Submits job to JobSystem
  - Returns immediately (async)
    ↓
Job thread (Controller internal):
  - LocalProcessExecutor::Launch() spawns profiler process
  - Monitor thread starts polling process state
    ↓
Monitor thread loop (every 100ms, Controller internal):
  - Read stdout/stderr → accumulate in Future's internal buffer
  - Check process state → update Future's state
  - Store trace path when detected
    ↓
Main thread (View - each frame):
  - AppWindow::Update() calls m_data_provider->Update()
  - DataProvider::Update() polls profiler state, invokes callback if changed
  - AppWindow::Update() calls m_profiler_launcher_dialog->Update()
    ↓
ProfilerLauncherDialog::Update()
  - Calls m_data_provider->GetProfilerState() [DataProvider wraps C API]
  - Calls m_data_provider->GetProfilerOutput() [DataProvider wraps C API]
  - Updates UI with new output and state
    ↓
DataProvider methods internally call:
  - GetProfilerState() → rocprofvis_profiler_get_state(m_profiler_future, &state)
  - GetProfilerOutput() → rocprofvis_profiler_get_output(m_profiler_future, buffer, &length)
    ↓
Process completes:
  - Monitor thread (Controller) detects exit, updates Future state to kCompleted
  - Stores trace path in Future
    ↓
Next frame update:
  - Dialog::Update() calls GetProfilerState(), sees kCompleted
  - Dialog calls m_data_provider->GetProfilerTracePath() [DataProvider method]
  - Changes dialog state to kCompleted
  - If auto_load enabled: calls m_app_window->OpenFile(trace_path)
    ↓
If auto_load enabled:
  - AppWindow::OpenFile() loads trace via DataProvider::FetchTrace()
  - New Project tab appears with profiling data
```

**Key Architecture Principles:**
- ✅ DataProvider is the ONLY component in View that calls Controller C API
- ✅ Dialog calls DataProvider methods, never direct C API
- ✅ Future JSON RPC: DataProvider methods can serialize to JSON instead of calling C API
- ✅ Update() called each frame to poll async state
- ✅ NO EventManager for View-Controller communication (only inter-view)
- ✅ Clean abstraction layer for remote Controller support

---

## File Organization Summary

**New Files:**
```
src/controller/inc/
  (No new headers - profiler API added to existing rocprofvis_controller.h)

src/controller/src/
  ├─ rocprofvis_controller_profiler_process.h  (internal C++ implementation)
  ├─ rocprofvis_controller_profiler_process.cpp (internal C++ implementation)

src/view/src/widgets/
  ├─ rocprofvis_profiler_launcher_dialog.h
  ├─ rocprofvis_profiler_launcher_dialog.cpp
```

**Modified Files:**
```
src/controller/inc/
  ├─ rocprofvis_controller.h          (add profiler C API functions)
  ├─ rocprofvis_controller_enums.h    (add profiler enums: type, state, config properties)

src/view/src/
  ├─ rocprofvis_data_provider.h       (add profiler methods: LaunchProfiler, GetProfilerState, etc.)
  ├─ rocprofvis_data_provider.cpp     (implement profiler methods, wrap C API calls)
  ├─ rocprofvis_appwindow.h           (add dialog member, Update call)
  ├─ rocprofvis_appwindow.cpp         (menu integration, Render+Update calls, pass DataProvider to dialog)
  ├─ rocprofvis_settings_manager.h    (profiler launcher settings)
  ├─ rocprofvis_settings_manager.cpp  (serialization)

CMakeLists.txt                        (add new source files)
```

---

## Platform-Specific Implementation Notes

### Windows (CreateProcess)

**Process Launch:**
```cpp
STARTUPINFO si = {};
si.cb = sizeof(si);
si.dwFlags = STARTF_USESTDHANDLES;
si.hStdOutput = stdout_write;
si.hStdError = stderr_write;

PROCESS_INFORMATION pi = {};

std::string command_line = BuildCommandLine(config);
CreateProcessA(nullptr, &command_line[0], nullptr, nullptr, TRUE, 
               CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);

m_process_handle = pi.hProcess;
CloseHandle(pi.hThread);
```

**Output Reading:**
```cpp
DWORD bytes_available;
PeekNamedPipe(m_stdout_read, nullptr, 0, nullptr, &bytes_available, nullptr);
if (bytes_available > 0) {
    char buffer[4096];
    DWORD bytes_read;
    ReadFile(m_stdout_read, buffer, sizeof(buffer), &bytes_read, nullptr);
    return std::string(buffer, bytes_read);
}
```

**Process State:**
```cpp
DWORD exit_code;
GetExitCodeProcess(m_process_handle, &exit_code);
return (exit_code == STILL_ACTIVE) ? ProcessState::kRunning : ProcessState::kCompleted;
```

### Linux (fork + execvp)

**Process Launch:**
```cpp
int stdout_pipe[2], stderr_pipe[2];
pipe(stdout_pipe);
pipe(stderr_pipe);

pid_t pid = fork();
if (pid == 0) {
    // Child process
    dup2(stdout_pipe[1], STDOUT_FILENO);
    dup2(stderr_pipe[1], STDERR_FILENO);
    close(stdout_pipe[0]);
    close(stderr_pipe[0]);
    
    std::vector<char*> argv = BuildArgv(config);
    execvp(argv[0], argv.data());
    _exit(1);
} else {
    // Parent
    m_pid = pid;
    m_stdout_fd = stdout_pipe[0];
    m_stderr_fd = stderr_pipe[0];
    close(stdout_pipe[1]);
    close(stderr_pipe[1]);
}
```

**Output Reading:**
```cpp
fd_set read_fds;
FD_ZERO(&read_fds);
FD_SET(m_stdout_fd, &read_fds);
struct timeval timeout = {0, 0};  // Non-blocking

if (select(m_stdout_fd + 1, &read_fds, nullptr, nullptr, &timeout) > 0) {
    char buffer[4096];
    ssize_t bytes_read = read(m_stdout_fd, buffer, sizeof(buffer));
    if (bytes_read > 0) {
        return std::string(buffer, bytes_read);
    }
}
```

**Process State:**
```cpp
int status;
pid_t result = waitpid(m_pid, &status, WNOHANG);
if (result == 0) return ProcessState::kRunning;
if (WIFEXITED(status)) {
    m_exit_code = WEXITCODE(status);
    return ProcessState::kCompleted;
}
```

---

## Phase 2 Preparation: Remote Profiling Architecture

The Phase 1 design is architected for easy Phase 2 extension:

**Abstract Interface:**
- `IProcessExecutor` allows swapping `LocalProcessExecutor` for `SSHProcessExecutor`

**Future Implementation:**
```cpp
class SSHProcessExecutor : public IProcessExecutor {
    LIBSSH2_SESSION* m_session;
    LIBSSH2_CHANNEL* m_channel;
    
    rocprofvis_result_t Connect(const std::string& hostname, const std::string& username);
    rocprofvis_result_t Launch(const ProfilerLaunchConfig& config) override;
    // Execute profiler command via libssh2_channel_exec()
    // Read output via libssh2_channel_read()
    // Transfer trace file via libssh2_scp_recv()
};
```

**Dialog Extension:**
- Add "Execution Mode" radio buttons: ⚪ Local  ⚪ Remote
- Show SSH configuration fields when Remote selected (hostname, username, auth method)
- ProfilerSessionManager selects executor based on mode

---

## Testing Strategy

### Unit Tests

1. **LocalProcessExecutor Tests:**
   - Test process launch with mock script
   - Verify stdout/stderr capture
   - Test process termination (graceful and forced)
   - Test state transitions

2. **ProfilerProcessController Tests:**
   - Mock executor to test callback invocation
   - Test Future integration
   - Test cancellation

3. **Event Tests:**
   - Verify ProfilerLaunchEvent dispatches correctly
   - Test event serialization (session_id, config)

### Integration Tests

1. **End-to-End Workflow:**
   - Launch dummy profiler (shell script that prints output and exits)
   - Verify output captured in dialog
   - Verify completion event fires
   - Verify trace file auto-load (if enabled)

2. **Settings Persistence:**
   - Set profiler paths, save settings
   - Restart app, verify paths restored

### Manual Testing

1. **Real Profiler Integration:**
   - Test with actual `rocprof-sys` on Linux
   - Test with actual `rocprof` on Windows/Linux
   - Verify trace files generated correctly

2. **Edge Cases:**
   - Invalid profiler path → Error message shown
   - Profiler crashes → Detect failure, show error
   - Cancel during profiling → Process terminated cleanly
   - No trace file generated → Warning shown

3. **UI/UX:**
   - Dialog responsive during long profiling sessions
   - Output scrolling works smoothly
   - Progress updates visible
   - Auto-close on completion works

---

## Implementation Sequencing

### Phase 1A: Single-Stage Profiler Support (Weeks 1-3)

**Week 1-2: Controller Infrastructure**
1. Implement `IProcessExecutor` interface and `LocalProcessExecutor` (Windows + Linux)
2. Implement `ProfilerProcessController` with stage-based execution helper
3. Implement `ExecuteStage()` helper method (reusable for single and multi-stage)
4. Include full `ProfilerLaunchConfig` struct with instrumentation fields (unused but present)
5. Write unit tests for process execution
6. Test with simple shell scripts

**Week 3: View Layer**
1. Implement `ProfilerLauncherDialog` UI with collapsible instrumentation section
2. Wire up DataProvider integration (include instrumentation config methods)
3. Add profiler C API functions to Controller header
4. Integrate dialog with AppWindow menu
5. Add settings persistence to SettingsManager (include instrumentation fields)

**Week 4: Testing & Polish (Phase 1A)**
1. Test with `rocprof-sys-sample` (single-stage workflow)
2. Test with `rocprof` (single-stage workflow)
3. Cross-platform testing (Windows + Linux)
4. Verify auto-load trace file works
5. Error handling refinement
6. Documentation and code review

### Phase 1B: Multi-Stage Workflow Support (Week 5)

**Week 5: Multi-Stage Enhancement**
1. Update dialog to show multi-stage section by default for systems/compute profilers
2. Set default values based on profiler type:
   - **Systems:** Stage 1 = `rocprof-sys-instrument`, Stage 2 = `rocprof-sys-run`
   - **Compute:** Stage 1 = `rocprof-compute` (capture), Stage 2 = `rocprof-compute --analysis`
3. Change `requires_two_stage_workflow` default based on profiler type
4. Test two-stage workflows:
   - **Systems:** `rocprof-sys-instrument -o ./app.inst -- ./app` → `rocprof-sys-run -- ./app.inst [args]`
   - **Compute:** `rocprof-compute [capture_flags] -- ./app` → `rocprof-compute --analysis <captured_data>`
5. Verify intermediate file management (`.inst` binaries, capture data)
6. Test sampling-only mode (`-M sampling`)
7. Update documentation with multi-stage examples for both profiler types
8. Code review and final polish

**Note:** Phase 1B is a **minimal extension** of Phase 1A because the architecture was designed with multi-stage support from day 1. Most code is reused; only configuration defaults and UI visibility change.

---

## Risk Mitigation

### Risk 1: Platform-Specific Process Handling Complexity
**Mitigation:**
- Abstract via `IProcessExecutor` interface
- Unit test each platform separately
- Consider using Boost.Process library if native APIs prove too complex

### Risk 2: Zombie Processes on Crash
**Mitigation:**
- RAII wrappers for process handles (`std::unique_ptr` with custom deleters)
- Ensure `Terminate()` called in `LocalProcessExecutor` destructor
- Cleanup logic in `ProfilerSessionManager::Shutdown()`

### Risk 3: Trace File Auto-Detection Fragility
**Mitigation:**
- Parse profiler stdout for explicit output path
- Fallback: scan `output_directory` for newly created `.db`/`.rpd` files (compare timestamps)
- Allow user to manually specify output file path

### Risk 4: UI Responsiveness During Long Profiling
**Mitigation:**
- All profiler execution happens in background threads (JobSystem)
- ImGui rendering loop never blocks
- Output updates batched (max 100ms intervals)

---

## Verification Plan

### End-to-End Testing

1. **Basic Workflow:**
   - Open visualizer
   - Click "Launch Profiler Session..."
   - Configure profiler (rocprof-sys, target app, output dir)
   - Click "Launch"
   - Verify output appears in dialog
   - Wait for completion
   - Verify trace file auto-loads
   - Verify data displayed correctly

2. **Settings Persistence:**
   - Configure profiler paths
   - Close visualizer
   - Reopen visualizer
   - Click "Launch Profiler Session..."
   - Verify paths restored from last session

3. **Cancellation:**
   - Launch long-running profiler
   - Click "Cancel" mid-execution
   - Verify process terminated
   - Verify no zombie processes (`ps aux | grep rocprof` on Linux)

4. **Error Handling:**
   - Invalid profiler path → Shows error message
   - Missing target executable → Shows error message
   - Profiler crashes → Shows failure message, does not hang

---

## Critical Files for Implementation

**Phase 1A (Single-Stage Support) and 1B (Multi-Stage Support):**

**New Files:**
- `src/controller/src/rocprofvis_controller_profiler_process.h` (internal C++ implementation with stage-based execution)
- `src/controller/src/rocprofvis_controller_profiler_process.cpp` (internal C++ implementation)
- `src/view/src/widgets/rocprofvis_profiler_launcher_dialog.h` (dialog UI with collapsible instrumentation section)
- `src/view/src/widgets/rocprofvis_profiler_launcher_dialog.cpp` (dialog implementation)

**Modified Files:**
- `src/controller/inc/rocprofvis_controller.h` (add profiler C API functions)
- `src/controller/inc/rocprofvis_controller_enums.h` (add profiler enums including multi-stage properties)
- `src/view/src/rocprofvis_data_provider.h` (add profiler methods with multi-stage config)
- `src/view/src/rocprofvis_data_provider.cpp` (implement profiler methods wrapping C API)
- `src/view/src/rocprofvis_appwindow.h` (add dialog member, Update call)
- `src/view/src/rocprofvis_appwindow.cpp` (menu integration, rendering, update loop, pass DataProvider)
- `src/view/src/rocprofvis_settings_manager.h` (add profiler settings struct with multi-stage fields)
- `src/view/src/rocprofvis_settings_manager.cpp` (serialization methods)
- `CMakeLists.txt` (add new source files)

**Phase 1A → 1B Transition (Minimal Changes):**
- Change `requires_two_stage_workflow` default based on profiler type
- Show multi-stage UI section by default for systems/compute profilers
- Set default stage commands based on profiler type:
  - Systems: stage1 = `rocprof-sys-instrument`, stage2 = `rocprof-sys-run`
  - Compute: stage1 = `rocprof-compute`, stage2 = `rocprof-compute --analysis`
- No architecture or API changes required

---

## Open Questions & Decisions

**Q1: Should we auto-detect installed profilers (scan PATH)?**  
**Decision:** No (per user preference). Manual path entry only. Users must provide explicit path to profiler executable.

**Q2: Support multiple concurrent profiling sessions?**  
**Decision:** Phase 1 - single session only (simplest). Phase 2 - could add support for multiple sessions via session list UI.

**Q3: Progress estimation strategy?**  
**Decision:** Indeterminate progress (pulse animation) for Phase 1A/1B. Future: parse profiler stdout for progress indicators if available.

**Q4: Trace file naming to avoid overwrites?**  
**Recommendation:** Let profilers use their default naming (usually includes timestamp). User can override via profiler flags.

**Q5: Profiler presets (e.g., "Quick Profile", "Full Trace")?**  
**Decision:** Not in Phase 1A/1B. Future enhancement could add preset dropdown with common flag combinations.

**Q6: Single-stage first (Option B) or multi-stage from start (Option A)?**  
**Decision:** Option B with future-proofing. Implement Phase 1A (single-stage) first with full config struct and stage-based execution logic. Phase 1B (multi-stage) becomes a trivial extension (~1 day) by changing defaults and showing hidden UI sections. This provides incremental value delivery with minimal rework risk.

**Q7: How to handle profiler-specific stage configurations?**  
**Decision:** Use generic "stage 1" and "stage 2" naming in architecture, but provide profiler-specific UI labels and tooltips. Systems profiler uses "Instrument → Run", compute profiler uses "Capture → Analyze". Both fit the same two-stage execution pattern.

---

This plan provides a complete, executable roadmap for implementing the profiler launcher feature with clear file organization, event flows, and platform-specific implementation details aligned with the existing codebase architecture.
