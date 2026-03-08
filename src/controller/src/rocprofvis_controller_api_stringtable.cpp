// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_controller_api.h"

#include <string>

namespace RocProfVis
{
namespace Controller
{

ApiStringTable::ApiStringTable()
{
    // Reserve slot 0 for empty string
    m_strings.push_back("");
    m_index_map[""] = 0;
}

ApiStringTable::~ApiStringTable() = default;

uint32_t ApiStringTable::AddString(const char* str)
{
    const char* safe_string = str ? str : "";
    std::string key(safe_string);

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

const char* ApiStringTable::GetString(uint32_t index) const
{
    if(index < m_strings.size())
    {
        return m_strings[index].c_str();
    }
    return "";  // Return empty string for invalid index
}

uint32_t ApiStringTable::GetCount() const
{
    return static_cast<uint32_t>(m_strings.size());
}

void ApiStringTable::Clear()
{
    m_strings.clear();
    m_index_map.clear();

    // Re-initialize with empty string at index 0
    m_strings.push_back("");
    m_index_map[""] = 0;
}

}  // namespace Controller
}  // namespace RocProfVis
