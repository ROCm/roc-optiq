// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_data_protocol.h"

namespace RocProfVis
{
namespace View
{

uint32_t StringTable::AddString(const char* str)
{
    std::string key = str ? str : "";
    auto it = m_index_map.find(key);
    if(it != m_index_map.end())
    {
        return it->second;
    }

    uint32_t index = static_cast<uint32_t>(m_strings.size());
    m_strings.push_back(key);
    m_index_map[key] = index;
    return index;
}

const char* StringTable::GetString(uint32_t index) const
{
    if(index < m_strings.size())
    {
        return m_strings[index].c_str();
    }
    return "";
}

void StringTable::Clear()
{
    m_strings.clear();
    m_index_map.clear();
}

}  // namespace View
}  // namespace RocProfVis
