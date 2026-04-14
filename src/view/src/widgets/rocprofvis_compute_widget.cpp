// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_widget.h"
#include "compute/rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_notification_manager.h"
#include "icons/rocprovfis_icon_defines.h"

namespace RocProfVis
{
namespace View
{
MetricTableBase::MetricTableBase()
: m_max_rows_in_table(0)
, m_set_to_kernel_table_callback(nullptr)
, m_table_title("")
, m_lust_column_index(0)
{
    m_table_flags =
        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg ;
}

void
MetricTableBase::SetPinMetricCallback(std::function<void(MetricId)> callback)
{
    m_pin_metric_clicked = callback;
}

void
MetricTableBase::SetToKernelTableCallback(
    std::function<void(MetricId metric_id, const std::string&)> callback)
{
    m_set_to_kernel_table_callback = callback;
}

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

        for (const auto& column : m_columns)
        {
            if (column.first == 0)
            {
                const float font_size = ImGui::GetFontSize();
                ImGui::TableSetupColumn(column.second.c_str(),
                                        ImGuiTableColumnFlags_WidthFixed, font_size);
            }
            else
            {
                ImGui::TableSetupColumn(column.second.c_str());
            }
        }

        ImGui::TableHeadersRow();

        uint32_t row_idx = 0;
        for(auto& row : m_rows)
        {
            ImGui::PushID(row_idx++);
            ImGui::TableNextRow();

            for(auto column_index = 0; column_index < m_columns.size() - 1;
                column_index++)
            {
                auto menu_func = [&](const char* value_to_copy) {
                    this->ContextMenu(value_to_copy, column_index, row);
                };
                RenderRowValues(column_index, row, menu_func);
            }

            RenderUnitValue(row);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::EndChild();
}

void
MetricTableBase::ChangePinState(const MetricId& metric_id)
{
    m_rows[metric_id].pinned = !m_rows[metric_id].pinned;
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

void
MetricTableBase::ContextMenu(const char* value_to_copy, uint32_t column_index,
                             std::pair<const MetricId, Row>& row)
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
                m_set_to_kernel_table_callback(row.first, m_columns.at(column_index));
            }
        }
        ImGui::EndPopup();
    }
}

void
MetricTableBase::RenderRowValues(uint32_t                        column_index,
                                 std::pair<const MetricId, Row>& row,
    std::function<void(const char* value_to_copy)> menu_func)
{
    if (column_index == 0)
    {
        RenderPinCheckBox(row);
    }
    else
    {
        ImGui::TableNextColumn();
        if(auto it = row.second.values.find(column_index); it == row.second.values.end())
        {
            ImGui::TextDisabled("N/A");
        }
        else
        {
            CopyableTextUnformatted(row.second.values.at(column_index).value.c_str(),
                                    "##value" + std::to_string(column_index),
                                    COPY_DATA_NOTIFICATION, false, true, menu_func);
            RenderTooltip(row.second.values.at(column_index));
        }
    }
}

void
MetricTableBase::RenderPinCheckBox(std::pair<const MetricId, Row>& row)
{
    ImGui::TableNextColumn();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
    if(ImGui::Checkbox("", &m_rows[row.first].pinned))
    {
        if(m_pin_metric_clicked)
        {
            m_pin_metric_clicked(
                { row.first.category_id, row.first.table_id, row.first.entry_id });
        }
    }
    ImGui::PopStyleVar();
}

void
MetricTableBase::RenderUnitValue(std::pair<const MetricId, Row>& row)
{
    auto menu_func = [&](const char* value_to_copy) {
        this->ContextMenu(value_to_copy, LAST_INDEX, row);
    };
    ImGui::TableNextColumn();
    if(row.second.values.at(LAST_INDEX).value.empty())
    {
        ImGui::TextDisabled("N/A");
    }
    else
    {
        CopyableTextUnformatted(row.second.values.at(LAST_INDEX).value.c_str(), "##metric_unit",
                                COPY_DATA_NOTIFICATION, false, true, menu_func);
        RenderTooltip(row.second.values.at(LAST_INDEX));
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
    m_columns[0]          = "Pin";
    m_columns[1]          = "Metric ID";
    m_columns[2]          = "Metric";
    m_lust_column_index   = 3;
    m_columns[LAST_INDEX] = "Unit";
}

bool
MetricTableBase::IsMetricPined(MetricId metric_id)
{
    return m_rows[metric_id].pinned;
}

//---------------------------------------------------------

void
MetricTable::ContextMenu(const char* value_to_copy, uint32_t column_index,
                                 std::pair<const MetricId, Row>& row)
{
    if(ImGui::BeginPopupContextItem())
    {
        if(ImGui::MenuItem("Copy"))
        {
            ImGui::SetClipboardText(value_to_copy);
            NotificationManager::GetInstance().Show(COPY_DATA_NOTIFICATION.data(),
                                                    NotificationLevel::Info);
        }
        if(m_pin_metric_clicked)
        {
            if (!row.second.pinned)
            {
                if(ImGui::MenuItem("Pin metric"))
                {
                    row.second.pinned = !row.second.pinned;
                    m_pin_metric_clicked({ row.first.category_id, row.first.table_id,
                                           row.first.entry_id });
                }
            }
            else
            {
                if(ImGui::MenuItem("Unpin metric"))
                {
                    row.second.pinned = !row.second.pinned;
                    m_pin_metric_clicked({ row.first.category_id, row.first.table_id,
                                           row.first.entry_id });
                }
            }

        }
        if(column_index != 0 && column_index != 1)
        {
            if(ImGui::MenuItem("Set Metric to kernel table"))
            {
                if(m_set_to_kernel_table_callback)
                {
                    m_set_to_kernel_table_callback(
                        { row.first.category_id, row.first.table_id, row.first.entry_id },
                        m_columns[column_index]);
                }
            }
        }

        ImGui::EndPopup();
    }
}

void
MetricTable::Populate(const AvailableMetrics::Table& table,
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
        row.values[1].value = metric_id.ToString();
        row.values[2].value   = entry->name;
        row.values[2].tooltip = entry->description;
        row.values[LAST_INDEX].value = entry->unit.empty() ? "N/A" : entry->unit;

        auto metric_value = get_value(entry->id);

        if(table.value_names.empty())
        {
            if(metric_value && metric_value->entry && !metric_value->values.empty())
            {
                snprintf(buf, sizeof(buf), "%.2f", metric_value->values.begin()->second);
                row.values[3].value = buf;
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
                    row.values[value_index + 3].value = buf;
                }
            }
        }
        m_rows[metric_id] = std::move(row);
    }
}

void
MetricTable::Clear()
{
    m_rows.clear();
    m_columns.clear();
}

bool
MetricTable::Empty() const
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

PinedMetricTable::PinedMetricTable(DataProvider&                     data_provider,
                                   std::shared_ptr<ComputeSelection> compute_selection,
                                   uint64_t                          client_id)
: MetricTableBase()
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(client_id)
{
    FillDefaultColumns();
    m_table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX;
    m_table_title = "Pinned Metrics";
    m_max_rows_in_table = 7;
}

void
PinedMetricTable::ContextMenu(const char* value_to_copy, uint32_t column_index,
                              std::pair<const MetricId, Row>& row)
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
            if(m_pin_metric_clicked)
            {
                m_pin_metric_clicked(
                    { row.first.category_id, row.first.table_id, row.first.entry_id });
            }
        }
        if(column_index != 0 && column_index != 1 && column_index != LAST_INDEX)
        // not equal MetricId, Metric Name and Metric Unit
        {
            if(ImGui::MenuItem("Set Metric to kernel table"))
            {
                if(m_set_to_kernel_table_callback)
                    m_set_to_kernel_table_callback(row.first, m_columns[column_index]);
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
        row.values[1].value = metric_id.ToString();

        row.values[2].value = entrie->second.name;
        row.values[2].tooltip = entrie->second.description;

        row.values[LAST_INDEX].value = entrie->second.unit;
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
PinedMetricTable::RemoveRow(MetricId metric_id)
{
    m_id_to_delete = metric_id;
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
    row.pinned = true;

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
        if(auto column_index = GetColumnIndex(value_name); column_index.has_value())
        {
            if(metric_value && metric_value->entry &&
               metric_value->values.count(value_name))
            {
                snprintf(buf, sizeof(buf), "%.2f", metric_value->values.at(value_name));
                row.values[column_index.value()].value = buf;
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
