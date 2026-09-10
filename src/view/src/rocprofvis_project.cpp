// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_project.h"

#include <algorithm>

namespace RocProfVis
{
namespace View
{

Project::Project(const std::string& id, const std::string& name, ImU32 color)
: m_id(id)
, m_name(name)
, m_color(color)
, m_collapsed(false)
{}

const std::string&
Project::GetID() const
{
    return m_id;
}

const std::string&
Project::GetName() const
{
    return m_name;
}

void
Project::SetName(const std::string& name)
{
    m_name = name;
}

ImU32
Project::GetColor() const
{
    return m_color;
}

void
Project::SetColor(ImU32 color)
{
    m_color = color;
}

bool
Project::IsCollapsed() const
{
    return m_collapsed;
}

void
Project::SetCollapsed(bool collapsed)
{
    m_collapsed = collapsed;
}

const std::vector<std::string>&
Project::GetItemIds() const
{
    return m_item_ids;
}

void
Project::SetItemOrder(const std::vector<std::string>& ordered)
{
    std::vector<std::string> result;
    result.reserve(m_item_ids.size());
    for(const std::string& id : ordered)
    {
        if(ContainsItem(id))
        {
            result.push_back(id);
        }
    }
    for(const std::string& id : m_item_ids)
    {
        if(std::find(result.begin(), result.end(), id) == result.end())
        {
            result.push_back(id);
        }
    }
    m_item_ids = result;
}

bool
Project::AddItem(const std::string& item_id)
{
    bool added = false;
    if(!ContainsItem(item_id))
    {
        m_item_ids.push_back(item_id);
        added = true;
    }
    return added;
}

bool
Project::RemoveItem(const std::string& item_id)
{
    bool                                 removed = false;
    std::vector<std::string>::iterator   it =
        std::find(m_item_ids.begin(), m_item_ids.end(), item_id);
    if(it != m_item_ids.end())
    {
        m_item_ids.erase(it);
        removed = true;
    }
    return removed;
}

bool
Project::ContainsItem(const std::string& item_id) const
{
    return std::find(m_item_ids.begin(), m_item_ids.end(), item_id) != m_item_ids.end();
}

const std::vector<Project::ClosedItem>&
Project::GetClosedItems() const
{
    return m_closed_items;
}

void
Project::AddClosedItem(const ClosedItem& item)
{
    m_closed_items.push_back(item);
}

void
Project::RemoveClosedItemAt(size_t index)
{
    if(index < m_closed_items.size())
    {
        m_closed_items.erase(m_closed_items.begin() + index);
    }
}

bool
Project::Empty() const
{
    return m_item_ids.empty() && m_closed_items.empty();
}

const std::string&
Project::GetFilePath() const
{
    return m_file_path;
}

void
Project::SetFilePath(const std::string& file_path)
{
    m_file_path = file_path;
}

bool
Project::IsSaved() const
{
    return !m_file_path.empty();
}

}  // namespace View
}  // namespace RocProfVis
