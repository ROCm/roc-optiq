// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_comparison.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_requests.h"
#include "rocprofvis_settings_manager.h"
#include "widgets/rocprofvis_gui_helpers.h"
#include "widgets/rocprofvis_split_containers.h"
#include "widgets/rocprofvis_tab_container.h"
#include <algorithm>
#include <bitset>
#include <iomanip>
#include <sstream>

namespace RocProfVis
{
namespace View
{

constexpr size_t BASELINE_BG_COLOR     = 4;
constexpr size_t TARGET_BG_COLOR       = 5;
constexpr size_t DIFFERENCE_BG_COLOR   = 2;
constexpr size_t ROW_TAG_INVALID_MATCH = 0;

ComputeComparisonView::ComputeComparisonView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_settings(SettingsManager::GetInstance())
, m_compute_selection(compute_selection)
, m_client_id_baseline(IdGenerator::GetInstance().GenerateId())
, m_client_id_target(IdGenerator::GetInstance().GenerateId())
, m_baseline_request_id(0)
, m_target_request_id(0)
, m_target_workload_id(ComputeSelection::INVALID_SELECTION_ID)
, m_target_kernel_id(ComputeSelection::INVALID_SELECTION_ID)
, m_inputs_changed(false)
, m_data_changed(false)
, m_loading(false)
, m_retry_fetch(false)
, m_filter_common_metrics(false)
, m_layout(nullptr)
, m_toolbar_spacer_width(0.0f)
, m_toolbar_available_width(0.0f)
, m_tab_container(nullptr)
, m_bookmark_table(nullptr)
, m_bookmark_item(nullptr)
, m_max_bookmark_height(FLT_MAX)
{
    m_widget_name = GenUniqueName("ComputeComparison");
    m_bookmark_table =
        std::make_unique<Table>("",
                                ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                                    ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
                                3, 1);
    m_bookmark_table->SetColumnColor("Baseline", true, BASELINE_BG_COLOR);
    m_bookmark_table->SetColumnColor("Target", true, TARGET_BG_COLOR);
    m_bookmark_table->SetColumnColor("Difference", true, DIFFERENCE_BG_COLOR);
    m_bookmark_table->SetRowSelectionHandler(
        [this](const Table& table, const size_t index, const bool state) {
            if(!state)
            {
                RemoveBookmark(table, index);
            }
        });
    LayoutItem toolbar_item(-1, 0);
    toolbar_item.m_child_flags = ImGuiChildFlags_AutoResizeY;
    toolbar_item.m_item =
        std::make_shared<RocCustomWidget>([this]() { RenderToolbar(); });
    LayoutItem bookmarks_item(-1, 0);
    bookmarks_item.m_child_flags = ImGuiChildFlags_AutoResizeY;
    bookmarks_item.m_item =
        std::make_shared<RocCustomWidget>([this]() { RenderBookmarks(); });
    LayoutItem content_item(-1, 0);
    content_item.m_item = std::make_shared<RocCustomWidget>([this]() { RenderTables(); });
    std::vector<LayoutItem> layout_items;
    layout_items.push_back(toolbar_item);
    layout_items.push_back(bookmarks_item);
    int bookmark_index = static_cast<int>(layout_items.size() - 1);
    layout_items.push_back(content_item);
    m_layout              = std::make_shared<VFixedContainer>(layout_items);
    m_bookmark_item       = m_layout->GetMutableAt(bookmark_index);
    m_baseline_request_id = RequestIdBuilder::MakeClientRequestId(
        RequestType::kFetchMetrics, m_client_id_baseline);
    m_target_request_id = RequestIdBuilder::MakeClientRequestId(
        RequestType::kFetchMetrics, m_client_id_target);
    SubscribeEvents();
}

ComputeComparisonView::~ComputeComparisonView() { UnsubscribeEvents(); }

void
ComputeComparisonView::Update()
{
    if(m_inputs_changed)
    {
        FetchMetrics();
        if(m_tab_container && m_tab_container->GetActiveTab())
        {
            m_active_tab_id = m_tab_container->GetActiveTab()->m_id;
        }
        else
        {
            m_active_tab_id.clear();
        }
        m_inputs_changed = false;
    }
    if(m_retry_fetch && !m_data_provider.IsRequestPending(m_baseline_request_id) &&
       !m_data_provider.IsRequestPending(m_target_request_id))
    {
        m_inputs_changed = true;
        m_retry_fetch    = false;
    }
    if(m_data_changed)
    {
        UpdateMetrics();
        UpdateBookmarks();
        if(m_tab_container && !m_active_tab_id.empty())
        {
            m_tab_container->SetActiveTab(m_active_tab_id);
        }
        m_data_changed = false;
    }
    for(const CategoryModel& category : m_categories)
    {
        for(const std::shared_ptr<Table>& table : category.tables)
        {
            if(table)
            {
                table->Update();
            }
        }
    }
    if(m_bookmark_table)
    {
        m_bookmark_table->Update();
    }
}

void
ComputeComparisonView::Render()
{
    m_loading = m_data_provider.IsRequestPending(m_baseline_request_id) ||
                m_data_provider.IsRequestPending(m_target_request_id);
    m_max_bookmark_height =
        ImGui::GetWindowHeight() * 0.5f - ImGui::GetFrameHeightWithSpacing();
    if(m_layout)
    {
        m_layout->Render();
    }
}

void
ComputeComparisonView::SubscribeEvents()
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            m_inputs_changed = true;
        }
    };
    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);
    auto kernel_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            m_inputs_changed = true;
        }
    };
    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        kernel_changed_handler);
    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath() &&
           (evt->GetClientId() == m_client_id_baseline ||
            evt->GetClientId() == m_client_id_target))
        {
            m_data_changed = true;
        }
    };
    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);
}

void
ComputeComparisonView::UnsubscribeEvents()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), m_metrics_fetched_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        m_kernel_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
}

void
ComputeComparisonView::FetchMetrics()
{
    // Do nothing unless we have both baseline + target...
    uint32_t baseline_workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t baseline_kernel_id   = m_compute_selection->GetSelectedKernel();
    if(m_data_provider.ComputeModel().GetKernelInfo(baseline_workload_id,
                                                    baseline_kernel_id) &&
       m_data_provider.ComputeModel().GetKernelInfo(m_target_workload_id,
                                                    m_target_kernel_id))
    {
        if(m_data_provider.IsRequestPending(m_baseline_request_id))
        {
            m_data_provider.CancelRequest(m_baseline_request_id);
        }
        if(m_data_provider.IsRequestPending(m_target_request_id))
        {
            m_data_provider.CancelRequest(m_target_request_id);
        }
        std::vector<uint32_t>                       kernel_ids = { baseline_kernel_id };
        std::vector<MetricsRequestParams::MetricID> metric_ids;
        // Fetch baseline...
        if(!m_data_provider.ComputeModel().GetMetricsData(m_client_id_baseline,
                                                          baseline_kernel_id))
        {
            m_data_provider.ComputeModel().ClearMetricValues(m_client_id_baseline);
            for(const AvailableMetrics::Category* category :
                m_data_provider.ComputeModel()
                    .GetWorkload(baseline_workload_id)
                    ->available_metrics.ordered_categories)
            {
                for(const AvailableMetrics::Table* table : category->ordered_tables)
                {
                    metric_ids.push_back({ category->id, table->id, std::nullopt });
                }
            }
            // Retry later if busy...
            m_retry_fetch |= !m_data_provider.FetchMetrics(MetricsRequestParams(
                baseline_workload_id, kernel_ids, metric_ids, m_client_id_baseline));
        }
        kernel_ids = { m_target_kernel_id };
        metric_ids.clear();
        // Fetch target...
        if(!m_data_provider.ComputeModel().GetMetricsData(m_client_id_target,
                                                          m_target_kernel_id))
        {
            m_data_provider.ComputeModel().ClearMetricValues(m_client_id_target);
            for(const AvailableMetrics::Category* category :
                m_data_provider.ComputeModel()
                    .GetWorkload(m_target_workload_id)
                    ->available_metrics.ordered_categories)
            {
                for(const AvailableMetrics::Table* table : category->ordered_tables)
                {
                    metric_ids.push_back({ category->id, table->id, std::nullopt });
                }
            }
            // Retry later if busy...
            m_retry_fetch |= !m_data_provider.FetchMetrics(MetricsRequestParams(
                m_target_workload_id, kernel_ids, metric_ids, m_client_id_target));
        }
    }
}

void
ComputeComparisonView::UpdateMetrics()
{
    // Do nothing unless we have data for baseline + target...
    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    if(m_data_provider.ComputeModel().GetMetricsData(m_client_id_baseline, kernel_id) &&
       m_data_provider.ComputeModel().GetMetricsData(m_client_id_target,
                                                     m_target_kernel_id))
    {
        uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
        if(m_data_provider.ComputeModel().GetKernelInfo(workload_id, kernel_id) &&
           m_data_provider.ComputeModel().GetKernelInfo(m_target_workload_id,
                                                        m_target_kernel_id))
        {
            m_categories.clear();
            m_tab_container = std::make_unique<TabContainer>();
            std::vector<const AvailableMetrics::Category*> baseline_categories =
                m_data_provider.ComputeModel()
                    .GetWorkload(workload_id)
                    ->available_metrics.ordered_categories;
            std::unordered_map<uint32_t, const AvailableMetrics::Entry*> row_entry;
            std::unordered_map<uint32_t, std::vector<std::string>>       row_value_names;
            std::unordered_map<uint32_t, std::vector<std::optional<double>>>
                row_value_data;
            // Go through our available metrics...
            for(const AvailableMetrics::Category* category : baseline_categories)
            {
                CategoryModel category_model{ category, {} };
                category_model.tables.resize(category->ordered_tables.size());
                bool empty = true;
                for(size_t i = 0; i < category->ordered_tables.size(); i++)
                {
                    category_model.tables[i] = std::make_shared<Table>(
                        category->ordered_tables[i]->name,
                        ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                            ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY,
                        3, 1);
                    category_model.tables[i]->ReserveRows(
                        category->ordered_tables[i]->ordered_entries.size());
                    // See if this metric is present in baseline and target..
                    ComputeDataModel::MetricValuesByEntryId* baseline_fetched_table =
                        m_data_provider.ComputeModel().GetMetricValuesByTable(
                            m_client_id_baseline, kernel_id, category->id,
                            category->ordered_tables[i]->id);
                    ComputeDataModel::MetricValuesByEntryId* target_fetched_table =
                        m_data_provider.ComputeModel().GetMetricValuesByTable(
                            m_client_id_target, m_target_kernel_id, category->id,
                            category->ordered_tables[i]->id);
                    for(size_t j = 0;
                        j < category->ordered_tables[i]->ordered_entries.size(); j++)
                    {
                        const AvailableMetrics::Entry& available_entry =
                            *category->ordered_tables[i]->ordered_entries[j];
                        const std::shared_ptr<MetricValue> fetched_baseline_entry =
                            baseline_fetched_table
                                ? baseline_fetched_table->count(available_entry.id)
                                      ? baseline_fetched_table->at(available_entry.id)
                                      : nullptr
                                : nullptr;
                        const std::shared_ptr<MetricValue> fetched_target_entry =
                            target_fetched_table
                                ? target_fetched_table->count(available_entry.id)
                                      ? target_fetched_table->at(available_entry.id)
                                      : nullptr
                                : nullptr;
                        row_entry.clear();
                        row_value_names.clear();
                        row_value_data.clear();
                        if(fetched_baseline_entry)
                        {
                            row_entry[0] = fetched_baseline_entry.get()->entry;
                        }
                        if(fetched_target_entry)
                        {
                            row_entry[1] = fetched_target_entry.get()->entry;
                        }
                        bool valid_match = fetched_baseline_entry &&
                                           fetched_target_entry &&
                                           fetched_baseline_entry->entry->name ==
                                               fetched_target_entry->entry->name;
                        // Go through metric's values...
                        for(const std::string& value_name :
                            category->ordered_tables[i]->value_names)
                        {
                            const double* baseline_value =
                                fetched_baseline_entry
                                    ? fetched_baseline_entry->values.count(value_name)
                                          ? &fetched_baseline_entry->values.at(value_name)
                                          : nullptr
                                    : nullptr;
                            const double* target_value =
                                fetched_target_entry
                                    ? fetched_target_entry->values.count(value_name)
                                          ? &fetched_target_entry->values.at(value_name)
                                          : nullptr
                                    : nullptr;
                            // Merge baseline + target into one row if they match,
                            // otherwise split into two separate rows...
                            row_value_names[0].emplace_back("Baseline " + value_name);
                            row_value_data[0].emplace_back(
                                baseline_value ? std::make_optional(*baseline_value)
                                               : std::nullopt);
                            row_value_names[0].emplace_back("Target " + value_name);
                            row_value_data[0].emplace_back(
                                valid_match && target_value
                                    ? std::make_optional(*target_value)
                                    : std::nullopt);
                            row_value_names[0].emplace_back("Difference##" + value_name);
                            row_value_data[0].emplace_back(
                                valid_match && baseline_value && target_value
                                    ? std::make_optional(*target_value - *baseline_value)
                                    : std::nullopt);
                            row_value_names[0].emplace_back("Difference (%)##" +
                                                            value_name);
                            row_value_data[0].emplace_back(
                                valid_match && baseline_value && target_value
                                    ? std::make_optional(
                                          (*target_value - *baseline_value) /
                                          (*baseline_value == 0.0 ? 1.0
                                                                  : *baseline_value) *
                                          100)
                                    : std::nullopt);
                            if(!valid_match && row_entry.count(1) > 0)
                            {
                                row_value_names[1].emplace_back("Baseline " + value_name);
                                row_value_data[1].emplace_back(std::nullopt);
                                row_value_names[1].emplace_back("Target " + value_name);
                                row_value_data[1].emplace_back(
                                    target_value ? std::make_optional(*target_value)
                                                 : std::nullopt);
                                row_value_names[1].emplace_back("Difference##" +
                                                                value_name);
                                row_value_data[1].emplace_back(std::nullopt);
                                row_value_names[1].emplace_back("Difference (%)##" +
                                                                value_name);
                                row_value_data[1].emplace_back(std::nullopt);
                            }
                        }
                        if(row_value_names.size() == row_value_data.size())
                        {
                            for(uint32_t k = 0; k < row_value_names.size(); k++)
                            {
                                if(row_entry.count(k) > 0 &&
                                   row_value_names.count(k) > 0 &&
                                   row_value_data.count(k) > 0)
                                {
                                    category_model.tables[i]->AddRow(
                                        row_entry.at(k), row_value_names.at(k),
                                        row_value_data.at(k),
                                        valid_match
                                            ? std::nullopt
                                            : std::make_optional(Colors::kTextDim),
                                        valid_match ? std::nullopt
                                                    : std::make_optional<size_t>(
                                                          ROW_TAG_INVALID_MATCH));
                                }
                            }
                        }
                    }
                    // Setup the table (handlers, coloring..etc)
                    category_model.tables[i]->SetRowSelectionHandler(
                        [this](const Table& table, const size_t index, const bool state) {
                            if(state)
                            {
                                AddBookmark(table, index);
                            }
                            else
                            {
                                RemoveBookmark(table, index);
                            }
                        });
                    empty &= category_model.tables[i]->Rows().empty();
                    category_model.tables[i]->SetColumnColor("Baseline", true,
                                                             BASELINE_BG_COLOR);
                    category_model.tables[i]->SetColumnColor("Target", true,
                                                             TARGET_BG_COLOR);
                    category_model.tables[i]->SetColumnColor("Difference", true,
                                                             DIFFERENCE_BG_COLOR);
                    if(m_filter_common_metrics)
                    {
                        category_model.tables[i]->ApplyRowFilter(ROW_TAG_INVALID_MATCH);
                    }
                }
                // Make a tab for non empty categories...
                if(!empty)
                {
                    m_categories.push_back(std::move(category_model));
                    size_t i = m_categories.size() - 1;
                    m_tab_container->AddTab(
                        TabItem{ category->name, category->name,
                                 std::make_shared<RocCustomWidget>(
                                     [this, i]() { RenderCategory(i); }),
                                 false });
                }
            }
            m_tab_container->SetAllowToolTips(true);
        }
    }
}

void
ComputeComparisonView::RenderToolbar()
{
    const ImGuiStyle& style = m_settings.GetDefaultStyle();
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::BeginChild("toolbar", ImVec2(-1, 0),
                      ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_FrameStyle);

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, style.FramePadding);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, style.FrameRounding);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("Compare With:");
    ImGui::SameLine(0, style.ItemSpacing.x);
    const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
        m_data_provider.ComputeModel().GetWorkloads();
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 10.0f);
    ImGui::BeginDisabled(workloads.empty());
    if(ImGui::BeginCombo("##TargetWorkloads",
                         workloads.count(m_target_workload_id) > 0
                             ? workloads.at(m_target_workload_id).name.c_str()
                             : "-"))
    {
        for(const std::pair<const uint32_t, WorkloadInfo>& workload : workloads)
        {
            if(ImGui::Selectable(workload.second.name.c_str(),
                                 m_target_workload_id == workload.second.id))
            {
                if(m_target_workload_id != workload.second.id)
                {
                    m_target_workload_id = workload.second.id;
                    std::vector<const KernelInfo*> kernel_list =
                        m_data_provider.ComputeModel().GetKernelInfoList(
                            m_target_workload_id);
                    if(!kernel_list.empty())
                    {
                        m_target_kernel_id = kernel_list[0]->id;
                    }
                    else
                    {
                        m_target_kernel_id = ComputeSelection::INVALID_SELECTION_ID;
                    }
                    m_inputs_changed = true;
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    VerticalSeparator(&m_settings);
    const KernelInfo* target_kernel_info = m_data_provider.ComputeModel().GetKernelInfo(
        m_target_workload_id, m_target_kernel_id);
    std::vector<const KernelInfo*> target_kernel_list =
        m_data_provider.ComputeModel().GetKernelInfoList(m_target_workload_id);
    ImGui::SetNextItemWidth(ImGui::GetFrameHeight() * 10.0f);
    ImGui::BeginDisabled(target_kernel_list.empty());
    if(ImGui::BeginCombo("##target_kernels",
                         target_kernel_info ? target_kernel_info->name.c_str() : "-"))
    {
        for(const KernelInfo* info : target_kernel_list)
        {
            if(ImGui::Selectable(info->name.c_str(), m_target_kernel_id == info->id))
            {
                if(m_target_kernel_id != info->id)
                {
                    m_target_kernel_id = info->id;
                    m_inputs_changed   = true;
                }
            }
        }
        ImGui::EndCombo();
    }
    ImGui::EndDisabled();
    VerticalSeparator(&m_settings);
    if(m_toolbar_spacer_width != 0.0)
    {
        ImGui::Dummy(ImVec2(m_toolbar_spacer_width, ImGui::GetFrameHeightWithSpacing()));
    }
    VerticalSeparator(&m_settings);
    if(ImGui::Button(m_filter_common_metrics ? "Show Common Metrics" : "Show All Metrics",
                     ImVec2(ImGui::CalcTextSize("Show Common Metrics").x +
                                style.FramePadding.x * 2.0f,
                            0)))
    {
        m_filter_common_metrics = !m_filter_common_metrics;
        if(m_bookmark_table)
        {
            if(m_filter_common_metrics)
            {
                m_bookmark_table->ApplyRowFilter(ROW_TAG_INVALID_MATCH);
            }
            else
            {
                m_bookmark_table->RemoveRowFilter(ROW_TAG_INVALID_MATCH);
            }
        }
        for(CategoryModel& category_model : m_categories)
        {
            for(std::shared_ptr<Table>& table : category_model.tables)
            {
                if(table)
                {
                    if(m_filter_common_metrics)
                    {
                        table->ApplyRowFilter(ROW_TAG_INVALID_MATCH);
                    }
                    else
                    {
                        table->RemoveRowFilter(ROW_TAG_INVALID_MATCH);
                    }
                }
            }
        }
    }
    float window_width = ImGui::GetWindowWidth();
    if(BeginItemTooltipStyled())
    {
        ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + window_width * 0.25f);
        ImGui::TextUnformatted("Toggle displaying of mismatched (grey-shaded) metrics, such "
                               "as archetecture specific metrics "
                               "when comparing across GPU archetectures.");
        ImGui::PopTextWrapPos();
        EndTooltipStyled();
    }
    if(m_bookmark_item)
    {
        VerticalSeparator(&m_settings);
        ImGui::TextUnformatted("Bookmarks");
        ImGui::SameLine();
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + style.FramePadding.x);
        if(ImGui::ArrowButton("toggle_bookmarks",
                              m_bookmark_item->m_visible ? ImGuiDir_Down : ImGuiDir_Up))
        {
            m_bookmark_item->m_visible = !m_bookmark_item->m_visible;
        }
    }
    ImGui::SameLine();
    m_toolbar_spacer_width =
        std::max(0.0f, m_toolbar_spacer_width + ImGui::GetContentRegionAvail().x);
    ImGui::PopStyleVar(2);
    ImGui::EndChild();
    ImGui::PopStyleVar(2);
}

void
ComputeComparisonView::RenderCategory(const size_t i)
{
    ImGui::PushID(i);
    ImGui::BeginChild("category_container");
    for(const std::shared_ptr<Table>& table : m_categories[i].tables)
    {
        if(table)
        {
            table->SetMaxSize(ImGui::GetWindowSize());
            table->Render();
        }
    }
    ImGui::EndChild();
    ImGui::PopID();
}

void
ComputeComparisonView::RenderBookmarks() const
{
    if(m_bookmark_table)
    {
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeightWithSpacing()),
            ImVec2(ImGui::GetContentRegionAvail().x, m_max_bookmark_height));
        ImGui::BeginChild("bookmarks", ImVec2(0.0f, 0.0f),
                          ImGuiChildFlags_Border | ImGuiChildFlags_AutoResizeY);
        if(m_bookmarks.empty())
        {
            CenterNextItem(ImGui::CalcTextSize("Use () to bookmark metrics.").x +
                           ImGui::GetFontSize());
            ImGui::SetCursorPosY(ImGui::GetStyle().FramePadding.y);
            ImGui::BeginDisabled();
            ImGui::TextUnformatted("Use (");
            ImGui::SameLine();
            bool hint = true;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));
            ImGui::Checkbox("##hint", &hint);
            ImGui::PopStyleVar();
            ImGui::SameLine();
            ImGui::TextUnformatted(") to bookmark metrics.");
            ImGui::EndDisabled();
        }
        else
        {
            m_bookmark_table->SetMaxSize(ImVec2(FLT_MAX, m_max_bookmark_height));
            m_bookmark_table->Render();
            if(m_loading)
            {
                RenderLoadingIndicator(m_settings.GetColor(Colors::kTextMain),
                                       "loading_indicator");
            }
        }
        ImGui::EndChild();
    }
}

void
ComputeComparisonView::RenderTables() const
{
    if(m_tab_container)
    {
        m_tab_container->Render();
    }
    else if(!m_loading)
    {
        CenterNextTextItem("Select a kernel for comparison.");
        ImGui::SetCursorPosY((ImGui::GetContentRegionAvail().y - ImGui::GetFontSize()) *
                             0.5f);
        ImGui::TextDisabled("Select a kernel for comparison.");
    }
    if(m_loading)
    {
        RenderLoadingIndicator(m_settings.GetColor(Colors::kTextMain),
                               "loading_indicator");
    }
}

void
ComputeComparisonView::AddBookmark(const Table& table, const size_t index)
{
    if(m_bookmark_table && index < table.Rows().size())
    {
        const Table::Row& row = table.Rows()[index];
        row.selected          = true;
        m_bookmarks.emplace_back(BookmarkModel{ row.id, row.entry, &row });
        m_bookmark_table->AddRow(table, index);
    }
}

void
ComputeComparisonView::RemoveBookmark(const Table& table, const size_t index)
{
    if(m_bookmark_table && index < table.Rows().size())
    {
        const Table::Row& row = table.Rows()[index];
        if(&table == m_bookmark_table.get())
        {
            // Removal initiated from bookmarks...
            if(m_bookmarks[index].row)
            {
                m_bookmarks[index].row->selected = false;
            }
            m_bookmark_table->RemoveRow(index);
        }
        else
        {
            // Removal initiated from main tables...
            for(size_t i = 0; i < m_bookmark_table->Rows().size(); i++)
            {
                if(row == m_bookmark_table->Rows()[i])
                {
                    m_bookmarks[i].row->selected = false;
                    m_bookmark_table->RemoveRow(i);
                    break;
                }
            }
        }
        m_bookmarks.erase(std::remove(m_bookmarks.begin(), m_bookmarks.end(),
                                      BookmarkModel{ row.id, row.entry, &row }),
                          m_bookmarks.end());
    }
}

void
ComputeComparisonView::UpdateBookmarks()
{
    if(m_bookmark_table)
    {
        m_bookmark_table->ClearRows();
        std::vector<std::pair<const Table*, size_t>> updated_bookmarks(
            m_bookmarks.size());
        std::unordered_map<std::string, std::unordered_map<std::string_view, size_t>>
            id_index_map;
        // Store the ordering of the bookmarks...
        for(size_t i = 0; i < m_bookmarks.size(); i++)
        {
            id_index_map[m_bookmarks[i].row_id.metric_id]
                        [m_bookmarks[i].row_id.entry_name] = i;
        }
        // Go through the categories and find bookmarked metrics...
        for(const CategoryModel& category : m_categories)
        {
            for(const std::shared_ptr<Table>& table : category.tables)
            {
                for(size_t i = 0; i < table->Rows().size(); i++)
                {
                    const Table::Row& row = table->Rows()[i];
                    if(id_index_map.count(row.id.metric_id) > 0 &&
                       id_index_map.at(row.id.metric_id).count(row.id.entry_name) > 0)
                    {
                        updated_bookmarks[id_index_map.at(row.id.metric_id)
                                              .at(row.id.entry_name)] =
                            std::make_pair(table.get(), i);
                    }
                }
            }
        }
        std::vector<std::string>           empty_name;
        std::vector<std::optional<double>> empty_data;
        for(size_t i = 0; i < m_bookmarks.size(); i++)
        {
            const Table* table     = updated_bookmarks[i].first;
            size_t&      row_index = updated_bookmarks[i].second;
            if(table && row_index < table->Rows().size())
            {
                // Bookmark exists...
                m_bookmarks[i].row           = &table->Rows()[row_index];
                m_bookmarks[i].row->selected = true;
                m_bookmark_table->AddRow(*updated_bookmarks[i].first,
                                         updated_bookmarks[i].second);
            }
            else
            {
                // Bookmark no longer exist...
                m_bookmarks[i].row = nullptr;
                m_bookmark_table->AddRow(
                    m_bookmarks[i].entry, empty_name, empty_data, Colors::kTextDim,
                    std::make_optional<size_t>(ROW_TAG_INVALID_MATCH));
                m_bookmark_table->Rows()[m_bookmark_table->Rows().size() - 1].selected =
                    true;
            }
        }
    }
}

ComputeComparisonView::Table::Table(std::string title, ImGuiTableFlags flags,
                                    int scroll_freeze_columns, int scroll_freeze_rows)
: RocWidget()
, m_settings(SettingsManager::GetInstance())
, m_title(std::move(title))
, m_flags(flags)
, m_max_size(ImVec2(FLT_MAX, FLT_MAX))
, m_scroll_freeze_columns(scroll_freeze_columns)
, m_scroll_freeze_rows(scroll_freeze_rows)
, m_rows_changed(false)
, m_row_filter_changed(false)
, m_scrollable(true)
, m_remove_row_index(std::nullopt)
, m_visible_row_count(0)
{
    m_widget_name = GenUniqueName("metric_table");
}

ComputeComparisonView::Table::~Table() {}

void
ComputeComparisonView::Table::Update()
{
    if(m_remove_row_index)
    {
        // Deferred row removal...
        if(m_remove_row_index.value() < m_rows.size())
        {
            for(const std::pair<const std::string, std::string>& value :
                m_rows[m_remove_row_index.value()].values_map)
            {
                if(m_value_columns.count(value.first) > 0)
                {
                    Column& column = m_value_columns.at(value.first);
                    if(column.ref_count > 1)
                    {
                        column.ref_count--;
                    }
                    else
                    {
                        m_value_columns.erase(value.first);
                        m_ordered_value_names.erase(
                            std::remove(m_ordered_value_names.begin(),
                                        m_ordered_value_names.end(), value.first),
                            m_ordered_value_names.end());
                    }
                }
            }
            m_rows.erase(m_rows.begin() + m_remove_row_index.value());
            m_rows_changed = true;
        }
        m_remove_row_index = std::nullopt;
    }
    if(m_rows_changed)
    {
        // (Check box, metric id, metric name, unit) + value columns
        m_columns.resize(4 + m_value_columns.size());
        int j = 0;
        // Setup columns...
        for(size_t i = 0; i < m_columns.size(); i++)
        {
            if(i == Column::Selection)
            {
                m_columns[i].type        = Column::Selection;
                m_columns[i].name        = "";
                m_columns[i].color_index = std::nullopt;
            }
            else if(i == Column::MetricID)
            {
                m_columns[i].type        = Column::MetricID;
                m_columns[i].name        = "Metric ID";
                m_columns[i].color_index = std::nullopt;
            }
            else if(i == Column::MetricName)
            {
                m_columns[i].type        = Column::MetricName;
                m_columns[i].name        = "Metric";
                m_columns[i].color_index = std::nullopt;
            }
            else if(i == m_columns.size() - 1)
            {
                m_columns[i].type        = Column::Unit;
                m_columns[i].name        = "Unit";
                m_columns[i].color_index = std::nullopt;
            }
            else if(j < m_ordered_value_names.size() &&
                    m_value_columns.count(m_ordered_value_names[j]) > 0)
            {
                m_columns[i] = m_value_columns.at(m_ordered_value_names[j++]);
                for(const ColumnColorRule& rule : m_column_color_rules)
                {
                    if(rule.fuzzy_match
                           ? (m_columns[i].name.find(rule.name) != std::string::npos)
                           : (m_columns[i].name == rule.name))
                    {
                        m_columns[i].color_index = rule.color_index;
                        break;
                    }
                }
            }
        }
        // Setup rows...
        m_visible_row_count = 0;
        for(Row& row : m_rows)
        {
            row.cells.resize(m_columns.size());
            for(size_t i = 0; i < m_columns.size(); i++)
            {
                switch(m_columns[i].type)
                {
                    case Column::Selection:
                    {
                        row.cells[i] = std::string_view{};
                        break;
                    }
                    case Column::MetricID:
                    {
                        row.cells[i] = row.id.metric_id;
                        break;
                    }
                    case Column::MetricName:
                    {
                        row.cells[i] = row.entry->name;
                        break;
                    }
                    case Column::Unit:
                    {
                        row.cells[i] = row.entry->unit;
                        break;
                    }
                    default:
                    {
                        if(row.values_map.count(m_columns[i].name) > 0)
                        {
                            row.cells[i] = row.values_map.at(m_columns[i].name);
                        }
                        else
                        {
                            row.cells[i] = std::string_view{};
                        }
                        break;
                    }
                }
            }
            if(!(row.tags & m_row_filter).any())
            {
                m_visible_row_count++;
            }
        }
        m_rows_changed = false;
    }
    if(m_row_filter_changed)
    {
        // Row filtering by tag...
        m_visible_row_count = 0;
        for(const Row& row : m_rows)
        {
            if(!(row.tags & m_row_filter).any())
            {
                m_visible_row_count++;
            }
        }
        m_row_filter_changed = false;
    }
}

void
ComputeComparisonView::Table::Render()
{
    ImGui::BeginChild(
        m_widget_name.c_str(),
        ImVec2(std::min(m_max_size.x, ImGui::GetContentRegionAvail().x),
               m_visible_row_count == 0
                   ? ImGui::GetFrameHeightWithSpacing() * 2.0f
                   : std::min(
                         m_max_size.y,
                         ImGui::GetFrameHeightWithSpacing() *
                                 (m_visible_row_count + (m_title.empty() ? 1.0f : 2.0f)) +
                             (m_scrollable ? ImGui::GetStyle().ScrollbarSize : 0.0f))),
        ImGuiChildFlags_Borders);
    float title_height = 0.0f;
    if(!m_title.empty())
    {
        SectionTitle(m_title.c_str(), true, &m_settings);
        title_height = ImGui::GetItemRectSize().y;
    }
    if(!m_rows.empty() && m_visible_row_count > 0)
    {
        if(ImGui::BeginTable(m_widget_name.c_str(), static_cast<int>(m_columns.size()),
                             m_flags, ImGui::GetContentRegionAvail()))
        {
            ImGui::TableSetupScrollFreeze(m_scroll_freeze_columns, m_scroll_freeze_rows);
            for(const Column& column : m_columns)
            {
                ImGui::TableSetupColumn(column.name.c_str());
            }
            ImGui::TableHeadersRow();
            const std::vector<ImU32>& color_wheel = m_settings.GetColorWheel();
            for(size_t i = 0; i < m_rows.size(); i++)
            {
                if(m_rows[i].cells.size() == m_columns.size() &&
                   !(m_rows[i].tags & m_row_filter).any())
                {
                    ImGui::PushID(static_cast<int>(i));
                    ImGui::TableNextRow();
                    for(size_t j = 0; j < m_columns.size(); j++)
                    {
                        ImGui::PushID(static_cast<int>(j));
                        ImGui::TableNextColumn();
                        if(m_columns[j].color_index)
                        {
                            ImGui::TableSetBgColor(
                                ImGuiTableBgTarget_CellBg,
                                (color_wheel[m_columns[j].color_index.value() %
                                             color_wheel.size()] &
                                 ~IM_COL32_A_MASK) |
                                    (64 << IM_COL32_A_SHIFT));
                        }
                        switch(m_columns[j].type)
                        {
                            case Column::Selection:
                            {
                                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,
                                                    ImVec2(0, 0));
                                if(ImGui::Checkbox("", &m_rows[i].selected))
                                {
                                    if(m_row_selection_callback)
                                    {
                                        m_row_selection_callback(*this, i,
                                                                 m_rows[i].selected);
                                    }
                                }
                                ImGui::PopStyleVar();
                                break;
                            }
                            default:
                            {
                                if(m_rows[i].cells[j].empty())
                                {
                                    ImGui::TextDisabled("N/A");
                                }
                                else
                                {
                                    if(m_rows[i].text_color)
                                    {
                                        ImGui::PushStyleColor(
                                            ImGuiCol_Text,
                                            m_settings.GetColor(
                                                m_rows[i].text_color.value()));
                                    }
                                    CopyableTextUnformatted(m_rows[i].cells[j].data(), "",
                                                            COPY_DATA_NOTIFICATION, false,
                                                            true);
                                    switch(m_columns[j].type)
                                    {
                                        case Column::MetricName:
                                        {
                                            float tooltip_width =
                                                ImGui::GetWindowWidth() * 0.25f;
                                            if(ImGui::IsItemHovered() &&
                                               !m_rows[i].entry->description.empty())
                                            {
                                                BeginTooltipStyled();
                                                ImGui::PushTextWrapPos(
                                                    ImGui::GetCursorPosX() +
                                                    tooltip_width);
                                                ImGui::TextUnformatted(
                                                    m_rows[i].entry->description.c_str());
                                                ImGui::PopTextWrapPos();
                                                EndTooltipStyled();
                                            }
                                            break;
                                        }
                                        default:
                                        {
                                            break;
                                        }
                                    }
                                    if(m_rows[i].text_color)
                                    {
                                        ImGui::PopStyleColor();
                                    }
                                }
                                break;
                            }
                        }
                        ImGui::PopID();
                    }
                    ImGui::PopID();
                }
            }
            m_scrollable = ImGui::GetScrollMaxX() > 0.0;
            ImGui::EndTable();
        }
    }
    else
    {
        ImGui::SetCursorPosY(title_height +
                             (ImGui::GetContentRegionAvail().y - ImGui::GetFontSize()) *
                                 0.5f);
        CenterNextTextItem("No data available.");
        ImGui::TextDisabled("No data available.");
    }
    ImGui::EndChild();
}

const std::vector<ComputeComparisonView::Table::Row>&
ComputeComparisonView::Table::Rows() const
{
    return m_rows;
}

const std::vector<std::string>&
ComputeComparisonView::Table::OrderedValueNames() const
{
    return m_ordered_value_names;
}

void
ComputeComparisonView::Table::ReserveRows(size_t rows)
{
    m_rows.reserve(rows);
}

void
ComputeComparisonView::Table::AddRow(const AvailableMetrics::Entry*      entry,
                                     std::vector<std::string>&           value_names,
                                     std::vector<std::optional<double>>& value_data,
                                     std::optional<Colors>               text_color,
                                     std::optional<size_t>               tag)
{
    // Add a row associated with a specfic Entry...
    if(value_names.size() == value_data.size())
    {
        std::unordered_map<std::string, std::string> value_map;
        for(size_t i = 0; i < value_names.size(); i++)
        {
            // Convert data to string...
            if(value_data[i])
            {
                std::ostringstream oss;
                oss << std::fixed << std::setprecision(2) << value_data[i].value();
                value_map[value_names[i]] = oss.str();
            }
            else
            {
                value_map[value_names[i]];
            }
            // Map value names (columns)...
            if(m_value_columns.count(value_names[i]) == 0)
            {
                m_ordered_value_names.push_back(value_names[i]);
                m_value_columns[value_names[i]] =
                    Column{ Column::Value, value_names[i], 1 };
            }
            else
            {
                m_value_columns.at(value_names[i]).ref_count++;
            }
        }
        m_rows.emplace_back(Row{ Row::ID{ std::to_string(entry->category_id) + "." +
                                              std::to_string(entry->table_id) + "." +
                                              std::to_string(entry->id),
                                          entry->name },
                                 entry,
                                 std::move(value_map),
                                 {},
                                 text_color,
                                 {},
                                 false });
        if(tag && tag.value() < MAX_ROW_TAGS)
        {
            m_rows.back().tags.set(tag.value());
        }
        m_rows_changed = true;
    }
}

void
ComputeComparisonView::Table::AddRow(const Table& table, const size_t index)
{
    // Copy a row from another Table...
    const Table::Row& other = table.Rows()[index];
    for(const std::string& value_name : table.OrderedValueNames())
    {
        if(other.values_map.count(value_name) > 0)
        {
            if(m_value_columns.count(value_name) == 0)
            {
                m_ordered_value_names.push_back(value_name);
                m_value_columns[value_name] = Column{ Column::Value, value_name, 1 };
            }
            else
            {
                m_value_columns.at(value_name).ref_count++;
            }
        }
    }
    m_rows.emplace_back(other);
    m_rows_changed = true;
}

void
ComputeComparisonView::Table::RemoveRow(size_t index)
{
    m_remove_row_index = index;
}

void
ComputeComparisonView::Table::ClearRows()
{
    m_ordered_value_names.clear();
    m_value_columns.clear();
    m_rows.clear();
    m_columns.clear();
    m_visible_row_count = 0;
}

void
ComputeComparisonView::Table::SetColumnColor(const std::string& column_name,
                                             bool fuzzy_match, size_t color_index)
{
    m_column_color_rules.push_back(
        ColumnColorRule{ column_name, fuzzy_match, color_index });
    m_rows_changed = true;
}

void
ComputeComparisonView::Table::ApplyRowFilter(size_t tag)
{
    m_row_filter.set(tag);
    m_row_filter_changed = true;
}

void
ComputeComparisonView::Table::RemoveRowFilter(size_t tag)
{
    m_row_filter.reset(tag);
    m_row_filter_changed = true;
}

void
ComputeComparisonView::Table::SetMaxSize(ImVec2 max_size)
{
    m_max_size = max_size;
}

void
ComputeComparisonView::Table::SetRowSelectionHandler(
    const std::function<void(const Table& table, const size_t index, const bool state)>&
        handler)
{
    m_row_selection_callback = handler;
}

}  // namespace View
}  // namespace RocProfVis
