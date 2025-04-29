// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once

#include "rocprofvis_controller.h"

#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace RocProfVis
{
namespace Controller
{

class StringTable
{
public:
    StringTable();
    ~StringTable();

    size_t AddString(std::string& string);
    char const* GetString(size_t id);

    static StringTable& Get();

private:
    std::unordered_map<std::string, size_t> m_entries;
    std::vector<std::string> m_strings;
    std::mutex m_mutex;
    static StringTable s_self;
};

}
}
