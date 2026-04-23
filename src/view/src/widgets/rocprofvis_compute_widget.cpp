// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_widget.h"
#include "compute/rocprofvis_compute_selection.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_events.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_gui_helpers.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_notification_manager.h"

namespace RocProfVis
{
namespace View
{
MetricTableBase::MetricTableBase(std::string event_source_id)
: m_max_rows_in_table(0)
, m_event_source_id(std::move(event_source_id))
, m_table_title("")
, m_last_column_index(0)
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
MetricTableBase::Render()
{
    if(!m_table_title.empty())
        SectionTitle(m_table_title.c_str());

    if(m_rows.empty())
    {
        RenderEmptyTable();
        return;
    }

    int num_columns = static_cast<int>(m_columns.size());
    if(!CanBePinned())
    {
        if(num_columns > 0)
            num_columns--;
        if(num_columns == 0)
        {
            RenderEmptyTable();
            return;
        }
    }

    const std::string child_id = "##" + m_table_title + "_layout_";
    const std::string table_id = child_id + "_table";

    if(ImGui::BeginChild(RocWidget::GenUniqueName(child_id).c_str(),
                         ImVec2(0, GetTableHight()), false,
                         ImGuiWindowFlags_HorizontalScrollbar))
    {
        if(ImGui::BeginTable(RocWidget::GenUniqueName(table_id).c_str(), num_columns,
                             m_table_flags))
        {
            for (const auto& column : m_columns)
            {
                if (column.first == 0)
                {
                    if (CanBePinned())
                    {
                        const float font_size = ImGui::GetFontSize();
                        ImGui::TableSetupColumn(column.second.c_str(),
                                                ImGuiTableColumnFlags_WidthFixed,
                                                font_size);
                    }
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

                for(auto column_index = 0; column_index < m_last_column_index;
                    column_index++)
                {
                    if(column_index == 0 && !CanBePinned())
                    {
                        continue;
                    }
                    auto menu_func = [&](const char* value_to_copy) {
                        ImU32 mainColor =
                            SettingsManager::GetInstance().GetColor(Colors::kTextMain);
                        ImGui::PushStyleColor(ImGuiCol_Text, mainColor);
                        this->ContextMenu(value_to_copy, column_index, row);
                        ImGui::PopStyleColor();
                    };
                    RenderRowValues(column_index, row, menu_func);
                }

                RenderUnitValue(row);
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    ImGui::EndChild();
}

void
MetricTableBase::ChangePinState(const MetricId& metric_id)
{
    auto it = m_rows.find(metric_id);
    if(it != m_rows.end())
    {
        it->second.pinned = !it->second.pinned;
    }
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
        if(ImGui::MenuItem(" Pin metric"))
        {
            row.second.pinned = !row.second.pinned;
            m_pin_metric_clicked(
                { row.first.category_id, row.first.table_id, row.first.entry_id });
        }
        if(!m_event_source_id.empty() &&
           ImGui::MenuItem(" Send Metric to kernel details"))
        {
            AddMetricToKernelDetails(row.first, m_columns[column_index]);
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
            ImGui::TableSetBgColor(ImGuiTableBgTarget_CellBg,
                                   SettingsManager::GetInstance().GetColor(Colors::kBorderGray),
                                   static_cast<int>(column_index));
            ImGui::TextDisabled("N/A");
        }
        else if (row.second.values.at(column_index).value.empty())
        {
            ImVec4 disabled_col = ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
            ImGui::PushStyleColor(ImGuiCol_Text, disabled_col);
            CopyableTextUnformatted("N/A",
                                    "##value" + std::to_string(column_index),
                                    COPY_DATA_NOTIFICATION, false, true, menu_func);
            ImGui::PopStyleColor();
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
MetricTableBase::FillDefaultColumns(std::map<uint32_t, std::string>& columns,
                                    std::uint32_t&                   last_column_index)
{
    columns[0]            = "Pin";
    columns[1]            = "Metric ID";
    columns[2]            = "Metric";
    last_column_index     = 3;
    columns[LAST_INDEX]   = "Unit";
}

bool
MetricTableBase::IsMetricPinned(MetricId metric_id)
{
    auto it = m_rows.find(metric_id);
    return it != m_rows.end() && it->second.pinned;
}

void
MetricTableBase::AddMetricToKernelDetails(const MetricId& metric_id,
                                          const std::string& value_name)
{
    if(m_event_source_id.empty())
    {
        return;
    }

    EventManager::GetInstance()->AddEvent(
        std::make_shared<ComputeAddMetricToKernelDetailsEvent>(
            metric_id.category_id,
            metric_id.table_id,
            metric_id.entry_id,
            value_name,
            m_event_source_id));
}

bool
MetricTableBase::IsValueColumn(uint32_t column_index) const
{
    // not equal pin column, MetricId, Metric Name and Metric Unit
    return column_index != 0 &&
           column_index != 1 &&
           column_index != 2 &&
           column_index != LAST_INDEX;
}

bool
MetricTableBase::CanBePinned()
{
    if(m_pin_metric_clicked != nullptr)
        return true;

    return false;
}

//---------------------------------------------------------

void
MetricTable::ContextMenu(const char* value_to_copy, uint32_t column_index,
                                 std::pair<const MetricId, Row>& row)
{
    if(ImGui::BeginPopupContextItem())
    {
        if(ImGui::MenuItem(" Copy"))
        {
            ImGui::SetClipboardText(value_to_copy);
            NotificationManager::GetInstance().Show(COPY_DATA_NOTIFICATION.data(),
                                                    NotificationLevel::Info);
        }
        if(IsValueColumn(column_index))
        // not equal pin column, MetricId, Metric Name and Metric Unit
        {
            if(!m_event_source_id.empty() &&
               ImGui::MenuItem(" Send Metric to kernel details"))
            {
                AddMetricToKernelDetails(row.first, m_columns[column_index]);
            }
        }
        ImGui::EndPopup();
    }
}

void
MetricTable::RenderEmptyTable()
{
    ImGui::TextDisabled("This table is empty for current selection");
}

MetricTable::MetricTable(std::string event_source_id)
: MetricTableBase(std::move(event_source_id))
{}

void
MetricTable::Populate(const AvailableMetrics::Table& table,
                           const MetricValueLookup&       get_value)
{
    FillDefaultColumns(m_columns, m_last_column_index);
    m_table_title = table.name;

    if (table.value_names.empty())
    {
        m_columns[m_last_column_index++] = "Value";
    }
    else
    {
        for(const auto& value_name : table.value_names)
            m_columns[m_last_column_index++] = value_name;
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
                else
                {
                    row.values[value_index + 3].value = "";
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

PinnedMetricTable::PinnedMetricTable(DataProvider&                     data_provider,
                                     std::shared_ptr<ComputeSelection> compute_selection,
                                     uint64_t                          client_id)
: MetricTableBase(data_provider.GetTraceFilePath())
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(client_id)
{
    FillDefaultColumns(m_columns, m_last_column_index);
    m_table_flags = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX;
    m_table_title = "Pinned Metrics";
    m_max_rows_in_table = 7;
}

void
PinnedMetricTable::Update()
{
    if(!m_rebuild_pending)
    {
        return;
    }

    std::map<MetricId, Row>         new_rows;
    std::map<uint32_t, std::string> new_columns;
    uint32_t                        new_last_column_index = 0;

    FillDefaultColumns(new_columns, new_last_column_index);

    for(const auto& metric_id : m_pending_pinned_ids)
    {
        if(!HasMetricInCurrentWorkload(metric_id))
        {
            FillUnavailableRow(metric_id, new_rows);
            continue;
        }

        UpdateColumns(metric_id, new_columns, new_last_column_index);
        FillTableRow(metric_id, new_columns, new_rows);
    }

    m_rows.swap(new_rows);
    m_columns.swap(new_columns);
    m_last_column_index = new_last_column_index;
    m_pending_pinned_ids.clear();
    m_rebuild_pending = false;
}

void
PinnedMetricTable::ContextMenu(const char* value_to_copy, uint32_t column_index,
                               std::pair<const MetricId, Row>& row)
{
    if(ImGui::BeginPopupContextItem())
    {
        if(value_to_copy && std::string_view(value_to_copy) != "N/A")
        {
            if(ImGui::MenuItem(" Copy"))
            {
                ImGui::SetClipboardText(value_to_copy);
                NotificationManager::GetInstance().Show(COPY_DATA_NOTIFICATION.data(),
                                                        NotificationLevel::Info);
            }
        }

        if(IsValueColumn(column_index))
        {
            if(!m_event_source_id.empty() &&
               ImGui::MenuItem(" Send Metric to kernel details"))
            {
                AddMetricToKernelDetails(row.first, m_columns[column_index]);
            }
        }

        ImGui::EndPopup();
    }
}

void
PinnedMetricTable::FillMandatoryColumns(const MetricId&                metric_id,
                                        const AvailableMetrics::Table& table,
                                        Row&                           row)
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
PinnedMetricTable::RefillTable(const std::set<MetricId>& pinned_ids)
{
    m_pending_pinned_ids = pinned_ids;
    m_rebuild_pending    = true;
}

void
PinnedMetricTable::AddRow(MetricId metric_id)
{
    if(!HasMetricInCurrentWorkload(metric_id))
    {
        FillUnavailableRow(metric_id, m_rows);
        return;
    }

    UpdateColumns(metric_id, m_columns, m_last_column_index);
    FillTableRow(metric_id, m_columns, m_rows);
}

void
PinnedMetricTable::UpdateColumns(MetricId                        metric_id,
                                 std::map<uint32_t, std::string>& columns,
                                 uint32_t&                        last_column_index)
{
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    for(const auto& name : GetTable(metric_id, workload_id).value_names)
    {
        if(GetColumnIndex(name, columns) == std::nullopt)
        {
            columns[last_column_index++] = name;
        }
    }
}

const AvailableMetrics::Table&
PinnedMetricTable::GetTable(const MetricId& metric_id, uint32_t workload_id)
{
    const auto& tree = m_data_provider.ComputeModel()
                           .GetWorkloads()
                           .at(workload_id)
                           .available_metrics.tree;
    return tree.at(metric_id.category_id).tables.at(metric_id.table_id);
}

bool
PinnedMetricTable::HasMetricInCurrentWorkload(const MetricId& metric_id) const
{
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID)
        return false;

    const auto& workloads = m_data_provider.ComputeModel().GetWorkloads();
    auto workload_it = workloads.find(workload_id);
    if(workload_it == workloads.end())
        return false;

    const auto& tree = workload_it->second.available_metrics.tree;
    auto category_it = tree.find(metric_id.category_id);
    if(category_it == tree.end())
        return false;

    auto table_it = category_it->second.tables.find(metric_id.table_id);
    if(table_it == category_it->second.tables.end())
        return false;

    return table_it->second.entries.count(metric_id.entry_id) > 0;
}

std::optional<uint32_t>
PinnedMetricTable::GetColumnIndex(const std::string&                  column_name,
                                  const std::map<uint32_t, std::string>& columns)
{
    for(const auto& column : columns)
    {
        if(column.second == column_name)
            return column.first;
    }
    return std::nullopt;
}

void
PinnedMetricTable::RenderEmptyTable()
{
    ImGui::TextDisabled(" Pin the metric to see it here");
}

void
PinnedMetricTable::FillUnavailableRow(const MetricId& metric_id,
                                      std::map<MetricId, Row>& rows)
{
    Row row;
    row.pinned = true;
    row.values[1].value = metric_id.ToString();
    row.values[LAST_INDEX].value = "";
    rows[metric_id] = std::move(row);
}

void
PinnedMetricTable::FillTableRow(const MetricId&                    metric_id,
                                const std::map<uint32_t, std::string>& columns,
                                std::map<MetricId, Row>&           rows)
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
    const auto& table = GetTable(metric_id, workload_id);
    FillMandatoryColumns(metric_id, table, row);

    char buf[64];
    auto metric_value = m_data_provider.ComputeModel().GetKernelMetricValue(
        m_client_id, kernel_id, metric_id.category_id,
                                   metric_id.table_id, metric_id.entry_id);
    for(const auto& value_name : table.value_names)
    {
        if(auto column_index = GetColumnIndex(value_name, columns); column_index.has_value())
        {
            if(metric_value && metric_value->entry &&
               metric_value->values.count(value_name))
            {
                snprintf(buf, sizeof(buf), "%.2f", metric_value->values.at(value_name));
                row.values[column_index.value()].value = buf;
            }
            else
            {
                row.values[column_index.value()].value = "";
            }
        }
    }
    rows[metric_id] = std::move(row);
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

}  // namespace View
}  // namespace RocProfVis
