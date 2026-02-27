// Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_metric_table.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"

#include "imgui.h"
#include "spdlog/spdlog.h"

namespace RocProfVis
{
namespace View
{

// ID, Name, duration, invocation columns that are always present in the table
constexpr int PERMANENT_COLUMN_COUNT = 4;

KernelMetricTable::KernelMetricTable(DataProvider&                     data_provider,
                                     std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_fetch_requested(false)
, m_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_sort_column_index(-1)
, m_sort_order(kRPVControllerSortOrderDescending)
, m_selected_row(-1)
, m_compute_selection(compute_selection)
, m_selected_kernel_id_local(ComputeSelection::INVALID_SELECTION_ID)
{}

void
KernelMetricTable::ClearData()
{
    m_metrics_params.clear();
    m_metrics_column_names.clear();
}

void
KernelMetricTable::FetchData(uint32_t workload_id)
{
    m_workload_id = workload_id;
    if(m_workload_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        spdlog::warn("Invalid workload ID, cannot fetch kernel metric data");
        return;
    }

    m_query_builder.SetWorkload(m_data_provider.ComputeModel().GetWorkload(workload_id));
    m_fetch_requested = true;
}

void
KernelMetricTable::HandleNewData()
{
    // update column names based on current metric params
    ComputeKernelSelectionTable& table =
        m_data_provider.ComputeModel().GetKernelSelectionTable();
    std::shared_ptr<RocProfVis::View::ComputeTableRequestParams> request_params =
        table.GetTableInfo().table_params;

    m_metrics_column_names.clear();

    ROCPROFVIS_ASSERT(m_metrics_params.size() == m_metrics_info.size());
    size_t metric_count = request_params->m_metric_selectors.size();
    for(size_t i = 0; i < metric_count; i++)
    {
        m_metrics_column_names.push_back(m_metrics_info[i].entry.name + " (" +
                                         m_metrics_info[i].value_name + ")");
    }
}

void
KernelMetricTable::Update()
{
    bool request_pending =
        m_data_provider.IsRequestPending(DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID);

    if(!request_pending && m_fetch_requested)
    {
        ComputeTableRequestParams params(m_workload_id, m_metrics_params);

        params.m_sort_column_index = m_sort_column_index;
        params.m_sort_order =
            static_cast<rocprofvis_controller_sort_order_t>(m_sort_order);

        spdlog::debug("Requesting sorted kernel selection table: column {}, order {}",
                      m_sort_column_index,
                      m_sort_order == kRPVControllerSortOrderAscending ? "ASC" : "DESC");
        m_data_provider.FetchMetricPivotTable(params);

        m_fetch_requested = false;
    }

    // check if kernel selection has changed and update selection if needed
    uint32_t selected_kernel_id = m_compute_selection->GetSelectedKernel();

    if(m_selected_kernel_id_local != selected_kernel_id)
    {
        m_selected_kernel_id_local = selected_kernel_id;
        if(m_selected_kernel_id_local == ComputeSelection::INVALID_SELECTION_ID)
        {
            m_selected_row = -1;
        }
        else
        {
            // Find the row with the selected kernel ID and update selection
            ComputeKernelSelectionTable& table =
                m_data_provider.ComputeModel().GetKernelSelectionTable();
            const std::vector<std::vector<std::string>>& data = table.GetTableData();
            for(size_t row = 0; row < data.size(); row++)
            {
                // TODO: add "Important Column" for indentifying id column instead of
                // assuming index 0
                if(!data[row].empty() &&
                   data[row][0] == std::to_string(selected_kernel_id))
                {
                    m_selected_row = static_cast<int>(row);
                    break;
                }
            }
        }
    }
}

void
KernelMetricTable::Render()
{
    int remove_index = -1;

    SettingsManager& settings     = SettingsManager::GetInstance();
    float            item_spacing = settings.GetDefaultStyle().ItemSpacing.x;

    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Kernel Selection Table");

    // if(m_workload_id == ComputeSelection::INVALID_SELECTION_ID)
    // {
    //     ImGui::Separator();
    //     ImGui::TextUnformatted("No workload selected");
    //     return;
    // }
    m_query_builder.SetWorkload(
        m_data_provider.ComputeModel().GetWorkload(m_workload_id));

    ImGui::SameLine(0.0f, item_spacing);

    ImGui::BeginDisabled(m_workload_id == ComputeSelection::INVALID_SELECTION_ID);
    if(ImGui::Button("Add Metric"))
    {
        m_query_builder.Show([this](const std::string& query) {
            m_metrics_params.push_back(query);
            const AvailableMetrics::Entry* entry =
                m_query_builder.GetSelectedMetricInfo();
            m_metrics_info.push_back({ entry ? *entry : AvailableMetrics::Entry(),
                                       m_query_builder.GetValueName() });
            m_fetch_requested = true;
        });
    }
    ImGui::EndDisabled();

    ImGui::Separator();

    ComputeKernelSelectionTable& table =
        m_data_provider.ComputeModel().GetKernelSelectionTable();
    const std::vector<std::string>&              header = table.GetTableHeader();
    const std::vector<std::vector<std::string>>& data   = table.GetTableData();

    bool request_pending =
        m_data_provider.IsRequestPending(DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID);

    float       line_height   = ImGui::GetTextLineHeightWithSpacing();
    ImGuiStyle& style         = ImGui::GetStyle();
    float       row_padding_v = style.CellPadding.y * 2.0f;
    line_height += row_padding_v;

    // Set a fixed height for the table container
    if(ImGui::BeginChild("kernel_metric_table_cont", ImVec2(0, 10.0f * line_height),
                         ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
    {
        if(!header.empty() && !data.empty() && m_workload_id != ComputeSelection::INVALID_SELECTION_ID)
        {
            ImGuiTableFlags table_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
                                          ImGuiTableFlags_Borders |
                                          ImGuiTableFlags_SizingStretchSame;
            if(!request_pending)
            {
                table_flags = table_flags | ImGuiTableFlags_Sortable;
            }

            int column_count = static_cast<int>(header.size());
            ImVec2 outer_size = ImVec2(0.0f, ImGui::GetContentRegionAvail().y);

            if(ImGui::BeginTable("kernel_selection_table", column_count, table_flags,
                                 outer_size))
            {
                ImGui::TableSetupScrollFreeze(0, 1);  // Freeze header row
                for(int col = 0; col < column_count; col++)
                {
                    if(col < PERMANENT_COLUMN_COUNT)
                    {
                        ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_None;
                        if(!header[col].empty() && header[col][0] == '_')
                        {
                            col_flags = ImGuiTableColumnFlags_DefaultHide |
                                        ImGuiTableColumnFlags_Disabled;
                        }
                        ImGui::TableSetupColumn(header[col].c_str(), col_flags);
                    }
                    else
                    {
                        int index = col - PERMANENT_COLUMN_COUNT;
                        // Since render reads directly from the data model, the
                        // m_metrics_column_names may not be synced for a few frames
                        if(index < static_cast<int>(m_metrics_column_names.size()))
                        {
                            ImGui::TableSetupColumn(
                                m_metrics_column_names[index].c_str());
                        }
                        else
                        {
                            ImGui::TableSetupColumn(
                                ("Metric " + std::to_string(index + 1)).c_str());
                        }
                    }
                }

                // Custom header row with hover detection and X button
                ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                for(int col = 0; col < column_count; col++)
                {
                    ImGui::TableSetColumnIndex(col);

                    // Skip X for non-removable columns (like ID, Name)
                    if(col < PERMANENT_COLUMN_COUNT)
                    {
                        ImGui::TableHeader(ImGui::TableGetColumnName(col));
                        continue;
                    }

                    // Sortable header with X button
                    const char* name = ImGui::TableGetColumnName(col);
                    ImGui::TableHeader(name);
                    ImVec2 text_size = ImGui::CalcTextSize(name);
                    ImGui::SameLine(text_size.x, item_spacing);

                    ImGui::PushID(col);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
                    if(XButton(nullptr, &settings))
                    {
                        remove_index = col - PERMANENT_COLUMN_COUNT;
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopID();
                }

                if(data.empty())
                {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextDisabled("None");
                }

                // Get sort specs
                ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
                if(sort_specs && sort_specs->SpecsDirty)
                {
                    int                                sort_column_index = -1;
                    rocprofvis_controller_sort_order_t sort_order =
                        kRPVControllerSortOrderDescending;
                    sort_column_index = sort_specs->Specs->ColumnIndex;
                    sort_order =
                        (sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending)
                            ? kRPVControllerSortOrderAscending
                            : kRPVControllerSortOrderDescending;

                    sort_specs->SpecsDirty = false;

                    if(m_sort_column_index != sort_column_index ||
                       m_sort_order != sort_order)
                    {
                        m_sort_column_index = sort_column_index;
                        m_sort_order        = sort_order;
                        m_fetch_requested   = true;
                    }
                }

                if(request_pending)
                {
                    ImGui::BeginDisabled();
                }

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(data.size()));
                while(clipper.Step())
                {
                    for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                    {
                        ImGui::TableNextRow();

                        bool is_selected       = (m_selected_row == row);
                        bool selectable_placed = false;

                        for(int col = 0; col < data[row].size(); col++)
                        {
                            const std::string& cell = data[row][col];
                            ImGui::TableNextColumn();

                            // Check if this is the first visible column
                            ImGuiTableColumnFlags flags = ImGui::TableGetColumnFlags(col);
                            bool is_visible = (flags & ImGuiTableColumnFlags_IsVisible) != 0;
                            bool is_enabled = (flags & ImGuiTableColumnFlags_IsEnabled) != 0;

                            if(!selectable_placed && is_visible && is_enabled)
                            {
                                if(ImGui::Selectable(cell.c_str(), is_selected,
                                                     ImGuiSelectableFlags_SpanAllColumns))
                                {
                                    if(is_selected)
                                    {
                                        m_selected_row = -1;  // Deselect if already selected
                                        m_compute_selection->SelectKernel(
                                            ComputeSelection::INVALID_SELECTION_ID);
                                    }
                                    else
                                    {
                                        m_selected_row = row;
                                        m_selected_kernel_id_local =
                                            data[row][0].empty()
                                                ? ComputeSelection::INVALID_SELECTION_ID
                                                : std::stoul(data[row][0]);

                                        m_compute_selection->SelectKernel(
                                            m_selected_kernel_id_local);
                                    }
                                }
                                selectable_placed = true;
                            }
                            else
                            {
                                ImGui::TextUnformatted(cell.c_str());
                            }
                        }
                    }
                }

                if(request_pending)
                {
                    ImGui::EndDisabled();
                }

                ImGui::EndTable();
            }

            if(request_pending)
            {
                RenderLoadingIndicator();
            }
        }
        else
        {
            if(m_workload_id == ComputeSelection::INVALID_SELECTION_ID)
            {
                ImGui::TextDisabled("No workload selected");
            }
            else
            {
                ImGui::TextDisabled("No data to display");
            }
        }
    }
    ImGui::EndChild();

    if(remove_index >= 0 && remove_index < static_cast<int>(m_metrics_params.size()))
    {
        ROCPROFVIS_ASSERT(m_metrics_params.size() == m_metrics_info.size());

        m_metrics_params.erase(m_metrics_params.begin() + remove_index);
        m_metrics_info.erase(m_metrics_info.begin() + remove_index);

        m_fetch_requested = true;
        spdlog::debug("Removed metric column at index {}", remove_index);
    }

    m_query_builder.Render();
}

void
KernelMetricTable::RenderLoadingIndicator() const
{
    ImGui::SetCursorPos(ImVec2(0, 0));
    // set transparent background for the overlay window
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));
    ImGui::BeginChild("kernel_metric_table_loading", ImGui::GetWindowSize(),
                      ImGuiChildFlags_None);

    float dot_radius  = 5.0f;
    int   num_dots    = 3;
    float dot_spacing = 5.0f;
    float anim_speed  = 5.0f;

    ImVec2 dot_size = MeasureLoadingIndicatorDots(dot_radius, num_dots, dot_spacing);

    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 view_rect  = ImGui::GetWindowSize();
    ImVec2 pos        = ImGui::GetCursorPos();
    ImVec2 center_pos = ImVec2(window_pos.x + (view_rect.x - dot_size.x) * 0.5f,
                               window_pos.y + (view_rect.y - dot_size.y) * 0.5f);

    ImGui::SetCursorScreenPos(center_pos);

    RenderLoadingIndicatorDots(dot_radius, num_dots, dot_spacing,
                               SettingsManager::GetInstance().GetColor(Colors::kTextMain),
                               anim_speed);

    // Reset cursor position after rendering spinner
    ImGui::SetCursorPos(pos);

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

}  // namespace View
}  // namespace RocProfVis
