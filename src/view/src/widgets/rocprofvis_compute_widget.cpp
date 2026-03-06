// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_widget.h"
#include "compute/rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_requests.h"

namespace RocProfVis
{
namespace View
{

void
MetricTableCache::Populate(const AvailableMetrics::Table& table,
                           const MetricValueLookup&       get_value)
{
    m_title    = table.name;
    m_table_id = "##" + table.name;
    m_column_names.clear();
    m_rows.clear();
    m_rows.reserve(table.entries.size());

    m_column_names.push_back("Metric ID");
    m_column_names.push_back("Metric");
    if(table.value_names.empty())
    {
        m_column_names.push_back("Value");
    }
    else
    {
        for(const auto& vn : table.value_names)
            m_column_names.push_back(vn);
    }
    m_column_names.push_back("Unit");

    char buf[64];
    for(const auto& entry_pair : table.entries)
    {
        uint32_t    eid   = entry_pair.first;
        const auto& entry = entry_pair.second;

        Row row;
        row.metric_id   = std::to_string(entry.category_id) + "." +
                          std::to_string(entry.table_id) + "." +
                          std::to_string(eid);
        row.name        = entry.name;
        row.description = entry.description;
        row.unit        = entry.unit.empty() ? "N/A" : entry.unit;

        auto mv = get_value(eid);

        if(table.value_names.empty())
        {
            if(mv && mv->entry && !mv->values.empty())
            {
                snprintf(buf, sizeof(buf), "%.2f", mv->values.begin()->second);
                row.values.push_back(buf);
            }
            else
            {
                row.values.emplace_back();
            }
        }
        else
        {
            for(const auto& vn : table.value_names)
            {
                if(mv && mv->entry && mv->values.count(vn))
                {
                    snprintf(buf, sizeof(buf), "%.2f", mv->values.at(vn));
                    row.values.push_back(buf);
                }
                else
                {
                    row.values.emplace_back();
                }
            }
        }

        m_rows.push_back(std::move(row));
    }
}

void
MetricTableCache::Render() const
{
    if(m_rows.empty())
        return;

    int num_columns = static_cast<int>(m_column_names.size());

    SectionTitle(m_title.c_str());
    if(!ImGui::BeginTable(m_table_id.c_str(), num_columns, ImGuiTableFlags_Borders))
        return;

    for(const auto& col : m_column_names)
        ImGui::TableSetupColumn(col.c_str());
    ImGui::TableHeadersRow();

    int row_idx = 0;
    for(const auto& row : m_rows)
    {
        ImGui::PushID(row_idx++);
        ImGui::TableNextRow();

        ImGui::TableNextColumn();
        CopyableTextUnformatted(row.metric_id.c_str(), "##mid",
                                COPY_DATA_NOTIFICATION, false, true);

        ImGui::TableNextColumn();
        CopyableTextUnformatted(row.name.c_str(), "##name",
                                COPY_DATA_NOTIFICATION, false, true);
        if(!row.description.empty() && ImGui::IsItemHovered())
        {
            constexpr float kTooltipMaxWidth = 400.0f;
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                                ImVec2(kTooltipMaxWidth, FLT_MAX));
            BeginTooltipStyled();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTooltipMaxWidth);
            ImGui::TextUnformatted(row.description.c_str());
            ImGui::PopTextWrapPos();
            EndTooltipStyled();
        }

        for(int vi = 0; vi < static_cast<int>(row.values.size()); vi++)
        {
            ImGui::TableNextColumn();
            if(!row.values[vi].empty())
            {
                CopyableTextUnformatted(row.values[vi].c_str(),
                                        std::string("##v") + std::to_string(vi),
                                        COPY_DATA_NOTIFICATION, false, true);
            }
            else
            {
                ImGui::TextDisabled("N/A");
            }
        }

        ImGui::TableNextColumn();
        if(row.unit != "N/A")
        {
            CopyableTextUnformatted(row.unit.c_str(), "##unit",
                                    COPY_DATA_NOTIFICATION, false, true);
        }
        else
        {
            ImGui::TextDisabled("N/A");
        }

        ImGui::PopID();
    }
    ImGui::EndTable();
}

void
MetricTableCache::Clear()
{
    m_rows.clear();
    m_column_names.clear();
}

bool
MetricTableCache::Empty() const
{
    return m_rows.empty();
}

MetricTableWidget::MetricTableWidget(DataProvider& data_provider,
                                     std::shared_ptr<ComputeSelection> compute_selection,
                                     uint32_t category_id, uint32_t table_id)
: RocWidget()
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_category_id(category_id)
, m_table_id(table_id)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    m_widget_name = GenUniqueName("MetricTableWidget");
}

void
MetricTableWidget::Render()
{
    m_table.Render();
}

void
MetricTableWidget::Clear()
{
    m_table.Clear();
}

void
MetricTableWidget::FetchMetrics()
{
    m_table.Clear();
    m_data_provider.ComputeModel().ClearMetricValues(m_client_id);

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID ||
       kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    m_data_provider.FetchMetrics(MetricsRequestParams(
        workload_id, {kernel_id}, {{m_category_id, m_table_id, std::nullopt}}, m_client_id));
}

void
MetricTableWidget::UpdateTable()
{
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID ||
       kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    auto& model = m_data_provider.ComputeModel();
    if(!model.GetWorkloads().count(workload_id))
        return;

    const auto& tree = model.GetWorkloads().at(workload_id).available_metrics.tree;
    if(!tree.count(m_category_id) || !tree.at(m_category_id).tables.count(m_table_id))
        return;

    m_table.Populate(tree.at(m_category_id).tables.at(m_table_id), [&](uint32_t eid) {
        return model.GetMetricValue(m_client_id, kernel_id, m_category_id, m_table_id, eid);
    });
}

}  // namespace View
}  // namespace RocProfVis
