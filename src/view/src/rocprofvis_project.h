// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once

#include "imgui.h"
#include <cstdint>
#include <string>
#include <vector>

namespace RocProfVis
{
namespace View
{

// A Project is a Chrome-style "tab group": a named, colored, ordered collection
// of Items (open tabs) plus a memory of Items that were closed but still belong
// to the project so they can be reopened. A single tab is an ProjectItem
// (rocprofvis_project_item.h); a Project groups several Items together.
class Project
{
public:
    // A member ProjectItem that was closed but is remembered so it can be reopened.
    struct ClosedItem
    {
        std::string              name;   // display label (file/tab name)
        std::vector<std::string> files;  // filelist to reopen (1 = trace/compute, 2+ = compare)
    };

    Project(const std::string& id, const std::string& name, ImU32 color);

    const std::string& GetID() const;
    const std::string& GetName() const;
    void               SetName(const std::string& name);
    ImU32              GetColor() const;
    void               SetColor(ImU32 color);
    // Reserved for the follow-up collapse/expand interaction.
    bool               IsCollapsed() const;
    void               SetCollapsed(bool collapsed);

    // Open member ProjectItem ids, in group order.
    const std::vector<std::string>& GetItemIds() const;
    // Reorders the open members to match the given order (ids not currently members
    // are ignored; any members omitted keep their relative order at the end).
    void SetItemOrder(const std::vector<std::string>& ordered);
    // Adds an open member if not already present. Returns true if it was added.
    bool AddItem(const std::string& item_id);
    // Removes an open member if present. Returns true if it was removed.
    bool RemoveItem(const std::string& item_id);
    bool ContainsItem(const std::string& item_id) const;

    const std::vector<ClosedItem>& GetClosedItems() const;
    void                           AddClosedItem(const ClosedItem& item);
    void                           RemoveClosedItemAt(size_t index);

    // A project is empty (safe to delete) when it has neither open nor closed items.
    bool Empty() const;

    // The .rpv file this project was loaded from / last saved to (empty if never
    // saved). Lets "Save" re-save the whole project without a dialog.
    const std::string& GetFilePath() const;
    void               SetFilePath(const std::string& file_path);
    bool               IsSaved() const;

private:
    std::string              m_id;
    std::string              m_name;
    ImU32                    m_color;
    bool                     m_collapsed;
    std::vector<std::string> m_item_ids;      // open members (tab ids), ordered
    std::vector<ClosedItem>  m_closed_items;  // closed-but-remembered members
    std::string              m_file_path;     // associated .rpv (empty = unsaved)
};

}  // namespace View
}  // namespace RocProfVis
