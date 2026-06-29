// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_cmdline.h"
#include "rocprofvis_controller_profiler_process.h"

#include <sstream>

namespace RocProfVis
{
namespace Controller
{
namespace Cmdline
{

namespace
{

void append_whitespace_split(std::vector<std::string>& out, std::string const& s)
{
    if (s.empty())
    {
        return;
    }
    std::istringstream iss(s);
    std::string        token;
    while (iss >> token)
    {
        out.push_back(token);
    }
}

// POSIX shell single-quote a single token. Embedded ' becomes '\''.
std::string posix_shell_quote(std::string const& tok)
{
    std::string out;
    out.reserve(tok.size() + 2);
    out.push_back('\'');
    for (char c : tok)
    {
        if (c == '\'')
        {
            out.append("'\\''");
        }
        else
        {
            out.push_back(c);
        }
    }
    out.push_back('\'');
    return out;
}

// Windows CRT (CommandLineToArgvW reverse) quoting for a single token.
// Algorithm follows Microsoft's documented rules:
//   - Tokens with no whitespace or quotes are passed through unchanged.
//   - Otherwise wrap in double quotes and double-up backslash runs that
//     immediately precede a quote, escaping the quote itself with one more
//     backslash.
std::string windows_quote(std::string const& tok)
{
    if (!tok.empty() && tok.find_first_of(" \t\n\v\"") == std::string::npos)
    {
        return tok;
    }

    std::string out;
    out.reserve(tok.size() + 2);
    out.push_back('"');

    for (auto it = tok.begin(); it != tok.end();)
    {
        size_t n_backslashes = 0;
        while (it != tok.end() && *it == '\\')
        {
            ++it;
            ++n_backslashes;
        }

        if (it == tok.end())
        {
            out.append(n_backslashes * 2, '\\');
            break;
        }
        else if (*it == '"')
        {
            out.append(n_backslashes * 2 + 1, '\\');
            out.push_back('"');
            ++it;
        }
        else
        {
            out.append(n_backslashes, '\\');
            out.push_back(*it);
            ++it;
        }
    }

    out.push_back('"');
    return out;
}

} // namespace

std::vector<std::string> BuildArgv(ProfilerConfig const& config)
{
    std::vector<std::string> argv;

    argv.push_back(config.GetProfilerPath());

    for (auto const& arg : config.GetProfilerArgv())
    {
        argv.push_back(arg);
    }

    append_whitespace_split(argv, config.GetProfilerArgs());

    if (!config.GetOutputDirectory().empty())
    {
        argv.emplace_back("--output");
        argv.push_back(config.GetOutputDirectory());
    }

    if (!config.GetTargetExecutable().empty())
    {
        argv.emplace_back("--");
        argv.push_back(config.GetTargetExecutable());
    }

    append_whitespace_split(argv, config.GetTargetArgs());

    return argv;
}

std::vector<std::pair<std::string, std::string>> BuildEnv(ProfilerConfig const& config)
{
    return config.GetEnvVars();
}

std::string ToPosixShellCommand(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env)
{
    std::ostringstream oss;
    bool               first = true;

    for (auto const& kv : env)
    {
        if (!first)
        {
            oss << ' ';
        }
        oss << kv.first << '=' << posix_shell_quote(kv.second);
        first = false;
    }

    for (auto const& tok : argv)
    {
        if (!first)
        {
            oss << ' ';
        }
        oss << posix_shell_quote(tok);
        first = false;
    }

    return oss.str();
}

std::string ToWindowsCommandLine(std::vector<std::string> const& argv)
{
    std::ostringstream oss;
    bool               first = true;
    for (auto const& tok : argv)
    {
        if (!first)
        {
            oss << ' ';
        }
        oss << windows_quote(tok);
        first = false;
    }
    return oss.str();
}

std::string ToDisplayString(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env)
{
    std::ostringstream oss;
    bool               first = true;

    for (auto const& kv : env)
    {
        if (!first)
        {
            oss << ' ';
        }
        oss << kv.first << '=' << kv.second;
        first = false;
    }

    for (auto const& tok : argv)
    {
        if (!first)
        {
            oss << ' ';
        }
        oss << tok;
        first = false;
    }

    return oss.str();
}

} // namespace Cmdline
} // namespace Controller
} // namespace RocProfVis
