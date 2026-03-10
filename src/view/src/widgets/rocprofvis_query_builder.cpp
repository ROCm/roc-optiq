// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_query_builder.h"
#include "imgui.h"
#include "rocprofvis_settings_manager.h"
#include "model/compute/rocprofvis_compute_data_model.h"

namespace RocProfVis
{
namespace View
{

constexpr char* TITLE_LABEL = "Select Metric";

QueryBuilder::QueryBuilder()
{
    m_widget_name = GenUniqueName("QueryBuilder");
    m_selections.resize(LEVEL_COUNT, std::nullopt);
}

void
QueryBuilder::SetWorkload(const WorkloadInfo* workload)
{
    if(m_workload != workload)
    {
        m_workload = workload;
        ClearFrom(LEVEL_CATEGORY);
    }
}

void
QueryBuilder::Show(std::function<void(const std::string&)> on_confirm_callback,
                    std::function<void()> on_cancel_callback)
{
    m_on_confirm  = on_confirm_callback;
    m_on_cancel   = on_cancel_callback;
    m_should_open = true;
}

void
QueryBuilder::Render()
{
    if(m_should_open)
    {
        ImGui::OpenPopup(TITLE_LABEL);
        m_should_open = false;
    }

    if(!ImGui::IsPopupOpen(TITLE_LABEL)) return;
    PopUpStyle popup_style;
    popup_style.PushPopupStyles();
    popup_style.PushTitlebarColors();
    popup_style.CenterPopup();

    ImGui::SetNextWindowSizeConstraints(ImVec2(800, 200), ImVec2(800, 600));
    if(ImGui::BeginPopupModal(TITLE_LABEL, nullptr,
                              ImGuiWindowFlags_AlwaysAutoResize))
    {
        GetQueryString();

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

        ImGui::Text("Metric ");
        ImGui::SameLine();

        if(m_selections[LEVEL_CATEGORY])
            ImGui::Text("%s", m_selections[LEVEL_CATEGORY]->id_str.c_str());
        else
            ImGui::TextDisabled("Category");
        ImGui::SameLine();

        ImGui::Text(".");
        ImGui::SameLine();

        if(m_selections[LEVEL_TABLE])
            ImGui::Text("%s", m_selections[LEVEL_TABLE]->id_str.c_str());
        else
            ImGui::TextDisabled("Table");
        ImGui::SameLine();

        ImGui::Text(".");
        ImGui::SameLine();

        if(m_selections[LEVEL_ENTRY])
        {
            ImGui::Text("%s ", m_selections[LEVEL_ENTRY]->id_str.c_str());
            ImGui::SameLine();
            ImGui::Text("%s", m_selections[LEVEL_ENTRY]->label.c_str());
        }
        else
            ImGui::TextDisabled("Entry");
        ImGui::SameLine();

        ImGui::Text(": ");
        ImGui::SameLine();

        if(!m_value_name.empty())
            ImGui::Text("%s", m_value_name.c_str());
        else
            ImGui::TextDisabled("Aggregation type");

        ImGui::PopStyleVar();
        ImGui::Separator();
        RenderTags();
        RenderList();
        float button_width =
            (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        bool query_complete = !m_value_name.empty();
        if(!query_complete) ImGui::BeginDisabled();
        if(ImGui::Button("Add", ImVec2(button_width, 0)))
        {
            if(m_on_confirm)
                m_on_confirm(GetQueryString());
            ImGui::CloseCurrentPopup();
        }
        if(!query_complete) ImGui::EndDisabled();
        ImGui::SameLine();
        if(ImGui::Button("Cancel", ImVec2(button_width, 0)))
        {
            if(m_on_cancel)
                m_on_cancel();
            ClearFrom(LEVEL_CATEGORY);
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void
QueryBuilder::RenderTags()
{
    int clear = -1;

    ImGui::BeginChild("##tags", ImVec2(-1, ImGui::GetFrameHeightWithSpacing()),
                      ImGuiChildFlags_FrameStyle, ImGuiWindowFlags_HorizontalScrollbar);

    for(int i = 0; i < LEVEL_COUNT; i++)
    {
        if(!m_selections[i]) continue;
        if(clear >= 0 || i > 0) ImGui::SameLine();

        ImGui::PushID(i);
        std::string tag = m_selections[i]->label + "  X";
        if(ImGui::SmallButton(tag.c_str())) clear = i;
        ImGui::PopID();
    }

    if(m_scroll_to_end)
    {
        ImGui::SetScrollHereX(1.0f);
        m_scroll_to_end = false;
    }

    ImGui::EndChild();

    if(clear >= 0) ClearFrom(clear);
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
        if(ImGui::Selectable(items[i].label.c_str(), selected)) Select(m_level, items[i]);
        ImGui::PopID();
    }
    if(items.empty())
        ImGui::TextDisabled(!m_workload ? "No workload selected"
                                        : "No items at this level");
    ImGui::EndChild();
}

std::vector<QueryBuilder::LevelItem>
QueryBuilder::GetItems() const
{
    std::vector<LevelItem> items;
    if(!m_workload) return items;

    const WorkloadInfo& workload = *m_workload;

    if(m_level == LEVEL_CATEGORY)
    {
        for(const auto* cat : workload.available_metrics.ordered_categories)
            items.push_back({ cat->id, std::to_string(cat->id), cat->name });
        return items;
    }

    if(!m_selections[LEVEL_CATEGORY]) return items;
    uint32_t cat_id = m_selections[LEVEL_CATEGORY]->id;
    auto     cat_it = workload.available_metrics.tree.find(cat_id);
    if(cat_it == workload.available_metrics.tree.end()) return items;
    const AvailableMetrics::Category& category = cat_it->second;

    if(m_level == LEVEL_TABLE)
    {
        for(const auto* tbl : category.ordered_tables)
            items.push_back({ tbl->id, std::to_string(tbl->id), tbl->name });
        return items;
    }

    if(!m_selections[LEVEL_TABLE]) return items;
    uint32_t tbl_id = m_selections[LEVEL_TABLE]->id;
    auto     tbl_it = category.tables.find(tbl_id);
    if(tbl_it == category.tables.end()) return items;
    const AvailableMetrics::Table& table = tbl_it->second;

    if(m_level == LEVEL_ENTRY)
    {
        for(const auto* entry : table.ordered_entries)
            items.push_back({ entry->id, std::to_string(entry->id), entry->name });
        return items;
    }

    if(m_level == LEVEL_VALUE_NAME)
    {
        for(uint32_t i = 0; i < table.value_names.size(); i++)
            items.push_back({ i, std::to_string(i), table.value_names[i] });
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

    if(level < LEVEL_COUNT - 1) m_level = level + 1;

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

std::string
QueryBuilder::GetMetricName() const
{
    if(m_selections[LEVEL_ENTRY]) return m_selections[LEVEL_ENTRY]->label;
    return "";
}

std::string
QueryBuilder::GetValueName() const
{
    return m_value_name;
}

const AvailableMetrics::Entry*
QueryBuilder::GetSelectedMetricInfo() const
{
    if(m_workload && m_selections[LEVEL_ENTRY] && m_selections[LEVEL_TABLE] &&
       m_selections[LEVEL_CATEGORY])
    {
        return ComputeDataModel::GetMetricInfo(
            *m_workload, m_selections[LEVEL_CATEGORY]->id, m_selections[LEVEL_TABLE]->id,
            m_selections[LEVEL_ENTRY]->id);
    }
    return nullptr;
}

}  // namespace View
}  // namespace RocProfVis
