// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_table_view.h"
#include "rocprofvis_compute_selection.h"
#include "model/compute/rocprofvis_compute_data_model.h"
#include "rocprofvis_requests.h"

namespace RocProfVis
{
namespace View
{

ComputeTableView::ComputeTableView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection,
    std::function<void(MetricId metric_id, const std::string&)>
        set_to_kernel_table_callback)
: RocWidget()
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
, m_set_to_kernel_table_callback(set_to_kernel_table_callback)
, m_pined_metric_table(data_provider, compute_selection, m_client_id)
{
    m_pined_metric_table.SetPinMetricCallback([this](MetricId metric_id) {
        m_table_widgets[metric_id.GetTableKey()].ChangePinState(metric_id);
        m_pined_metric_table.RemoveRow(metric_id);
    });
    m_pined_metric_table.SetToKernelTableCallback(set_to_kernel_table_callback);

    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            RebuildTabs();
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto kernel_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            FetchAllMetrics();
        }
    };

    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        kernel_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            if(m_fetch_pending)
                FetchAllMetrics();
            if(evt->GetClientId() == m_client_id)
                RebuildTableDataCache();
        }
        m_pined_metric_table.RefillTable();
    };

    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    m_widget_name = GenUniqueName("ComputeTableView");
}

ComputeTableView::~ComputeTableView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        m_kernel_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched),
        m_metrics_fetched_token);
}

void
ComputeTableView::RebuildTabs()
{
    m_tabs.reset();
    m_table_widgets.clear();
    m_data_provider.ComputeModel().ClearKernelMetricValues(m_client_id);

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID)
        return;

    const auto& workloads = m_data_provider.ComputeModel().GetWorkloads();
    if(!workloads.count(workload_id))
        return;

    const auto& workload = workloads.at(workload_id);
    m_tabs = std::make_shared<TabContainer>();
    m_tabs->SetAllowToolTips(true);
    for(const auto* cat : workload.available_metrics.ordered_categories)
    {
        auto widget = std::make_shared<RocCustomWidget>(
            [this, cat]() { RenderCategory(*cat); });
        m_tabs->AddTab({ cat->name, cat->name, widget, false });
    }
}

void
ComputeTableView::FetchAllMetrics()
{
    m_data_provider.ComputeModel().ClearKernelMetricValues(m_client_id);
    m_table_widgets.clear();
    m_fetch_pending = false;

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID ||
       kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    const auto& workloads = m_data_provider.ComputeModel().GetWorkloads();
    if(!workloads.count(workload_id))
        return;

    const auto& workload = workloads.at(workload_id);
    std::vector<uint32_t>                   kernel_ids = { kernel_id };
    std::vector<MetricsRequestParams::MetricID> metric_ids;
    for(const auto* cat : workload.available_metrics.ordered_categories)
    {
        for(const auto* tbl : cat->ordered_tables)
            metric_ids.push_back({ cat->id, tbl->id, std::nullopt });
    }

    bool success = m_data_provider.FetchMetrics(
        MetricsRequestParams(workload.id, kernel_ids, metric_ids, m_client_id));

    if(!success)
    {
        m_fetch_pending = true;
    }
}

void
ComputeTableView::Update()
{
    if(m_tabs)
        m_tabs->Update();

    m_pined_metric_table.Update();
}

void
ComputeTableView::Render()
{
    if(!m_compute_selection ||
       m_compute_selection->GetSelectedWorkload() == ComputeSelection::INVALID_SELECTION_ID)
    {
        ImGui::TextDisabled("Select a workload from the toolbar.");
        return;
    }

    if(m_compute_selection->GetSelectedKernel() == ComputeSelection::INVALID_SELECTION_ID)
    {
        ImGui::TextDisabled("Select a kernel from the toolbar.");
        return;
    }

    m_pined_metric_table.Render();

    if(m_tabs)
        m_tabs->Render();
}

void
ComputeTableView::RebuildTableDataCache()
{
    m_table_widgets.clear();

    auto&    model       = m_data_provider.ComputeModel();
    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();

    if(workload_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    const auto& workloads = model.GetWorkloads();
    if(!workloads.count(workload_id))
        return;

    const auto& workload = workloads.at(workload_id);
    for(const auto* cat : workload.available_metrics.ordered_categories)
    {
        for(const auto* tbl : cat->ordered_tables)
        {
            AddTable(cat->id, tbl);
        }
    }
}

void
ComputeTableView::AddTable(uint32_t category_id, const AvailableMetrics::Table* table)
{
    uint64_t     key = MetricId::GetTableKey(category_id, table->id);
    MetricTable& widget          = m_table_widgets[key];
    auto         pin_metric_func = [this, &widget](MetricId metric_id) {
        //widget.ChangePinState(metric_id);
        if(widget.IsMetricPined(metric_id))
        {
            m_pined_metric_table.AddRow(metric_id);
        }
        else
        {
            m_pined_metric_table.RemoveRow(metric_id);
            
        }
    };
    widget.SetPinMetricCallback(pin_metric_func);
    widget.SetToKernelTableCallback(m_set_to_kernel_table_callback);

    auto& model = m_data_provider.ComputeModel();
    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    if(kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    widget.Populate(*table, [&](uint32_t eid) {
        return model.GetMetricValue(m_client_id, kernel_id, category_id, table->id, eid);
    });

    if(!widget.Empty())
    {
        m_table_widgets[key] = std::move(widget);
    }
}

void
ComputeTableView::RenderCategory(const AvailableMetrics::Category& category)
{
    bool category_has_data = false;

    ImGui::BeginChild("scroll", ImVec2(-1, -1));
    for(const auto* table : category.ordered_tables)
    {
        uint64_t key = MetricId::GetTableKey(category.id, table->id);
        auto it = m_table_widgets.find(key);
        if(it == m_table_widgets.end())
            continue;

        category_has_data = true;
        it->second.Render();
    }

    if(!category_has_data)
    {
        ImGui::TextDisabled("No data available for this category.");
    }

    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
