// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Windows: ensure NOMINMAX is defined before any header drags in windows.h,
// so std::min/std::max are not shadowed by the min/max macros.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "rocprofvis_controller.h"
#include "profiler/rocprofvis_controller_profiler_process.h"
#include "profiler/rocprofvis_controller_profiler_scrape_rules.h"
#include "profiler/rocprofvis_controller_profiler_tool.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>

// The pipeline loop: how ProfilerProcessController turns a list of stages into a
// sequence of child processes, and what it reports when one of them does not do
// what the next stage needs.
//
// These drive the controller's C++ classes directly because the stage C ABI does
// not exist yet. rocprofvis_controller_profiler_pipeline_tests.cpp covers the
// same ground through that ABI and is held out of the build until it lands.
//
// A stage names a tool rather than a path, so a test that wants to run a script
// installs the platform shell under a real tool's filename. That is not a
// shortcut around the tool table - it is the same mechanism a user with ROCm in
// a non-standard location uses - and it means the stage picks up that tool's
// shipped scrape rules, so the fixtures below print what the real tool prints.

using RocProfVis::Controller::ProfilerConfig;
using RocProfVis::Controller::ProfilerProcessController;
using RocProfVis::Controller::ProfilerStageSpec;
namespace ProfilerTool = RocProfVis::Controller::ProfilerTool;

namespace
{

#ifdef _WIN32
constexpr char const* kRealExecutable       = "C:\\Windows\\System32\\cmd.exe";
constexpr char const* kMissingToolDirectory = "C:\\nonexistent-roc-optiq-tool-directory";
#else
constexpr char const* kRealExecutable       = "/bin/sh";
constexpr char const* kMissingToolDirectory = "/nonexistent-roc-optiq-tool-directory";
#endif

std::string tool_file_name(rocprofvis_profiler_tool_t tool)
{
    std::string name = ProfilerTool::GetBinaryName(tool);
#ifdef _WIN32
    name += ".exe";
#endif
    return name;
}

class ScratchToolDir
{
public:
    explicit ScratchToolDir(rocprofvis_profiler_tool_t tool)
        : m_dir(std::filesystem::temp_directory_path() /
                ("roc-optiq pipeline tool " + tool_file_name(tool)))
    {
        std::filesystem::remove_all(m_dir);
        std::filesystem::create_directories(m_dir);
        // A copy rather than a link so this behaves the same everywhere;
        // copy_file carries the executable bit across on POSIX.
        std::filesystem::copy_file(kRealExecutable, m_dir / tool_file_name(tool));
    }

    ~ScratchToolDir() { std::filesystem::remove_all(m_dir); }

    ScratchToolDir(ScratchToolDir const&)            = delete;
    ScratchToolDir& operator=(ScratchToolDir const&) = delete;

    std::string Path() const { return m_dir.string(); }

private:
    std::filesystem::path m_dir;
};

// A directory a stage can run in or move a file to.
class ScratchDir
{
public:
    explicit ScratchDir(char const* name, bool create = true)
        : m_dir(std::filesystem::temp_directory_path() / (std::string("roc-optiq ") + name))
    {
        std::filesystem::remove_all(m_dir);
        if (create)
        {
            std::filesystem::create_directories(m_dir);
        }
    }

    ~ScratchDir() { std::filesystem::remove_all(m_dir); }

    ScratchDir(ScratchDir const&)            = delete;
    ScratchDir& operator=(ScratchDir const&) = delete;

    std::string           Path() const { return m_dir.string(); }
    std::filesystem::path File(char const* leaf) const { return m_dir / leaf; }

private:
    std::filesystem::path m_dir;
};

ProfilerStageSpec tool_stage(char const*                     label,
                             ScratchToolDir const&           dir,
                             rocprofvis_profiler_tool_t      tool = kRPVProfilerToolRocprofSysRun,
                             rocprofvis_profiler_operation_t operation =
                                 kRPVProfilerOperationDefault)
{
    ProfilerStageSpec stage;
    stage.label          = label;
    stage.tool           = tool;
    stage.operation      = operation;
    stage.tool_directory = dir.Path();
    return stage;
}

struct PipelineResult
{
    rocprofvis_result_t         launched  = kRocProfVisResultUnknownError;
    rocprofvis_profiler_state_t state     = kRPVProfilerStateIdle;
    int                         exit_code = -1;
    std::string                 output;
};

// Launches, then pumps the monitor loop on this thread until it finishes, which
// is what the job system does for the ABI.
PipelineResult run_pipeline(ProfilerProcessController& controller, ProfilerConfig const& config)
{
    PipelineResult result;
    result.launched = controller.LaunchAsync(&config);
    if (result.launched == kRocProfVisResultSuccess)
    {
        ProfilerProcessController::ExecuteJob(&controller, nullptr);
    }
    result.state     = controller.GetState();
    result.exit_code = controller.GetExitCode();
    result.output    = controller.GetOutput();
    return result;
}

rocprofvis_profiler_scrape_status_t status_of(ProfilerProcessController const& controller,
                                             char const*                      key)
{
    rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapePending;
    REQUIRE(controller.GetScrapeStatus(key, status) == kRocProfVisResultSuccess);
    return status;
}

// ==================================================================================
// Pre-flight, which needs no child process and so runs everywhere
// ==================================================================================

TEST_CASE("A missing tool in a later stage fails before the first one runs",
          "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofSysRun);

    ProfilerConfig    config;
    ProfilerStageSpec capture = tool_stage("Capture", tool_dir);
    ProfilerStageSpec analyze = tool_stage("Analyze", tool_dir);
    analyze.tool_directory    = kMissingToolDirectory;

    REQUIRE(config.AddStage(capture) == kRocProfVisResultSuccess);
    REQUIRE(config.AddStage(analyze) == kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    // The whole point of resolving every tool up front: a misconfigured analyze
    // stage must not be discovered after a capture that took twenty minutes.
    CHECK(controller.LaunchAsync(&config) == kRocProfVisResultToolNotFound);

    CHECK(controller.GetState() == kRPVProfilerStateFailed);
    CHECK(controller.GetStageCount() == 2);
    CHECK(controller.GetFailingStage() == 1);

    rocprofvis_profiler_state_t first = kRPVProfilerStateRunning;
    REQUIRE(controller.GetStageState(0, first) == kRocProfVisResultSuccess);
    CHECK(first == kRPVProfilerStateIdle);
    CHECK(controller.GetOutput().empty());
}

TEST_CASE("A config with no stages runs as a single stage", "[profiler][pipeline]")
{
    ProfilerConfig config;
    REQUIRE(config.SetTool(kRPVProfilerToolRocprofSysRun) == kRocProfVisResultSuccess);
    REQUIRE(config.SetToolDirectory(kMissingToolDirectory) == kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    // Fails on the missing tool, but only after the flat config was wrapped as
    // one stage, which is what this is checking.
    CHECK(controller.LaunchAsync(&config) == kRocProfVisResultToolNotFound);
    CHECK(controller.GetStageCount() == 1);
    CHECK(controller.GetFailingStage() == 0);
}

TEST_CASE("A stage index past the end is rejected rather than clamped",
          "[profiler][pipeline]")
{
    ProfilerConfig config;
    REQUIRE(config.SetTool(kRPVProfilerToolRocprofSysRun) == kRocProfVisResultSuccess);
    REQUIRE(config.SetToolDirectory(kMissingToolDirectory) == kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    controller.LaunchAsync(&config);

    rocprofvis_profiler_state_t state = kRPVProfilerStateRunning;
    CHECK(controller.GetStageState(1, state) == kRocProfVisResultInvalidArgument);
    CHECK(state == kRPVProfilerStateRunning);
}

TEST_CASE("A stage naming no tool is refused at authoring time", "[profiler][pipeline]")
{
    ProfilerConfig    config;
    ProfilerStageSpec stage;
    stage.label = "Nameless";
    CHECK(config.AddStage(stage) == kRocProfVisResultInvalidArgument);
    CHECK(config.GetStages().empty());
}

// ==================================================================================
// Running stages. POSIX only, matching the existing process tests: these need a
// shell whose scripting is the same everywhere it runs.
// ==================================================================================

#ifndef _WIN32

ProfilerStageSpec shell_stage(char const*                     label,
                              ScratchToolDir const&           dir,
                              std::string const&              script,
                              rocprofvis_profiler_tool_t      tool = kRPVProfilerToolRocprofSysRun,
                              rocprofvis_profiler_operation_t operation =
                                  kRPVProfilerOperationDefault)
{
    ProfilerStageSpec stage = tool_stage(label, dir, tool, operation);
    stage.argv.push_back("-c");
    stage.argv.push_back(script);
    return stage;
}

TEST_CASE("A two-stage pipeline advances and substitutes the first stage's value",
          "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofSysRun);

    ProfilerConfig config;
    REQUIRE(config.AddStage(shell_stage("Capture", tool_dir,
                                        "echo \"Database: '/tmp/wl.db'\"")) ==
            kRocProfVisResultSuccess);
    REQUIRE(config.AddStage(shell_stage("Analyze", tool_dir,
                                        "echo analyze {stage0.trace_db}")) ==
            kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    PipelineResult            run = run_pipeline(controller, config);

    REQUIRE(run.launched == kRocProfVisResultSuccess);
    CHECK(run.state == kRPVProfilerStateCompleted);
    CHECK(run.exit_code == 0);
    CHECK(controller.GetFailingStage() == -1);

    CHECK(run.output.find("=== Stage 1/2: Capture ===") != std::string::npos);
    CHECK(run.output.find("=== Stage 2/2: Analyze ===") != std::string::npos);
    CHECK(run.output.find("analyze /tmp/wl.db") != std::string::npos);

    rocprofvis_profiler_state_t first  = kRPVProfilerStateIdle;
    rocprofvis_profiler_state_t second = kRPVProfilerStateIdle;
    REQUIRE(controller.GetStageState(0, first) == kRocProfVisResultSuccess);
    REQUIRE(controller.GetStageState(1, second) == kRocProfVisResultSuccess);
    CHECK(first == kRPVProfilerStateCompleted);
    CHECK(second == kRPVProfilerStateCompleted);

    // No artifact key was set: the rule table already says a rocprof-sys stage
    // produces trace_db.
    std::string artifact;
    REQUIRE(controller.GetArtifactPath(artifact) == kRocProfVisResultSuccess);
    CHECK(artifact == "/tmp/wl.db");
}

TEST_CASE("A single-stage run's console is unchanged by the pipeline",
          "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofSysRun);

    ProfilerConfig config;
    REQUIRE(config.SetTool(kRPVProfilerToolRocprofSysRun) == kRocProfVisResultSuccess);
    REQUIRE(config.SetToolDirectory(tool_dir.Path().c_str()) == kRocProfVisResultSuccess);
    REQUIRE(config.AddProfilerArg("-c") == kRocProfVisResultSuccess);
    REQUIRE(config.AddProfilerArg("echo flat") == kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    PipelineResult            run = run_pipeline(controller, config);

    CHECK(run.state == kRPVProfilerStateCompleted);
    CHECK(controller.GetStageCount() == 1);
    // Byte-for-byte what this run printed before there was a pipeline: one
    // stage gets no banner, because every caller today is one stage.
    CHECK(run.output == "flat\n");
}

TEST_CASE("A failing stage halts the pipeline and attributes the failure",
          "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofSysRun);

    ProfilerConfig config;
    REQUIRE(config.AddStage(shell_stage("First", tool_dir, "exit 7")) ==
            kRocProfVisResultSuccess);
    REQUIRE(config.AddStage(shell_stage("Second", tool_dir, "echo SHOULD_NOT_RUN")) ==
            kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    PipelineResult            run = run_pipeline(controller, config);

    CHECK(run.state == kRPVProfilerStateFailed);
    CHECK(run.exit_code == 7);
    CHECK(run.output.find("SHOULD_NOT_RUN") == std::string::npos);
    // "Capture succeeded, analyze failed" is a different situation for the user
    // than "capture failed", so which one it was has to survive the run.
    CHECK(controller.GetFailingStage() == 0);

    rocprofvis_profiler_state_t second = kRPVProfilerStateRunning;
    REQUIRE(controller.GetStageState(1, second) == kRocProfVisResultSuccess);
    CHECK(second == kRPVProfilerStateIdle);

    // Both stages run rocprof-sys and so both declare trace_db; a key-only
    // query answers from the last, which never ran.
    CHECK(status_of(controller, "trace_db") == kRPVProfilerScrapeStageSkipped);
}

TEST_CASE("An unresolved placeholder fails the pipeline with an explanation",
          "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofSysRun);

    ProfilerConfig config;
    // Prints nothing matching trace_db, so the next stage has nothing to
    // substitute.
    REQUIRE(config.AddStage(shell_stage("First", tool_dir, "echo hello")) ==
            kRocProfVisResultSuccess);
    REQUIRE(config.AddStage(shell_stage("Second", tool_dir, "echo {stage0.trace_db}")) ==
            kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    PipelineResult            run = run_pipeline(controller, config);

    CHECK(run.state == kRPVProfilerStateFailed);
    // A half-substituted command line is never run; the message says which
    // layer went wrong.
    CHECK(run.output.find("did not report") != std::string::npos);
    CHECK(controller.GetFailingStage() == 1);
}

TEST_CASE("A placeholder that names a key no stage produces says so",
          "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofSysRun);

    ProfilerConfig config;
    REQUIRE(config.AddStage(shell_stage("First", tool_dir, "echo hello")) ==
            kRocProfVisResultSuccess);
    REQUIRE(config.AddStage(shell_stage("Second", tool_dir, "echo {stage0.typo_dir}")) ==
            kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    PipelineResult            run = run_pipeline(controller, config);

    CHECK(run.state == kRPVProfilerStateFailed);
    CHECK(run.output.find("no stage produces") != std::string::npos);
    CHECK(controller.GetFailingStage() == 1);
}

TEST_CASE("Placeholders resolve in the working directory and env, not just argv",
          "[profiler][pipeline]")
{
    ScratchToolDir compute_dir(kRPVProfilerToolRocprofCompute);
    ScratchToolDir sys_dir(kRPVProfilerToolRocprofSysRun);
    ScratchDir     produced("pipeline placeholder cwd");

    ProfilerConfig config;
    REQUIRE(config.AddStage(shell_stage("Capture", compute_dir,
                                        "echo 'Output directory: " + produced.Path() + "'",
                                        kRPVProfilerToolRocprofCompute,
                                        kRPVProfilerOperationCapture)) ==
            kRocProfVisResultSuccess);

    // Steering compute's analyze means pointing its cwd at a directory an
    // earlier stage reported, so the substitution has to reach past argv.
    ProfilerStageSpec analyze =
        shell_stage("Analyze", sys_dir, "printf 'ok' > from-cwd.txt; printf '%s' \"$STAGE_DIR\"");
    analyze.working_directory = "{stage0.workload_dir}";
    analyze.env.emplace_back("STAGE_DIR", "{stage0.workload_dir}");
    REQUIRE(config.AddStage(analyze) == kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    PipelineResult            run = run_pipeline(controller, config);

    CHECK(run.state == kRPVProfilerStateCompleted);
    // Written by the child through a relative path, so it landed wherever the
    // substituted working directory pointed.
    CHECK(std::filesystem::exists(produced.File("from-cwd.txt")));
    CHECK(run.output.find(produced.Path()) != std::string::npos);
}

TEST_CASE("Cancelling mid-pipeline abandons the later stages", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofSysRun);

    ProfilerConfig config;
    REQUIRE(config.AddStage(shell_stage("First", tool_dir, "sleep 30")) ==
            kRocProfVisResultSuccess);
    REQUIRE(config.AddStage(shell_stage("Second", tool_dir, "echo SHOULD_NOT_RUN")) ==
            kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    REQUIRE(controller.LaunchAsync(&config) == kRocProfVisResultSuccess);

    std::thread monitor(
        [&controller] { ProfilerProcessController::ExecuteJob(&controller, nullptr); });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    REQUIRE(controller.Cancel() == kRocProfVisResultSuccess);
    monitor.join();

    CHECK(controller.GetState() == kRPVProfilerStateCancelled);
    CHECK(controller.GetOutput().find("SHOULD_NOT_RUN") == std::string::npos);
    CHECK(status_of(controller, "trace_db") == kRPVProfilerScrapeStageSkipped);
}

TEST_CASE("A completed run with nothing scraped reports no artifact",
          "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofSysRun);

    ProfilerConfig config;
    REQUIRE(config.AddStage(shell_stage("Only", tool_dir, "echo hello")) ==
            kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    PipelineResult            run = run_pipeline(controller, config);

    // The run itself succeeded; only the artifact is missing, and those are
    // different facts.
    CHECK(run.state == kRPVProfilerStateCompleted);
    CHECK(status_of(controller, "trace_db") == kRPVProfilerScrapeUnmatched);

    std::string artifact = "untouched";
    CHECK(controller.GetArtifactPath(artifact) == kRocProfVisResultNotAvailable);
    CHECK(artifact == "untouched");
}

TEST_CASE("A relocated artifact is reported at its destination", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofCompute);
    ScratchDir     scratch("pipeline relocate src");
    ScratchDir     destination("pipeline relocate dest", false);

    ProfilerStageSpec stage =
        shell_stage("Analyze", tool_dir,
                    "printf 'hello' > artifact.db; echo 'Created file: artifact.db'",
                    kRPVProfilerToolRocprofCompute, kRPVProfilerOperationAnalyze);
    stage.working_directory = scratch.Path();
    stage.relocate_to       = destination.Path();

    ProfilerConfig config;
    REQUIRE(config.AddStage(stage) == kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    PipelineResult            run = run_pipeline(controller, config);

    CHECK(run.state == kRPVProfilerStateCompleted);

    // The reported path is the final one, so nothing downstream has to know a
    // move happened.
    std::string artifact;
    REQUIRE(controller.GetArtifactPath(artifact) == kRocProfVisResultSuccess);
    CHECK(std::filesystem::path(artifact) == destination.File("artifact.db"));
    CHECK(std::filesystem::exists(destination.File("artifact.db")));
    CHECK_FALSE(std::filesystem::exists(scratch.File("artifact.db")));
}

TEST_CASE("A failed relocation keeps the artifact and stays resolved",
          "[profiler][pipeline]")
{
    ScratchToolDir tool_dir(kRPVProfilerToolRocprofCompute);
    ScratchDir     scratch("pipeline relocate fail src");
    ScratchDir     destination("pipeline relocate fail dest");
    {
        // Not produced by this run, so the move must refuse rather than
        // overwrite it.
        std::ofstream blocker(destination.File("artifact.db"));
        blocker << "existing";
    }

    ProfilerStageSpec stage =
        shell_stage("Analyze", tool_dir,
                    "printf 'hello' > artifact.db; echo 'Created file: artifact.db'",
                    kRPVProfilerToolRocprofCompute, kRPVProfilerOperationAnalyze);
    stage.working_directory = scratch.Path();
    stage.relocate_to       = destination.Path();

    ProfilerConfig config;
    REQUIRE(config.AddStage(stage) == kRocProfVisResultSuccess);

    ProfilerProcessController controller;
    PipelineResult            run = run_pipeline(controller, config);

    // Analysis succeeded and a valid file exists, so this is a warning about
    // where it ended up, not an unresolved artifact.
    CHECK(run.state == kRPVProfilerStateCompleted);
    CHECK(status_of(controller, "analysis_db") == kRPVProfilerScrapeResolved);

    std::string artifact;
    REQUIRE(controller.GetArtifactPath(artifact) == kRocProfVisResultSuccess);
    CHECK(std::filesystem::path(artifact) == scratch.File("artifact.db"));
    CHECK(std::filesystem::exists(scratch.File("artifact.db")));
    CHECK(run.output.find("leaving it at the original path") != std::string::npos);
}

#endif  // !_WIN32

}  // namespace
