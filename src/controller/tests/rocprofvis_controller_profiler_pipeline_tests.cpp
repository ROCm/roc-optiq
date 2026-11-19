// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

// Stage sequencing through the C ABI. The rules these stages scrape with come
// from the controller's rule table, so a stage here names a real tool and the
// scratch directory supplies a shell under that tool's filename.

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#endif

#include "rocprofvis_controller.h"
#include "rocprofvis_profiler.h"

#include <catch2/catch_test_macros.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifndef _WIN32
#include <chrono>
#endif

namespace
{

void check_buffer_untouched(rocprofvis_result_t result, char const* sentinel, uint32_t length)
{
    CHECK(result != kRocProfVisResultSuccess);
    for (uint32_t i = 0; i < length; ++i)
    {
        CHECK(static_cast<unsigned char>(sentinel[i]) == 0xAB);
    }
}

} // namespace

#ifndef _WIN32

constexpr char const* kRealExecutable = "/bin/sh";
constexpr char const* kMissingToolDirectory = "/nonexistent-roc-optiq-tool-directory";

/*
 * A directory holding /bin/sh under a profiler's real filename, so a stage can
 * name a tool by enum - and therefore pick up that tool's shipped scrape rules
 * - while actually running a shell script we control.
 */
class ScratchToolDir
{
public:
    explicit ScratchToolDir(char const* tool_file_name)
        : m_tool_file_name(tool_file_name)
        , m_dir(std::filesystem::temp_directory_path() /
                ("roc-optiq pipeline tool " + std::string(tool_file_name)))
    {
        std::filesystem::remove_all(m_dir);
        std::filesystem::create_directories(m_dir);
        std::filesystem::copy_file(kRealExecutable, m_dir / m_tool_file_name);
    }

    ~ScratchToolDir() { std::filesystem::remove_all(m_dir); }

    ScratchToolDir(ScratchToolDir const&)            = delete;
    ScratchToolDir& operator=(ScratchToolDir const&) = delete;

    std::string Path() const { return m_dir.string(); }

private:
    std::string           m_tool_file_name;
    std::filesystem::path m_dir;
};

void use_tool_in_stage(rocprofvis_profiler_stage_t*    stage,
                       ScratchToolDir const&           dir,
                       rocprofvis_profiler_tool_t      tool      = kRPVProfilerToolRocprofSysRun,
                       rocprofvis_profiler_operation_t operation = kRPVProfilerOperationDefault)
{
    REQUIRE(rocprofvis_profiler_stage_set_tool(stage, tool) == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_profiler_stage_set_operation(stage, operation) ==
            kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_profiler_stage_set_tool_directory(stage, dir.Path().c_str()) ==
            kRocProfVisResultSuccess);
}

struct PipelineRun
{
    rocprofvis_profiler_t*          profiler = nullptr;
    rocprofvis_controller_future_t* future   = nullptr;
    rocprofvis_result_t             launched = kRocProfVisResultUnknownError;
    rocprofvis_profiler_state_t     state    = kRPVProfilerStateIdle;
    int32_t                         exit_code = -1;
    std::string                     output;

    ~PipelineRun()
    {
        if (future != nullptr)
        {
            rocprofvis_controller_future_free(future);
        }
        if (profiler != nullptr)
        {
            rocprofvis_profiler_free(profiler);
        }
    }
};

PipelineRun launch_pipeline(rocprofvis_profiler_config_t* config, bool wait)
{
    PipelineRun run;
    run.profiler = rocprofvis_profiler_alloc();
    run.future   = rocprofvis_controller_future_alloc();
    REQUIRE(run.profiler != nullptr);
    REQUIRE(run.future != nullptr);

    run.launched = rocprofvis_profiler_launch_async(run.profiler, config, run.future);
    if (wait && run.launched == kRocProfVisResultSuccess)
    {
        REQUIRE(rocprofvis_controller_future_wait(run.future, 30.0f) !=
                kRocProfVisResultTimeout);
        uint32_t length = 0;
        rocprofvis_profiler_get_output(run.profiler, nullptr, &length);
        if (length > 0)
        {
            std::vector<char> buffer(length + 1, '\0');
            rocprofvis_profiler_get_output(run.profiler, buffer.data(), &length);
            run.output.assign(buffer.data());
        }
    }
    rocprofvis_profiler_get_state(run.profiler, &run.state);
    rocprofvis_profiler_get_exit_code(run.profiler, &run.exit_code);
    return run;
}

TEST_CASE("Two-stage pipeline advances and substitutes placeholders", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir("rocprof-sys-run");
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_stage_t* capture = rocprofvis_profiler_stage_alloc();
    rocprofvis_profiler_stage_t* analyze = rocprofvis_profiler_stage_alloc();
    REQUIRE(capture != nullptr);
    REQUIRE(analyze != nullptr);

    // The stage names a tool, not a pattern; the trace_db rule the controller
    // picks for rocprof-sys-run is what reads this line.
    rocprofvis_profiler_stage_set_label(capture, "Capture");
    use_tool_in_stage(capture, tool_dir);
    rocprofvis_profiler_stage_add_arg(capture, "-c");
    rocprofvis_profiler_stage_add_arg(capture, "echo \"Database: '/tmp/wl.db'\"");

    rocprofvis_profiler_stage_set_label(analyze, "Analyze");
    use_tool_in_stage(analyze, tool_dir);
    rocprofvis_profiler_stage_add_arg(analyze, "-c");
    rocprofvis_profiler_stage_add_arg(analyze, "echo \"analyze {stage0.trace_db}\"");

    REQUIRE(rocprofvis_profiler_config_add_stage(config, capture) == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_profiler_config_add_stage(config, analyze) == kRocProfVisResultSuccess);

    PipelineRun run = launch_pipeline(config, true);
    CHECK(run.state == kRPVProfilerStateCompleted);
    CHECK(run.output.find("=== Stage 1/2: Capture ===") != std::string::npos);
    CHECK(run.output.find("=== Stage 2/2: Analyze ===") != std::string::npos);
    CHECK(run.output.find("analyze /tmp/wl.db") != std::string::npos);

    uint32_t count = 0;
    REQUIRE(rocprofvis_profiler_get_stage_count(run.profiler, &count) ==
            kRocProfVisResultSuccess);
    CHECK(count == 2);

    rocprofvis_profiler_state_t stage0 = kRPVProfilerStateIdle;
    rocprofvis_profiler_state_t stage1 = kRPVProfilerStateIdle;
    REQUIRE(rocprofvis_profiler_get_stage_state(run.profiler, 0, &stage0) ==
            kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_profiler_get_stage_state(run.profiler, 1, &stage1) ==
            kRocProfVisResultSuccess);
    CHECK(stage0 == kRPVProfilerStateCompleted);
    CHECK(stage1 == kRPVProfilerStateCompleted);

    uint32_t length = 0;
    REQUIRE(rocprofvis_profiler_get_artifact_path(run.profiler, nullptr, &length) ==
            kRocProfVisResultSuccess);
    // No config_set_artifact_key call: the rule table already declared
    // trace_db as what a rocprof-sys stage produces.
    std::vector<char> buffer(length + 1, '\0');
    REQUIRE(rocprofvis_profiler_get_artifact_path(run.profiler, buffer.data(), &length) ==
            kRocProfVisResultSuccess);
    CHECK(std::string(buffer.data()) == "/tmp/wl.db");

    rocprofvis_profiler_stage_free(capture);
    rocprofvis_profiler_stage_free(analyze);
    rocprofvis_profiler_config_free(config);
}

TEST_CASE("A failing stage halts the pipeline and attributes the failure", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir("rocprof-sys-run");
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_stage_t* first  = rocprofvis_profiler_stage_alloc();
    rocprofvis_profiler_stage_t* second = rocprofvis_profiler_stage_alloc();
    use_tool_in_stage(first, tool_dir);
    use_tool_in_stage(second, tool_dir);
    rocprofvis_profiler_stage_add_arg(first, "-c");
    rocprofvis_profiler_stage_add_arg(first, "exit 7");
    rocprofvis_profiler_stage_add_arg(second, "-c");
    rocprofvis_profiler_stage_add_arg(second, "echo SHOULD_NOT_RUN");

    rocprofvis_profiler_config_add_stage(config, first);
    rocprofvis_profiler_config_add_stage(config, second);

    PipelineRun run = launch_pipeline(config, true);
    CHECK(run.state == kRPVProfilerStateFailed);
    CHECK(run.exit_code == 7);
    CHECK(run.output.find("SHOULD_NOT_RUN") == std::string::npos);

    int32_t failing = 99;
    REQUIRE(rocprofvis_profiler_get_failing_stage(run.profiler, &failing) ==
            kRocProfVisResultSuccess);
    CHECK(failing == 0);

    // Both stages run rocprof-sys and so both declare trace_db. The key-only
    // query answers from the last stage, which never ran.
    rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapePending;
    REQUIRE(rocprofvis_profiler_get_scrape_status(run.profiler, "trace_db", &status) ==
            kRocProfVisResultSuccess);
    CHECK(status == kRPVProfilerScrapeStageSkipped);

    rocprofvis_profiler_stage_free(first);
    rocprofvis_profiler_stage_free(second);
    rocprofvis_profiler_config_free(config);
}

TEST_CASE("An unresolved placeholder fails the pipeline with a message", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir("rocprof-sys-run");
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_stage_t* first  = rocprofvis_profiler_stage_alloc();
    rocprofvis_profiler_stage_t* second = rocprofvis_profiler_stage_alloc();
    use_tool_in_stage(first, tool_dir);
    use_tool_in_stage(second, tool_dir);
    rocprofvis_profiler_stage_add_arg(first, "-c");
    // Stage 0 prints nothing matching trace_db, so stage 1's placeholder has
    // nothing to resolve against.
    rocprofvis_profiler_stage_add_arg(first, "echo hello");
    rocprofvis_profiler_stage_add_arg(second, "-c");
    rocprofvis_profiler_stage_add_arg(second, "echo {stage0.trace_db}");

    rocprofvis_profiler_config_add_stage(config, first);
    rocprofvis_profiler_config_add_stage(config, second);

    PipelineRun run = launch_pipeline(config, true);
    CHECK(run.state == kRPVProfilerStateFailed);
    CHECK(run.output.find("did not report") != std::string::npos);

    int32_t failing = -1;
    rocprofvis_profiler_get_failing_stage(run.profiler, &failing);
    CHECK(failing == 1);

    rocprofvis_profiler_stage_free(first);
    rocprofvis_profiler_stage_free(second);
    rocprofvis_profiler_config_free(config);
}

TEST_CASE("A pipeline whose second stage tool is missing fails before stage 0",
          "[profiler][pipeline][tool]")
{
    ScratchToolDir tool_dir("rocprof-sys-run");
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_stage_t* first  = rocprofvis_profiler_stage_alloc();
    rocprofvis_profiler_stage_t* second = rocprofvis_profiler_stage_alloc();
    use_tool_in_stage(first, tool_dir);
    rocprofvis_profiler_stage_add_arg(first, "-c");
    rocprofvis_profiler_stage_add_arg(first, "echo SHOULD_NOT_RUN");
    REQUIRE(rocprofvis_profiler_stage_set_tool(second, kRPVProfilerToolRocprofSysRun) ==
            kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_profiler_stage_set_tool_directory(second, kMissingToolDirectory) ==
            kRocProfVisResultSuccess);

    rocprofvis_profiler_config_add_stage(config, first);
    rocprofvis_profiler_config_add_stage(config, second);

    PipelineRun run = launch_pipeline(config, false);
    CHECK(run.launched == kRocProfVisResultToolNotFound);
    CHECK(run.state == kRPVProfilerStateFailed);
    CHECK(run.output.find("SHOULD_NOT_RUN") == std::string::npos);

    int32_t failing = -1;
    rocprofvis_profiler_get_failing_stage(run.profiler, &failing);
    CHECK(failing == 1);

    rocprofvis_profiler_stage_free(first);
    rocprofvis_profiler_stage_free(second);
    rocprofvis_profiler_config_free(config);
}

TEST_CASE("Completed run with an unmatched artifact stays Completed", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir("rocprof-sys-run");
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_stage_t* stage = rocprofvis_profiler_stage_alloc();
    use_tool_in_stage(stage, tool_dir);
    rocprofvis_profiler_stage_add_arg(stage, "-c");
    rocprofvis_profiler_stage_add_arg(stage, "echo hello");
    rocprofvis_profiler_config_add_stage(config, stage);

    PipelineRun run = launch_pipeline(config, true);
    CHECK(run.state == kRPVProfilerStateCompleted);

    rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapePending;
    REQUIRE(rocprofvis_profiler_get_scrape_status(run.profiler, "trace_db", &status) ==
            kRocProfVisResultSuccess);
    CHECK(status == kRPVProfilerScrapeUnmatched);

    char     sentinel[16];
    uint32_t length = sizeof(sentinel);
    std::memset(sentinel, 0xAB, sizeof(sentinel));
    rocprofvis_result_t got =
        rocprofvis_profiler_get_artifact_path(run.profiler, sentinel, &length);
    CHECK(got == kRocProfVisResultNotAvailable);
    check_buffer_untouched(got, sentinel, sizeof(sentinel));
    CHECK(length == sizeof(sentinel));

    rocprofvis_profiler_stage_free(stage);
    rocprofvis_profiler_config_free(config);
}

TEST_CASE("Pending scrape getters leave the caller buffer untouched", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir("rocprof-compute");
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_stage_t* stage = rocprofvis_profiler_stage_alloc();
    use_tool_in_stage(stage, tool_dir, kRPVProfilerToolRocprofCompute,
                      kRPVProfilerOperationCapture);
    rocprofvis_profiler_stage_add_arg(stage, "-c");
    rocprofvis_profiler_stage_add_arg(stage, "sleep 5; echo 'Output directory: /tmp/x'");
    rocprofvis_profiler_config_add_stage(config, stage);

    PipelineRun run = launch_pipeline(config, false);
    REQUIRE(run.launched == kRocProfVisResultSuccess);

    char     sentinel[16];
    uint32_t length = sizeof(sentinel);
    std::memset(sentinel, 0xAB, sizeof(sentinel));
    rocprofvis_result_t got =
        rocprofvis_profiler_get_scraped_value(run.profiler, "workload_dir", sentinel, &length);
    CHECK(got == kRocProfVisResultPending);
    check_buffer_untouched(got, sentinel, sizeof(sentinel));
    CHECK(length == sizeof(sentinel));

    rocprofvis_profiler_cancel(run.profiler);
    REQUIRE(rocprofvis_controller_future_wait(run.future, 30.0f) != kRocProfVisResultTimeout);

    rocprofvis_profiler_stage_free(stage);
    rocprofvis_profiler_config_free(config);
}

TEST_CASE("Cancel mid-pipeline abandons later stages", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir("rocprof-sys-run");
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_stage_t* first  = rocprofvis_profiler_stage_alloc();
    rocprofvis_profiler_stage_t* second = rocprofvis_profiler_stage_alloc();
    use_tool_in_stage(first, tool_dir);
    use_tool_in_stage(second, tool_dir);
    rocprofvis_profiler_stage_add_arg(first, "-c");
    rocprofvis_profiler_stage_add_arg(first, "sleep 30");
    rocprofvis_profiler_stage_add_arg(second, "-c");
    rocprofvis_profiler_stage_add_arg(second, "echo SHOULD_NOT_RUN");

    rocprofvis_profiler_config_add_stage(config, first);
    rocprofvis_profiler_config_add_stage(config, second);

    PipelineRun run = launch_pipeline(config, false);
    REQUIRE(run.launched == kRocProfVisResultSuccess);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    REQUIRE(rocprofvis_profiler_cancel(run.profiler) == kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_controller_future_wait(run.future, 30.0f) != kRocProfVisResultTimeout);

    rocprofvis_profiler_state_t state = kRPVProfilerStateIdle;
    rocprofvis_profiler_get_state(run.profiler, &state);
    CHECK(state == kRPVProfilerStateCancelled);

    rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapePending;
    REQUIRE(rocprofvis_profiler_get_scrape_status(run.profiler, "trace_db", &status) ==
            kRocProfVisResultSuccess);
    CHECK(status == kRPVProfilerScrapeStageSkipped);

    uint32_t length = 0;
    rocprofvis_profiler_get_output(run.profiler, nullptr, &length);
    std::string output;
    if (length > 0)
    {
        std::vector<char> buffer(length + 1, '\0');
        rocprofvis_profiler_get_output(run.profiler, buffer.data(), &length);
        output.assign(buffer.data());
    }
    CHECK(output.find("SHOULD_NOT_RUN") == std::string::npos);

    rocprofvis_profiler_stage_free(first);
    rocprofvis_profiler_stage_free(second);
    rocprofvis_profiler_config_free(config);
}

TEST_CASE("Artifact relocation reports the destination path", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir("rocprof-compute");
    std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "roc-optiq relocate src";
    std::filesystem::path dest_dir =
        std::filesystem::temp_directory_path() / "roc-optiq relocate dest";
    std::filesystem::remove_all(scratch);
    std::filesystem::remove_all(dest_dir);
    std::filesystem::create_directories(scratch);

    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    rocprofvis_profiler_stage_t*  stage  = rocprofvis_profiler_stage_alloc();
    use_tool_in_stage(stage, tool_dir, kRPVProfilerToolRocprofCompute,
                      kRPVProfilerOperationAnalyze);
    rocprofvis_profiler_stage_set_working_directory(stage, scratch.string().c_str());
    rocprofvis_profiler_stage_add_arg(stage, "-c");
    rocprofvis_profiler_stage_add_arg(
        stage, "printf 'hello' > artifact.txt; echo 'Created file: artifact.txt'");
    rocprofvis_profiler_stage_set_artifact_destination(stage, dest_dir.string().c_str());
    rocprofvis_profiler_config_add_stage(config, stage);

    PipelineRun run = launch_pipeline(config, true);
    CHECK(run.state == kRPVProfilerStateCompleted);

    uint32_t length = 0;
    REQUIRE(rocprofvis_profiler_get_artifact_path(run.profiler, nullptr, &length) ==
            kRocProfVisResultSuccess);
    std::vector<char> buffer(length + 1, '\0');
    REQUIRE(rocprofvis_profiler_get_artifact_path(run.profiler, buffer.data(), &length) ==
            kRocProfVisResultSuccess);
    std::filesystem::path reported(buffer.data());
    CHECK(reported == dest_dir / "artifact.txt");
    CHECK(std::filesystem::exists(dest_dir / "artifact.txt"));
    CHECK_FALSE(std::filesystem::exists(scratch / "artifact.txt"));

    rocprofvis_profiler_stage_free(stage);
    rocprofvis_profiler_config_free(config);
    std::filesystem::remove_all(scratch);
    std::filesystem::remove_all(dest_dir);
}

TEST_CASE("A failed relocation keeps the scratch path and stays Resolved", "[profiler][pipeline]")
{
    ScratchToolDir tool_dir("rocprof-compute");
    std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "roc-optiq relocate fail src";
    std::filesystem::path dest_dir =
        std::filesystem::temp_directory_path() / "roc-optiq relocate fail dest";
    std::filesystem::remove_all(scratch);
    std::filesystem::remove_all(dest_dir);
    std::filesystem::create_directories(scratch);
    std::filesystem::create_directories(dest_dir);
    {
        std::ofstream blocker(dest_dir / "artifact.txt");
        blocker << "existing";
    }

    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    rocprofvis_profiler_stage_t*  stage  = rocprofvis_profiler_stage_alloc();
    use_tool_in_stage(stage, tool_dir, kRPVProfilerToolRocprofCompute,
                      kRPVProfilerOperationAnalyze);
    rocprofvis_profiler_stage_set_working_directory(stage, scratch.string().c_str());
    rocprofvis_profiler_stage_add_arg(stage, "-c");
    rocprofvis_profiler_stage_add_arg(
        stage, "printf 'hello' > artifact.txt; echo 'Created file: artifact.txt'");
    rocprofvis_profiler_stage_set_artifact_destination(stage, dest_dir.string().c_str());
    rocprofvis_profiler_config_add_stage(config, stage);

    PipelineRun run = launch_pipeline(config, true);
    CHECK(run.state == kRPVProfilerStateCompleted);

    rocprofvis_profiler_scrape_status_t status = kRPVProfilerScrapePending;
    REQUIRE(rocprofvis_profiler_get_scrape_status(run.profiler, "analysis_db", &status) ==
            kRocProfVisResultSuccess);
    CHECK(status == kRPVProfilerScrapeResolved);

    uint32_t length = 0;
    REQUIRE(rocprofvis_profiler_get_artifact_path(run.profiler, nullptr, &length) ==
            kRocProfVisResultSuccess);
    std::vector<char> buffer(length + 1, '\0');
    REQUIRE(rocprofvis_profiler_get_artifact_path(run.profiler, buffer.data(), &length) ==
            kRocProfVisResultSuccess);
    std::filesystem::path reported(buffer.data());
    CHECK(reported == scratch / "artifact.txt");
    CHECK(std::filesystem::exists(scratch / "artifact.txt"));
    CHECK(run.output.find("leaving it at the original path") != std::string::npos);

    rocprofvis_profiler_stage_free(stage);
    rocprofvis_profiler_config_free(config);
    std::filesystem::remove_all(scratch);
    std::filesystem::remove_all(dest_dir);
}

#endif  // !_WIN32

}  // namespace
