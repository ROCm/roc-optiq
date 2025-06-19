// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_controller_string_table.h"
#include "rocprofvis_core_assert.h"

namespace RocProfVis
{
namespace Controller
{

StringTable StringTable::s_self;
    
StringTable& StringTable::Get()
{
    return s_self;
}

StringTable::StringTable()
{
}

StringTable::~StringTable()
{
}

size_t StringTable::AddString(std::string& string)
{
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    auto it = m_entries.find(string);
    if (it == m_entries.end())
    {
        m_entries.insert(std::make_pair(string, m_strings.size()));
        it = m_entries.find(string);
        m_strings.push_back(it->first.c_str());
    }
    return it->second;
}

char const* StringTable::GetString(size_t id)
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    char const* string = "";
    if (m_strings.size() > id)
    {
        string = m_strings[id].c_str();
    }
    return string;
}

}
}
