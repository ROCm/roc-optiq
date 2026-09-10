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
#include "profiler/rocprofvis_controller_profiler_tool.h"

#include <catch2/catch_test_macros.hpp>

#include <cfloat>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <cstdlib>
#include <sys/stat.h>
#endif

// Tests for how a ProfilerConfig becomes a real command line and a real
// process. The command line is the security- and correctness-sensitive part of
// the profiler launch: every argument must reach the child exactly as composed,
// with no re-splitting on whitespace and no shell interpretation, the tool that
// runs must be the one the tool table resolved rather than any caller-supplied
// string, and the child must run in the requested directory or not run at all.

using RocProfVis::Controller::ProfilerConfig;
namespace Cmdline     = RocProfVis::Controller::Cmdline;
namespace ProfilerTool = RocProfVis::Controller::ProfilerTool;

namespace
{

// An executable that certainly exists, used as a stand-in for a ROCm tool so
// these tests need no ROCm install.
#ifdef _WIN32
constexpr char const* kRealExecutable = "C:\\Windows\\System32\\cmd.exe";
#else
constexpr char const* kRealExecutable = "/bin/sh";
#endif

// The file name the tool table expects for kRPVProfilerToolRocprofSysRun.
#ifdef _WIN32
constexpr char const* kToolFileName = "rocprof-sys-run.exe";
#else
constexpr char const* kToolFileName = "rocprof-sys-run";
#endif

// An absolute directory that does not exist. It has to be absolute per this
// platform's rules, or resolution rejects it as relative before it ever looks
// at the filesystem - and on Windows "absolute" means a drive letter, so a
// leading '/' alone is not enough.
#ifdef _WIN32
constexpr char const* kMissingToolDirectory = "C:\\nonexistent-roc-optiq-tool-directory";
#else
constexpr char const* kMissingToolDirectory = "/nonexistent-roc-optiq-tool-directory";
#endif

// A scratch directory containing a stand-in for a ROCm tool.
//
// A tool is named, never pathed, so making a launch run some other program means
// installing that program under the tool's own name and pointing the tool
// directory at it - the same mechanism a user with a ROCm install in a
// non-standard location would use. There is deliberately no API that sets argv[0]
// to an arbitrary string, so this is how these tests get a runnable tool, and
// exercising it here keeps that the only route.
class ScratchToolDir
{
public:
    explicit ScratchToolDir(char const* binary = kRealExecutable)
        : m_dir(std::filesystem::temp_directory_path() /
                ("roc-optiq tool dir " + std::filesystem::path(binary).filename().string()))
    {
        std::filesystem::remove_all(m_dir);
        std::filesystem::create_directories(m_dir);
        // A copy rather than a link so this works the same on every platform;
        // copy_file carries the executable bit across on POSIX.
        std::filesystem::copy_file(binary, m_dir / kToolFileName);
    }

    ~ScratchToolDir() { std::filesystem::remove_all(m_dir); }

    ScratchToolDir(ScratchToolDir const&)            = delete;
    ScratchToolDir& operator=(ScratchToolDir const&) = delete;

    std::string Path() const { return m_dir.string(); }
    std::string ToolPath() const { return (m_dir / kToolFileName).string(); }

private:
    std::filesystem::path m_dir;
};

// Points a config at a stand-in tool and resolves it, leaving GetResolvedToolPath
// populated the way a launch would.
void use_tool_in(ProfilerConfig& config, ScratchToolDir const& dir)
{
    REQUIRE(config.SetTool(kRPVProfilerToolRocprofSysRun) == kRocProfVisResultSuccess);
    REQUIRE(config.SetToolDirectory(dir.Path().c_str()) == kRocProfVisResultSuccess);
    REQUIRE(config.ResolveToolPath() == kRocProfVisResultSuccess);
}

// ==================================================================================
// Command-line composition (platform independent)
// ==================================================================================

TEST_CASE("BuildArgv passes explicit arguments through verbatim", "[profiler][cmdline]")
{
    ScratchToolDir tool_dir;
    ProfilerConfig config;
    use_tool_in(config, tool_dir);
    config.AddProfilerArg("--preset=balanced");
    config.AddProfilerArg("-I");
    // A single argument containing spaces must survive as ONE argument. This is
    // what the old join-then-split round-trip destroyed.
    config.AddProfilerArg("my func(int, float)");
    config.AddProfilerArg("");

    std::vector<std::string> argv = Cmdline::BuildArgv(config);

    REQUIRE(argv == std::vector<std::string>{tool_dir.ToolPath(),
                                             "--preset=balanced",
                                             "-I",
                                             "my func(int, float)",
                                             ""});
}

TEST_CASE("BuildArgv synthesizes no flags of its own", "[profiler][cmdline]")
{
    // Each profiler spells its output flag differently, and some take none at
    // all, so neither the output directory nor a "--" separator may be
    // synthesized here - the command line must contain only what the caller put
    // there.
    ScratchToolDir tool_dir;
    ProfilerConfig config;
    use_tool_in(config, tool_dir);
    config.SetOutputDirectory("/tmp/out");
    config.SetWorkingDirectory("/tmp");

    std::vector<std::string> argv = Cmdline::BuildArgv(config);

    REQUIRE(argv == std::vector<std::string>{tool_dir.ToolPath()});
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
// Tool identity and resolution
// ==================================================================================

TEST_CASE("Every tool maps to its binary name", "[profiler][tool]")
{
    CHECK(std::string(ProfilerTool::GetBinaryName(kRPVProfilerToolRocprofSysRun)) ==
          "rocprof-sys-run");
    CHECK(std::string(ProfilerTool::GetBinaryName(kRPVProfilerToolRocprofSysSample)) ==
          "rocprof-sys-sample");
    CHECK(std::string(ProfilerTool::GetBinaryName(kRPVProfilerToolRocprofSysInstrument)) ==
          "rocprof-sys-instrument");
    CHECK(std::string(ProfilerTool::GetBinaryName(kRPVProfilerToolRocprofSysAvail)) ==
          "rocprof-sys-avail");
    CHECK(std::string(ProfilerTool::GetBinaryName(kRPVProfilerToolRocprofCompute)) ==
          "rocprof-compute");
    CHECK(std::string(ProfilerTool::GetBinaryName(kRPVProfilerToolRocprofV3)) == "rocprofv3");

    // Nothing to run, and nothing outside the enum is guessed at.
    CHECK(ProfilerTool::GetBinaryName(kRPVProfilerToolNone) == nullptr);
    CHECK(ProfilerTool::GetBinaryName(static_cast<rocprofvis_profiler_tool_t>(999)) == nullptr);
}

TEST_CASE("A config refuses a tool it cannot name", "[profiler][tool]")
{
    ProfilerConfig config;
    CHECK(config.SetTool(kRPVProfilerToolNone) == kRocProfVisResultInvalidEnum);
    CHECK(config.SetTool(static_cast<rocprofvis_profiler_tool_t>(999)) ==
          kRocProfVisResultInvalidEnum);
    CHECK(config.SetTool(kRPVProfilerToolRocprofCompute) == kRocProfVisResultSuccess);
    CHECK(config.GetTool() == kRPVProfilerToolRocprofCompute);
}

TEST_CASE("A configured tool directory decides where the tool is looked for",
          "[profiler][tool]")
{
    ScratchToolDir tool_dir;
    std::string    resolved;

    SECTION("the tool is found under its own name in the configured directory")
    {
        REQUIRE(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, tool_dir.Path(),
                                          resolved) == kRocProfVisResultSuccess);
        CHECK(resolved == tool_dir.ToolPath());
    }

    SECTION("a relative directory is refused rather than resolved against the cwd")
    {
        // The child may be given a working directory of its own, so a relative
        // directory would not mean the same thing to it as to us.
        CHECK(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, "build/bin", resolved) ==
              kRocProfVisResultInvalidArgument);
        CHECK(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, "./bin", resolved) ==
              kRocProfVisResultInvalidArgument);
    }

    SECTION("a directory that does not hold the tool is a failure, not a different tool")
    {
        // The directory has rocprof-sys-run in it but not rocprofv3.
        CHECK(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofV3, tool_dir.Path(), resolved) ==
              kRocProfVisResultToolNotFound);
    }

    SECTION("a directory that does not exist is a failure")
    {
        CHECK(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, kMissingToolDirectory,
                                        resolved) == kRocProfVisResultToolNotFound);
    }

    SECTION("a directory cannot name the executable")
    {
        // Pointing at the executable itself finds <exe>/rocprof-sys-run, which is
        // not a thing. The filename is the tool table's to choose, and this is
        // what stops a directory from being a path to an arbitrary program.
        CHECK(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, kRealExecutable,
                                        resolved) == kRocProfVisResultToolNotFound);
    }

    SECTION("an unknown tool cannot be conjured out of a directory")
    {
        CHECK(ProfilerTool::ResolvePath(kRPVProfilerToolNone, tool_dir.Path(), resolved) ==
              kRocProfVisResultInvalidArgument);
    }
}

// Resolution reads $ROCM_PATH and $PATH, so the remaining cases install a fake
// tool in a scratch directory and point those at it. Environment manipulation and
// the executable bit are POSIX-specific here; the ordering logic they cover is
// shared with Windows.
#ifndef _WIN32

// Sets an environment variable for the duration of a scope and restores whatever
// was there before, so one test cannot leak a $PATH into the next.
class ScopedEnv
{
public:
    ScopedEnv(char const* name, std::string const& value)
        : m_name(name)
        , m_had_value(false)
    {
        char const* previous = std::getenv(name);
        if(previous != nullptr)
        {
            m_previous  = previous;
            m_had_value = true;
        }
        ::setenv(name, value.c_str(), 1);
    }

    ~ScopedEnv()
    {
        if(m_had_value)
        {
            ::setenv(m_name.c_str(), m_previous.c_str(), 1);
        }
        else
        {
            ::unsetenv(m_name.c_str());
        }
    }

    ScopedEnv(ScopedEnv const&)            = delete;
    ScopedEnv& operator=(ScopedEnv const&) = delete;

private:
    std::string m_name;
    std::string m_previous;
    bool        m_had_value;
};

// Writes an executable no-op script named `name` into `dir` and returns its path.
std::filesystem::path install_fake_tool(std::filesystem::path const& dir,
                                        std::string const&           name)
{
    std::filesystem::create_directories(dir);
    std::filesystem::path path = dir / name;
    {
        std::ofstream out(path);
        out << "#!/bin/sh\nexit 0\n";
    }
    ::chmod(path.c_str(), 0755);
    return path;
}

TEST_CASE("Resolution prefers ROCM_PATH, then PATH, then fails", "[profiler][tool]")
{
    std::filesystem::path scratch =
        std::filesystem::temp_directory_path() / "roc-optiq tool resolution";
    std::filesystem::remove_all(scratch);

    std::filesystem::path rocm_bin = scratch / "rocm" / "bin";
    std::filesystem::path path_dir = scratch / "elsewhere";
    std::string const     tool_name = "rocprof-sys-run";
    std::string           resolved;

    SECTION("$ROCM_PATH/bin wins over a copy earlier in $PATH")
    {
        std::filesystem::path in_rocm = install_fake_tool(rocm_bin, tool_name);
        install_fake_tool(path_dir, tool_name);

        ScopedEnv rocm("ROCM_PATH", (scratch / "rocm").string());
        ScopedEnv path("PATH", path_dir.string());

        REQUIRE(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, "", resolved) ==
                kRocProfVisResultSuccess);
        CHECK(resolved == in_rocm.string());
    }

    SECTION("$PATH is searched when the ROCm install has no such tool")
    {
        install_fake_tool(rocm_bin, "rocprof-compute");
        std::filesystem::path in_path = install_fake_tool(path_dir, tool_name);

        ScopedEnv rocm("ROCM_PATH", (scratch / "rocm").string());
        // A leading empty entry and a directory that does not exist: both are
        // skipped rather than ending the search.
        ScopedEnv path("PATH", ":" + (scratch / "missing").string() + ":" + path_dir.string());

        REQUIRE(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, "", resolved) ==
                kRocProfVisResultSuccess);
        CHECK(resolved == in_path.string());
    }

    SECTION("a non-executable file of the right name is not a match")
    {
        std::filesystem::create_directories(path_dir);
        std::ofstream(path_dir / tool_name) << "not executable\n";
        ::chmod((path_dir / tool_name).c_str(), 0644);

        ScopedEnv rocm("ROCM_PATH", (scratch / "rocm").string());
        ScopedEnv path("PATH", path_dir.string());

        CHECK(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, "", resolved) ==
              kRocProfVisResultToolNotFound);
    }

    SECTION("an uninstalled tool is reported as not found, not left for execvp")
    {
        ScopedEnv rocm("ROCM_PATH", (scratch / "rocm").string());
        ScopedEnv path("PATH", path_dir.string());

        CHECK(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofV3, "", resolved) ==
              kRocProfVisResultToolNotFound);
    }

    SECTION("a relative $PATH entry still resolves to an absolute path")
    {
        // argv[0] has to be absolute: the child may be given a different working
        // directory, which would change what a relative path means.
        install_fake_tool(path_dir, tool_name);
        std::filesystem::path saved_cwd = std::filesystem::current_path();
        std::filesystem::current_path(scratch);

        {
            ScopedEnv rocm("ROCM_PATH", (scratch / "rocm").string());
            ScopedEnv path("PATH", "elsewhere");

            REQUIRE(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, "", resolved) ==
                    kRocProfVisResultSuccess);
            CHECK(std::filesystem::path(resolved).is_absolute());
        }

        std::filesystem::current_path(saved_cwd);
    }

    SECTION("a configured directory suppresses the default search entirely")
    {
        // The tool is installed in both $ROCM_PATH/bin and $PATH, and the
        // configured directory has it too - resolution must take that one.
        install_fake_tool(rocm_bin, tool_name);
        install_fake_tool(path_dir, tool_name);
        std::filesystem::path chosen_dir = scratch / "chosen";
        std::filesystem::path in_chosen  = install_fake_tool(chosen_dir, tool_name);

        ScopedEnv rocm("ROCM_PATH", (scratch / "rocm").string());
        ScopedEnv path("PATH", path_dir.string());

        REQUIRE(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, chosen_dir.string(),
                                          resolved) == kRocProfVisResultSuccess);
        CHECK(resolved == in_chosen.string());

        // And when the configured directory lacks the tool, an installed copy
        // elsewhere must not stand in for it: someone configures a directory
        // precisely because they have more than one build, so quietly running the
        // other one is the confusion they were trying to end.
        std::filesystem::remove(in_chosen);
        CHECK(ProfilerTool::ResolvePath(kRPVProfilerToolRocprofSysRun, chosen_dir.string(),
                                        resolved) == kRocProfVisResultToolNotFound);
    }

    std::filesystem::remove_all(scratch);
}

#endif  // !_WIN32

TEST_CASE("The C ABI reports tool names with the shared string convention", "[profiler][tool]")
{
    // *length is a byte count without a terminator, so a caller can feed the
    // queried length straight back in; see copy_string_to_buffer.
    uint32_t length = 0;
    REQUIRE(rocprofvis_profiler_tool_get_binary_name(kRPVProfilerToolRocprofCompute, nullptr,
                                                     &length) == kRocProfVisResultSuccess);
    REQUIRE(length == std::string("rocprof-compute").size());

    std::vector<char> buffer(length + 1, '\0');
    REQUIRE(rocprofvis_profiler_tool_get_binary_name(kRPVProfilerToolRocprofCompute,
                                                     buffer.data(),
                                                     &length) == kRocProfVisResultSuccess);
    CHECK(std::string(buffer.data()) == "rocprof-compute");

    length = 0;
    CHECK(rocprofvis_profiler_tool_get_binary_name(kRPVProfilerToolNone, nullptr, &length) ==
          kRocProfVisResultInvalidEnum);
    CHECK(rocprofvis_profiler_tool_resolve_path(kRPVProfilerToolNone, nullptr, nullptr, &length) ==
          kRocProfVisResultInvalidArgument);
}

TEST_CASE("The C ABI resolves a tool inside a configured directory", "[profiler][tool]")
{
    ScratchToolDir tool_dir;

    uint32_t length = 0;
    REQUIRE(rocprofvis_profiler_tool_resolve_path(kRPVProfilerToolRocprofSysRun,
                                                  tool_dir.Path().c_str(), nullptr,
                                                  &length) == kRocProfVisResultSuccess);
    REQUIRE(length == tool_dir.ToolPath().size());

    std::vector<char> buffer(length + 1, '\0');
    REQUIRE(rocprofvis_profiler_tool_resolve_path(kRPVProfilerToolRocprofSysRun,
                                                  tool_dir.Path().c_str(), buffer.data(),
                                                  &length) == kRocProfVisResultSuccess);
    CHECK(std::string(buffer.data()) == tool_dir.ToolPath());

    // A directory without the tool reports that, rather than reaching past it.
    length = 0;
    CHECK(rocprofvis_profiler_tool_resolve_path(kRPVProfilerToolRocprofV3,
                                                tool_dir.Path().c_str(), nullptr,
                                                &length) == kRocProfVisResultToolNotFound);
}

// ==================================================================================
// Process launch
// ==================================================================================
//
// These run a real child process, so they use POSIX system utilities and are
// skipped on Windows. /usr/bin/printf repeats its format once per remaining
// argument, which makes each argv entry individually visible in the output.
//
// A ScratchToolDir installs the utility under the tool's own name, since naming a
// tool is the only way to select a binary and nothing here is a ROCm tool. The
// utilities used do not inspect argv[0], so running under another name is
// indistinguishable to them.

#ifndef _WIN32

// Runs whatever `dir` holds as the named tool.
void use_tool_in(rocprofvis_profiler_config_t* config, ScratchToolDir const& dir)
{
    REQUIRE(rocprofvis_profiler_config_set_tool(config, kRPVProfilerToolRocprofSysRun) ==
            kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_profiler_config_set_tool_directory(config, dir.Path().c_str()) ==
            kRocProfVisResultSuccess);
}

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

    ScratchToolDir tool_dir("/usr/bin/printf");
    use_tool_in(config, tool_dir);
    rocprofvis_profiler_config_add_profiler_arg(config, "[%s]\n");
    rocprofvis_profiler_config_add_profiler_arg(config, "--simple");
    rocprofvis_profiler_config_add_profiler_arg(config, "with spaces  and   runs");
    rocprofvis_profiler_config_add_profiler_arg(config, "quo\"te's");
    // No shell is involved, so these are ordinary characters.
    rocprofvis_profiler_config_add_profiler_arg(config, "meta;|&$(echo hi)*?");
    rocprofvis_profiler_config_add_profiler_arg(config, "/path/with space/trace.db");

    // Must not leak onto the command line.
    rocprofvis_profiler_config_set_output_directory(config, "/should/not/appear");

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

    ScratchToolDir tool_dir;
    use_tool_in(config, tool_dir);
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

    ScratchToolDir tool_dir;
    use_tool_in(config, tool_dir);
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

    ScratchToolDir tool_dir;
    use_tool_in(config, tool_dir);
    rocprofvis_profiler_config_add_profiler_arg(config, "-c");
    rocprofvis_profiler_config_add_profiler_arg(config, "printf '%s' \"$PWD\"");

    rocprofvis_profiler_state_t state     = kRPVProfilerStateIdle;
    int32_t                     exit_code = -1;
    std::string output = run_to_completion(config, &state, &exit_code);

    CHECK(state == kRPVProfilerStateCompleted);
    CHECK(output == std::filesystem::current_path().string());

    rocprofvis_profiler_config_free(config);
}

TEST_CASE("A tool that cannot be resolved fails the launch without spawning anything",
          "[profiler][process][tool]")
{
    // The launch must not get as far as exec: reporting "not found" here is the
    // whole reason resolution happens up front rather than being left to execvp,
    // which would surface only as exit code 127 after a process had been created.
    rocprofvis_profiler_config_t* config = rocprofvis_profiler_config_alloc();
    REQUIRE(config != nullptr);

    REQUIRE(rocprofvis_profiler_config_set_tool(config, kRPVProfilerToolRocprofSysRun) ==
            kRocProfVisResultSuccess);
    REQUIRE(rocprofvis_profiler_config_set_tool_directory(config, kMissingToolDirectory) ==
            kRocProfVisResultSuccess);

    rocprofvis_profiler_t*          profiler = rocprofvis_profiler_alloc();
    rocprofvis_controller_future_t* future   = rocprofvis_controller_future_alloc();
    REQUIRE(profiler != nullptr);
    REQUIRE(future != nullptr);

    CHECK(rocprofvis_profiler_launch_async(profiler, config, future) ==
          kRocProfVisResultToolNotFound);

    rocprofvis_profiler_state_t state = kRPVProfilerStateIdle;
    rocprofvis_profiler_get_state(profiler, &state);
    CHECK(state == kRPVProfilerStateFailed);

    rocprofvis_controller_future_free(future);
    rocprofvis_profiler_free(profiler);
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

    ScratchToolDir tool_dir;
    use_tool_in(config, tool_dir);
    rocprofvis_profiler_config_add_profiler_arg(config, "-c");
    rocprofvis_profiler_config_add_profiler_arg(config, "printf 'SHOULD NOT RUN'");
    rocprofvis_profiler_config_set_working_directory(
        config, "/nonexistent-roc-optiq-working-directory");

    rocprofvis_profiler_t*          profiler = rocprofvis_profiler_alloc();
    rocprofvis_controller_future_t* future   = rocprofvis_controller_future_alloc();
    REQUIRE(profiler != nullptr);
    REQUIRE(future != nullptr);

    // The directory is checked before the process is created, so this is a
    // launch error rather than a child that exited 126 after its chdir failed.
    CHECK(rocprofvis_profiler_launch_async(profiler, config, future) ==
          kRocProfVisResultInvalidArgument);

    rocprofvis_profiler_state_t state = kRPVProfilerStateIdle;
    rocprofvis_profiler_get_state(profiler, &state);
    CHECK(state == kRPVProfilerStateFailed);

    uint32_t    length = 0;
    std::string output;
    rocprofvis_profiler_get_output(profiler, nullptr, &length);
    if(length > 0)
    {
        std::vector<char> buffer(length + 1, '\0');
        rocprofvis_profiler_get_output(profiler, buffer.data(), &length);
        output.assign(buffer.data());
    }

    CHECK(output.find("SHOULD NOT RUN") == std::string::npos);
    // The reason is reported to the console the user is looking at.
    CHECK(output.find("working directory") != std::string::npos);

    rocprofvis_controller_future_free(future);
    rocprofvis_profiler_free(profiler);
    rocprofvis_profiler_config_free(config);
}

#endif  // !_WIN32

}  // namespace
