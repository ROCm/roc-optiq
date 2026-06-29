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
 * Build the canonical argv from a ProfilerConfig. argv[0] is the profiler
 * executable path; argv[1..] are the arguments in the order:
 *   1. Explicit profiler argv entries (config.GetProfilerArgv()).
 *   2. Legacy whitespace-split profiler_args (config.GetProfilerArgs()).
 *   3. "--output <output_directory>" if set.
 *   4. "--" "<target_executable>" if set.
 *   5. Whitespace-split target args (config.GetTargetArgs()).
 *
 * The whitespace-split behavior for legacy profiler_args / target_args is
 * preserved for backwards compatibility with existing callers; new callers
 * should prefer AddProfilerArg() so tokens containing spaces survive.
 */
std::vector<std::string> BuildArgv(ProfilerConfig const& config);

/*
 * Return the env vars from the config as a vector of (name, value) pairs.
 * Currently a pass-through; exists as a seam for future per-executor tweaks.
 */
std::vector<std::pair<std::string, std::string>> BuildEnv(ProfilerConfig const& config);

/*
 * Serialize argv (and optional env) into a single string suitable for a POSIX
 * /bin/sh interpreter - i.e. ssh user@host "<this>" or sh -c "<this>". Each
 * token is single-quoted; embedded single quotes are emitted as '\''. Env
 * pairs are prepended as KEY='VALUE'.
 */
std::string ToPosixShellCommand(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env = {});

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


/* 
 * SSHLIB2 example
 * 

auto remote_cmd = Cmdline::ToPosixShellCommand(argv, env);  

libssh2_session_handshake(session, sock);
libssh2_userauth_publickey_fromfile(session, info.user.c_str(),
                                    pub_path.c_str(), priv_path.c_str(), nullptr);
auto* ch = libssh2_channel_open_session(session);
libssh2_channel_exec(ch, remote_cmd.c_str());

// then read loop + signal/close + sftp fetch...

*/

} // namespace Cmdline
} // namespace Controller
} // namespace RocProfVis
