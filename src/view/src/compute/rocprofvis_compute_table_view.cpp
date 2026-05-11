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

constexpr const char* JSON_KEY_PINNED_METRICS    = "pins";
constexpr const char* JSON_KEY_PINNED_METRICS_ID = "id";

ComputeTableView::ComputeTableView(DataProvider&                     data_provider,
                                   std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
, m_pinned_metric_table(data_provider, compute_selection, m_client_id)
{
    m_pinned_metric_table.SetPinMetricCallback([this](MetricId metric_id) {
        m_pinned_metrics.erase(metric_id);
        auto table_it = m_table_widgets.find(metric_id.GetTableKey());
        if(table_it != m_table_widgets.end())
        {
            table_it->second.ChangePinState(metric_id);
        }
        m_pinned_metric_table.RefillTable(m_pinned_metrics);
    });

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
        m_pinned_metric_table.RefillTable(m_pinned_metrics);
    };

    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    m_widget_name = GenUniqueName("ComputeTableView");
    m_preset = std::make_unique<Preset>(*this);
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
    // GetActiveTab() returns null until the container has rendered at least
    // one frame, so guard against a workload change firing before then.
    if(m_tabs && m_tabs->GetActiveTab())
        m_active_tab_id = m_tabs->GetActiveTab()->m_id;
    m_tabs.reset();
    m_table_widgets.clear();
    m_data_provider.ComputeModel().ClearKernelMetricValues(m_client_id);

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID)
        return;

    const WorkloadInfo* workload =
        m_data_provider.ComputeModel().GetWorkload(workload_id);
    if(!workload)
        return;

    m_tabs = std::make_shared<TabContainer>();
    m_tabs->SetAllowToolTips(true);
    for(const auto* cat : workload->available_metrics.ordered_categories)
    {
        auto widget = std::make_shared<RocCustomWidget>(
            [this, cat]() { RenderCategory(*cat); });
        m_tabs->AddTab({ cat->name, cat->name, widget, false });
    }
    m_tabs->SetActiveTab(m_active_tab_id);
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

    const WorkloadInfo* workload =
        m_data_provider.ComputeModel().GetWorkload(workload_id);
    if(!workload)
        return;

    std::vector<uint32_t>                   kernel_ids = { kernel_id };
    std::vector<MetricsRequestParams::MetricID> metric_ids;
    for(const auto* cat : workload->available_metrics.ordered_categories)
    {
        for(const auto* tbl : cat->ordered_tables)
            metric_ids.push_back({ cat->id, tbl->id, std::nullopt });
    }

    bool success = m_data_provider.FetchMetrics(
        MetricsRequestParams(workload->id, kernel_ids, metric_ids, m_client_id));

    if(!success)
    {
        m_fetch_pending = true;
    }
}

void
ComputeTableView::Update()
{
    m_pinned_metric_table.Update();

    if(m_tabs)
        m_tabs->Update();
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

    m_pinned_metric_table.Render();

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

    const WorkloadInfo* workload = model.GetWorkload(workload_id);
    if(!workload)
        return;

    for(const auto* cat : workload->available_metrics.ordered_categories)
    {
        for(const auto* tbl : cat->ordered_tables)
        {
            AddTable(cat->id, tbl);
        }
    }

    RestoreMetricPining();
}

void
ComputeTableView::AddTable(uint32_t category_id, const AvailableMetrics::Table* table)
{
    uint64_t     key = MetricId::GetTableKey(category_id, table->id);
    auto         [it, inserted]  =
        m_table_widgets.try_emplace(key, m_data_provider.GetTraceFilePath());
    MetricTable& widget          = it->second;
    auto pin_metric_func = [this, &widget](MetricId metric_id) {
        if (widget.IsMetricPinned(metric_id))
        {
            m_pinned_metrics.insert(metric_id);
        }
        else
        {
            m_pinned_metrics.erase(metric_id);
        }
        m_pinned_metric_table.RefillTable(m_pinned_metrics);
    };
    widget.SetPinMetricCallback(pin_metric_func);

    auto& model = m_data_provider.ComputeModel();
    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();
    if(kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    widget.Populate(*table, [&](uint32_t eid) {
        return model.GetKernelMetricValue(m_client_id, kernel_id, category_id, table->id,
                                          eid);
    });
}

void
ComputeTableView::RestoreMetricPining()
{
    for(MetricId id : m_pinned_metrics)
    {
        auto table_it = m_table_widgets.find(id.GetTableKey());
        if(table_it != m_table_widgets.end())
        {
            table_it->second.ChangePinState(id);
        }
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

ComputeTableView::Preset::Preset(ComputeTableView& widget)
: PresetComponent(PresetManager::ComputeTableView,
                  widget.m_data_provider.GetTraceFilePath())
, m_widget(widget)
{}

bool
ComputeTableView::Preset::ToJson(jt::Json& json)
{
    if(!m_widget.m_pinned_metrics.empty())
    {
        jt::Json& pins = json[JSON_KEY_PINNED_METRICS];
        int       i    = 0;
        for(const MetricId& id : m_widget.m_pinned_metrics)
        {
            pins[i][JSON_KEY_PINNED_METRICS_ID][0] = id.category_id;
            pins[i][JSON_KEY_PINNED_METRICS_ID][1] = id.table_id;
            pins[i][JSON_KEY_PINNED_METRICS_ID][2] = id.entry_id;
            i++;
        }
    }
    return true;
}

bool
ComputeTableView::Preset::FromJson(jt::Json& json)
{
    bool result = true;
    if(json.isObject() && json.contains(JSON_KEY_PINNED_METRICS))
    {
        jt::Json& pins = json[JSON_KEY_PINNED_METRICS];
        if(pins.isArray())
        {
            for(jt::Json& obj : pins.getArray())
            {
                result &= obj.isObject() && obj.contains(JSON_KEY_PINNED_METRICS_ID) &&
                          obj[JSON_KEY_PINNED_METRICS_ID].isArray() &&
                          obj[JSON_KEY_PINNED_METRICS_ID].getArray().size() == 3 &&
                          obj[JSON_KEY_PINNED_METRICS_ID].getArray()[0].isLong() &&
                          obj[JSON_KEY_PINNED_METRICS_ID].getArray()[1].isLong() &&
                          obj[JSON_KEY_PINNED_METRICS_ID].getArray()[2].isLong();
            }
            if(result)
            {
                Reset();
                for(jt::Json& obj : pins.getArray())
                {
                    MetricId id;
                    id.category_id = static_cast<uint32_t>(
                        obj[JSON_KEY_PINNED_METRICS_ID].getArray()[0].getLong());
                    id.table_id = static_cast<uint32_t>(
                        obj[JSON_KEY_PINNED_METRICS_ID].getArray()[1].getLong());
                    id.entry_id = static_cast<uint32_t>(
                        obj[JSON_KEY_PINNED_METRICS_ID].getArray()[2].getLong());
                    m_widget.m_pinned_metrics.insert(std::move(id));
                }
                m_widget.RestoreMetricPining();
                m_widget.m_pinned_metric_table.RefillTable(m_widget.m_pinned_metrics);
            }
        }
    }
    return result;
}

void
ComputeTableView::Preset::Reset()
{
    for(const MetricId& id : m_widget.m_pinned_metrics)
    {
        if(m_widget.m_table_widgets.count(id.GetTableKey()) > 0)
        {
            m_widget.m_table_widgets.at(id.GetTableKey()).ChangePinState(id);
        }
    }
    m_widget.m_pinned_metrics.clear();
    m_widget.m_pinned_metric_table.RefillTable(m_widget.m_pinned_metrics);
}

}  // namespace View
}  // namespace RocProfVis
