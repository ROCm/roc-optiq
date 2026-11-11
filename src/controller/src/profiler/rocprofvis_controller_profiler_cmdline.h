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
 * Env for a local launch, which runs no shell and so cannot source the venv's
 * bin/activate. When the config names a Python environment this does what
 * activate does to the environment: VIRTUAL_ENV=<venv>, and PATH with
 * <venv>/bin in front - of the config's own PATH entries, changed in place, or
 * when it has none, of this process's PATH, appended. The caller also unsets
 * PYTHONHOME. POSIX only; on Windows this is BuildEnv.
 */
std::vector<std::pair<std::string, std::string>> BuildLocalEnv(ProfilerConfig const& config);

/*
 * The composition behind BuildLocalEnv: `env` with <venv>/bin in front of its
 * PATH entries, or a PATH appended when it has none, then VIRTUAL_ENV appended.
 * `inherited_path` is the base PATH when env sets none - BuildLocalEnv passes
 * this process's, the command preview passes "$PATH".
 */
std::vector<std::pair<std::string, std::string>> ComposeLocalEnv(
    std::vector<std::pair<std::string, std::string>> const& env,
    std::string const&                                      python_environment,
    std::string const&                                      inherited_path);

/*
 * Env for a remote launch, as ToPosixShellCommand sets it after sourcing the
 * venv's bin/activate: a PATH entry gets <venv>/bin in front of its value, since
 * a plain assignment would replace the PATH activate set. Unchanged without a
 * venv.
 */
std::vector<std::pair<std::string, std::string>> ComposeRemoteEnv(
    std::vector<std::pair<std::string, std::string>> const& env,
    std::string const&                                      python_environment);

/*
 * `relative` ("bin", "bin/activate") under the venv directory, joined with '/'
 * whether or not the directory has a trailing separator.
 */
std::string PythonEnvironmentPath(std::string const& python_environment,
                                  std::string const& relative);

/*
 * True if `name` is [A-Za-z_][A-Za-z0-9_]*. Invalid names are rejected because
 * ToPosixShellCommand emits the name unquoted.
 */
bool IsValidEnvName(std::string const& name);

/*
 * POSIX /bin/sh command: each token single-quoted (embedded ' as '\''),
 * env as KEY='VALUE'. Non-empty working_dir prefixes "cd '<dir>' && " so a
 * missing directory fails the launch instead of running in the wrong cwd.
 *
 * A non-empty python_environment sources '<venv>/bin/activate' after the cd,
 * joined with && so a missing venv fails the launch too, and wraps the whole
 * command in sh -c '...' so the activation runs under a POSIX shell whatever
 * the remote user's login shell is. env goes through ComposeRemoteEnv, so a
 * PATH entry keeps <venv>/bin in front of its value.
 */
std::string ToPosixShellCommand(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env = {},
    std::string const&                                      working_dir = std::string(),
    std::string const&                                      python_environment = std::string());

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

/*
 * The launcher's command preview: a "cd <working_dir> &&" line when one is set,
 * the venv activation for a remote run, one NAME=value line per entry of the
 * already composed `env` (empty values included), then argv with each flag on
 * its own continuation line and everything after "--" kept on one. Not
 * shell-safe.
 */
std::string ToPreviewString(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env,
    std::string const&                                      working_dir,
    std::string const&                                      python_environment,
    bool                                                    remote);

} // namespace Cmdline
} // namespace Controller
} // namespace RocProfVis
