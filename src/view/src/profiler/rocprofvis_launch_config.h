// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include "rocprofvis_controller_enums.h"
#include <map>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

enum class ConnectionType
{
    kLocal,
    kSsh
};

// Note: only the connection MODE (local vs. SSH) is stored on a LaunchConfig.
// The actual SSH connection details (host/user/auth/output path) live in
// RemoteUri, edited via the SshSettingsDialog and persisted separately.

// What a run profiles and where its results go. Owned by each backend (in its
// settings payload), so every profiler keeps a target of its own.
struct TargetSpec
{
    std::string path;
    std::string arguments;
    std::string working_directory;
    std::string output_directory;
    bool        auto_load_trace = true;

    jt::Json          ToJson() const;
    static TargetSpec FromJson(jt::Json const& json);

    /*
     * Appends the target to argv. With `executable`, appends "--", the target
     * executable, and its arguments word-split with SplitArguments; without it,
     * appends just the path (e.g. a workload directory). Appends nothing when
     * path is empty.
     */
    void Argv(bool executable, std::vector<std::string>& argv_out) const;
};

// Extra command-line arguments and environment variables added on top of a
// profiler's own options. Owned by each backend (in its settings payload), so
// every profiler keeps its own.
struct ExtraArguments
{
    std::map<std::string, std::string> env;
    std::vector<std::string>           argv;

    // {"extra_env": {NAME: value, ...}, "extra_argv": [entry, ...]}
    jt::Json              ToJson() const;
    static ExtraArguments FromJson(jt::Json const& json);

    /*
     * Appends each entry as one argument, verbatim: nothing is split on
     * whitespace or unquoted. This is the override hatch: a backend calls it last
     * among the profiler's own flags, still ahead of TargetSpec::Argv, since
     * tokens past the "--" separator belong to the target.
     */
    void Argv(std::vector<std::string>& argv_out) const;
};

struct LaunchConfig
{
    std::string              profiler_id;
    // Which tool of that profiler to run. kRPVProfilerToolNone means "not chosen
    // yet" - the launcher replaces it with the backend's first tool, so a launch
    // never sees it. Persisted as the enum's integer value, hence the append-only
    // rule on rocprofvis_profiler_tool_t.
    rocprofvis_profiler_tool_t tool = kRPVProfilerToolNone;
    // Which mode of that tool to run (rocprof-compute's Capture / Analyze);
    // kRPVProfilerOperationDefault for single-mode tools. Persisted as the
    // enum's integer value, hence the append-only rule on
    // rocprofvis_profiler_operation_t.
    rocprofvis_profiler_operation_t operation = kRPVProfilerOperationDefault;
    // Directory holding the profiler tools, for a ROCm install in a non-standard
    // location. Empty (the default) searches $ROCM_PATH/bin then $PATH. A
    // directory rather than a path to an executable: the filename comes from the
    // controller's tool table, so nothing saved can name a program to run. Not
    // saved in a profile: it belongs to the run destination (see
    // LaunchEnvironment), since a remote host needs a path on that host.
    std::string              tool_directory;
    // Python virtual environment the profiler runs in, for tools that are
    // Python scripts. Empty (the default) uses the system Python. Like
    // tool_directory a directory, never a command, and kept per run destination
    // rather than in a profile.
    std::string    python_environment;
    ConnectionType           connection = ConnectionType::kLocal;
    // Id of the SSH connection profile (SshConnectionConfig::id) used when
    // connection == kSsh. Empty for local launches or when unset.
    std::string              ssh_connection_ref;
    jt::Json                 backend_payload;

    LaunchConfig();

    jt::Json ToJson() const;
    static LaunchConfig FromJson(jt::Json const& json);
};

/**
 * Splits a user-typed argument string (TargetSpec::arguments) into individual
 * argv entries the way a POSIX shell would word-split it, so that a quoted
 * argument stays a single entry: --msg "hello world" yields two entries, not
 * three. Whitespace separates entries; single quotes are literal; double quotes
 * honor \" and \\; a backslash outside quotes escapes the next character.
 *
 * Only word-splitting and quote removal are performed. Nothing here is
 * expanded - no globbing, variables, command substitution, or operators - and
 * the resulting entries are passed to the process directly rather than through
 * a shell, so shell metacharacters have no special meaning.
 *
 * An unterminated quote is not an error: the remainder of the string becomes
 * the final entry, which keeps a half-typed command line previewing sensibly
 * while the user is still editing it.
 */
std::vector<std::string> SplitArguments(std::string const& arguments);

/**
 * Resolves an output path a profiler printed against the directory it ran in.
 * A relative path is joined onto working_directory; an absolute path, or an
 * empty working_directory (the run inherited the directory the path is later
 * read from), is returned unchanged. Remote paths follow POSIX rules whatever
 * this host is.
 * @param path: the path as the profiler printed it; empty stays empty.
 * @param working_directory: the run's working directory (TargetSpec::working_directory).
 * @param remote: true for an SSH run, whose paths are on the remote host.
 */
std::string ResolveOutputPath(std::string const& path, std::string const& working_directory,
                              bool remote);

/**
 * The executable name of a profiler tool ("rocprof-sys-run"), as the controller
 * spells it. Empty for kRPVProfilerToolNone. Use for labels and for a command
 * preview when the tool could not be resolved to a real path.
 */
std::string GetToolBinaryName(rocprofvis_profiler_tool_t tool);

/**
 * Validates an integer read from a saved profile and returns the tool it names,
 * or kRPVProfilerToolNone if this build has no such tool - which covers both a
 * corrupted value and a profile written by a newer build that knows more tools.
 * Bounded by the enum's own __kRPVProfilerToolLast, so there is no second list of
 * valid values to keep in step with it.
 */
rocprofvis_profiler_tool_t ToolFromInt(int32_t value);

/**
 * Validates an integer read from a saved profile and returns the operation it
 * names, or kRPVProfilerOperationDefault if this build has no such operation.
 * Bounded by the enum's own __kRPVProfilerOperationLast, like ToolFromInt.
 */
rocprofvis_profiler_operation_t OperationFromInt(int32_t value);

/**
 * Finds the absolute path a profiler tool would be launched from, searching
 * tool_directory alone if it is set and otherwise python_environment's bin (off
 * Windows) before the default locations, so the
 * UI can show what will actually run and can tell the user a tool is missing
 * before they commit to a run. Returns empty on failure, with a message written
 * to out_error suitable for display.
 *
 * Applies to LOCAL launches only. A remote launch resolves its tool on the
 * remote host, so calling this for one would report the wrong machine.
 *
 * This does not decide what gets launched: the launch passes the tool enum and
 * the controller resolves it again at that point. Resolving twice is deliberate
 * - it keeps the path out of the launch input entirely - and the small window
 * where the answer could change between preview and launch is not worth a
 * cached path that could be stale for other reasons.
 */
std::string ResolveToolPath(rocprofvis_profiler_tool_t tool,
                            std::string const&         tool_directory,
                            std::string const&         python_environment,
                            std::string&               out_error);

} // namespace View
} // namespace RocProfVis
