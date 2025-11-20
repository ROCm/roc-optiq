// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "rocprofvis_controller.h"

#include <shared_mutex>
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

    size_t AddString(const char* string, bool store);
    char const* GetString(size_t id);

    static StringTable& Get();

private:
    std::unordered_map<std::string, size_t> m_string_entries;
    std::unordered_map<const char*, size_t> m_charptr_entries;
    std::vector<const char*> m_strings;
    std::shared_mutex m_mutex;
    static StringTable s_self;
};

}
}
