// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <string>

namespace RocProfVis
{
namespace View
{

// Define the Option structure
struct CmdOption
{
    std::string short_flag;
    std::string long_flag;
    std::string description;
    bool        take_arg;
};

// Define the Result structure
struct CmdArgResult
{
    bool        found = false;
    std::string argument;
};

class CLIParser
{
public:
    CLIParser();
    ~CLIParser();

    void SetAppDescription(const std::string& name, const std::string& desc);
    bool AddOption(const std::string& short_flag, const std::string& long_flag, const std::string& desc, bool take_arg);
    
    void Parse(int argc, char** argv);
    
    bool WasOptionFound(const std::string& option) const;
    std::string GetOptionValue(const std::string& option) const;
    size_t      GetOptionCount() const;
    
    std::string GetHelp() const;

    static void AttachToConsole();

private:
    std::string                         m_app_name;
    std::string                         m_app_desc;
    std::map<std::string, CmdOption>    m_options;
    std::map<std::string, std::string>  m_short_to_long;
    std::map<std::string, CmdArgResult> m_results;
};

}  // namespace View
}  // namespace RocProfVis
