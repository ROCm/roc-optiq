// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_cli_parser.h"
#include "spdlog/spdlog.h"

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

const size_t MIN_LONG_FLAG_SIZE = 3;

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

bool
CLIParser::AddOption(const std::string& short_flag,
                     const std::string& long_flag,
                     const std::string& desc,
                     bool               take_arg)
{
    if(short_flag.empty() || long_flag.empty())
    {
        spdlog::warn("CLIParser::AddOption: Flags cannot be empty");
        return false;
    }

    // Check for duplicate long flag
    if(m_options.find(long_flag) != m_options.end())
    {
        spdlog::warn("CLIParser::AddOption: long flag already exists. {}", long_flag);
        return false;
    }
    
    // Check for duplicate short flag
    if(m_short_to_long.find(short_flag) != m_short_to_long.end())
    {
        spdlog::warn("CLIParser::AddOption: short flag already exists. {}", short_flag);
        return false;
    }
    
    m_options[long_flag] = { short_flag, long_flag, desc, take_arg };
    m_short_to_long[short_flag] = long_flag;
    return true;
}

void
CLIParser::Parse(int argc, char** argv)
{
    m_results.clear();

    for(int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        std::map<std::string, CmdOption>::const_iterator opt_it = m_options.end();

        // Check for long flag (--flag)
        if(arg.size() >= MIN_LONG_FLAG_SIZE && arg[0] == '-' && arg[1] == '-')
        {
            std::string long_flag = arg.substr(2);
            opt_it = m_options.find(long_flag);
        }
        // Check for short flag (-f)
        else if(arg.size() > 1 && arg[0] == '-')
        {
            std::string short_flag = arg.substr(1);
            auto short_it = m_short_to_long.find(short_flag);
            if(short_it != m_short_to_long.end())
            {
                opt_it = m_options.find(short_it->second);
            }
        }

        if(opt_it != m_options.end())
        {
            const std::string& long_flag = opt_it->first;
            const CmdOption& opt = opt_it->second;
            
            m_results[long_flag].found = true;

            if(opt.take_arg && (i + 1 < argc))
            {
                m_results[long_flag].argument = argv[i + 1];
                i++;  // Skip the next argument as it's consumed
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
    for(const auto& [_, opt] : m_options)
    {
        ss << "[-" << opt.short_flag << (opt.take_arg ? " <arg>] " : "] ");
    }
    ss << "\n\nOptions:\n";

    for(const auto& [_, opt] : m_options)
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

void
CLIParser::DetachFromConsole()
{
#ifdef _WIN32
    FreeConsole();
#endif
}

}  // namespace View
}  // namespace RocProfVis
