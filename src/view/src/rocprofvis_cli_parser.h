// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include <map>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

// Define the Option structure
struct CmdOption
{
    std::string name;       
    std::string short_flag;  
    std::string long_flag;   
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

    void        Parse(int argc, char** argv);
    bool        IsFileProvided() const;
    std::string GetProvidedFilePath() const;

private:
    bool        m_is_file_provided;
    std::string m_provided_file_path;
    void        AttachToConsole();  // New helper method
    void        PrintVersion();     // New helper method
};

}  // namespace View
}  // namespace RocProfVis
