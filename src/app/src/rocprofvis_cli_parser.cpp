// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_cli_parser.h"
#include <iostream>
#include <sstream>
#include <iomanip>

#ifdef _WIN32
#    define NOMINMAX
#    include <windows.h>
#endif

namespace RocProfVis
{
namespace View
{

CLIParser::CLIParser()
: m_app_name("")
, m_app_desc("")
{}

CLIParser::~CLIParser() = default;

void
CLIParser::SetAppDescription(const std::string& name, const std::string& desc)
{
    m_app_name = name;
    m_app_desc = desc;
}

void
CLIParser::AddOption(const std::string& short_flag,
                     const std::string& long_flag,
                     const std::string& desc,
                     bool               take_arg)
{
    m_options.push_back({ short_flag, long_flag, desc, take_arg });
}

void
CLIParser::Parse(int argc, char** argv)
{
    m_results.clear();

    for(int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];

        for(const auto& opt : m_options)
        {
            std::string short_opt = "-" + opt.short_flag;
            std::string long_opt  = "--" + opt.long_flag;

            if(arg == short_opt || arg == long_opt)
            {
                m_results[opt.long_flag].found = true;

                if(opt.take_arg && (i + 1 < argc))
                {
                    m_results[opt.long_flag].argument = argv[i + 1];
                    i++;  // Skip the next argument as it's consumed
                }
                break;
            }
        }
    }
}

bool
CLIParser::WasOptionFound(const std::string& option) const
{
    auto it = m_results.find(option);
    return (it != m_results.end() && it->second.found);
}

std::string
CLIParser::GetOptionValue(const std::string& option) const
{
    auto it = m_results.find(option);
    if(it != m_results.end())
    {
        return it->second.argument;
    }
    return "";
}

size_t
CLIParser::GetOptionCount() const
{
    size_t count = 0;
    for(const auto& pair : m_results)
    {
        if(pair.second.found)
            count++;
    }
    return count;
}

std::string
CLIParser::GetHelp() const
{
    std::stringstream ss;
    ss << m_app_name << ": " << m_app_desc << "\n";
    ss << "Usage: " << m_app_name << " ";
    for(const auto& opt : m_options)
    {
        ss << "[-" << opt.short_flag << (opt.take_arg ? " <arg>] " : "] ");
    }
    ss << "\n\nOptions:\n";

    for(const auto& opt : m_options)
    {
        ss << "  -" << opt.short_flag << ", --" << std::left << std::setw(20) << opt.long_flag
           << opt.description << "\n";
    }

    return ss.str();
}

void
CLIParser::AttachToConsole()
{
#ifdef _WIN32
    // Attach to parent console if present. If already attached, ERROR_ACCESS_DENIED is
    // expected, so still wire stdout/stderr to the console handles.
    if(AttachConsole(ATTACH_PARENT_PROCESS) || GetLastError() == ERROR_ACCESS_DENIED)
    {
        FILE* pCout;
        freopen_s(&pCout, "CONOUT$", "w", stdout);
        freopen_s(&pCout, "CONOUT$", "w", stderr);
    }
#endif
}

}  // namespace View
}  // namespace RocProfVis
