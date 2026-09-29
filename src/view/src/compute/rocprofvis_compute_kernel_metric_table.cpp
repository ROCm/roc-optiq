// Copyright Advanced Micro Devices, Inc.
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
constexpr float COL_NAME_MIN_CHARS        = 12.0f;
constexpr float FROZEN_NAME_MAX_SHARE     = 0.5f;
constexpr float COL_DEFAULT_CHAR_LIMIT    = 30.0f;
constexpr float COL_INVOCATION_CHAR_LIMIT = COL_FILTER_CHAR_LIMIT;

constexpr float kTooltipMaxWidth = 600.0f;

constexpr const char* JSON_KEY_SELECTION            = "selection";
constexpr const char* JSON_KEY_SELECTION_ID         = "id";
constexpr const char* JSON_KEY_SELECTION_NAME       = "name";
constexpr const char* JSON_KEY_SELECTION_VALUE_NAME = "value";

constexpr const char* CELL_CONTEXT_MENU_ID = "##kernel_table_cell_menu";
// Tint of the compare target's row.
constexpr float MARKED_ROW_ALPHA = 0.25f;

// Kernel id of a row's ID cell. strtoul, not stoul: a non-numeric cell must not
// throw (project rule: no C++ exceptions).
static uint32_t
ParseKernelId(const std::string& cell)
{
    if(cell.empty())
    {
        return ComputeSelection::INVALID_SELECTION_ID;
    }
    char*               parse_end = nullptr;
    const unsigned long value     = std::strtoul(cell.c_str(), &parse_end, 10);
    return parse_end != cell.c_str() ? static_cast<uint32_t>(value)
                                     : ComputeSelection::INVALID_SELECTION_ID;
}

void
KernelMetricTable::SetCompareCallback(
    std::function<void(uint32_t workload_id, uint32_t kernel_id)> callback)
{
    m_compare_callback = std::move(callback);
}

void
KernelMetricTable::SetMarkedKernel(uint32_t workload_id, uint32_t kernel_id)
{
    m_marked_workload_id = workload_id;
    m_marked_kernel_id   = kernel_id;
}

bool
KernelMetricTable::ShowsWorkloadColumn() const
{
    return m_data_provider.ComputeModel().GetWorkloadList().size() > 1;
}

bool
KernelMetricTable::IsDefaultSort() const
{
    return m_sort_column_index == DURATION_COLUMN_INDEX &&
           m_sort_order == kRPVControllerSortOrderDescending;
}

bool
KernelMetricTable::IsSortAscending() const
{
    return m_sort_order == kRPVControllerSortOrderAscending;
}

std::string
KernelMetricTable::GetSortColumnName() const
{
    if(m_sort_column_index == ID_COLUMN_INDEX && ShowsWorkloadColumn())
    {
        return "Workload";
    }
    if(m_sort_column_index >= 0 && m_sort_column_index < PERMANENT_COLUMN_COUNT)
    {
        return m_permanent_column_names[m_sort_column_index];
    }
    const int metric = m_sort_column_index - PERMANENT_COLUMN_COUNT;
    if(metric >= 0 && metric < static_cast<int>(m_metrics_column_names.size()))
    {
        return m_metrics_column_names[metric];
    }
    return std::string();
}

size_t
KernelMetricTable::GetActiveFilterCount() const
{
    size_t count = 0;
    for(const ColumnFilter& filter : m_column_filters)
    {
        if(filter.is_active && strlen(filter.filter_text) > 0)
        {
            count++;
        }
    }
    return count;
}

KernelMetricTable::KernelMetricTable(DataProvider&                     data_provider,
                                     std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_fetch_requested(false)
, m_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_selected_workload_id_local(ComputeSelection::INVALID_SELECTION_ID)
, m_marked_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_marked_kernel_id(ComputeSelection::INVALID_SELECTION_ID)
, m_sort_column_index(DURATION_COLUMN_INDEX)
, m_sort_order(kRPVControllerSortOrderDescending)
, m_selected_row(-1)
, m_compute_selection(compute_selection)
, m_selected_kernel_id_local(ComputeSelection::INVALID_SELECTION_ID)
, m_show_kernel_table(true)
, m_update_table_selection(false)
, m_allow_deselect(false)
, m_permanent_column_names({ "ID", "Name", "Duration (ns)", "Invocations" })
{
    m_widget_name = GenUniqueName("KernelMetricTable");
    m_preset      = std::make_unique<Preset>(*this);
}

void
KernelMetricTable::ClearData()
{
    m_metrics_info.clear();
    m_metrics_params.clear();
    m_metrics_column_names.clear();
    m_bar_chart_columns.clear();
    ClearAllFilters();
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
    if(m_rows.empty() && !m_cycle_active)
    {
        m_fetch_requested = true;
    }
}

void
KernelMetricTable::StartFetchCycle()
{
    // Metric columns are named from the selection's workload.
    const WorkloadInfo* workload = m_data_provider.ComputeModel().GetWorkload(m_workload_id);
    if(workload)
    {
        for(MetricInfo& metric : m_metrics_info)
        {
            if(const AvailableMetrics::Entry* entry = ComputeDataModel::GetMetricInfo(
                   *workload, metric.entry.category_id, metric.entry.table_id,
                   metric.entry.id))
            {
                metric.entry = *entry;
            }
        }
    }

    m_fetch_workloads.clear();
    for(const WorkloadInfo* candidate : m_data_provider.ComputeModel().GetWorkloadList())
    {
        if(candidate && !candidate->kernels.empty())
        {
            m_fetch_workloads.push_back(candidate->id);
        }
    }
    m_fetch_index = 0;
    m_fetch_next  = false;
    m_pending_header.clear();
    m_pending_rows.clear();
    m_pending_row_workloads.clear();
    m_cycle_active = !m_fetch_workloads.empty();
    if(m_cycle_active)
    {
        RequestWorkloadRows(m_fetch_workloads.front());
    }
}

void
KernelMetricTable::RequestWorkloadRows(uint32_t workload_id)
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
        workload_id, m_metrics_params, m_sort_column_index,
        static_cast<rocprofvis_controller_sort_order_t>(m_sort_order), filter_map);
    spdlog::debug("Requesting kernel selection table for workload {}: column {}, order {}, "
                  "filters {}",
                  workload_id, m_sort_column_index,
                  m_sort_order == kRPVControllerSortOrderAscending ? "ASC" : "DESC",
                  filter_map.size());
    if(!m_data_provider.FetchMetricPivotTable(params))
    {
        m_cycle_active = false;
    }
}

void
KernelMetricTable::HandleNewData(bool success)
{
    if(!m_cycle_active || m_fetch_index >= m_fetch_workloads.size())
    {
        return;
    }
    if(success)
    {
        ComputeKernelSelectionTable& table =
            m_data_provider.ComputeModel().GetKernelSelectionTable();
        m_pending_header = table.GetTableHeader();
        for(const std::vector<std::string>& row : table.GetTableData())
        {
            m_pending_rows.push_back(row);
            m_pending_row_workloads.push_back(m_fetch_workloads[m_fetch_index]);
        }
    }
    ++m_fetch_index;
    if(m_fetch_index < m_fetch_workloads.size())
    {
        m_fetch_next = true;
    }
    else
    {
        FinishFetchCycle();
    }
}

void
KernelMetricTable::FinishFetchCycle()
{
    m_cycle_active = false;

    // Each workload came back sorted; order them together the same way. Values
    // that are not numbers (N/A) go last either way.
    const ComputeDataModel& model     = m_data_provider.ComputeModel();
    const int               column    = m_sort_column_index;
    const bool              ascending = m_sort_order == kRPVControllerSortOrderAscending;
    const bool              workloads = ShowsWorkloadColumn();
    auto workload_name = [&model](uint32_t id) -> std::string {
        const WorkloadInfo* workload = model.GetWorkload(id);
        return workload ? workload->name : std::string();
    };
    std::vector<size_t> order(m_pending_rows.size());
    for(size_t i = 0; i < order.size(); ++i)
    {
        order[i] = i;
    }
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const std::vector<std::string>& row_a = m_pending_rows[a];
        const std::vector<std::string>& row_b = m_pending_rows[b];
        if(column == ID_COLUMN_INDEX && workloads)
        {
            const std::string name_a = workload_name(m_pending_row_workloads[a]);
            const std::string name_b = workload_name(m_pending_row_workloads[b]);
            return ascending ? name_a < name_b : name_a > name_b;
        }
        if(column < 0 || column >= static_cast<int>(row_a.size()) ||
           column >= static_cast<int>(row_b.size()))
        {
            return false;
        }
        const std::string& cell_a = row_a[column];
        const std::string& cell_b = row_b[column];
        if(column == NAME_COLUMN_INDEX)
        {
            return ascending ? cell_a < cell_b : cell_a > cell_b;
        }
        char*        end_a   = nullptr;
        char*        end_b   = nullptr;
        const double value_a = std::strtod(cell_a.c_str(), &end_a);
        const double value_b = std::strtod(cell_b.c_str(), &end_b);
        const bool   number_a = end_a != cell_a.c_str() && !std::isnan(value_a);
        const bool   number_b = end_b != cell_b.c_str() && !std::isnan(value_b);
        if(number_a != number_b)
        {
            return number_a;
        }
        if(!number_a)
        {
            return false;
        }
        return ascending ? value_a < value_b : value_a > value_b;
    });

    m_rows.clear();
    m_row_workloads.clear();
    m_shown_kernels.clear();
    m_rows.reserve(order.size());
    m_row_workloads.reserve(order.size());
    m_shown_kernels.reserve(order.size());
    for(size_t index : order)
    {
        const std::vector<std::string>& row = m_pending_rows[index];
        m_shown_kernels.push_back(
            { m_pending_row_workloads[index],
              row.empty() ? ComputeSelection::INVALID_SELECTION_ID
                          : ParseKernelId(row[ID_COLUMN_INDEX]) });
        m_rows.push_back(std::move(m_pending_rows[index]));
        m_row_workloads.push_back(m_pending_row_workloads[index]);
    }
    m_pending_rows.clear();
    m_pending_row_workloads.clear();
    if(!m_pending_header.empty())
    {
        m_header = m_pending_header;
    }

    // Metric columns are those after the permanent ones.
    m_metrics_column_names.clear();
    ROCPROFVIS_ASSERT(m_metrics_params.size() == m_metrics_info.size());
    const size_t metric_count =
        m_header.size() > PERMANENT_COLUMN_COUNT
            ? std::min(m_header.size() - PERMANENT_COLUMN_COUNT, m_metrics_info.size())
            : 0;
    for(size_t i = 0; i < metric_count; i++)
    {
        m_metrics_column_names.push_back(
            m_metrics_info[i].entry.name + " " + m_metrics_info[i].value_name +
            (m_metrics_info[i].entry.unit.empty()
                 ? ""
                 : " (" + m_metrics_info[i].entry.unit + ")"));
    }

    ComputeColumnMaxValues(m_rows);
    m_update_table_selection = true;
    // A new order or query moves the selected kernel; bring it back into view.
    m_scroll_to_selected = true;
}

void
KernelMetricTable::Update()
{
    bool request_pending =
        m_data_provider.IsRequestPending(DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID);

    // A new query or sort restarts the cycle once the request in flight is done.
    if(!request_pending && m_fetch_requested)
    {
        m_fetch_requested = false;
        StartFetchCycle();
    }
    else if(!request_pending && m_fetch_next)
    {
        m_fetch_next = false;
        if(m_cycle_active && m_fetch_index < m_fetch_workloads.size())
        {
            RequestWorkloadRows(m_fetch_workloads[m_fetch_index]);
        }
    }

    // check if kernel selection has changed and update selection if needed
    const uint32_t selected_kernel_id   = m_compute_selection->GetSelectedKernel();
    const uint32_t selected_workload_id = m_compute_selection->GetSelectedWorkload();

    if(m_selected_kernel_id_local != selected_kernel_id ||
       m_selected_workload_id_local != selected_workload_id)
    {
        m_selected_kernel_id_local   = selected_kernel_id;
        m_selected_workload_id_local = selected_workload_id;
        if(m_selected_kernel_id_local == ComputeSelection::INVALID_SELECTION_ID)
        {
            m_selected_row = -1;
        }
        else
        {
            m_update_table_selection = true;
        }
    }

    if(m_update_table_selection)
    {
        // The selection's row, if a filter has not hidden it.
        m_selected_row             = -1;
        const std::string kernel_id = std::to_string(selected_kernel_id);
        for(size_t row = 0; row < m_rows.size(); row++)
        {
            if(!m_rows[row].empty() && m_row_workloads[row] == selected_workload_id &&
               m_rows[row][ID_COLUMN_INDEX] == kernel_id)
            {
                m_selected_row = static_cast<int>(row);
                break;
            }
        }
        if(m_selected_row < 0)
        {
            m_scroll_to_selected = false;
        }
        m_update_table_selection = false;
    }
}

void
KernelMetricTable::Render()
{
    int remove_index = -1;

    SettingsManager& settings     = SettingsManager::GetInstance();
    ImFont*           icon_font    = settings.GetFontManager().GetFont(FontType::kIcon);
    const ImGuiStyle &style = settings.GetDefaultStyle();
    const float      cell_padding = style.CellPadding.x * 2.0f;
    const float      char_width = ImGui::CalcTextSize("M").x;

    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          settings.GetColor(m_chromeless ? Colors::kTransparent
                                                         : Colors::kBgPanel));
    ImGui::PushStyleColor(ImGuiCol_Border, settings.GetColor(Colors::kBorderColor));
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding,
                        settings.GetDefaultStyle().ChildRounding);
    ImGuiChildFlags card_flags =
        m_chromeless ? ImGuiChildFlags_None
                     : ImGuiChildFlags_Borders | ImGuiChildFlags_AlwaysUseWindowPadding;
    if(!m_fill_parent)
    {
        card_flags |= ImGuiChildFlags_AutoResizeY;
    }
    ImGui::BeginChild("kernel_metric_table_card", ImVec2(0, 0), card_flags);

    if(!m_chromeless)
    {
        SectionTitle("Kernel Selection Table");
    }

    // The merged rows of every workload, replaced only once a fetch cycle is done.
    const std::vector<std::string>&              header = m_header;
    const std::vector<std::vector<std::string>>& data   = m_rows;
    const bool                                   workload_column = ShowsWorkloadColumn();

    // Toolbar row.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, settings.GetColor(Colors::kTransparent));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::BeginChild("toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY);

    ImGui::AlignTextToFramePadding();
    if(m_fill_parent)
    {
        m_show_kernel_table = true;
    }
    else
    {
        const char* icon = m_show_kernel_table ? ICON_CHEVRON_DOWN : ICON_CHEVRON_RIGHT;

        ImGui::PushFont(icon_font, 0.0f);
        ImVec2 icon_size = ImGui::CalcTextSize(ICON_CHEVRON_DOWN); // use larger icon for consistent spacing
        ImGui::PopFont();

        if(IconButton(icon, icon_font,
                      ImVec2(icon_size.x + style.FramePadding.x * 2.0f,
                             icon_size.y + style.FramePadding.y * 2.0f),
                      m_show_kernel_table ? "Hide Table" : "Show Table", false,
                      style.FramePadding,
                      SettingsManager::GetInstance().GetColor(Colors::kTransparent),
                      SettingsManager::GetInstance().GetColor(Colors::kButtonHovered),
                      SettingsManager::GetInstance().GetColor(Colors::kTransparent)))
        {
            m_show_kernel_table = !m_show_kernel_table;
        }

        ImGui::SameLine(); //No spacing on purpose
        ImGui::TextUnformatted("Table");
        VerticalSeparator();
    }

    m_query_builder.SetWorkload(
        m_data_provider.ComputeModel().GetWorkload(m_workload_id));

    ImGui::BeginDisabled(!m_show_kernel_table ||
                         m_workload_id == ComputeSelection::INVALID_SELECTION_ID);
    if(ImGui::Button("Add Metric"))
    {
        m_query_builder.Show([this](const std::string& query) { 
            this->SetQuery(query);
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
    const size_t active_count = GetActiveFilterCount();
    if(active_count > 0)
    {
        ImGui::SameLine(0.0f, style.ItemSpacing.x);
        ImGui::TextDisabled("(%zu active filters)", active_count);
    }

    ImGui::EndDisabled();

    // End toolbar
    ImGui::EndChild();
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    bool request_pending =
        m_data_provider.IsRequestPending(DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID);

    float       row_padding_v = style.CellPadding.y * 2.0f;
    float       line_height   = ImGui::GetTextLineHeight() + row_padding_v;

    // Filter row height (InputText widgets are taller than text)
    float filter_row_height = ImGui::GetFrameHeight() + row_padding_v;

    int data_row_count = static_cast<int>(data.size());
    int rows_to_render = std::max(std::min(10, data_row_count), 5);

    // Calculate total table height: header + filter row + data rows
    float total_table_height = line_height + filter_row_height + (rows_to_render * line_height);

    if(m_show_kernel_table)
    {
    // Fixed height from the row count, or the rest of the pane in fill mode.
    if(ImGui::BeginChild("kernel_metric_table_cont",
                         ImVec2(0, m_fill_parent ? 0.0f : total_table_height),
                         ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
    {
        if(!header.empty() && !data.empty() && m_workload_id != ComputeSelection::INVALID_SELECTION_ID)
        {
            // Stays sortable while a fetch runs: dropping the flag would make ImGui
            // rebuild the sort specs, losing the chosen sort.
            ImGuiTableFlags table_flags =
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings |
                ImGuiTableFlags_Sortable;

            int column_count = static_cast<int>(header.size());
            ImVec2 outer_size = ImVec2(ImGui::GetContentRegionAvail());

            if(ImGui::BeginTable("kernel_selection_table", column_count, table_flags,
                                 outer_size))
            {
                // Freeze the name (and workload) columns and the header + filter rows.
                ImGui::TableSetupScrollFreeze(workload_column ? 2 : 1, 2);

                // Calculate minimum column widths based on character counts. The
                // frozen name leaves at least half the table for the columns that
                // scroll past it.
                float name_min_width = std::min(char_width * COL_NAME_CHAR_LIMIT,
                                                std::max(outer_size.x * FROZEN_NAME_MAX_SHARE,
                                                         char_width * COL_NAME_MIN_CHARS));
                float default_min_width = char_width * COL_DEFAULT_CHAR_LIMIT;
                float invocation_min_width = char_width * COL_INVOCATION_CHAR_LIMIT;

                for(int col = 0; col < column_count; col++)
                {
                    ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_WidthFixed;
                    // ImGui rebuilds the sort specs whenever the columns change (a
                    // metric added or removed), so the current sort is the default.
                    if(col == m_sort_column_index)
                    {
                        col_flags |= ImGuiTableColumnFlags_DefaultSort |
                                     (m_sort_order == kRPVControllerSortOrderAscending
                                          ? ImGuiTableColumnFlags_PreferSortAscending
                                          : ImGuiTableColumnFlags_PreferSortDescending);
                    }
                    else
                    {
                        col_flags |= ImGuiTableColumnFlags_PreferSortDescending;
                    }
                    if(col < PERMANENT_COLUMN_COUNT)
                    {
                        // The id column shows each row's workload when there are several.
                        const bool shows_workload = col == ID_COLUMN_INDEX && workload_column;
                        if(!shows_workload && !header[col].empty() && header[col][0] == '_')
                        {
                            col_flags |= ImGuiTableColumnFlags_DefaultHide |
                                        ImGuiTableColumnFlags_Disabled;
                        }

                        // Set minimum width based on column type
                        float min_width = default_min_width;
                        if(shows_workload)
                        {
                            // Frozen with the name, so only as wide as its longest
                            // workload name.
                            min_width = ImGui::CalcTextSize("Workload").x;
                            for(const WorkloadInfo* workload :
                                m_data_provider.ComputeModel().GetWorkloadList())
                            {
                                if(workload)
                                {
                                    min_width = std::max(
                                        min_width, ImGui::CalcTextSize(workload->name.c_str()).x);
                                }
                            }
                            min_width += cell_padding + char_width;
                        }
                        else if(col == NAME_COLUMN_INDEX)
                        {
                            min_width = name_min_width;
                        }
                        else if(col == INVOCATION_COLUMN_INDEX)
                        {
                            min_width = invocation_min_width;
                        }

                        ImGui::TableSetupColumn(shows_workload
                                                    ? "Workload"
                                                    : m_permanent_column_names[col].c_str(),
                                                col_flags, min_width);
                    }
                    else
                    {
                        int index = col - PERMANENT_COLUMN_COUNT;
                        
                        // Since render reads directly from the data model, the
                        // m_metrics_column_names may not be synced for a few frames
                        if(index < static_cast<int>(m_metrics_column_names.size()))
                        {
                            // Calculate width based on name + padding for close button
                            float column_size = static_cast<float>(
                                ImGui::CalcTextSize(m_metrics_column_names[index].c_str()).x +
                                cell_padding + char_width * 2.0);

                            column_size = std::max(column_size, default_min_width);

                            ImGui::TableSetupColumn(m_metrics_column_names[index].c_str(),
                                                    col_flags, column_size);
                        }
                        else
                        {
                            ImGui::TableSetupColumn(
                                ("Metric " + std::to_string(index + 1)).c_str(), col_flags,
                                default_min_width);
                        }
                    }
                }

                // Custom header row with hover detection and X button
                ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
                for(int col = 0; col < column_count; col++)
                {
                    ImGui::TableSetColumnIndex(col);

                    // Headers are keyed by column as in ImGui::TableHeadersRow, so
                    // columns of one name stay apart.
                    // Skip X for non-removable columns (like ID, Name)
                    if(col < PERMANENT_COLUMN_COUNT)
                    {
                        ImGui::PushID(col);
                        ImGui::TableHeader(ImGui::TableGetColumnName(col));
                        ImGui::PopID();
                        RenderBarChartContextMenu(col);
                        continue;
                    }

                    // Sortable header with X button
                    const char* name = ImGui::TableGetColumnName(col);
                    ImGui::PushID(col);
                    ImGui::TableHeader(name);
                    ImGui::PopID();
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

                // Filter row. The workload column has no filter: its data is the id.
                ImGui::TableNextRow();
                for(int col = 0; col < column_count; col++)
                {
                    if(!ImGui::TableSetColumnIndex(col) ||
                       (col == ID_COLUMN_INDEX && workload_column))
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

                ImGui::PushStyleColor(ImGuiCol_Header,
                                      settings.GetColor(Colors::kSelection));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                      settings.GetColor(Colors::kHighlightChart));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                                      settings.GetColor(Colors::kHighlightChart));

                if(request_pending)
                {
                    ImGui::BeginDisabled();
                }

                bool open_menu = false;

                ImGuiListClipper clipper;
                clipper.Begin(static_cast<int>(data.size()));
                // The selection's row is submitted even when clipped, to scroll to it.
                const bool scroll_to_selected =
                    m_scroll_to_selected && !m_update_table_selection && m_selected_row >= 0 &&
                    m_selected_row < static_cast<int>(data.size());
                if(scroll_to_selected)
                {
                    clipper.IncludeItemByIndex(m_selected_row);
                }
                while(clipper.Step())
                {
                    for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
                    {
                        ImGui::TableNextRow();
                        ImGui::PushID(row);  // Push row ID for unique identification

                        bool is_selected       = (m_selected_row == row);
                        bool selectable_placed = false;
                        const uint32_t row_workload = m_row_workloads[row];
                        if(is_selected && scroll_to_selected)
                        {
                            ImGui::SetScrollHereY(0.5f);
                            m_scroll_to_selected = false;
                        }
                        // The compare target's row.
                        if(row_workload == m_marked_workload_id &&
                           data[row][ID_COLUMN_INDEX] == std::to_string(m_marked_kernel_id))
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_RowBg1,
                                ApplyAlpha(settings.GetColor(Colors::kComparisonTarget),
                                           MARKED_ROW_ALPHA));
                        }
                        std::string workload_label;
                        if(workload_column)
                        {
                            const WorkloadInfo* workload =
                                m_data_provider.ComputeModel().GetWorkload(row_workload);
                            workload_label = workload ? workload->name : std::string();
                        }

                        for(int col = 0; col < data[row].size(); col++)
                        {
                            const std::string& cell = col == ID_COLUMN_INDEX && workload_column
                                                          ? workload_label
                                                          : data[row][col];
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
                                        m_selected_row               = row;
                                        m_selected_kernel_id_local   = ParseKernelId(data[row][0]);
                                        m_selected_workload_id_local = row_workload;

                                        m_compute_selection->Select(row_workload,
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
                            bool cell_hovered =
                                ImGui::IsWindowHovered() &&
                                ImGui::IsMouseHoveringRect(cell_min, cell_max, true);
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

                            // Resolve the column from the cell rect; the row-spanning
                            // selectable and frozen column make item hover unreliable.
                            if(cell_hovered &&
                               ImGui::IsMouseClicked(ImGuiMouseButton_Right))
                            {
                                m_cell_menu.row    = row;
                                m_cell_menu.column = col;
                                open_menu          = true;
                            }
                        }
                        ImGui::PopID();  // Pop row ID
                    }
                }

                if(request_pending)
                {
                    ImGui::EndDisabled();
                }

                if(open_menu)
                {
                    ImGui::OpenPopup(CELL_CONTEXT_MENU_ID);
                }
                if(BeginCellContextMenu(CELL_CONTEXT_MENU_ID))
                {
                    if(m_cell_menu.row >= 0 &&
                       m_cell_menu.row < static_cast<int>(data.size()))
                    {
                        const std::vector<std::string>& menu_row = data[m_cell_menu.row];
                        std::vector<std::string>        row_cells;
                        row_cells.reserve(header.size());
                        // Hidden columns have a header name starting with '_'. Remap
                        // the data-column index to its visible-list position.
                        int cell_index = 0;
                        for(int col = 0; col < static_cast<int>(header.size()) &&
                                         col < static_cast<int>(menu_row.size());
                            col++)
                        {
                            if(!header[col].empty() && header[col][0] == '_')
                            {
                                continue;
                            }
                            if(col == m_cell_menu.column)
                            {
                                cell_index = static_cast<int>(row_cells.size());
                            }
                            row_cells.push_back(menu_row[col].empty() ? "N/A"
                                                                      : menu_row[col]);
                        }
                        AddCopyRowCellMenuItems(row_cells.data(),
                                                static_cast<int>(row_cells.size()),
                                                cell_index);
                        if(m_compare_callback)
                        {
                            const uint32_t kernel_id   = ParseKernelId(menu_row[0]);
                            const uint32_t workload_id = m_row_workloads[m_cell_menu.row];
                            const bool     is_a =
                                workload_id == m_compute_selection->GetSelectedWorkload() &&
                                kernel_id == m_compute_selection->GetSelectedKernel();
                            ImGui::Separator();
                            if(ImGui::MenuItem(
                                   "Compare with this kernel (B)", nullptr, false,
                                   kernel_id != ComputeSelection::INVALID_SELECTION_ID &&
                                       !is_a))
                            {
                                m_compare_callback(workload_id, kernel_id);
                            }
                        }
                    }
                    EndCellContextMenu();
                }

                ImGui::PopStyleColor(3);
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

        // The sort follows its column; sorting by the removed one falls back to
        // the default.
        if(m_sort_column_index == removed_col)
        {
            m_sort_column_index = DURATION_COLUMN_INDEX;
            m_sort_order        = kRPVControllerSortOrderDescending;
        }
        else if(m_sort_column_index > removed_col)
        {
            m_sort_column_index--;
        }

        m_fetch_requested = true;
        spdlog::debug("Removed metric column at index {}", remove_index);
    }
    }

    ImGui::EndChild();  // kernel_metric_table_card
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    m_query_builder.Render();
}

void
KernelMetricTable::SetQuery(const std::string& query)
{
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
  
    const AvailableMetrics::Entry* entry = m_query_builder.GetSelectedMetricInfo();
    AppendMetricQuery(query, entry ? *entry : AvailableMetrics::Entry(),
                      m_query_builder.GetValueName());
}

void
KernelMetricTable::SetExternalQuery(MetricId metric_id, const std::string& value_name)
{
    auto query = metric_id.ToString() + ":" + value_name;
    auto workload = m_data_provider.ComputeModel().GetWorkload(m_workload_id);
    if(!workload)
        return;

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

    auto entry = ComputeDataModel::GetMetricInfo(*workload, metric_id.category_id,
                                                 metric_id.table_id, metric_id.entry_id);
    if(!entry)
        return;

    AppendMetricQuery(query, *entry, value_name);
}

void
KernelMetricTable::AppendMetricQuery(const std::string& query,
                                     const AvailableMetrics::Entry& entry,
                                     const std::string& value_name)
{
    m_metrics_params.push_back(query);
    m_metrics_info.emplace_back(MetricInfo{ entry, value_name });

    // Add filter slot for the new metric column.
    m_column_filters.emplace_back(ColumnFilter());
    m_pending_column_filters.emplace_back(ColumnFilter());
  
    if(!m_bar_chart_columns.empty())
    {
        int new_col = PERMANENT_COLUMN_COUNT +
                      static_cast<int>(m_metrics_params.size()) - 1;
        m_bar_chart_columns.insert(new_col);
    }

    m_fetch_requested = true;
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
        if(IconMenuItem(ICON_CHART_BAR, has_bars ? "Hide Bar Chart" : "Show Bar Chart"))
        {
            if(has_bars)
            {
                m_bar_chart_columns.erase(col);
                m_column_max_values.erase(col);
            }
            else
            {
                m_bar_chart_columns.insert(col);
                ComputeColumnMaxValues(m_rows);
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
        // Must be: operator + number
        // Check for operators followed by numbers
        static const std::vector<std::string> ops = {">=", "<=", "!=", ">", "<", "="};
        for(const auto& op : ops)
        {
            if(trimmed.find(op) == 0)
            {
                std::string value = trimmed.substr(op.length());
                // Trim value
                value.erase(0, value.find_first_not_of(" \t\n\r"));
                value.erase(value.find_last_not_of(" \t\n\r") + 1);
                // Check if value is numeric
                if(!value.empty() && (std::isdigit(value[0]) || value[0] == '.'))
                {
                    return true;
                }
                return false;
            }
        }
        return false;
    }
    else
    {
        // Text column: LIKE operator required
        auto starts_with_like = [](const std::string& str) {
            if(str.length() < 4) return false;
            std::string prefix = str.substr(0, 4);
            std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                [](unsigned char c) -> char { return static_cast<char>(std::tolower(c)); });
            return prefix == "like";
        };
        return starts_with_like(trimmed);
    }
}

KernelMetricTable::Preset::Preset(KernelMetricTable& widget)
: PresetComponent(PresetManager::ComputeKernelMetricTable,
                  widget.m_data_provider.GetTraceFilePath())
, m_widget(widget)
{}

bool
KernelMetricTable::Preset::ToJson(jt::Json& json)
{
    if(!m_widget.m_metrics_info.empty())
    {
        jt::Json& selection = json[JSON_KEY_SELECTION];
        for(size_t i = 0; i < m_widget.m_metrics_info.size(); i++)
        {
            const MetricInfo& info                      = m_widget.m_metrics_info[i];
            selection[i][JSON_KEY_SELECTION_ID][0]      = info.entry.category_id;
            selection[i][JSON_KEY_SELECTION_ID][1]      = info.entry.table_id;
            selection[i][JSON_KEY_SELECTION_ID][2]      = info.entry.id;
            selection[i][JSON_KEY_SELECTION_NAME]       = info.entry.name;
            selection[i][JSON_KEY_SELECTION_VALUE_NAME] = info.value_name;
        }
    }
    return true;
}

bool
KernelMetricTable::Preset::FromJson(jt::Json& json)
{
    bool result = true;
    if(json.isObject() && json.contains(JSON_KEY_SELECTION))
    {
        jt::Json& selection = json[JSON_KEY_SELECTION];
        if(selection.isArray())
        {
            for(jt::Json& obj : selection.getArray())
            {
                result &= obj.isObject() && obj.contains(JSON_KEY_SELECTION_ID) &&
                          obj.contains(JSON_KEY_SELECTION_NAME) &&
                          obj.contains(JSON_KEY_SELECTION_VALUE_NAME) &&
                          obj[JSON_KEY_SELECTION_ID].isArray() &&
                          obj[JSON_KEY_SELECTION_ID].getArray().size() == 3 &&
                          obj[JSON_KEY_SELECTION_ID].getArray()[0].isLong() &&
                          obj[JSON_KEY_SELECTION_ID].getArray()[1].isLong() &&
                          obj[JSON_KEY_SELECTION_ID].getArray()[2].isLong() &&
                          obj[JSON_KEY_SELECTION_NAME].isString() &&
                          obj[JSON_KEY_SELECTION_VALUE_NAME].isString();
            }
            if(result)
            {
                const WorkloadInfo* workload =
                    m_widget.m_data_provider.ComputeModel().GetWorkload(
                        m_widget.m_workload_id);
                result = workload;
                if(result)
                {
                    Reset();
                    for(jt::Json& obj : selection.getArray())
                    {
                        uint32_t category_id = static_cast<uint32_t>(
                            obj[JSON_KEY_SELECTION_ID].getArray()[0].getLong());
                        uint32_t table_id = static_cast<uint32_t>(
                            obj[JSON_KEY_SELECTION_ID].getArray()[1].getLong());
                        uint32_t id = static_cast<uint32_t>(
                            obj[JSON_KEY_SELECTION_ID].getArray()[2].getLong());
                        std::string& name = obj[JSON_KEY_SELECTION_NAME].getString();
                        std::string& value_name =
                            obj[JSON_KEY_SELECTION_VALUE_NAME].getString();
                        AvailableMetrics::Entry entry;
                        if(workload->available_metrics.tree.count(category_id) > 0 &&
                           workload->available_metrics.tree.at(category_id)
                                   .tables.count(table_id) > 0 &&
                           workload->available_metrics.tree.at(category_id)
                                   .tables.at(table_id)
                                   .entries.count(id) > 0 &&
                           workload->available_metrics.tree.at(category_id)
                                   .tables.at(table_id)
                                   .entries.at(id)
                                   .name == name)
                        {
                            entry = workload->available_metrics.tree.at(category_id)
                                        .tables.at(table_id)
                                        .entries.at(id);
                        }
                        else
                        {
                            entry.category_id = category_id;
                            entry.table_id    = table_id;
                            entry.id          = id;
                            entry.name        = name;
                        }
                        m_widget.AppendMetricQuery(
                            std::to_string(category_id) + "." + std::to_string(table_id) +
                                "." + std::to_string(id) + ":" + value_name,
                            std::move(entry), value_name);
                    }
                }
            }
        }
    }
    return result;
}

void
KernelMetricTable::Preset::Reset()
{
    m_widget.ClearData();
    m_widget.m_fetch_requested = true;
}

}  // namespace View
}  // namespace RocProfVis
