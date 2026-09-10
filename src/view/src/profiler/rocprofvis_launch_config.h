// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "json.h"
#include "rocprofvis_controller_enums.h"
#include <string>
#include <vector>
#include <map>

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

struct TargetSpec
{
    std::string executable;
    std::string arguments;
    std::string working_directory;
    std::string output_directory;
    bool        auto_load_trace = true;
};

struct LaunchConfig
{
    std::string              profiler_id;
    // Which tool of that profiler to run. kRPVProfilerToolNone means "not chosen
    // yet" - the launcher replaces it with the backend's first tool, so a launch
    // never sees it. Persisted as the enum's integer value, hence the append-only
    // rule on rocprofvis_profiler_tool_t.
    rocprofvis_profiler_tool_t tool = kRPVProfilerToolNone;
    // Directory holding the profiler tools, for a ROCm install in a non-standard
    // location. Empty (the default) searches $ROCM_PATH/bin then $PATH. A
    // directory rather than a path to an executable: the filename comes from the
    // controller's tool table, so a profile cannot name a program to run. It
    // lives on the profile rather than in app settings because a remote profile
    // needs a path on the *remote* host, which a machine-local setting could
    // never express.
    std::string              tool_directory;
    ConnectionType           connection = ConnectionType::kLocal;
    // Id of the SSH connection profile (SshConnectionConfig::id) used when
    // connection == kSsh. Empty for local launches or when unset.
    std::string              ssh_connection_ref;
    TargetSpec               target;
    std::map<std::string, std::string> extra_env;
    std::vector<std::string> extra_argv;
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
 * Finds the absolute path a profiler tool would be launched from, searching
 * tool_directory alone if it is set and the default locations otherwise, so the
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
                            std::string&               out_error);

} // namespace View
} // namespace RocProfVis
