// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_cli_parser.h"
#include "rocprofvis_version.h"
#include <filesystem>
#include <iostream>
#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#endif

const char* APP_NAME = "ROCm(TM) Optiq Beta";

namespace RocProfVis
{
namespace View
{

CLIParser::CLIParser()
: m_is_file_provided(false)
, m_provided_file_path("")
{}

CLIParser::~CLIParser() = default;

void
CLIParser::Parse(int argc, char** argv)
{
    std::vector<CmdOption> options = { { "file", "-f", "--file", true },
                                       { "version", "-v", "--version", false }

    };

    std::map<std::string, CmdArgResult> results;

    for(int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        for(const auto& opt : options)
        {
            if(arg == opt.short_flag || arg == opt.long_flag)
            {
                results[opt.name].found = true;

                if(opt.take_arg && (i + 1 < argc))
                {
                    results[opt.name].argument = argv[i + 1];
                    i                          = i + 1;
                }
                break;
            }
        }
    }

    for(const auto& pair : results)
    {
        if(pair.first == "file" && pair.second.found)
        {
            if(std::filesystem::is_regular_file(pair.second.argument))
            {
                // Check if the argument is a valid file. If not, skip setting the file
                // path.
                m_provided_file_path = pair.second.argument;
                m_is_file_provided   = true;
            }
        }
        if(pair.first == "version")
        {
            PrintVersion();
            exit(0);
        }
    }
}

void
CLIParser::AttachToConsole()
{
#ifdef _WIN32
    // For windows attach to existing CLI window do not make a new window.
    if(AttachConsole(ATTACH_PARENT_PROCESS) || GetLastError() == ERROR_ACCESS_DENIED)
    {
        FILE* pCout;
        freopen_s(&pCout, "CONOUT$", "w", stdout);
        freopen_s(&pCout, "CONOUT$", "w", stderr);

        std::ios::sync_with_stdio(true);
    }
#endif
}

void
CLIParser::PrintVersion()
{
    AttachToConsole();

    std::cout << APP_NAME << " version: " << ROCPROFVIS_VERSION_MAJOR << "."
              << ROCPROFVIS_VERSION_MINOR << "." << ROCPROFVIS_VERSION_PATCH << "."
              << ROCPROFVIS_VERSION_BUILD << std::endl;

    std::cout.flush();
}

bool
CLIParser::IsFileProvided() const
{
    return m_is_file_provided;
}

std::string
CLIParser::GetProvidedFilePath() const
{
    return m_provided_file_path;
}

}  // namespace View
}  // namespace RocProfVis
