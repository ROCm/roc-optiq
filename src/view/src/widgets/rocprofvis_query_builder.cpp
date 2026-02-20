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
QueryBuilder::SetSelectionChangedCallback(const SelectionCallback& callback)
{
    m_on_changed = callback;
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
        ImGui::Text("%s", GetLevelLabel(m_current_level));
        ImGui::Separator();

        RenderTags();
        RenderList();

        if(ImGui::Button("Close", ImVec2(-1, 0)))
            ImGui::CloseCurrentPopup();

        ImGui::EndPopup();
    }
}

void
QueryBuilder::RenderTags()
{
    SettingsManager& settings    = SettingsManager::GetInstance();
    int              clear_level = -1;
    float            row_h       = ImGui::GetFrameHeightWithSpacing();

    ImGui::BeginChild("##tags", ImVec2(-1, row_h),
                      ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_HorizontalScrollbar);

    for(int i = 0; i < LEVEL_COUNT; i++)
    {
        if(!m_selections[i].has_value())
            continue;

        if(i > 0)
            ImGui::SameLine();

        std::string tag =
            "L" + std::to_string(i + 1) + ": " + m_selections[i].value().name + "  X";

        ImGui::PushID(i);
        ImGui::PushStyleColor(ImGuiCol_Button, settings.GetColor(Colors::kAccentRed));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,
                              settings.GetColor(Colors::kAccentRedHover));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,
                              settings.GetColor(Colors::kAccentRedActive));

        if(ImGui::SmallButton(tag.c_str()))
            clear_level = i;

        ImGui::PopStyleColor(3);
        ImGui::PopID();
    }

    if(m_scroll_tags_to_end)
    {
        ImGui::SetScrollHereX(1.0f);
        m_scroll_tags_to_end = false;
    }

    ImGui::EndChild();

    if(clear_level >= 0)
        ClearFromLevel(clear_level);
}

void
QueryBuilder::RenderList()
{
    std::vector<ListEntry> items = GetCurrentLevelItems();

    ImGui::BeginChild("##list", ImVec2(-1, ImGui::GetTextLineHeightWithSpacing() * 12),
                      ImGuiChildFlags_Borders);

    for(size_t i = 0; i < items.size(); i++)
    {
        ImGui::PushID(static_cast<int>(i));

        bool selected = m_selections[m_current_level].has_value() &&
                        m_selections[m_current_level].value().id == items[i].id;

        if(ImGui::Selectable(items[i].label.c_str(), selected))
            SelectItem(items[i]);

        if(!items[i].tooltip.empty() && ImGui::BeginItemTooltip())
        {
            ImGui::PushTextWrapPos(400.0f);
            ImGui::TextUnformatted(items[i].tooltip.c_str());
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }

        ImGui::PopID();
    }

    if(items.empty())
    {
        if(!m_workloads || m_workloads->empty())
            ImGui::TextDisabled("No workloads loaded");
        else
            ImGui::TextDisabled("No items at this level");
    }

    ImGui::EndChild();
}

std::vector<QueryBuilder::ListEntry>
QueryBuilder::GetCurrentLevelItems() const
{
    std::vector<ListEntry> items;
    if(!m_workloads)
        return items;

    if(m_current_level == LEVEL_WORKLOAD)
    {
        for(const auto& [id, wl] : *m_workloads)
            items.push_back({ id, wl.name, "" });
        return items;
    }

    if(!m_selections[LEVEL_WORKLOAD].has_value())
        return items;
    uint32_t wl_id = m_selections[LEVEL_WORKLOAD].value().id;
    if(m_workloads->count(wl_id) == 0)
        return items;
    const WorkloadInfo& workload = m_workloads->at(wl_id);

    if(m_current_level == LEVEL_CATEGORY)
    {
        for(const auto& [id, cat] : workload.available_metrics.tree)
            items.push_back(
                { id, "[" + std::to_string(id) + "] " + cat.name, "" });
        return items;
    }

    if(!m_selections[LEVEL_CATEGORY].has_value())
        return items;
    uint32_t cat_id = m_selections[LEVEL_CATEGORY].value().id;
    if(workload.available_metrics.tree.count(cat_id) == 0)
        return items;
    const AvailableMetrics::Category& category =
        workload.available_metrics.tree.at(cat_id);

    if(m_current_level == LEVEL_TABLE)
    {
        for(const auto& [id, tbl] : category.tables)
            items.push_back({ id,
                              "[" + std::to_string(cat_id) + "." +
                                  std::to_string(id) + "] " + tbl.name,
                              "" });
        return items;
    }

    if(!m_selections[LEVEL_TABLE].has_value())
        return items;
    uint32_t tbl_id = m_selections[LEVEL_TABLE].value().id;
    if(category.tables.count(tbl_id) == 0)
        return items;
    const AvailableMetrics::Table& table = category.tables.at(tbl_id);

    if(m_current_level == LEVEL_ENTRY)
    {
        for(const auto& [id, entry] : table.entries)
            items.push_back({ id,
                              "[" + std::to_string(cat_id) + "." +
                                  std::to_string(tbl_id) + "." +
                                  std::to_string(id) + "] " + entry.name,
                              entry.description });
        return items;
    }

    if(m_current_level == LEVEL_VALUE_NAME)
    {
        uint32_t idx = 0;
        for(const std::string& vn : table.value_names)
        {
            items.push_back({ idx, vn, "" });
            idx++;
        }
    }

    return items;
}

void
QueryBuilder::SelectItem(const ListEntry& item)
{
    m_selections[m_current_level] = Selection{ item.id, item.label };

    if(m_current_level == LEVEL_VALUE_NAME)
        m_value_name = item.label;

    for(int i = m_current_level + 1; i < LEVEL_COUNT; i++)
        m_selections[i] = std::nullopt;
    if(m_current_level < LEVEL_VALUE_NAME)
        m_value_name.clear();

    if(m_current_level < LEVEL_COUNT - 1)
        m_current_level++;

    m_scroll_tags_to_end = true;

    if(m_on_changed)
        m_on_changed(GetQueryString());
}

void
QueryBuilder::ClearFromLevel(int level)
{
    for(int i = level; i < LEVEL_COUNT; i++)
        m_selections[i] = std::nullopt;
    m_value_name.clear();
    m_current_level = level;

    if(m_on_changed)
        m_on_changed(GetQueryString());
}

const char*
QueryBuilder::GetLevelLabel(int level) const
{
    switch(level)
    {
    case LEVEL_WORKLOAD: return "Level 1 - Workload";
    case LEVEL_CATEGORY: return "Level 2 - Category";
    case LEVEL_TABLE: return "Level 3 - Table";
    case LEVEL_ENTRY: return "Level 4 - Entry";
    case LEVEL_VALUE_NAME: return "Level 5 - Value Name";
    default: return "Unknown";
    }
}

std::string
QueryBuilder::GetQueryString() const
{
    std::string result;
    if(m_selections[LEVEL_CATEGORY].has_value())
        result = std::to_string(m_selections[LEVEL_CATEGORY].value().id);
    if(m_selections[LEVEL_TABLE].has_value())
        result += "." + std::to_string(m_selections[LEVEL_TABLE].value().id);
    if(m_selections[LEVEL_ENTRY].has_value())
        result += "." + std::to_string(m_selections[LEVEL_ENTRY].value().id);
    if(!m_value_name.empty())
        result += ":" + m_value_name;
    return result;
}

bool
QueryBuilder::HasCompleteSelection() const
{
    return m_selections[LEVEL_WORKLOAD].has_value() &&
           m_selections[LEVEL_CATEGORY].has_value() &&
           m_selections[LEVEL_TABLE].has_value();
}

std::optional<uint32_t>
QueryBuilder::GetSelectedWorkloadId() const
{
    if(m_selections[LEVEL_WORKLOAD].has_value())
        return m_selections[LEVEL_WORKLOAD].value().id;
    return std::nullopt;
}

std::optional<uint32_t>
QueryBuilder::GetSelectedCategoryId() const
{
    if(m_selections[LEVEL_CATEGORY].has_value())
        return m_selections[LEVEL_CATEGORY].value().id;
    return std::nullopt;
}

std::optional<uint32_t>
QueryBuilder::GetSelectedTableId() const
{
    if(m_selections[LEVEL_TABLE].has_value())
        return m_selections[LEVEL_TABLE].value().id;
    return std::nullopt;
}

std::optional<uint32_t>
QueryBuilder::GetSelectedEntryId() const
{
    if(m_selections[LEVEL_ENTRY].has_value())
        return m_selections[LEVEL_ENTRY].value().id;
    return std::nullopt;
}

std::optional<std::string>
QueryBuilder::GetSelectedValueName() const
{
    if(!m_value_name.empty())
        return m_value_name;
    return std::nullopt;
}

}  // namespace View
}  // namespace RocProfVis
