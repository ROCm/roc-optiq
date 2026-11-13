// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_profiler_cmdline.h"
#include "rocprofvis_controller_profiler_process.h"

#include <cctype>
#include <cstdlib>
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

std::vector<std::pair<std::string, std::string>> BuildLocalEnv(ProfilerConfig const& config)
{
    char const* inherited = std::getenv("PATH");
    return ComposeLocalEnv(BuildEnv(config), config.GetPythonEnvironment(),
                           inherited != nullptr ? inherited : "");
}

std::vector<std::pair<std::string, std::string>> ComposeLocalEnv(
    std::vector<std::pair<std::string, std::string>> const& env,
    std::string const&                                      python_environment,
    std::string const&                                      inherited_path)
{
    std::vector<std::pair<std::string, std::string>> composed = env;
#ifndef _WIN32
    if (!python_environment.empty())
    {
        // A PATH the user set gets <venv>/bin in front in place, as
        // ComposeRemoteEnv does; without one, the inherited PATH is the base.
        std::string venv_bin  = PythonEnvironmentPath(python_environment, "bin");
        bool        user_path = false;
        for (std::pair<std::string, std::string>& kv : composed)
        {
            if (kv.first == "PATH")
            {
                kv.second = kv.second.empty() ? venv_bin : venv_bin + ":" + kv.second;
                user_path = true;
            }
        }
        if (!user_path)
        {
            composed.emplace_back("PATH", inherited_path.empty() ? venv_bin
                                                                 : venv_bin + ":" + inherited_path);
        }
        composed.emplace_back("VIRTUAL_ENV", python_environment);
    }
#else
    (void)python_environment;
    (void)inherited_path;
#endif
    return composed;
}

std::vector<std::pair<std::string, std::string>> ComposeRemoteEnv(
    std::vector<std::pair<std::string, std::string>> const& env,
    std::string const&                                      python_environment)
{
    std::vector<std::pair<std::string, std::string>> composed = env;
    if (!python_environment.empty())
    {
        // Keeps <venv>/bin first, as ComposeLocalEnv does: a plain assignment
        // after activate would replace the PATH it just set.
        std::string venv_bin = PythonEnvironmentPath(python_environment, "bin");
        for (std::pair<std::string, std::string>& kv : composed)
        {
            if (kv.first == "PATH")
            {
                kv.second = kv.second.empty() ? venv_bin : venv_bin + ":" + kv.second;
            }
        }
    }
    return composed;
}

std::string PythonEnvironmentPath(std::string const& python_environment,
                                  std::string const& relative)
{
    return (!python_environment.empty() && python_environment.back() == '/')
               ? python_environment + relative
               : python_environment + "/" + relative;
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
    std::string const&                                      working_dir,
    std::string const&                                      python_environment)
{
    std::ostringstream oss;
    bool               first = true;

    if (!working_dir.empty())
    {
        oss << "cd " << posix_shell_quote(working_dir) << " &&";
        first = false;
    }

    if (!python_environment.empty())
    {
        if (!first)
        {
            oss << ' ';
        }
        // "." rather than "source": the latter is not POSIX sh.
        oss << ". "
            << posix_shell_quote(PythonEnvironmentPath(python_environment, "bin/activate"))
            << " &&";
        first = false;
    }

    for (std::pair<std::string, std::string> const& kv : ComposeRemoteEnv(env, python_environment))
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

    return python_environment.empty() ? oss.str() : "sh -c " + posix_shell_quote(oss.str());
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

std::string ToPreviewString(
    std::vector<std::string> const&                         argv,
    std::vector<std::pair<std::string, std::string>> const& env,
    std::string const&                                      working_dir,
    std::string const&                                      python_environment,
    bool                                                    remote)
{
    std::ostringstream preview;

    if (!working_dir.empty())
    {
        preview << "cd " << working_dir << " && \\\n";
    }
    if (remote && !python_environment.empty())
    {
        preview << ". " << PythonEnvironmentPath(python_environment, "bin/activate")
                << " && \\\n";
    }
    for (std::pair<std::string, std::string> const& kv : env)
    {
        preview << kv.first << '=' << kv.second << " \\\n";
    }

    // Only line breaks are added: each flag starts a continuation line (its
    // value stays beside it), and everything after "--" is the target's own
    // command line.
    constexpr char const* CONTINUATION = " \\\n    ";
    for (size_t i = 0; i < argv.size(); i++)
    {
        std::string const& arg = argv[i];
        if (i == 0)
        {
            preview << arg;
        }
        else if (arg == "--")
        {
            preview << CONTINUATION << arg;
            for (size_t j = i + 1; j < argv.size(); j++)
            {
                preview << ' ' << argv[j];
            }
            break;
        }
        else
        {
            bool const is_flag = arg.size() > 1 && arg[0] == '-';
            preview << (is_flag ? CONTINUATION : " ") << arg;
        }
    }

    return preview.str();
}

} // namespace Cmdline
} // namespace Controller
} // namespace RocProfVis
