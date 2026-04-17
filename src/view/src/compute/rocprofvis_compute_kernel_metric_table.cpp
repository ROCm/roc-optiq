// Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_metric_table.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_common_defs.h"
#include "widgets/rocprofvis_notification_manager.h"

#include "imgui.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace RocProfVis
{
namespace View
{

// ID, Name, duration, invocation columns that are always present in the table
constexpr int PERMANENT_COLUMN_COUNT = 4;

constexpr int ID_COLUMN_INDEX         = 0;
constexpr int NAME_COLUMN_INDEX       = 1;
constexpr int DURATION_COLUMN_INDEX   = 2;
constexpr int INVOCATION_COLUMN_INDEX = 3;

// Minimum character limits for calculating column widths
constexpr std::string_view FILTER_TEXT_HINT_STR = "LIKE %text%";
constexpr std::string_view FILTER_TEXT_HINT_NUMERICAL = ">, <, =, >=, <=, !=";
constexpr float            COL_FILTER_CHAR_LIMIT      = static_cast<float>(
    std::max(FILTER_TEXT_HINT_STR.length(), FILTER_TEXT_HINT_NUMERICAL.length()));

constexpr float COL_NAME_CHAR_LIMIT       = 40.0f;
constexpr float COL_DEFAULT_CHAR_LIMIT    = 30.0f;
constexpr float COL_INVOCATION_CHAR_LIMIT = COL_FILTER_CHAR_LIMIT;

constexpr float kTooltipMaxWidth = 600.0f;

KernelMetricTable::KernelMetricTable(DataProvider&                     data_provider,
                                     std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_fetch_requested(false)
, m_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_sort_column_index(DURATION_COLUMN_INDEX)
, m_sort_order(kRPVControllerSortOrderDescending)
, m_selected_row(-1)
, m_compute_selection(compute_selection)
, m_selected_kernel_id_local(ComputeSelection::INVALID_SELECTION_ID)
, m_show_kernel_table(true)
, m_update_table_selection(false)
, m_allow_deselect(false)
, m_sort_specs_initialized(false)
, m_permanent_column_names({ "ID", "Name", "Duration (ns)", "Invocations" })
{
    m_widget_name = GenUniqueName("KernelMetricTable");
}

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
        m_metrics_column_names.push_back(m_metrics_info[i].entry.name + " " +
                                         m_metrics_info[i].value_name + " " + "(" +
                                         m_metrics_info[i].entry.unit + ")");
    }

    ComputeColumnMaxValues(table.GetTableData());

    m_update_table_selection = true; 
}

void
KernelMetricTable::Update()
{
    bool request_pending =
        m_data_provider.IsRequestPending(DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID);

    if(!request_pending && m_fetch_requested)
    {
        // Build filter map from vector - only include active filters
        std::unordered_map<uint64_t, std::string> filter_map;

        for(size_t i = 0; i < m_column_filters.size(); i++)
        {
            const ColumnFilter& filter = m_column_filters[i];
            if(filter.is_active && strlen(filter.filter_text) > 0)
            {
                filter_map[i] = std::string(filter.filter_text);
            }
        }

        ComputeTableRequestParams params(
            m_workload_id,
            m_metrics_params,
            m_sort_column_index,
            static_cast<rocprofvis_controller_sort_order_t>(m_sort_order),
            filter_map  // Pass filters by column index
        );

        spdlog::debug("Requesting kernel selection table: column {}, order {}, filters {}",
                      m_sort_column_index,
                      m_sort_order == kRPVControllerSortOrderAscending ? "ASC" : "DESC",
                      filter_map.size());
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
            m_update_table_selection = true;
        }
    }

    if(m_update_table_selection) {
        // Find the row with the selected kernel ID and update selection
        ComputeKernelSelectionTable& table =
            m_data_provider.ComputeModel().GetKernelSelectionTable();
        const std::vector<std::vector<std::string>>& data = table.GetTableData();
        // reset selection (filter may have removed the selected kernel from the table)
        m_selected_row = -1;
        for(size_t row = 0; row < data.size(); row++)
        {
            // TODO: add "Important Column" for indentifying id column instead of
            // assuming index 0?
            if(!data[row].empty() &&
            data[row][ID_COLUMN_INDEX] == std::to_string(selected_kernel_id))
            {
                m_selected_row = static_cast<int>(row);
                break;
            }
        }
        m_update_table_selection = false;
    }
}

void
KernelMetricTable::Render()
{
    int remove_index = -1;

    SettingsManager& settings     = SettingsManager::GetInstance();
    ImFont*          icon_font  = settings.GetFontManager().GetIconFont(FontType::kDefault);
    const ImGuiStyle &style = settings.GetDefaultStyle();
    const float      cell_padding = style.CellPadding.x * 2.0f;
    const float      char_width = ImGui::CalcTextSize("M").x;

    SectionTitle("Kernel Selection Table");

    ComputeKernelSelectionTable& table =
        m_data_provider.ComputeModel().GetKernelSelectionTable();
    const std::vector<std::string>&              header = table.GetTableHeader();
    const std::vector<std::vector<std::string>>& data   = table.GetTableData();

    // Toolbar with actions
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImGui::ColorConvertU32ToFloat4(
                                                SettingsManager::GetInstance().GetColor(Colors::kBgPanel)));
    ImGui::PushStyleColor(
        ImGuiCol_Border,
        ImGui::ColorConvertU32ToFloat4(SettingsManager::GetInstance().GetColor(Colors::kBorderColor)));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, style.WindowPadding);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);
    ImGui::BeginChild("toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders);

    ImGui::AlignTextToFramePadding();
    const char* icon = m_show_kernel_table ? ICON_CHEVRON_DOWN : ICON_CHEVRON_RIGHT;
    
    ImGui::PushFont(icon_font);
    ImVec2 icon_size = ImGui::CalcTextSize(ICON_CHEVRON_DOWN); // use larger icon for consistent spacing
    ImGui::PopFont();

    if(IconButton(icon, icon_font,
                  ImVec2(icon_size.x + style.FramePadding.x * 2.0f,
                         icon_size.y + style.FramePadding.y * 2.0f),
                  m_show_kernel_table ? "Hide Table" : "Show Table", style.WindowPadding,
                  false, style.FramePadding,
                  SettingsManager::GetInstance().GetColor(Colors::kTransparent),
                  SettingsManager::GetInstance().GetColor(Colors::kButtonHovered),
                  SettingsManager::GetInstance().GetColor(Colors::kTransparent)))
    {
        m_show_kernel_table = !m_show_kernel_table;
    }

    ImGui::SameLine(); //No spacing on purpose
    ImGui::TextUnformatted("Table");
    VerticalSeparator();

    m_query_builder.SetWorkload(
        m_data_provider.ComputeModel().GetWorkload(m_workload_id));

    ImGui::BeginDisabled(!m_show_kernel_table ||
                         m_workload_id == ComputeSelection::INVALID_SELECTION_ID);
    if(ImGui::Button("Add Metric"))
    {
        m_query_builder.Show([this](const std::string& query) {
            // only add metric if not already added
            if(std::find(m_metrics_params.begin(), m_metrics_params.end(), query) !=
               m_metrics_params.end())
            {
                // show notification that metric is already added
                NotificationManager::GetInstance().Show("The metric '" + query +
                                                            "' is already in the table.",
                                                        NotificationLevel::Warning);
                return;
            }

            m_metrics_params.push_back(query);
            const AvailableMetrics::Entry* entry =
                m_query_builder.GetSelectedMetricInfo();
            m_metrics_info.push_back({ entry ? *entry : AvailableMetrics::Entry(),
                                       m_query_builder.GetValueName() });

            // Add filter slot for the new metric column
            m_column_filters.push_back(ColumnFilter());
            m_pending_column_filters.push_back(ColumnFilter());

            if(!m_bar_chart_columns.empty())
            {
                int new_col = PERMANENT_COLUMN_COUNT +
                              static_cast<int>(m_metrics_params.size()) - 1;
                m_bar_chart_columns.insert(new_col);
            }

            m_fetch_requested = true;
        });
    }
    
    ImGui::SameLine(0.0f, style.ItemSpacing.x);

    // Filter control buttons
    if(ImGui::Button("Apply Filters"))
    {
        ApplyFilters();
    }
    ImGui::SameLine(0.0f, style.ItemSpacing.x);
    if(ImGui::Button("Clear All Filters"))
    {
        ClearAllFilters();
    }
    ImGui::SameLine(0.0f, style.ItemSpacing.x);
    if(ImGui::Button(m_bar_chart_columns.empty() ? "Show Bar Charts" : "Hide Bar Charts"))
    {
        if(m_bar_chart_columns.empty())
        {
            int col_count = static_cast<int>(header.size());
            for(int c = 0; c < col_count; c++)
            {
                if(c != ID_COLUMN_INDEX && c != NAME_COLUMN_INDEX)
                    m_bar_chart_columns.insert(c);
            }
            ComputeColumnMaxValues(data);
        }
        else
        {
            m_bar_chart_columns.clear();
            m_column_max_values.clear();
        }
    }

    // Show active filter count
    size_t active_count = 0;
    for(const auto& filter : m_column_filters)
    {
        if(filter.is_active && strlen(filter.filter_text) > 0)
        {
            active_count++;
        }
    }
    if(active_count > 0)
    {
        ImGui::SameLine(0.0f, style.ItemSpacing.x);
        ImGui::TextDisabled("(%zu active filters)", active_count);
    }

    ImGui::EndDisabled();

    // End toolbar
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::PopStyleColor(2);

    bool request_pending =
        m_data_provider.IsRequestPending(DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID);

    float       line_height   = ImGui::GetTextLineHeightWithSpacing();
    //ImGuiStyle& style         = ImGui::GetStyle();
    float       row_padding_v = style.CellPadding.y * 2.0f;
    line_height += row_padding_v;

    // Filter row height (InputText widgets are taller than text)
    float filter_row_height = ImGui::GetFrameHeightWithSpacing() + row_padding_v;

    int data_row_count = static_cast<int>(data.size());
    int rows_to_render = std::max(std::min(10, data_row_count), 5);

    // Calculate total table height: header + filter row + data rows
    float total_table_height = line_height + filter_row_height + (rows_to_render * line_height);

    if(m_show_kernel_table)
    {
    // Set a fixed height for the table container
    if(ImGui::BeginChild("kernel_metric_table_cont", ImVec2(0, total_table_height),
                         ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
    {
        if(!header.empty() && !data.empty() && m_workload_id != ComputeSelection::INVALID_SELECTION_ID)
        {
            ImGuiTableFlags table_flags =
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings;
            if(!request_pending)
            {
                table_flags = table_flags | ImGuiTableFlags_Sortable;
            }

            int column_count = static_cast<int>(header.size());
            ImVec2 outer_size = ImVec2(ImGui::GetContentRegionAvail());

            if(ImGui::BeginTable("kernel_selection_table", column_count, table_flags,
                                 outer_size))
            {
                ImGui::TableSetupScrollFreeze(1, 2);  // Freeze Name column and header+filter rows

                // Calculate minimum column widths based on character counts
                float name_min_width = char_width * COL_NAME_CHAR_LIMIT;
                float default_min_width = char_width * COL_DEFAULT_CHAR_LIMIT;
                float invocation_min_width = char_width * COL_INVOCATION_CHAR_LIMIT;

                for(int col = 0; col < column_count; col++)
                {
                    ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_WidthFixed;
                    if(col < PERMANENT_COLUMN_COUNT)
                    {
                        if(!header[col].empty() && header[col][0] == '_')
                        {
                            col_flags |= ImGuiTableColumnFlags_DefaultHide |
                                        ImGuiTableColumnFlags_Disabled;
                        }
                        if(!m_sort_specs_initialized && col == DURATION_COLUMN_INDEX)
                        {
                            col_flags |= ImGuiTableColumnFlags_DefaultSort;
                            m_sort_specs_initialized = true;
                        }
                        col_flags |= ImGuiTableColumnFlags_PreferSortDescending;

                        // Set minimum width based on column type
                        float min_width = default_min_width;
                        if(col == NAME_COLUMN_INDEX)
                        {
                            min_width = name_min_width;
                        }
                        else if(col == INVOCATION_COLUMN_INDEX)
                        {
                            min_width = invocation_min_width;
                        }

                        ImGui::TableSetupColumn(m_permanent_column_names[col].c_str(), col_flags, min_width);
                    }
                    else
                    {
                        int index = col - PERMANENT_COLUMN_COUNT;
                        
                        // Since render reads directly from the data model, the
                        // m_metrics_column_names may not be synced for a few frames
                        if(index < static_cast<int>(m_metrics_column_names.size()))
                        {
                            // Calculate width based on name + padding for close button
                            float column_size =
                                ImGui::CalcTextSize(m_metrics_column_names[index].c_str()).x +
                                cell_padding + char_width * 2.0;

                            column_size = std::max(column_size, default_min_width);

                            ImGui::TableSetupColumn(
                                m_metrics_column_names[index].c_str(),
                                ImGuiTableColumnFlags_WidthFixed,
                                column_size);
                            }
                        else
                        {
                            ImGui::TableSetupColumn(
                                ("Metric " + std::to_string(index + 1)).c_str(),
                                ImGuiTableColumnFlags_WidthFixed,
                                default_min_width);
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
                        RenderBarChartContextMenu(col);
                        continue;
                    }

                    // Sortable header with X button
                    const char* name = ImGui::TableGetColumnName(col);
                    ImGui::TableHeader(name);
                    bool header_hovered = ImGui::IsItemHovered();
                    RenderBarChartContextMenu(col);
                    ImVec2 text_size = ImGui::CalcTextSize(name);
                    if(header_hovered)
                    {
                        int index = col - PERMANENT_COLUMN_COUNT;
                        if(index < static_cast<int>(m_metrics_info.size()))
                        {
                            const std::string &desc = m_metrics_info[index].entry.description;
                            if(!desc.empty())
                            {
                                ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                                                    ImVec2(kTooltipMaxWidth, FLT_MAX));
                                BeginTooltipStyled();
                                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTooltipMaxWidth);
                                ImGui::TextUnformatted(desc.c_str());
                                ImGui::PopTextWrapPos();
                                EndTooltipStyled();                                
                            }
                        }
                    }
                    ImGui::SameLine(text_size.x, style.ItemInnerSpacing.x);

                    ImGui::PushID(col);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.0f, 0.0f));
                    if(XButton(nullptr, "Remove Metric", &settings))
                    {
                        remove_index = col - PERMANENT_COLUMN_COUNT;
                    }
                    ImGui::PopStyleVar();
                    ImGui::PopID();
                }

                // Filter row
                ImGui::TableNextRow();
                for(int col = 0; col < column_count; col++)
                {
                    if(!ImGui::TableSetColumnIndex(col))
                        continue;
                    RenderColumnFilter(col);
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
                        ImGui::PushID(row);  // Push row ID for unique identification

                        bool is_selected       = (m_selected_row == row);
                        bool selectable_placed = false;

                        for(int col = 0; col < data[row].size(); col++)
                        {
                            const std::string& cell = data[row][col];
                            ImGui::TableNextColumn();

                            // Track hover using the current table cell bounds instead of
                            // item hover state because the first selectable spans all columns.
                            ImVec2 cell_min = ImGui::GetCursorScreenPos();
                            float  cell_width = ImGui::GetContentRegionAvail().x;
                            float  cell_height = line_height;
                            ImVec2 cell_max(cell_min.x + cell_width, cell_min.y + cell_height);

                            // Check if this is the first visible column
                            ImGuiTableColumnFlags flags = ImGui::TableGetColumnFlags(col);
                            bool is_visible = (flags & ImGuiTableColumnFlags_IsVisible) != 0;
                            bool is_enabled = (flags & ImGuiTableColumnFlags_IsEnabled) != 0;
                            bool need_tooltip = false;
                            if(col == NAME_COLUMN_INDEX)
                            {
                                // Measure text and if larger than the cell, use a tooltip
                                ImVec2 text_size = ImGui::CalcTextSize(cell.c_str());
                                float available_width = ImGui::GetContentRegionAvail().x;
                                if(text_size.x > available_width)
                                {
                                    need_tooltip = true;
                                }
                            }

                            if(!selectable_placed && is_visible && is_enabled)
                            {
                                if(ImGui::Selectable(cell.c_str(), is_selected,
                                                     ImGuiSelectableFlags_SpanAllColumns))
                                {
                                    if(is_selected)
                                    {
                                        if(m_allow_deselect)
                                        {
                                            m_selected_row = -1;  // Deselect if already selected
                                            m_compute_selection->SelectKernel(
                                                ComputeSelection::INVALID_SELECTION_ID);
                                        }
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
                                if(cell.empty())
                                {
                                    ImGui::TextDisabled("N/A");
                                }
                                else
                                {
                                    if(m_bar_chart_columns.count(col) > 0 && !cell.empty())
                                    {
                                        auto it = m_column_max_values.find(col);
                                        if(it != m_column_max_values.end() && it->second > 0.0)
                                        {
                                            char*  end = nullptr;
                                            double val = std::strtod(cell.c_str(), &end);
                                            if(end != cell.c_str())
                                            {
                                                float ratio = static_cast<float>(
                                                    std::abs(val) / it->second);
                                                ratio = std::min(ratio, 1.0f);

                                                ImVec2     pos       = ImGui::GetCursorScreenPos();
                                                float      w         = ImGui::GetContentRegionAvail().x;
                                                float      h         = ImGui::GetTextLineHeightWithSpacing();
                                                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                                                // Intersect with the current clip rect so the bar is
                                                // correctly hidden behind frozen rows/columns when the
                                                // table is scrolled vertically.
                                                draw_list->PushClipRect(
                                                    pos, ImVec2(pos.x + w, pos.y + h), true);
                                                draw_list->AddRectFilled(
                                                    pos,
                                                    ImVec2(pos.x + w * ratio, pos.y + h),
                                                    SettingsManager::GetInstance().GetColor(
                                                        Colors::kHighlightChart));
                                                draw_list->PopClipRect();
                                            }
                                        }
                                    }
                                    ImGui::TextUnformatted(cell.c_str());
                                }
                            }
                            bool cell_hovered = ImGui::IsMouseHoveringRect(cell_min, cell_max, true);
                            if(need_tooltip && cell_hovered)
                            {
                                ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0),
                                                                    ImVec2(kTooltipMaxWidth, FLT_MAX));
                                BeginTooltipStyled();
                                ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTooltipMaxWidth);
                                ImGui::TextUnformatted(cell.c_str());
                                ImGui::PopTextWrapPos();
                                EndTooltipStyled();
                            }
                        }
                        ImGui::PopID();  // Pop row ID
                    }
                }

                if(request_pending)
                {
                    ImGui::EndDisabled();
                }

                ImGui::EndTable();
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
                if(request_pending)
                {
                    ImGui::TextDisabled("Loading data...");
                }
                else
                {
                    ImGui::TextDisabled("No data to display");
                }
            }
        }

        if(request_pending)
        {
            RenderLoadingIndicator(
                SettingsManager::GetInstance().GetColor(Colors::kTextMain),
                "kernel_metric_table_loading");
        }
    }
    ImGui::EndChild();

    if(remove_index >= 0 && remove_index < static_cast<int>(m_metrics_params.size()))
    {
        ROCPROFVIS_ASSERT(m_metrics_params.size() == m_metrics_info.size());

        m_metrics_params.erase(m_metrics_params.begin() + remove_index);
        m_metrics_info.erase(m_metrics_info.begin() + remove_index);

        // Remove corresponding filter for the removed metric column
        size_t filter_index = PERMANENT_COLUMN_COUNT + remove_index;
        if(filter_index < m_column_filters.size())
        {
            m_column_filters.erase(m_column_filters.begin() + filter_index);
            m_pending_column_filters.erase(m_pending_column_filters.begin() + filter_index);
        }

        int removed_col = PERMANENT_COLUMN_COUNT + remove_index;
        m_bar_chart_columns.erase(removed_col);
        std::set<int> adjusted;
        for(int c : m_bar_chart_columns)
            adjusted.insert(c > removed_col ? c - 1 : c);
        m_bar_chart_columns = adjusted;

        m_fetch_requested = true;
        spdlog::debug("Removed metric column at index {}", remove_index);
    }
    }

    m_query_builder.Render();
}

void
KernelMetricTable::RenderColumnFilter(int column_index)
{
    // Ensure vectors are sized correctly
    size_t total_columns = PERMANENT_COLUMN_COUNT + m_metrics_params.size();
    if(m_pending_column_filters.size() != total_columns)
    {
        m_pending_column_filters.resize(total_columns);
    }

    if(column_index < 0 || column_index >= static_cast<int>(total_columns))
    {
        return;
    }

    ColumnFilter& filter = m_pending_column_filters[column_index];

    // Determine hint based on column type (column 1 is Name - text column)
    const char* hint = (column_index == 1) ? FILTER_TEXT_HINT_STR.data() : FILTER_TEXT_HINT_NUMERICAL.data();

    ImGui::PushID(column_index);

    ImVec2 cell_min  = ImGui::GetCursorScreenPos();
    float  cell_width = ImGui::GetContentRegionAvail().x;
    ImGui::PushClipRect(
        cell_min,
        ImVec2(cell_min.x + cell_width, cell_min.y + ImGui::GetFrameHeightWithSpacing()),
        true);

    ImGui::SetNextItemWidth(cell_width);

    if(ImGui::InputTextWithHint("##filter", hint, filter.filter_text,
                                sizeof(filter.filter_text)))
    {
        filter.is_active = (strlen(filter.filter_text) > 0);
    }

    ImGui::PopClipRect();
    ImGui::PopID();
}

void
KernelMetricTable::ApplyFilters()
{
    // Validate each filter before applying
    for(size_t i = 0; i < m_pending_column_filters.size(); i++)
    {
        const ColumnFilter& filter = m_pending_column_filters[i];
        if(!filter.is_active || strlen(filter.filter_text) == 0)
            continue;

        bool is_numeric_column = (i != NAME_COLUMN_INDEX);  // Name Column is text, others are numeric

        if(!ValidateFilterExpression(filter.filter_text, is_numeric_column))
        {
            spdlog::warn("Invalid filter expression for column {}: {}", i, filter.filter_text);
            NotificationManager::GetInstance().Show(
                "Invalid filter expression: " + std::string(filter.filter_text),
                NotificationLevel::Error);

            return;  // Don't apply invalid filters
        }
    }

    m_column_filters = m_pending_column_filters;
    m_fetch_requested = true;
}

void
KernelMetricTable::ClearAllFilters()
{
    size_t total_columns = PERMANENT_COLUMN_COUNT + m_metrics_params.size();
    m_pending_column_filters.assign(total_columns, ColumnFilter());
    m_column_filters.assign(total_columns, ColumnFilter());
    m_fetch_requested = true;
}

void
KernelMetricTable::ComputeColumnMaxValues(
    const std::vector<std::vector<std::string>>& data)
{
    m_column_max_values.clear();
    if(data.empty() || m_bar_chart_columns.empty())
        return;

    for(int col : m_bar_chart_columns)
    {
        double max_val = 0.0;
        for(const auto& row : data)
        {
            if(col >= static_cast<int>(row.size()) || row[col].empty())
                continue;
            char*  end = nullptr;
            double val = std::strtod(row[col].c_str(), &end);
            if(end != row[col].c_str())
                max_val = std::max(max_val, std::abs(val));
        }
        if(max_val > 0.0)
            m_column_max_values[col] = max_val;
    }
}

void
KernelMetricTable::RenderBarChartContextMenu(int col)
{
    if(col == ID_COLUMN_INDEX || col == NAME_COLUMN_INDEX)
        return;

    ImGui::PushID(col + 10000);
    if(ImGui::BeginPopupContextItem("##bar_ctx"))
    {
        bool has_bars = m_bar_chart_columns.count(col) > 0;
        if(ImGui::MenuItem(has_bars ? "Hide Bar Chart" : "Show Bar Chart"))
        {
            if(has_bars)
            {
                m_bar_chart_columns.erase(col);
                m_column_max_values.erase(col);
            }
            else
            {
                m_bar_chart_columns.insert(col);
                ComputeColumnMaxValues(
                    m_data_provider.ComputeModel()
                        .GetKernelSelectionTable()
                        .GetTableData());
            }
        }
        ImGui::EndPopup();
    }
    ImGui::PopID();
}

bool
KernelMetricTable::ValidateFilterExpression(const char* expr, bool is_numeric_column)
{
    std::string trimmed(expr);
    // Trim whitespace
    trimmed.erase(0, trimmed.find_first_not_of(" \t\n\r"));
    trimmed.erase(trimmed.find_last_not_of(" \t\n\r") + 1);

    if(trimmed.empty()) return true;

    if(is_numeric_column)
    {
        // Must be: operator + number, or just number
        // Check for operators followed by numbers
        static const std::vector<std::string> ops = {">=", "<=", "!=", "<>", ">", "<", "="};
        for(const auto& op : ops)
        {
            if(trimmed.find(op) == 0)
            {
                std::string value = trimmed.substr(op.length());
                // Trim value
                value.erase(0, value.find_first_not_of(" \t\n\r"));
                value.erase(value.find_last_not_of(" \t\n\r") + 1);
                // Check if value is numeric
                if(!value.empty() && (std::isdigit(value[0]) || value[0] == '-' || value[0] == '.'))
                {
                    return true;
                }
                return false;
            }
        }
        // If no operator, check if it's just a number
        return !trimmed.empty() && (std::isdigit(trimmed[0]) || trimmed[0] == '-' || trimmed[0] == '.');
    }
    else
    {
        // Text column: LIKE operator required
        auto starts_with_like = [](const std::string& str) {
            if(str.length() < 4) return false;
            std::string prefix = str.substr(0, 4);
            std::transform(prefix.begin(), prefix.end(), prefix.begin(), ::tolower);
            return prefix == "like";
        };
        return starts_with_like(trimmed);
    }
}

}  // namespace View
}  // namespace RocProfVis
