// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class ProfilerConfig;

namespace Cmdline
{

/*
 * Shared command-line composition helpers for profiler launches.
 *
 * The argv schema (order and conventions) is defined once here so that every
 * sink - POSIX execvp, Windows CreateProcessA, and the future SSH executor -
 * produces the same logical command from a given ProfilerConfig.
 *
 * Typical usage:
 *
 *   auto argv = BuildArgv(config);
 *   auto env  = BuildEnv(config);
 *
 *   // POSIX local:  pass argv to execvp / posix_spawnp directly.
 *   // Windows:      auto cmd = ToWindowsCommandLine(argv);
 *   // SSH remote:   auto cmd = ToPosixShellCommand(argv, env);
 *   // Logging/UI:   auto s   = ToDisplayString(argv, env);
 */

/*
 * Build the canonical argv from a ProfilerConfig: argv[0] is the profiler
 * executable path, argv[1..] are the config's explicit argv entries
 * (AddProfilerArg) in the order they were added.
 *
 * This is deliberately a pass-through - no flag is synthesized here. Each
 * profiler has its own CLI shape (where the output path goes, whether a "--"
 * separator precedes the target, whether the target is an argument at all), so
 * composing argv is the caller's job and every token arrives as a discrete
 * entry. Nothing is split on whitespace, so paths and arguments containing
 * spaces survive to execvp / CreateProcess intact.
 *
 * The config's output_directory does NOT contribute to argv; a caller that
 * wants it on the command line must add it explicitly. Profilers spell their
 * output flag differently, and some write only to their working directory.
 */
std::vector<std::string> BuildArgv(ProfilerConfig const& config);

/*
 * Return the env vars from the config as a vector of (name, value) pairs.
 * Currently a pass-through; exists as a seam for future per-executor tweaks.
 */
std::vector<std::pair<std::string, std::string>> BuildEnv(ProfilerConfig const& config);

/*
 * True if `name` is a valid POSIX environment variable name
 * ([A-Za-z_][A-Za-z0-9_]*). Malformed names must be kept out of the remote
 * shell command (ToPosixShellCommand) because the name is emitted unquoted and
 * would otherwise allow shell-syntax injection.
 */
bool IsValidEnvName(std::string const& name);

/*
 * Serialize argv (and optional env) into a single string suitable for a POSIX
 * /bin/sh interpreter - i.e. ssh user@host "<this>" or sh -c "<this>". Each
 * token is single-quoted; embedded single quotes are emitted as '\''. Env
 * pairs are prepended as KEY='VALUE'.
 *
 * When working_dir is non-empty the command is prefixed with
 * "cd '<working_dir>' && ", which is the remote equivalent of the child-side
 * chdir / lpCurrentDirectory used for local launches. "&&" is deliberate: if
 * the directory does not exist the profiler must not run at all, since a tool
 * that writes output relative to its cwd would otherwise silently produce it
 * in the wrong place.
 */
std::string ToPosixShellCommand(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env = {},
    std::string const&                                      working_dir = std::string());

/*
 * Serialize argv into a single string suitable for the lpCommandLine
 * parameter of Windows CreateProcessA/W. Tokens are quoted following the
 * MSVC CommandLineToArgvW reverse rules: backslash-doubling before quotes,
 * double-quote-wrapping for tokens containing whitespace/quotes.
 *
 * Does not include env vars (Windows passes env via a separate block).
 */
std::string ToWindowsCommandLine(std::vector<std::string> const& argv);

/*
 * Format argv + env as a human-readable single string for logging and the
 * launcher dialog's preamble. Not safe to feed back to a real shell - use
 * the platform-specific serializers above for that.
 */
std::string ToDisplayString(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env = {});

} // namespace Cmdline
} // namespace Controller
} // namespace RocProfVis
