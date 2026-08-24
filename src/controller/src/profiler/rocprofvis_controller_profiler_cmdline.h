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
 * Shared argv/env composition for every launch sink (execvp, CreateProcessA,
 * SSH). Typical use:
 *
 *   auto argv = BuildArgv(config);
 *   auto env  = BuildEnv(config);
 *   // POSIX:  execvp / posix_spawnp
 *   // Windows: ToWindowsCommandLine(argv)
 *   // SSH:     ToPosixShellCommand(argv, env, working_dir)
 *   // Log/UI:  ToDisplayString(argv, env)
 */

/*
 * argv[0] = resolved tool path, argv[1..] = AddProfilerArg entries in order.
 * Pass-through: no flags are synthesized (output path, "--", target), and
 * nothing is split on whitespace. output_directory is not on argv; add it
 * explicitly if the profiler needs it.
 */
std::vector<std::string> BuildArgv(ProfilerConfig const& config);

/*
 * Env vars from the config. Pass-through; a seam for per-executor tweaks.
 */
std::vector<std::pair<std::string, std::string>> BuildEnv(ProfilerConfig const& config);

/*
 * True if `name` is [A-Za-z_][A-Za-z0-9_]*. Invalid names are rejected because
 * ToPosixShellCommand emits the name unquoted.
 */
bool IsValidEnvName(std::string const& name);

/*
 * POSIX /bin/sh command: each token single-quoted (embedded ' as '\''),
 * env as KEY='VALUE'. Non-empty working_dir prefixes "cd '<dir>' && " so a
 * missing directory fails the launch instead of running in the wrong cwd.
 */
std::string ToPosixShellCommand(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env = {},
    std::string const&                                      working_dir = std::string());

/*
 * CreateProcessA lpCommandLine string (CommandLineToArgvW reverse quoting).
 * Env is a separate block on Windows, not included here.
 */
std::string ToWindowsCommandLine(std::vector<std::string> const& argv);

/*
 * Human-readable argv+env for logs and the launcher preamble. Not shell-safe.
 */
std::string ToDisplayString(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env = {});

} // namespace Cmdline
} // namespace Controller
} // namespace RocProfVis
