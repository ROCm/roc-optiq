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

ComputeTableView::ComputeTableView(DataProvider&                     data_provider,
                                   std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto selection_event =
               std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() != selection_event->GetSourceId())
            {
                return;
            }
            RebuildTabs();
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto kernel_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto selection_event =
               std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() != selection_event->GetSourceId())
            {
                return;
            }
            FetchAllMetrics();
        }
    };

    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        kernel_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto fetched_event =
               std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() != fetched_event->GetSourceId())
            {
                return;
            }
            if(m_fetch_pending)
            {
                FetchAllMetrics();
            }
            RebuildTableDataCache();
        }
    };

    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    m_widget_name = GenUniqueName("ComputeTableView");

    if(m_compute_selection->GetSelectedWorkload() != ComputeSelection::INVALID_SELECTION_ID)
    {
        RebuildTabs();
        if(m_compute_selection->GetSelectedKernel() != ComputeSelection::INVALID_SELECTION_ID)
        {
            FetchAllMetrics();
        }
    }
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
    m_non_empty_tables.clear();
    m_data_provider.ComputeModel().ClearMetricValues(m_client_id);

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID) return;

    const auto& workloads = m_data_provider.ComputeModel().GetWorkloads();
    if(!workloads.count(workload_id)) return;

    const auto& workload = workloads.at(workload_id);
    m_tabs = std::make_shared<TabContainer>();
    m_tabs->SetAllowToolTips(false);
    for(const auto& cp : workload.available_metrics.tree)
    {
        const auto& cat    = cp.second;
        auto        widget = std::make_shared<RocCustomWidget>(
            [this, &cat]() { RenderCategory(cat); });
        m_tabs->AddTab({ cat.name, cat.name, widget, false });
    }
}

void
ComputeTableView::FetchAllMetrics()
{
    m_data_provider.ComputeModel().ClearMetricValues(m_client_id);
    m_non_empty_tables.clear();
    m_fetch_pending = false;

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID ||
       kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    const auto& workloads = m_data_provider.ComputeModel().GetWorkloads();
    if(!workloads.count(workload_id)) return;

    const auto& workload = workloads.at(workload_id);
    std::vector<uint32_t>                   kernel_ids = { kernel_id };
    std::vector<MetricsRequestParams::MetricID> metric_ids;
    for(const auto& cp : workload.available_metrics.tree)
        for(const auto& tp : cp.second.tables)
            metric_ids.push_back({ cp.first, tp.first, std::nullopt });

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
    if(m_tabs) m_tabs->Update();
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

    if(m_tabs) m_tabs->Render();
}

void
ComputeTableView::RebuildTableDataCache()
{
    m_non_empty_tables.clear();

    uint32_t workload_id = m_compute_selection->GetSelectedWorkload();
    uint32_t kernel_id   = m_compute_selection->GetSelectedKernel();
    if(workload_id == ComputeSelection::INVALID_SELECTION_ID ||
       kernel_id == ComputeSelection::INVALID_SELECTION_ID)
    {
        return;
    }

    const auto& workloads = m_data_provider.ComputeModel().GetWorkloads();
    if(!workloads.count(workload_id)) return;

    const auto& workload = workloads.at(workload_id);
    for(const auto& cp : workload.available_metrics.tree)
    {
        for(const auto& tp : cp.second.tables)
        {
            auto* entries = m_data_provider.ComputeModel().GetMetricValuesByTable(
                m_client_id, kernel_id, cp.first, tp.first);
            if(entries && !entries->empty())
            {
                TableKey key{};
                key.fields.category_id = cp.first;
                key.fields.table_id    = tp.first;
                m_non_empty_tables.insert(key.id);
            }
        }
    }
}

void
ComputeTableView::RenderCategory(const AvailableMetrics::Category& cat)
{
    auto&    model     = m_data_provider.ComputeModel();
    uint32_t kernel_id = m_compute_selection->GetSelectedKernel();

    bool category_has_data = false;

    ImGui::BeginChild("scroll", ImVec2(-1, -1));
    for(const auto& tbl_pair : cat.tables)
    {
        uint32_t    tbl_id = tbl_pair.first;
        const auto& tbl    = tbl_pair.second;

        TableKey key{};
        key.fields.category_id = cat.id;
        key.fields.table_id    = tbl_id;
        if(m_non_empty_tables.find(key.id) == m_non_empty_tables.end())
            continue;

        category_has_data = true;

        int value_columns = std::max(1, static_cast<int>(tbl.value_names.size()));
        int num_columns   = 1 + value_columns + 1;  // Metric + values + Unit

        ImGui::SeparatorText(tbl.name.c_str());
        if(ImGui::BeginTable("##t", num_columns, ImGuiTableFlags_Borders))
        {
            ImGui::TableSetupColumn("Metric");
            if(tbl.value_names.empty())
            {
                ImGui::TableSetupColumn("Value");
            }
            else
            {
                for(const auto& vn : tbl.value_names)
                    ImGui::TableSetupColumn(vn.c_str());
            }
            ImGui::TableSetupColumn("Unit");
            ImGui::TableHeadersRow();

            for(const auto& entry_pair : tbl.entries)
            {
                uint32_t    eid   = entry_pair.first;
                const auto& entry = entry_pair.second;

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(entry.name.c_str());

                if(tbl.value_names.empty())
                {
                    ImGui::TableNextColumn();
                    auto mv = model.GetMetricValue(
                        m_client_id, kernel_id, cat.id, tbl_id, eid);
                    if(mv && mv->entry && !mv->values.empty())
                    {
                        ImGui::Text("%.2f", mv->values.begin()->second);
                    }
                    else
                    {
                        ImGui::TextDisabled("N/A");
                    }
                }
                else
                {
                    for(const auto& vn : tbl.value_names)
                    {
                        ImGui::TableNextColumn();
                        auto mv = model.GetMetricValue(
                            m_client_id, kernel_id, cat.id, tbl_id, eid);
                        if(mv && mv->entry && mv->values.count(vn))
                        {
                            ImGui::Text("%.2f", mv->values.at(vn));
                        }
                        else
                        {
                            ImGui::TextDisabled("N/A");
                        }
                    }
                }

                ImGui::TableNextColumn();
                if(!entry.unit.empty())
                {
                    ImGui::TextUnformatted(entry.unit.c_str());
                }
                else
                {
                    ImGui::TextDisabled("N/A");
                }
            }
            ImGui::EndTable();
        }
    }

    if(!category_has_data)
    {
        ImGui::TextDisabled("No data available for this category.");
    }

    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
