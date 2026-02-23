// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_query_builder.h"
#include "rocprofvis_settings_manager.h"
#include "imgui.h"

namespace RocProfVis
{
namespace View
{

QueryBuilder::QueryBuilder()
{
    m_widget_name = GenUniqueName("QueryBuilder");
    m_selections.resize(LEVEL_COUNT, std::nullopt);
}

void
QueryBuilder::SetWorkloads(const std::unordered_map<uint32_t, WorkloadInfo>* workloads)
{
    m_workloads = workloads;
}

void
QueryBuilder::Open()
{
    m_should_open = true;
}

void
QueryBuilder::Render()
{
    if(m_should_open)
    {
        ImGui::OpenPopup("Query Builder");
        m_should_open = false;
    }

    if(!ImGui::IsPopupOpen("Query Builder"))
        return;

    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();

    ImGui::SetNextWindowSizeConstraints(ImVec2(400, 200), ImVec2(400, 600));
    if(ImGui::BeginPopupModal("Query Builder", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
    {
        ImGui::Text("%s", GetLevelLabel(m_level));
        ImGui::Separator();
        RenderTags();
        RenderList();
        if(ImGui::Button("Search", ImVec2(-1, 0)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void
QueryBuilder::RenderTags()
{
    SettingsManager& settings = SettingsManager::GetInstance();
    int clear = -1;

    ImGui::BeginChild("##tags", ImVec2(-1, ImGui::GetFrameHeightWithSpacing()),
                      ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_HorizontalScrollbar);

    for(int i = 0; i < LEVEL_COUNT; i++)
    {
        if(!m_selections[i])
            continue;
        if(i > 0)
            ImGui::SameLine();

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kAccentRed));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, settings.GetColor(Colors::kAccentRedHover));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, settings.GetColor(Colors::kAccentRedActive));

        std::string tag = "L" + std::to_string(i + 1) + ": " + m_selections[i]->label + "  X";
        if(ImGui::SmallButton(tag.c_str()))
            clear = i;

        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    if(m_scroll_to_end)
    {
        ImGui::SetScrollHereX(1.0f);
        m_scroll_to_end = false;
    }

    ImGui::EndChild();

    if(clear >= 0)
        ClearFrom(clear);
}

void
QueryBuilder::RenderList()
{
    std::vector<LevelItem> items = GetItems();

    ImGui::BeginChild("##list", ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 12),
                      ImGuiChildFlags_Borders);
    for(size_t i = 0; i < items.size(); i++)
    {
        ImGui::PushID(static_cast<int>(i));
        bool selected = m_selections[m_level] && m_selections[m_level]->id == items[i].id;
        if(ImGui::Selectable(items[i].label.c_str(), selected))
            Select(m_level, items[i]);
        ImGui::PopID();
    }
    if(items.empty())
        ImGui::TextDisabled((!m_workloads || m_workloads->empty()) ? "No workloads loaded"
                                                                    : "No items at this level");
    ImGui::EndChild();
}

std::vector<QueryBuilder::LevelItem>
QueryBuilder::GetItems() const
{
    std::vector<LevelItem> items;
    if(!m_workloads)
        return items;

    if(m_level == LEVEL_WORKLOAD)
    {
        for(const auto& [id, wl] : *m_workloads)
            items.push_back({ id, wl.name });
        return items;
    }

    if(!m_selections[LEVEL_WORKLOAD])
        return items;
    auto wl_it = m_workloads->find(m_selections[LEVEL_WORKLOAD]->id);
    if(wl_it == m_workloads->end())
        return items;
    const WorkloadInfo& workload = wl_it->second;

    if(m_level == LEVEL_CATEGORY)
    {
        for(const auto& [id, cat] : workload.available_metrics.tree)
            items.push_back({ id, "[" + std::to_string(id) + "] " + cat.name });
        return items;
    }

    if(!m_selections[LEVEL_CATEGORY])
        return items;
    uint32_t cat_id = m_selections[LEVEL_CATEGORY]->id;
    auto cat_it = workload.available_metrics.tree.find(cat_id);
    if(cat_it == workload.available_metrics.tree.end())
        return items;
    const AvailableMetrics::Category& category = cat_it->second;

    if(m_level == LEVEL_TABLE)
    {
        for(const auto& [id, tbl] : category.tables)
            items.push_back({ id, "[" + std::to_string(cat_id) + "." + std::to_string(id) + "] " + tbl.name });
        return items;
    }

    if(!m_selections[LEVEL_TABLE])
        return items;
    uint32_t tbl_id = m_selections[LEVEL_TABLE]->id;
    auto tbl_it = category.tables.find(tbl_id);
    if(tbl_it == category.tables.end())
        return items;
    const AvailableMetrics::Table& table = tbl_it->second;

    if(m_level == LEVEL_ENTRY)
    {
        std::string prefix = std::to_string(cat_id) + "." + std::to_string(tbl_id) + ".";
        for(const auto& [id, entry] : table.entries)
            items.push_back({ id, "[" + prefix + std::to_string(id) + "] " + entry.name });
        return items;
    }

    if(m_level == LEVEL_VALUE_NAME)
    {
        for(uint32_t i = 0; i < table.value_names.size(); i++)
            items.push_back({ i, table.value_names[i] });
    }

    return items;
}

void
QueryBuilder::Select(int level, const LevelItem& item)
{
    m_selections[level] = item;
    for(int i = level + 1; i < LEVEL_COUNT; i++)
        m_selections[i] = std::nullopt;

    m_value_name = (level == LEVEL_VALUE_NAME) ? item.label : "";

    if(level < LEVEL_COUNT - 1)
        m_level = level + 1;

    m_scroll_to_end = true;
}

void
QueryBuilder::ClearFrom(int level)
{
    for(int i = level; i < LEVEL_COUNT; i++)
        m_selections[i] = std::nullopt;
    m_value_name.clear();
    m_level = level;
}

const char*
QueryBuilder::GetLevelLabel(int level) const
{
    static const char* labels[] = {
        "Level 1 - Workload",
        "Level 2 - Category",
        "Level 3 - Table",
        "Level 4 - Entry",
        "Level 5 - Value Name"
    };
    return (level >= 0 && level < LEVEL_COUNT) ? labels[level] : "Unknown";
}

std::string
QueryBuilder::GetQueryString() const
{
    std::string result;
    if(m_selections[LEVEL_CATEGORY])
        result = std::to_string(m_selections[LEVEL_CATEGORY]->id);
    if(m_selections[LEVEL_TABLE])
        result += "." + std::to_string(m_selections[LEVEL_TABLE]->id);
    if(m_selections[LEVEL_ENTRY])
        result += "." + std::to_string(m_selections[LEVEL_ENTRY]->id);
    if(!m_value_name.empty())
        result += ":" + m_value_name;
    return result;
}

std::optional<uint32_t>
QueryBuilder::GetSelectedWorkloadId() const
{
    if(m_selections[LEVEL_WORKLOAD])
        return m_selections[LEVEL_WORKLOAD]->id;
    return std::nullopt;
}

}  // namespace View
}  // namespace RocProfVis
