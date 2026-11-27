// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_metric_table.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_core_assert.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_settings_manager.h"
#include "icons/rocprovfis_icon_defines.h"
#include "widgets/rocprofvis_compute_widget.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "rocprofvis_common_defs.h"
#include "widgets/rocprofvis_notification_manager.h"

#include "imgui.h"
#include "spdlog/spdlog.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numeric>

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

// Rows a fixed-height (not fill-parent) table is sized for.
constexpr int TABLE_MIN_FIXED_ROWS = 5;
constexpr int TABLE_MAX_FIXED_ROWS = 10;

constexpr float kTooltipMaxWidth = 600.0f;
// Templated kernel names run to thousands of characters; a tooltip shows this many.
constexpr size_t TOOLTIP_MAX_CHARS = 400;

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

// A tooltip wrapped at kTooltipMaxWidth.
static void
WrappedTooltip(const std::string& text)
{
    ImGui::SetNextWindowSizeConstraints(ImVec2(0, 0), ImVec2(kTooltipMaxWidth, FLT_MAX));
    BeginTooltipStyled();
    ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + kTooltipMaxWidth);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    EndTooltipStyled();
}

// The pivot query's name for `entry`'s value `value_name`, "category.table.entry"
// then ":value" when there is one.
static std::string
MetricQuery(const AvailableMetrics::Entry& entry, const std::string& value_name)
{
    return MetricId{ entry.category_id, entry.table_id, entry.id }.ToString() +
           (value_name.empty() ? "" : ":" + value_name);
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

const std::string&
KernelMetricTable::WorkloadName(uint32_t workload_id) const
{
    static const std::string NO_NAME;
    const WorkloadInfo*      workload =
        m_data_provider.ComputeModel().GetWorkload(workload_id);
    return workload ? workload->name : NO_NAME;
}

size_t
KernelMetricTable::GetActiveFilterCount() const
{
    size_t count = 0;
    for(const ColumnFilter& filter : m_column_filters)
    {
        if(filter.IsSet())
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
, m_sort_column_index(DURATION_COLUMN_INDEX)
, m_sort_order(kRPVControllerSortOrderDescending)
, m_selected_row(-1)
, m_selected_workload_id_local(ComputeSelection::INVALID_SELECTION_ID)
, m_marked_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_marked_kernel_id(ComputeSelection::INVALID_SELECTION_ID)
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
        ROCPROFVIS_ASSERT(m_metrics_params.size() == m_metrics_info.size());
        for(size_t i = 0; i < m_metrics_info.size(); ++i)
        {
            MetricInfo&                    metric = m_metrics_info[i];
            const AvailableMetrics::Entry* entry =
                metric.entry.name.empty()
                    ? ComputeDataModel::GetMetricInfo(*workload, metric.entry.category_id,
                                                      metric.entry.table_id, metric.entry.id)
                    : MatchMetricEntry(FindMetricTable(workload, metric.entry.category_id,
                                                       metric.entry.table_id),
                                       metric.entry);
            if(entry)
            {
                metric.entry        = *entry;
                m_metrics_params[i] = MetricQuery(*entry, metric.value_name);
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
    m_fetch_index       = 0;
    m_fetch_failures    = 0;
    m_fetch_successes   = 0;
    m_fetch_error_shown = false;
    m_fetch_next        = false;
    m_pending_header.clear();
    m_pending_rows.clear();
    m_pending_row_workloads.clear();
    m_cycle_active = !m_fetch_workloads.empty();
    if(m_cycle_active)
    {
        RequestWorkloadRows();
    }
}

bool
KernelMetricTable::BuildWorkloadQuery(const WorkloadInfo&                        workload,
                                      std::vector<std::string>&                  metrics,
                                      std::unordered_map<uint64_t, std::string>& filters,
                                      int&                                       sort_column)
{
    ROCPROFVIS_ASSERT(m_metrics_params.size() == m_metrics_info.size());
    m_fetch_metric_present.assign(m_metrics_info.size(), false);
    // The column each picked metric takes in this workload's response; -1 if none.
    std::vector<int> response_column(m_metrics_info.size(), -1);
    for(size_t i = 0; i < m_metrics_info.size(); ++i)
    {
        const MetricInfo& info = m_metrics_info[i];
        // A metric typed in without a known entry can only be queried by its id.
        if(info.entry.name.empty())
        {
            response_column[i] = PERMANENT_COLUMN_COUNT + static_cast<int>(metrics.size());
            metrics.push_back(m_metrics_params[i]);
            m_fetch_metric_present[i] = true;
            continue;
        }
        const AvailableMetrics::Table* table =
            FindMetricTable(&workload, info.entry.category_id, info.entry.table_id);
        const AvailableMetrics::Entry* entry = MatchMetricEntry(table, info.entry);
        const bool has_value =
            entry && (info.value_name.empty() || table->value_names.empty() ||
                      std::find(table->value_names.begin(), table->value_names.end(),
                                info.value_name) != table->value_names.end());
        if(!has_value)
        {
            continue;
        }
        response_column[i] = PERMANENT_COLUMN_COUNT + static_cast<int>(metrics.size());
        metrics.push_back(MetricQuery(*entry, info.value_name));
        m_fetch_metric_present[i] = true;
    }

    for(size_t i = 0; i < m_column_filters.size(); ++i)
    {
        if(!m_column_filters[i].IsSet())
        {
            continue;
        }
        int column = static_cast<int>(i);
        if(column >= PERMANENT_COLUMN_COUNT)
        {
            const size_t metric = static_cast<size_t>(column - PERMANENT_COLUMN_COUNT);
            column = metric < response_column.size() ? response_column[metric] : -1;
            if(column < 0)
            {
                return false;
            }
        }
        filters[static_cast<uint64_t>(column)] = m_column_filters[i].filter_text;
    }

    sort_column = m_sort_column_index;
    if(sort_column >= PERMANENT_COLUMN_COUNT)
    {
        const size_t metric = static_cast<size_t>(sort_column - PERMANENT_COLUMN_COUNT);
        sort_column = metric < response_column.size() ? response_column[metric] : -1;
        // FinishFetchCycle orders every workload's rows together; this workload's
        // only need some order.
        if(sort_column < 0)
        {
            sort_column = DURATION_COLUMN_INDEX;
        }
    }
    return true;
}

void
KernelMetricTable::RequestWorkloadRows()
{
    for(; m_fetch_index < m_fetch_workloads.size(); ++m_fetch_index)
    {
        const uint32_t      workload_id = m_fetch_workloads[m_fetch_index];
        const WorkloadInfo* workload =
            m_data_provider.ComputeModel().GetWorkload(workload_id);
        std::vector<std::string>                  metrics;
        std::unordered_map<uint64_t, std::string> filters;
        int                                       sort_column = DURATION_COLUMN_INDEX;
        if(!workload || !BuildWorkloadQuery(*workload, metrics, filters, sort_column))
        {
            continue;
        }

        ComputeTableRequestParams params(
            workload_id, metrics, static_cast<uint64_t>(sort_column),
            static_cast<rocprofvis_controller_sort_order_t>(m_sort_order), filters);
        spdlog::debug("Requesting kernel selection table for workload {}: column {}, "
                      "order {}, filters {}",
                      workload_id, sort_column,
                      m_sort_order == kRPVControllerSortOrderAscending ? "ASC" : "DESC",
                      filters.size());
        if(m_data_provider.FetchMetricPivotTable(params))
        {
            m_response_outstanding = true;
        }
        else
        {
            m_cycle_active = false;
        }
        return;
    }
    EndFetchCycle();
}

void
KernelMetricTable::HandleNewData(bool success, const std::string& error_message)
{
    m_response_outstanding = false;
    // A new query or sort is waiting: these rows answer the old one.
    if(m_fetch_requested || !m_cycle_active || m_fetch_index >= m_fetch_workloads.size())
    {
        return;
    }
    // An invalid filter succeeds with no rows and a message. One query runs per
    // workload, so each of them reports it; show it once.
    if(!m_fetch_error_shown && !error_message.empty())
    {
        NotificationManager::GetInstance().Show(error_message, NotificationLevel::Error);
        m_fetch_error_shown = true;
    }
    if(!success)
    {
        ++m_fetch_failures;
    }
    else
    {
        ++m_fetch_successes;
        // Back to a column per picked metric, empty for those left out.
        const std::vector<bool>& present = m_fetch_metric_present;
        auto with_every_metric = [&present](const std::vector<std::string>& cells) {
            std::vector<std::string> full(cells.begin(),
                                          cells.begin() + std::min(cells.size(),
                                                                   static_cast<size_t>(
                                                                       PERMANENT_COLUMN_COUNT)));
            size_t next = full.size();
            for(bool queried : present)
            {
                full.push_back(queried && next < cells.size() ? cells[next++] : std::string());
            }
            return full;
        };
        ComputeKernelSelectionTable& table =
            m_data_provider.ComputeModel().GetKernelSelectionTable();
        const std::vector<std::vector<std::string>>& rows = table.GetTableData();
        m_pending_header = with_every_metric(table.GetTableHeader());
        m_pending_rows.reserve(m_pending_rows.size() + rows.size());
        for(const std::vector<std::string>& row : rows)
        {
            m_pending_rows.push_back(with_every_metric(row));
        }
        m_pending_row_workloads.insert(m_pending_row_workloads.end(), rows.size(),
                                       m_fetch_workloads[m_fetch_index]);
    }
    ++m_fetch_index;
    if(m_fetch_index < m_fetch_workloads.size())
    {
        m_fetch_next = true;
    }
    else
    {
        EndFetchCycle();
    }
}

void
KernelMetricTable::EndFetchCycle()
{
    // Every workload asked failed: keep the rows shown. Workloads skipped by
    // the filters add none.
    if(m_fetch_failures > 0 && m_fetch_successes == 0)
    {
        m_cycle_active = false;
        m_pending_header.clear();
        m_pending_rows.clear();
        m_pending_row_workloads.clear();
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
    const int           column    = m_sort_column_index;
    const bool          ascending = m_sort_order == kRPVControllerSortOrderAscending;
    const bool          workloads = ShowsWorkloadColumn();
    std::vector<size_t> order(m_pending_rows.size());
    std::iota(order.begin(), order.end(), static_cast<size_t>(0));
    std::stable_sort(order.begin(), order.end(), [&](size_t a, size_t b) {
        const std::vector<std::string>& row_a = m_pending_rows[a];
        const std::vector<std::string>& row_b = m_pending_rows[b];
        if(column == ID_COLUMN_INDEX && workloads)
        {
            const std::string& name_a = WorkloadName(m_pending_row_workloads[a]);
            const std::string& name_b = WorkloadName(m_pending_row_workloads[b]);
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
    m_rows.reserve(order.size());
    m_row_workloads.reserve(order.size());
    for(size_t index : order)
    {
        m_rows.push_back(std::move(m_pending_rows[index]));
        m_row_workloads.push_back(m_pending_row_workloads[index]);
    }
    m_pending_rows.clear();
    m_pending_row_workloads.clear();
    if(!m_pending_header.empty())
    {
        m_header = std::move(m_pending_header);
        m_pending_header.clear();
    }
    else if(!m_header.empty())
    {
        // The filters skipped every workload; the columns still follow the picks.
        m_header.resize(PERMANENT_COLUMN_COUNT + m_metrics_info.size());
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

    // A new query or sort restarts the cycle once the request in flight is done
    // and its response handled.
    if(!request_pending && !m_response_outstanding && m_fetch_requested)
    {
        m_fetch_requested = false;
        StartFetchCycle();
    }
    else if(!request_pending && !m_fetch_requested && m_fetch_next)
    {
        m_fetch_next = false;
        if(m_cycle_active)
        {
            RequestWorkloadRows();
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
    SettingsManager& settings = SettingsManager::GetInstance();
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

    RenderToolbar();
    if(m_show_kernel_table)
    {
        RenderTable();
    }

    ImGui::EndChild();  // kernel_metric_table_card
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(2);

    m_query_builder.Render();
}

void
KernelMetricTable::RenderToolbar()
{
    SettingsManager&  settings  = SettingsManager::GetInstance();
    ImFont*           icon_font = settings.GetFontManager().GetFont(FontType::kIcon);
    const ImGuiStyle& style     = settings.GetDefaultStyle();

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
                      settings.GetColor(Colors::kTransparent),
                      settings.GetColor(Colors::kButtonHovered),
                      settings.GetColor(Colors::kTransparent)))
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
            int col_count = static_cast<int>(m_header.size());
            for(int c = 0; c < col_count; c++)
            {
                if(c != ID_COLUMN_INDEX && c != NAME_COLUMN_INDEX)
                {
                    m_bar_chart_columns.insert(c);
                }
            }
            ComputeColumnMaxValues(m_rows);
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
}

void
KernelMetricTable::RenderTable()
{
    SettingsManager&  settings = SettingsManager::GetInstance();
    const ImGuiStyle& style    = settings.GetDefaultStyle();
    const bool        request_pending =
        m_data_provider.IsRequestPending(DataProvider::METRIC_PIVOT_TABLE_REQUEST_ID);

    const float row_padding_v = style.CellPadding.y * 2.0f;
    const float line_height   = ImGui::GetTextLineHeight() + row_padding_v;

    // Filter row height (InputText widgets are taller than text)
    const float filter_row_height = ImGui::GetFrameHeight() + row_padding_v;

    const int data_row_count = static_cast<int>(m_rows.size());
    const int rows_to_render =
        std::max(std::min(TABLE_MAX_FIXED_ROWS, data_row_count), TABLE_MIN_FIXED_ROWS);

    // Calculate total table height: header + filter row + data rows
    const float total_table_height =
        line_height + filter_row_height + (rows_to_render * line_height);

    int remove_index = -1;
    // Fixed height from the row count, or the rest of the pane in fill mode.
    if(ImGui::BeginChild("kernel_metric_table_cont",
                         ImVec2(0, m_fill_parent ? 0.0f : total_table_height),
                         ImGuiChildFlags_None, ImGuiWindowFlags_NoMove))
    {
        if(!m_header.empty() && m_workload_id != ComputeSelection::INVALID_SELECTION_ID)
        {
            // Stays sortable while a fetch runs: dropping the flag would make ImGui
            // rebuild the sort specs, losing the chosen sort.
            ImGuiTableFlags table_flags =
                ImGuiTableFlags_ScrollY | ImGuiTableFlags_ScrollX |
                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoSavedSettings |
                ImGuiTableFlags_Sortable;

            const int    column_count = static_cast<int>(m_header.size());
            const ImVec2 outer_size   = ImGui::GetContentRegionAvail();

            if(ImGui::BeginTable("kernel_selection_table", column_count, table_flags,
                                 outer_size))
            {
                // Freeze the name (and workload) columns and the header + filter rows.
                ImGui::TableSetupScrollFreeze(ShowsWorkloadColumn() ? 2 : 1, 2);
                SetupTableColumns(column_count, outer_size.x);
                remove_index = RenderHeaderRow(column_count);
                RenderFilterRow(column_count);

                if(m_rows.empty())
                {
                    // The ID column is hidden, so the note goes under Name.
                    ImGui::TableNextRow();
                    ImGui::TableSetColumnIndex(NAME_COLUMN_INDEX);
                    ImGui::TextDisabled(GetActiveFilterCount() > 0
                                            ? "No kernels match the applied filters"
                                            : "None");
                }

                UpdateSortFromTable();

                ImGui::PushStyleColor(ImGuiCol_Header,
                                      settings.GetColor(Colors::kSelection));
                ImGui::PushStyleColor(ImGuiCol_HeaderHovered,
                                      settings.GetColor(Colors::kHighlightChart));
                ImGui::PushStyleColor(ImGuiCol_HeaderActive,
                                      settings.GetColor(Colors::kHighlightChart));
                if(RenderRows(request_pending, line_height))
                {
                    ImGui::OpenPopup(CELL_CONTEXT_MENU_ID);
                }
                RenderCellContextMenu();
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
            RenderLoadingIndicator(settings.GetColor(Colors::kTextMain),
                                   "kernel_metric_table_loading");
        }
    }
    ImGui::EndChild();

    if(remove_index >= 0)
    {
        RemoveMetricColumn(remove_index);
    }
}

void
KernelMetricTable::SetupTableColumns(int column_count, float outer_width)
{
    const ImGuiStyle& style           = SettingsManager::GetInstance().GetDefaultStyle();
    const float       cell_padding    = style.CellPadding.x * 2.0f;
    const float       char_width      = ImGui::CalcTextSize("M").x;
    const bool        workload_column = ShowsWorkloadColumn();

    // Calculate minimum column widths based on character counts. The frozen name
    // leaves at least half the table for the columns that scroll past it.
    float name_min_width = std::min(char_width * COL_NAME_CHAR_LIMIT,
                                    std::max(outer_width * FROZEN_NAME_MAX_SHARE,
                                             char_width * COL_NAME_MIN_CHARS));
    float default_min_width    = char_width * COL_DEFAULT_CHAR_LIMIT;
    float invocation_min_width = char_width * COL_INVOCATION_CHAR_LIMIT;

    for(int col = 0; col < column_count; col++)
    {
        ImGuiTableColumnFlags col_flags = ImGuiTableColumnFlags_WidthFixed;
        // ImGui rebuilds the sort specs whenever the columns change (a metric added
        // or removed), so the current sort is the default.
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
            if(!shows_workload && !m_header[col].empty() && m_header[col][0] == '_')
            {
                col_flags |= ImGuiTableColumnFlags_DefaultHide |
                             ImGuiTableColumnFlags_Disabled;
            }

            // Set minimum width based on column type
            float min_width = default_min_width;
            if(shows_workload)
            {
                // Frozen with the name, so only as wide as its longest workload
                // name.
                min_width = ImGui::CalcTextSize("Workload").x;
                for(const WorkloadInfo* workload :
                    m_data_provider.ComputeModel().GetWorkloadList())
                {
                    if(workload)
                    {
                        min_width = std::max(min_width,
                                             ImGui::CalcTextSize(workload->name.c_str()).x);
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

            ImGui::TableSetupColumn(shows_workload ? "Workload"
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

                ImGui::TableSetupColumn(m_metrics_column_names[index].c_str(), col_flags,
                                        column_size);
            }
            else
            {
                ImGui::TableSetupColumn(("Metric " + std::to_string(index + 1)).c_str(),
                                        col_flags, default_min_width);
            }
        }
    }
}

int
KernelMetricTable::RenderHeaderRow(int column_count)
{
    SettingsManager&  settings     = SettingsManager::GetInstance();
    const ImGuiStyle& style        = settings.GetDefaultStyle();
    int               remove_index = -1;

    // Custom header row with hover detection and X button
    ImGui::TableNextRow(ImGuiTableRowFlags_Headers);
    for(int col = 0; col < column_count; col++)
    {
        ImGui::TableSetColumnIndex(col);

        // Headers are keyed by column as in ImGui::TableHeadersRow, so columns of
        // one name stay apart.
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
                const std::string& desc = m_metrics_info[index].entry.description;
                if(!desc.empty())
                {
                    WrappedTooltip(desc);
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
    return remove_index;
}

void
KernelMetricTable::RenderFilterRow(int column_count)
{
    // The workload column has no filter: its data is the id.
    const bool workload_column = ShowsWorkloadColumn();
    ImGui::TableNextRow();
    for(int col = 0; col < column_count; col++)
    {
        if(ImGui::TableSetColumnIndex(col) && !(col == ID_COLUMN_INDEX && workload_column))
        {
            RenderColumnFilter(col);
        }
    }
}

void
KernelMetricTable::UpdateSortFromTable()
{
    ImGuiTableSortSpecs* sort_specs = ImGui::TableGetSortSpecs();
    if(!sort_specs || !sort_specs->SpecsDirty)
    {
        return;
    }
    const int                                sort_column_index = sort_specs->Specs->ColumnIndex;
    const rocprofvis_controller_sort_order_t sort_order =
        (sort_specs->Specs->SortDirection == ImGuiSortDirection_Ascending)
            ? kRPVControllerSortOrderAscending
            : kRPVControllerSortOrderDescending;

    sort_specs->SpecsDirty = false;

    if(m_sort_column_index != sort_column_index || m_sort_order != sort_order)
    {
        m_sort_column_index = sort_column_index;
        m_sort_order        = sort_order;
        m_fetch_requested   = true;
    }
}

bool
KernelMetricTable::RenderRows(bool request_pending, float line_height)
{
    if(request_pending)
    {
        ImGui::BeginDisabled();
    }

    const bool workload_column = ShowsWorkloadColumn();
    bool       open_menu       = false;

    ImGuiListClipper clipper;
    clipper.Begin(static_cast<int>(m_rows.size()));
    // The selection's row is submitted even when clipped, to scroll to it.
    const bool scroll_to_selected =
        m_scroll_to_selected && !m_update_table_selection && m_selected_row >= 0 &&
        m_selected_row < static_cast<int>(m_rows.size());
    if(scroll_to_selected)
    {
        clipper.IncludeItemByIndex(m_selected_row);
    }
    const std::string marked_kernel_id = std::to_string(m_marked_kernel_id);
    while(clipper.Step())
    {
        for(int row = clipper.DisplayStart; row < clipper.DisplayEnd; row++)
        {
            ImGui::TableNextRow();
            ImGui::PushID(row);  // Push row ID for unique identification

            const std::vector<std::string>& cells             = m_rows[row];
            const bool                      is_selected       = (m_selected_row == row);
            bool                            selectable_placed = false;
            const uint32_t                  row_workload      = m_row_workloads[row];
            if(is_selected && scroll_to_selected)
            {
                ImGui::SetScrollHereY(0.5f);
                m_scroll_to_selected = false;
            }
            // The compare target's row is tinted by the same selectable that draws
            // the selection, so both highlights match.
            const bool is_marked = !is_selected && row_workload == m_marked_workload_id &&
                                   cells[ID_COLUMN_INDEX] == marked_kernel_id;
            const std::string* workload_label =
                workload_column ? &WorkloadName(row_workload) : nullptr;

            for(int col = 0; col < static_cast<int>(cells.size()); col++)
            {
                const std::string& cell =
                    (workload_label && col == ID_COLUMN_INDEX) ? *workload_label : cells[col];
                ImGui::TableNextColumn();

                // Track hover using the current table cell bounds instead of item
                // hover state because the first selectable spans all columns.
                const ImVec2 cell_min = ImGui::GetCursorScreenPos();
                const ImVec2 cell_max(cell_min.x + ImGui::GetContentRegionAvail().x,
                                      cell_min.y + line_height);

                // A name wider than its cell gets a tooltip.
                const bool need_tooltip =
                    col == NAME_COLUMN_INDEX &&
                    ImGui::CalcTextSize(cell.c_str()).x > ImGui::GetContentRegionAvail().x;

                // The first visible column holds the row's selectable.
                const ImGuiTableColumnFlags flags = ImGui::TableGetColumnFlags(col);
                if(!selectable_placed && (flags & ImGuiTableColumnFlags_IsVisible) != 0 &&
                   (flags & ImGuiTableColumnFlags_IsEnabled) != 0)
                {
                    RenderRowSelectable(row, cell, is_selected, is_marked);
                    selectable_placed = true;
                }
                else
                {
                    RenderValueCell(col, cell);
                }
                const bool cell_hovered = ImGui::IsWindowHovered() &&
                                          ImGui::IsMouseHoveringRect(cell_min, cell_max, true);
                if(need_tooltip && cell_hovered)
                {
                    WrappedTooltip(cell.size() > TOOLTIP_MAX_CHARS
                                       ? cell.substr(0, TOOLTIP_MAX_CHARS) + TEXT_ELLIPSIS
                                       : cell);
                }

                // Resolve the column from the cell rect; the row-spanning selectable
                // and frozen column make item hover unreliable.
                if(cell_hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
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
    return open_menu;
}

void
KernelMetricTable::RenderRowSelectable(int row, const std::string& cell, bool is_selected,
                                       bool is_marked)
{
    if(is_marked)
    {
        ImGui::PushStyleColor(
            ImGuiCol_Header,
            ApplyAlpha(SettingsManager::GetInstance().GetColor(Colors::kComparisonTarget),
                       MARKED_ROW_ALPHA));
    }
    const bool clicked = ImGui::Selectable(cell.c_str(), is_selected || is_marked,
                                           ImGuiSelectableFlags_SpanAllColumns);
    if(is_marked)
    {
        ImGui::PopStyleColor();
    }
    if(!clicked)
    {
        return;
    }
    if(is_selected)
    {
        if(m_allow_deselect)
        {
            m_selected_row = -1;  // Deselect if already selected
            m_compute_selection->SelectKernel(ComputeSelection::INVALID_SELECTION_ID);
        }
        return;
    }
    m_selected_row               = row;
    m_selected_kernel_id_local   = ParseKernelId(m_rows[row][ID_COLUMN_INDEX]);
    m_selected_workload_id_local = m_row_workloads[row];
    m_compute_selection->Select(m_selected_workload_id_local, m_selected_kernel_id_local);
}

void
KernelMetricTable::RenderValueCell(int col, const std::string& cell)
{
    if(cell.empty())
    {
        ImGui::TextDisabled("N/A");
        return;
    }
    if(m_bar_chart_columns.count(col) > 0)
    {
        auto it = m_column_max_values.find(col);
        if(it != m_column_max_values.end() && it->second > 0.0)
        {
            char*  end = nullptr;
            double val = std::strtod(cell.c_str(), &end);
            if(end != cell.c_str())
            {
                float ratio = static_cast<float>(std::abs(val) / it->second);
                ratio       = std::min(ratio, 1.0f);

                ImVec2      pos       = ImGui::GetCursorScreenPos();
                float       w         = ImGui::GetContentRegionAvail().x;
                float       h         = ImGui::GetTextLineHeightWithSpacing();
                ImDrawList* draw_list = ImGui::GetWindowDrawList();
                // Intersect with the current clip rect so the bar is correctly
                // hidden behind frozen rows/columns when the table is scrolled
                // vertically.
                draw_list->PushClipRect(pos, ImVec2(pos.x + w, pos.y + h), true);
                draw_list->AddRectFilled(
                    pos, ImVec2(pos.x + w * ratio, pos.y + h),
                    SettingsManager::GetInstance().GetColor(Colors::kHighlightChart));
                draw_list->PopClipRect();
            }
        }
    }
    ImGui::TextUnformatted(cell.c_str());
}

void
KernelMetricTable::RenderCellContextMenu()
{
    if(!BeginCellContextMenu(CELL_CONTEXT_MENU_ID))
    {
        return;
    }
    if(m_cell_menu.row >= 0 && m_cell_menu.row < static_cast<int>(m_rows.size()))
    {
        const bool                      workload_column = ShowsWorkloadColumn();
        const std::vector<std::string>& menu_row        = m_rows[m_cell_menu.row];
        std::vector<std::string>        row_cells;
        row_cells.reserve(m_header.size());
        // Hidden columns have a header name starting with '_'. Remap the
        // data-column index to its visible-list position. The id column is shown
        // as the row's workload when there are several.
        int cell_index = 0;
        for(int col = 0; col < static_cast<int>(m_header.size()) &&
                         col < static_cast<int>(menu_row.size());
            col++)
        {
            const bool shows_workload = col == ID_COLUMN_INDEX && workload_column;
            if(!shows_workload && !m_header[col].empty() && m_header[col][0] == '_')
            {
                continue;
            }
            if(col == m_cell_menu.column)
            {
                cell_index = static_cast<int>(row_cells.size());
            }
            const std::string& cell = shows_workload
                                          ? WorkloadName(m_row_workloads[m_cell_menu.row])
                                          : menu_row[col];
            row_cells.push_back(cell.empty() ? "N/A" : cell);
        }
        AddCopyRowCellMenuItems(row_cells.data(), static_cast<int>(row_cells.size()),
                                cell_index);
        if(m_compare_callback)
        {
            const uint32_t kernel_id   = ParseKernelId(menu_row[ID_COLUMN_INDEX]);
            const uint32_t workload_id = m_row_workloads[m_cell_menu.row];
            const bool     is_a = workload_id == m_compute_selection->GetSelectedWorkload() &&
                              kernel_id == m_compute_selection->GetSelectedKernel();
            ImGui::Separator();
            if(ImGui::MenuItem("Compare with this kernel (B)", nullptr, false,
                               kernel_id != ComputeSelection::INVALID_SELECTION_ID && !is_a))
            {
                m_compare_callback(workload_id, kernel_id);
            }
        }
    }
    EndCellContextMenu();
}

void
KernelMetricTable::RemoveMetricColumn(int remove_index)
{
    if(remove_index < 0 || remove_index >= static_cast<int>(m_metrics_params.size()))
    {
        return;
    }
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
    {
        adjusted.insert(c > removed_col ? c - 1 : c);
    }
    m_bar_chart_columns = adjusted;

    // The sort follows its column; sorting by the removed one falls back to the
    // default.
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

void
KernelMetricTable::SetQuery(const std::string& query)
{
    const AvailableMetrics::Entry* entry = m_query_builder.GetSelectedMetricInfo();
    AppendMetricQuery(query, entry ? *entry : AvailableMetrics::Entry(),
                      m_query_builder.GetValueName());
}

void
KernelMetricTable::SetExternalQuery(MetricId metric_id, const std::string& value_name)
{
    const WorkloadInfo* workload = m_data_provider.ComputeModel().GetWorkload(m_workload_id);
    if(!workload)
    {
        return;
    }
    const AvailableMetrics::Entry* entry = ComputeDataModel::GetMetricInfo(
        *workload, metric_id.category_id, metric_id.table_id, metric_id.entry_id);
    if(!entry)
    {
        return;
    }
    AppendMetricQuery(MetricQuery(*entry, value_name), *entry, value_name);
}

bool
KernelMetricTable::HasMetric(const std::string& query, const AvailableMetrics::Entry& entry,
                             const std::string& value_name) const
{
    ROCPROFVIS_ASSERT(m_metrics_params.size() == m_metrics_info.size());
    for(size_t i = 0; i < m_metrics_info.size(); ++i)
    {
        const MetricInfo& info = m_metrics_info[i];
        if(entry.name.empty() || info.entry.name.empty())
        {
            if(m_metrics_params[i] == query)
            {
                return true;
            }
        }
        else if(info.entry.category_id == entry.category_id &&
                info.entry.table_id == entry.table_id && info.entry.name == entry.name &&
                info.value_name == value_name)
        {
            return true;
        }
    }
    return false;
}

void
KernelMetricTable::AppendMetricQuery(const std::string& query,
                                     const AvailableMetrics::Entry& entry,
                                     const std::string& value_name)
{
    if(HasMetric(query, entry, value_name))
    {
        const std::string name =
            entry.name.empty() ? query
                               : entry.name + (value_name.empty() ? "" : " " + value_name);
        NotificationManager::GetInstance().Show("The metric '" + name +
                                                    "' is already in the table.",
                                                NotificationLevel::Warning);
        return;
    }
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
                        m_widget.AppendMetricQuery(MetricQuery(entry, value_name), entry,
                                                   value_name);
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
