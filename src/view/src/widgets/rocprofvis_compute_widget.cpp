// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_widget.h"
#include "compute/rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{

MetricTableBase::MetricTableBase(
    std::function<void(MetricId metric_id, const std::string&)>
        set_to_kernel_table_callback)
: m_set_to_kernel_table_callback(set_to_kernel_table_callback)
{}

void
MetricTableBase::Render()
{
    if(!m_table_title.empty())
        SectionTitle(m_table_title.c_str());

    if(m_rows.empty())
    {
        ImGui::TextDisabled("Pin the metric to see it here");
        return;
    }

    if(ImGui::BeginChild(("##" + m_table_title + "layout").c_str(),
                         ImVec2(0, GetTableHight()), false,
                         ImGuiWindowFlags_HorizontalScrollbar))
    {

        int num_columns = static_cast<int>(m_columns.size());
        if(!ImGui::BeginTable(("##" + m_table_title).c_str(), num_columns,
                              m_table_flags))
            return;

        for(const auto& column : m_columns)
            ImGui::TableSetupColumn(column.second.c_str());

        ImGui::TableHeadersRow();

        uint32_t row_idx = 0;
        for(auto& row : m_rows)
        {
            ImGui::PushID(row_idx++);
            ImGui::TableNextRow();

            for(auto index = 0; index < m_columns.size() - 1; index++)
            {
                auto menu_func = [&](const char* value_to_copy) {
                    this->ContextMenu(value_to_copy, index, row);
                };
                RenderRowValues(index, row, menu_func);
            }

            RenderUnitValue(row);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

float
MetricTableBase::GetTableHight() const
{
    const ImGuiStyle& style = SettingsManager::GetInstance().GetDefaultStyle();
    float             line_height =
        ImGui::GetTextLineHeightWithSpacing() + style.CellPadding.y * 2.0f;

    uint32_t max_rows = 0;
    if (m_max_rows_in_table == 0)
    {
        max_rows = m_rows.size();
    }
    else
    {
        max_rows =
            m_rows.size() < m_max_rows_in_table ? m_rows.size() : m_max_rows_in_table;
    }
        
    return style.ScrollbarSize + line_height + (max_rows * line_height);
}

void MetricTableBase::ContextMenu(const char* value_to_copy, uint32_t index, const std::pair<MetricId, Row>& row)
{
    if(ImGui::BeginPopupContextItem())
    {
        if(ImGui::MenuItem("Copy"))
        {
            ImGui::SetClipboardText(value_to_copy);
            NotificationManager::GetInstance().Show(
                    COPY_DATA_NOTIFICATION.data(), NotificationLevel::Info);
        }
        if (m_set_to_kernel_table_callback)
        {
            if(ImGui::MenuItem("Show in kernel table"))
            {
                m_set_to_kernel_table_callback(row.first, m_columns.at(index));
            }
        }
        ImGui::EndPopup();
    }
}

void
MetricTableBase::RenderRowValues(
    uint32_t index, const std::pair<MetricId, Row>& row,
    std::function<void(const char* value_to_copy)> menu_func)
{
    ImGui::TableNextColumn();
    if(auto it = row.second.find(index); it == row.second.end())
    {
        ImGui::TextDisabled("N/A");
    }
    else
    {
        CopyableTextUnformatted(row.second.at(index).value.c_str(),
                                "##value" + std::to_string(index), COPY_DATA_NOTIFICATION,
                                false, true, menu_func);
        RenderTooltip(row.second.at(index));
    }
}

void
MetricTableBase::RenderUnitValue(const std::pair<MetricId, Row>& row)
{
    auto menu_func = [&](const char* value_to_copy) {
        this->ContextMenu(value_to_copy, LAST_INDEX, row);
    };
    ImGui::TableNextColumn();
    if(row.second.at(LAST_INDEX).value.empty())
    {
        ImGui::TextDisabled("N/A");
    }
    else
    {
        CopyableTextUnformatted(row.second.at(LAST_INDEX).value.c_str(), "##metric_unit",
                                COPY_DATA_NOTIFICATION, false, true, menu_func);
        RenderTooltip(row.second.at(LAST_INDEX));
    }
}

void
MetricTableBase::RenderTooltip(const RowValue& row)
{
    auto tooltip_text = row.tooltip.empty() ? row.value : row.tooltip;

    if(ImGui::IsItemHovered())
    {
        ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                            ImVec2(TABLE_TOOLTIP_MAX_WIDTH, FLT_MAX));
        BeginTooltipStyled();
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + TABLE_TOOLTIP_MAX_WIDTH);
        ImGui::TextUnformatted(tooltip_text.c_str());
        ImGui::PopTextWrapPos();
        EndTooltipStyled();
    }
}

void
MetricTableBase::FillDefaultColumns()
{
    m_columns[0]          = "Metric ID";
    m_columns[1]          = "Metric";
    m_lust_column_index   = 2;
    m_columns[LAST_INDEX] = "Unit";
}

//---------------------------------------------------------
MetricTableCache::MetricTableCache(
    std::function<void(MetricId)> add_row_func,
    std::function<void(MetricId metric_id, const std::string& value_name)>
        set_to_kernel_table)
: m_add_row_to_custom(add_row_func)
, m_set_to_kernel_table_callback(set_to_kernel_table)
{}

void
MetricTableCache::Populate(const AvailableMetrics::Table& table,
                           const MetricValueLookup&       get_value)
{
    m_title    = table.name;
    m_table_id = "##" + table.name;
    m_column_names.clear();
    m_rows.clear();
    m_rows.reserve(table.ordered_entries.size());

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
    for(const auto* entry : table.ordered_entries)
    {
        uint32_t eid = entry->id;

        Row row;

        row.metric_id   = { entry->category_id, entry->table_id, eid};
        row.name        = entry->name;
        row.description = entry->description;
        row.unit        = entry->unit.empty() ? "N/A" : entry->unit;

        auto metric_value = get_value(eid);

        if(table.value_names.empty())
        {
            if(metric_value && metric_value->entry && !metric_value->values.empty())
            {
                snprintf(buf, sizeof(buf), "%.2f", metric_value->values.begin()->second);
                row.values.push_back(buf);
            }
            else
            {
                row.values.emplace_back();
            }
        }
        else
        {
            for(const auto& value_name : table.value_names)
            {
                if(metric_value && metric_value->entry &&
                   metric_value->values.count(value_name))
                {
                    snprintf(buf, sizeof(buf), "%.2f",
                             metric_value->values.at(value_name));
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
MetricTableCache::Render()
{
    int num_columns = static_cast<int>(m_column_names.size());

    SectionTitle(m_title.c_str());
    if(!m_column_names.empty() &&
       ImGui::BeginTable(m_table_id.c_str(), num_columns, ImGuiTableFlags_Borders))
    {
        for(const auto& col : m_column_names)
            ImGui::TableSetupColumn(col.c_str());
        ImGui::TableHeadersRow();

        int row_idx = 0;
        for(const auto& row : m_rows)
        {
            auto menu_func = [&](const char* value_to_copy) {
                if(ImGui::BeginPopupContextItem())
                {
                    if(ImGui::MenuItem("Copy"))
                    {
                        ImGui::SetClipboardText(value_to_copy);
                        NotificationManager::GetInstance().Show(
                                COPY_DATA_NOTIFICATION.data(), NotificationLevel::Info);
                    }
                    if (m_add_row_to_custom)
                    {
                        if(ImGui::MenuItem("Pin metric"))
                        {
                            m_add_row_to_custom(
                                {
                                  row.metric_id.category_id,
                                  row.metric_id.table_id,
                                  row.metric_id.entry_id
                                });
                        }
                    }

                    ImGui::EndPopup();
                }
            };
            ImGui::PushID(row_idx++);
            ImGui::TableNextRow();

        ImGui::TableNextColumn();

        //TODO: Ugly lambdas unify it in the future
        CopyableTextUnformatted(row.metric_id.ToString().c_str(), "##mid",
                                COPY_DATA_NOTIFICATION, false, true,
                                [&](const char* value_to_copy) {
                                    this->ContextMenu(value_to_copy, row, 0);
                                });

        ImGui::TableNextColumn();
        CopyableTextUnformatted(row.name.c_str(), "##name", COPY_DATA_NOTIFICATION, false,
                                true, [&](const char* value_to_copy) {
                                    this->ContextMenu(value_to_copy, row, 1);
                                });
        if(!row.description.empty() && ImGui::IsItemHovered())
        {
            ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                                ImVec2(TABLE_TOOLTIP_MAX_WIDTH, FLT_MAX));
            BeginTooltipStyled();
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + TABLE_TOOLTIP_MAX_WIDTH);
            ImGui::TextUnformatted(row.description.c_str());
            ImGui::PopTextWrapPos();
            EndTooltipStyled();
        }

        for(int value_index = 0; value_index < static_cast<int>(row.values.size());
            value_index++)
        {
            auto menu_func = [&](const char* value_to_copy) {
                this->ContextMenu(value_to_copy, row, value_index+2);
            };
            ImGui::TableNextColumn();
            if(!row.values[value_index].empty())
            {
                CopyableTextUnformatted(row.values[value_index].c_str(),
                                        std::string("##v") + std::to_string(value_index),
                                        COPY_DATA_NOTIFICATION, false, true, menu_func);
            }
            else
            {
                ImGui::TextDisabled("N/A");
            }
        }

        ImGui::TableNextColumn();
        if(row.unit != "N/A")
        {
            CopyableTextUnformatted(row.unit.c_str(), "##unit", COPY_DATA_NOTIFICATION,
                                    false, true, [&](const char* value_to_copy) {
                                        this->ContextMenu(value_to_copy, row, 0);
                                    });
        }
        else
        {
            ImGui::TextDisabled("N/A");
        }

        ImGui::PopID();
    }
    ImGui::EndTable();
}
}

void
MetricTableCache::ContextMenu(const char* value_to_copy, const Row& row, uint32_t index)
{
    if(ImGui::BeginPopupContextItem())
    {
        if(ImGui::MenuItem("Copy"))
        {
            ImGui::SetClipboardText(value_to_copy);
            NotificationManager::GetInstance().Show(COPY_DATA_NOTIFICATION.data(),
                                                    NotificationLevel::Info);
        }
        if(m_add_row_to_custom)
        {
            if(ImGui::MenuItem("Pin metric"))
            {
                m_add_row_to_custom({ row.metric_id.category_id, row.metric_id.table_id,
                                      row.metric_id.entry_id });
            }
        }
        if(index != 0 && index != 1)
        {
            if(ImGui::MenuItem("Set Metric to kernel table"))
            {
                if(m_set_to_kernel_table_callback)
                {
                    m_set_to_kernel_table_callback({ row.metric_id.category_id,
                                                     row.metric_id.table_id,
                                                     row.metric_id.entry_id },
                                                   m_column_names[index]);
                }
            }
        }

        ImGui::EndPopup();
    }
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

//---------------------------------------------------------

newMetricTableCache::newMetricTableCache(
    std::function<void(MetricId)> add_row_func,
    std::function<void(MetricId metric_id, const std::string& value_name)>
        set_to_kernel_table)
: MetricTableBase(set_to_kernel_table)
, m_pin_metric_func(add_row_func)
{
}

void
newMetricTableCache::ContextMenu(const char* value_to_copy, uint32_t index,
                                 const std::pair<MetricId, Row>& row)
{
    if(ImGui::BeginPopupContextItem())
    {
        if(ImGui::MenuItem("Copy"))
        {
            ImGui::SetClipboardText(value_to_copy);
            NotificationManager::GetInstance().Show(COPY_DATA_NOTIFICATION.data(),
                                                    NotificationLevel::Info);
        }
        if(m_pin_metric_func)
        {
            if(ImGui::MenuItem("Pin metric"))
            {
                m_pin_metric_func(
                    { row.first.category_id, row.first.table_id, row.first.entry_id });
            }
        }
        if(index != 0 && index != 1)
        {
            if(ImGui::MenuItem("Set Metric to kernel table"))
            {
                if(m_set_to_kernel_table_callback)
                {
                    m_set_to_kernel_table_callback(
                        { row.first.category_id, row.first.table_id, row.first.entry_id },
                        m_columns[index]);
                }
            }
        }

        ImGui::EndPopup();
    }
}

void
newMetricTableCache::Populate(const AvailableMetrics::Table& table,
                           const MetricValueLookup&       get_value)
{
    FillDefaultColumns();
    m_table_title = table.name;

    if (table.value_names.empty())
    {
        m_columns[m_lust_column_index++] = "Value";
    }
    else
    {
        for(const auto& value_name : table.value_names)
            m_columns[m_lust_column_index++] = value_name;
    }

    char buf[64];
    for(const auto* entry : table.ordered_entries)
    {
        Row row;
        MetricId metric_id{ entry->category_id, entry->table_id, entry->id };
        row[0].value = metric_id.ToString();
        row[1].value = entry->name;
        row[1].tooltip = entry->description;
        row[LAST_INDEX].value = entry->unit.empty() ? "N/A" : entry->unit;

        auto metric_value = get_value(entry->id);

        if(table.value_names.empty())
        {
            if(metric_value && metric_value->entry && !metric_value->values.empty())
            {
                snprintf(buf, sizeof(buf), "%.2f", metric_value->values.begin()->second);
                row[2].value = buf;
            }
        }
        else
        {
            for(size_t value_index = 0; value_index < table.value_names.size();
                value_index++)
            {
                const auto& value_name = table.value_names[value_index];
                if(metric_value && metric_value->entry &&
                   metric_value->values.count(value_name))
                {
                    snprintf(buf, sizeof(buf), "%.2f",
                             metric_value->values.at(value_name));
                    row[value_index + 2].value = buf;
                }
            }
        }
        m_rows[metric_id] = std::move(row);
    }
}


void
newMetricTableCache::Clear()
{
    m_rows.clear();
    m_columns.clear();
}

bool
newMetricTableCache::Empty() const
{
    return m_rows.empty();
}

//---------------------------------------------------------

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
    m_data_provider.ComputeModel().ClearKernelMetricValues(m_client_id);

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
        return model.GetKernelMetricValue(m_client_id, kernel_id, m_category_id, m_table_id, eid);
    });
}

//---------------------------------------------------------

WorkloadMetricTableWidget::WorkloadMetricTableWidget(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection,
    uint32_t category_id, uint32_t table_id)
: MetricTableWidget(data_provider, compute_selection, category_id, table_id)
{}

void
WorkloadMetricTableWidget::FetchMetrics()
{
    m_table.Clear();
    m_data_provider.ComputeModel().ClearWorkloadMetricValues(m_client_id);

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    m_data_provider.FetchMetrics(MetricsRequestParams(
        workload_id, {}, { { m_category_id, m_table_id, std::nullopt } }, m_client_id));
}

void
WorkloadMetricTableWidget::UpdateTable()
{
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    auto& model = m_data_provider.ComputeModel();
    if(!model.GetWorkloads().count(workload_id)) return;

    const auto& tree = model.GetWorkloads().at(workload_id).available_metrics.tree;
    if(!tree.count(m_category_id) || !tree.at(m_category_id).tables.count(m_table_id))
        return;

    m_table.Populate(tree.at(m_category_id).tables.at(m_table_id), [&](uint32_t eid) {
        return model.GetWorkloadMetricValue(m_client_id, workload_id, m_category_id,
                                            m_table_id, eid);
    });
}

//---------------------------------------------------------

PinedMetricTable::PinedMetricTable(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection,
    uint64_t client_id,
    std::function<void(MetricId metric_id, const std::string&)> set_query_callback)
: MetricTableBase(set_query_callback)
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(client_id)
{
    FillDefaultColumns();
    m_table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                    ImGuiTableFlags_Resizable | ImGuiTableFlags_ScrollX;
    m_table_title = "Pinned Metrics";
    m_max_rows_in_table = 7;
}


void
PinedMetricTable::ContextMenu(const char* value_to_copy, uint32_t index,
                              const std::pair<MetricId, Row>& row)
{
    if(ImGui::BeginPopupContextItem())
    {
        if(ImGui::MenuItem("Copy"))
        {
            ImGui::SetClipboardText(value_to_copy);
            NotificationManager::GetInstance().Show(COPY_DATA_NOTIFICATION.data(),
                                                    NotificationLevel::Info);
        }
        if(ImGui::MenuItem("Unpin metric"))
        {
            m_id_to_delete = row.first;
        }
        if(index != 0 && index != 1 && index != LAST_INDEX)
        // not equal MetricId, Metric Name and Metric Unit
        {
            if(ImGui::MenuItem("Set Metric to kernel table"))
            {
                if(m_set_to_kernel_table_callback)
                    m_set_to_kernel_table_callback(row.first, m_columns[index]);
            }
        }

        ImGui::EndPopup();
    }
}

void
PinedMetricTable::FillMandatoryColumns(const MetricId&                metric_id,
                                       const AvailableMetrics::Table& table,
                         Row& row,
                         std::shared_ptr<MetricValue>     metric_value)
{
    if(auto entrie = table.entries.find(metric_id.entry_id); entrie != table.entries.end())
    {
        row[0].value                                    = metric_id.ToString();

        row[1].value                                    = entrie->second.name;
        row[1].tooltip                                  = entrie->second.description;

        row[LAST_INDEX].value = entrie->second.unit;
    }
}

void
PinedMetricTable::RefillTable()
{
    m_columns.clear();
    m_lust_column_index = 0;
    FillDefaultColumns();
    for(const auto& [metric_id, row] : m_rows)
    {
        AddRow(metric_id);
    }
}

void
PinedMetricTable::AddRow(MetricId metric_id)
{
    UpdateColumns(metric_id);
    FillTableRow(metric_id);
}

void
PinedMetricTable::UpdateColumns(MetricId metric_id)
{
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    if(m_columns.empty())
        FillDefaultColumns();

    for(const auto& name : GetTable(metric_id, workload_id).value_names)
    {
        if(GetColumnIndex(name) == std::nullopt)
            m_columns[m_lust_column_index++] = name;
    }
}

const AvailableMetrics::Table&
PinedMetricTable::GetTable(const MetricId& metric_id, uint32_t workload_id)
{
    const auto& tree = m_data_provider.ComputeModel()
                           .GetWorkloads()
                           .at(workload_id)
                           .available_metrics.tree;
    return tree.at(metric_id.category_id).tables.at(metric_id.table_id);
}

std::optional<uint32_t>
PinedMetricTable::GetColumnIndex(const std::string& column_name)
{
    for(auto column : m_columns)
    {
        if(column.second == column_name)
            return column.first;
    }
    return std::nullopt;
}

void
PinedMetricTable::FillTableRow(const MetricId& metric_id)
{
    Row row;

    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID ||
       kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }
    auto& model = m_data_provider.ComputeModel();
    auto  metric_value =
        model.GetKernelMetricValue(m_client_id, kernel_id, metric_id.category_id,
                             metric_id.table_id, metric_id.entry_id);

    const auto& table = GetTable(metric_id, workload_id);
    FillMandatoryColumns(metric_id, table, row, metric_value);

    char buf[64];
    for(const auto& value_name : table.value_names)
    {
        if(auto index = GetColumnIndex(value_name); index.has_value())
        {
            if(metric_value && metric_value->entry &&
               metric_value->values.count(value_name))
            {
                snprintf(buf, sizeof(buf), "%.2f", metric_value->values.at(value_name));
                row[index.value()].value = buf;
            }
        }
    }
    m_rows[metric_id] = std::move(row);
}

void
PinedMetricTable::Update()
{
    if(m_id_to_delete.has_value())
    {
        m_rows.erase(m_id_to_delete.value());

        m_columns.clear();
        m_lust_column_index = 0;
        for(const auto& [metric_id, row] : m_rows)
        {
            UpdateColumns(metric_id);
        }
        m_id_to_delete = std::nullopt;
    }
}

}  // namespace View
}  // namespace RocProfVis
