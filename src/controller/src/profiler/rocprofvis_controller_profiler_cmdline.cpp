// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_cmdline.h"
#include "rocprofvis_controller_profiler_process.h"

#include <cctype>
#include <sstream>

namespace RocProfVis
{
namespace Controller
{
namespace Cmdline
{

namespace
{

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

// CommandLineToArgvW reverse quoting: quote if whitespace/quotes; double
// backslashes that run up to a quote (and the quote itself).
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
    argv.reserve(config.GetProfilerArgv().size() + 1);

    argv.push_back(config.GetResolvedToolPath());

    for (auto const& arg : config.GetProfilerArgv())
    {
        argv.push_back(arg);
    }

    return argv;
}

std::vector<std::pair<std::string, std::string>> BuildEnv(ProfilerConfig const& config)
{
    return config.GetEnvVars();
}

bool IsValidEnvName(std::string const& name)
{
    if (name.empty())
    {
        return false;
    }
    if (!(std::isalpha(static_cast<unsigned char>(name[0])) || name[0] == '_'))
    {
        return false;
    }
    for (char c : name)
    {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_'))
        {
            return false;
        }
    }
    return true;
}

std::string ToPosixShellCommand(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env,
    std::string const&                                      working_dir)
{
    std::ostringstream oss;
    bool               first = true;

    if (!working_dir.empty())
    {
        oss << "cd " << posix_shell_quote(working_dir) << " &&";
        first = false;
    }

    for (auto const& kv : env)
    {
        // Name is emitted unquoted; skip invalid ones (AddEnvVar already rejects).
        if (!IsValidEnvName(kv.first))
        {
            continue;
        }
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
