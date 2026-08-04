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
#include "rocprofvis_core.h"
#include "rocprofvis_profiler.h"
#include "profiler/rocprofvis_controller_profiler_cmdline.h"
#include "profiler/rocprofvis_controller_profiler_process.h"

#include <catch2/catch_test_macros.hpp>

#include <cfloat>
#include <filesystem>
#include <string>
#include <vector>

// Tests for how a ProfilerConfig becomes a real command line and a real
// process. The command line is the security- and correctness-sensitive part of
// the profiler launch: every argument must reach the child exactly as composed,
// with no re-splitting on whitespace and no shell interpretation, and the child
// must run in the requested directory or not run at all.

using RocProfVis::Controller::ProfilerConfig;
namespace Cmdline = RocProfVis::Controller::Cmdline;

namespace
{

// ==================================================================================
// Command-line composition (platform independent)
// ==================================================================================

TEST_CASE("BuildArgv passes explicit arguments through verbatim", "[profiler][cmdline]")
{
    ProfilerConfig config;
    config.SetProfilerPath("/opt/rocm/bin/rocprof-sys-run");
    config.AddProfilerArg("--preset=balanced");
    config.AddProfilerArg("-I");
    // A single argument containing spaces must survive as ONE argument. This is
    // what the old join-then-split round-trip destroyed.
    config.AddProfilerArg("my func(int, float)");
    config.AddProfilerArg("");

    std::vector<std::string> argv = Cmdline::BuildArgv(config);

    REQUIRE(argv == std::vector<std::string>{"/opt/rocm/bin/rocprof-sys-run",
                                             "--preset=balanced",
                                             "-I",
                                             "my func(int, float)",
                                             ""});
}

TEST_CASE("BuildArgv synthesizes no flags of its own", "[profiler][cmdline]")
{
    // target_executable and output_directory are descriptive metadata. Each
    // profiler spells its output flag differently and disagrees about whether a
    // "--" separator precedes the target, so the command line must contain only
    // what the caller put there.
    ProfilerConfig config;
    config.SetProfilerPath("/usr/bin/tool");
    config.SetTargetExecutable("/home/me/app");
    config.SetOutputDirectory("/tmp/out");
    config.SetWorkingDirectory("/tmp");

    std::vector<std::string> argv = Cmdline::BuildArgv(config);

    REQUIRE(argv == std::vector<std::string>{"/usr/bin/tool"});
}

TEST_CASE("ToPosixShellCommand quotes every token", "[profiler][cmdline]")
{
    std::vector<std::string> argv{"/usr/bin/tool", "--filter", "a b", "it's", "x;rm -rf /"};

    std::string cmd = Cmdline::ToPosixShellCommand(argv);

    // Metacharacters must be inside quotes so the remote shell treats them as
    // data. The ' in "it's" is closed, escaped, and reopened.
    REQUIRE(cmd == "'/usr/bin/tool' '--filter' 'a b' 'it'\\''s' 'x;rm -rf /'");
}

TEST_CASE("ToPosixShellCommand prefixes the working directory with &&", "[profiler][cmdline]")
{
    std::vector<std::string> argv{"/usr/bin/tool"};

    SECTION("empty working directory adds no prefix")
    {
        REQUIRE(Cmdline::ToPosixShellCommand(argv, {}, "") == "'/usr/bin/tool'");
    }

    SECTION("cd is quoted and joined with && so a missing directory aborts the run")
    {
        REQUIRE(Cmdline::ToPosixShellCommand(argv, {}, "/tmp/my results") ==
                "cd '/tmp/my results' && '/usr/bin/tool'");
    }

    SECTION("cd precedes the environment assignments")
    {
        REQUIRE(Cmdline::ToPosixShellCommand(argv, {{"ROCPROFSYS_USE_PID", "false"}}, "/tmp") ==
                "cd '/tmp' && ROCPROFSYS_USE_PID='false' '/usr/bin/tool'");
    }
}

TEST_CASE("Env vars with names that are not identifiers are rejected", "[profiler][cmdline]")
{
    // The name is emitted unquoted in the remote command, so a malformed name
    // would be a shell injection point.
    ProfilerConfig config;
    REQUIRE(config.AddEnvVar("PATH_OK", "1") == kRocProfVisResultSuccess);
    REQUIRE(config.AddEnvVar("BAD NAME", "1") == kRocProfVisResultInvalidArgument);
    REQUIRE(config.AddEnvVar("X; rm -rf /", "1") == kRocProfVisResultInvalidArgument);
    REQUIRE(config.AddEnvVar("2LEADING_DIGIT", "1") == kRocProfVisResultInvalidArgument);

    REQUIRE(config.GetEnvVars().size() == 1);
    REQUIRE(Cmdline::ToPosixShellCommand({"/usr/bin/tool"}, config.GetEnvVars()) ==
            "PATH_OK='1' '/usr/bin/tool'");
}

// ==================================================================================
// Process launch
// ==================================================================================
//
// These run a real child process, so they use POSIX system utilities and are
// skipped on Windows. /usr/bin/printf repeats its format once per remaining
// argument, which makes each argv entry individually visible in the output.

#ifndef _WIN32

// Runs a config to completion and returns the child's captured output.
// out_exit_code receives the child's exit code.
std::string run_to_completion(rocprofvis_profiler_config_t* config,
                              rocprofvis_profiler_state_t*  out_state,
                              int32_t*                      out_exit_code)
{
    rocprofvis_profiler_t*          profiler = rocprofvis_profiler_alloc();
    rocprofvis_controller_future_t* future   = rocprofvis_controller_future_alloc();
    REQUIRE(profiler != nullptr);
    REQUIRE(future != nullptr);

    rocprofvis_result_t launched = rocprofvis_profiler_launch_async(profiler, config, future);
    std::string         output;

    if(launched == kRocProfVisResultSuccess)
    {
        // Seconds. Generous: these children are trivial, so anything close to
        // this means something is wedged rather than slow.
        REQUIRE(rocprofvis_controller_future_wait(future, 30.0f) == kRocProfVisResultSuccess);

        uint32_t length = 0;
        rocprofvis_profiler_get_output(profiler, nullptr, &length);
        if(length > 0)
        {
            std::vector<char> buffer(length + 1, '\0');
            rocprofvis_profiler_get_output(profiler, buffer.data(), &length);
            output.assign(buffer.data());
        }
    }

    *out_state = kRPVProfilerStateIdle;
    rocprofvis_profiler_get_state(profiler, out_state);
    *out_exit_code = -1;
    rocprofvis_profiler_get_exit_code(profiler, out_exit_code);

    // The future must be freed first: ~ProfilerProcessController blocks until the
    // monitor job's Job object is destroyed, and that happens in ~Future. Freeing
    // the profiler first deadlocks.
    rocprofvis_controller_future_free(future);
    rocprofvis_profiler_free(profiler);
    return output;
}

TEST_CASE("Arguments reach the child process unsplit and uninterpreted", "[profiler][process]")
{
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_config_set_profiler_path(config, "/usr/bin/printf");
    rocprofvis_profiler_config_add_profiler_arg(config, "[%s]\n");
    rocprofvis_profiler_config_add_profiler_arg(config, "--simple");
    rocprofvis_profiler_config_add_profiler_arg(config, "with spaces  and   runs");
    rocprofvis_profiler_config_add_profiler_arg(config, "quo\"te's");
    // No shell is involved, so these are ordinary characters.
    rocprofvis_profiler_config_add_profiler_arg(config, "meta;|&$(echo hi)*?");
    rocprofvis_profiler_config_add_profiler_arg(config, "/path/with space/trace.db");

    // Metadata that must not leak onto the command line.
    rocprofvis_profiler_config_set_target_executable(config, "/should/not/appear");
    rocprofvis_profiler_config_set_output_directory(config, "/should/not/appear/either");

    rocprofvis_profiler_state_t state     = kRPVProfilerStateIdle;
    int32_t                     exit_code = -1;
    std::string output = run_to_completion(config, &state, &exit_code);

    CHECK(state == kRPVProfilerStateCompleted);
    CHECK(exit_code == 0);

    // One bracketed line per argument: nothing was split, merged, expanded, or
    // added.
    REQUIRE(output ==
            "[--simple]\n"
            "[with spaces  and   runs]\n"
            "[quo\"te's]\n"
            "[meta;|&$(echo hi)*?]\n"
            "[/path/with space/trace.db]\n");

    rocprofvis_profiler_config_free(config);
}

TEST_CASE("Environment variables are set in the child", "[profiler][process]")
{
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_config_set_profiler_path(config, "/bin/sh");
    rocprofvis_profiler_config_add_profiler_arg(config, "-c");
    rocprofvis_profiler_config_add_profiler_arg(config, "printf '%s' \"$ROCPROFVIS_TEST_VAR\"");
    rocprofvis_profiler_config_add_env_var(config, "ROCPROFVIS_TEST_VAR", "a value");

    rocprofvis_profiler_state_t state     = kRPVProfilerStateIdle;
    int32_t                     exit_code = -1;
    std::string output = run_to_completion(config, &state, &exit_code);

    CHECK(state == kRPVProfilerStateCompleted);
    REQUIRE(output == "a value");

    rocprofvis_profiler_config_free(config);
}

TEST_CASE("The child runs in the configured working directory", "[profiler][process]")
{
    // A directory with a space in it, since the working directory reaches the
    // child without passing through any quoting layer.
    std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "roc-optiq profiler wd test";
    std::filesystem::create_directories(scratch);

    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_config_set_profiler_path(config, "/bin/sh");
    rocprofvis_profiler_config_add_profiler_arg(config, "-c");
    rocprofvis_profiler_config_add_profiler_arg(config, "printf '%s' \"$PWD\"");
    rocprofvis_profiler_config_set_working_directory(config, scratch.string().c_str());

    std::filesystem::path before = std::filesystem::current_path();

    rocprofvis_profiler_state_t state     = kRPVProfilerStateIdle;
    int32_t                     exit_code = -1;
    std::string output = run_to_completion(config, &state, &exit_code);

    CHECK(state == kRPVProfilerStateCompleted);
    CHECK(output == scratch.string());
    // Only the child moved. Optiq's own working directory is global state shared
    // by every open trace and relative path in the process.
    CHECK(std::filesystem::current_path() == before);

    rocprofvis_profiler_config_free(config);
    std::filesystem::remove_all(scratch);
}

TEST_CASE("An empty working directory inherits Optiq's", "[profiler][process]")
{
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_config_set_profiler_path(config, "/bin/sh");
    rocprofvis_profiler_config_add_profiler_arg(config, "-c");
    rocprofvis_profiler_config_add_profiler_arg(config, "printf '%s' \"$PWD\"");

    rocprofvis_profiler_state_t state     = kRPVProfilerStateIdle;
    int32_t                     exit_code = -1;
    std::string output = run_to_completion(config, &state, &exit_code);

    CHECK(state == kRPVProfilerStateCompleted);
    CHECK(output == std::filesystem::current_path().string());

    rocprofvis_profiler_config_free(config);
}

TEST_CASE("A missing working directory fails the run instead of running elsewhere",
          "[profiler][process]")
{
    // Silently falling back to the inherited directory would let a tool that
    // writes output relative to its own cwd scatter results somewhere the user
    // never asked for, and report success while doing it.
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    rocprofvis_profiler_config_set_profiler_path(config, "/bin/sh");
    rocprofvis_profiler_config_add_profiler_arg(config, "-c");
    rocprofvis_profiler_config_add_profiler_arg(config, "printf 'SHOULD NOT RUN'");
    rocprofvis_profiler_config_set_working_directory(
        config, "/nonexistent-roc-optiq-working-directory");

    rocprofvis_profiler_state_t state     = kRPVProfilerStateIdle;
    int32_t                     exit_code = -1;
    std::string output = run_to_completion(config, &state, &exit_code);

    CHECK(state == kRPVProfilerStateFailed);
    CHECK(exit_code == 126);
    CHECK(output.find("SHOULD NOT RUN") == std::string::npos);
    // The reason is reported to the console the user is looking at.
    CHECK(output.find("chdir failed") != std::string::npos);

    rocprofvis_profiler_config_free(config);
}

#endif  // !_WIN32

}  // namespace
