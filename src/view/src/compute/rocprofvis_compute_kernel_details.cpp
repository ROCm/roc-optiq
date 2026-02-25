// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_details.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"
#include "rocprofvis_requests.h"
#include "model/compute/rocprofvis_compute_data_model.h"

#include "imgui.h"

namespace RocProfVis
{
namespace View
{

ComputeKernelDetailsView::ComputeKernelDetailsView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_memory_chart(data_provider, compute_selection)
, m_roofline(nullptr)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            m_sol_table = {};
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto kernel_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            m_memory_chart.FetchMemChartMetrics();
            if(m_roofline)
            {
                m_roofline->SetWorkload(m_compute_selection->GetSelectedWorkload());
                m_roofline->SetKernel(evt->GetId());
            }
        }
    };

    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        kernel_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        auto evt = std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e);
        if(evt && evt->GetSourceId() == m_data_provider.GetTraceFilePath())
        {
            if(m_memory_chart.GetClientId() == evt->GetClientId())
            {
                m_memory_chart.UpdateMetrics();
                FetchSOLMetrics();
            }
            if(m_client_id == evt->GetClientId())
            {
                UpdateSOLTable();
            }
        }
    };

    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    m_roofline = std::make_unique<RocProfVis::View::Roofline>(data_provider);

    m_widget_name = GenUniqueName("ComputeKernelDetailsView");
}

ComputeKernelDetailsView::~ComputeKernelDetailsView()
{
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        m_kernel_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), m_metrics_fetched_token);
}

void
ComputeKernelDetailsView::Update()
{
    if(m_roofline)
    {
        m_roofline->Update();
    }
}

void
ComputeKernelDetailsView::Render()
{
    ImGui::BeginChild("kernel_details");
    ImGui::Text("Memory Chart");
    m_memory_chart.Render();
    m_sol_table.Render();
    if(m_roofline)
    {
        m_roofline->Render();
    }
    ImGui::EndChild();
}

void
ComputeKernelDetailsView::FetchSOLMetrics()
{
    m_sol_table = {};
    m_data_provider.ComputeModel().ClearMetricValues(m_client_id);

    uint32_t wid = m_compute_selection->GetSelectedWorkload();
    uint32_t kid = m_compute_selection->GetSelectedKernel();
    if(wid == ComputeSelection::INVALID_SELECTION_ID ||
       kid == ComputeSelection::INVALID_SELECTION_ID)
        return;

    m_data_provider.FetchMetrics(MetricsRequestParams(
        wid, {kid}, {{2, 1, std::nullopt}}, m_client_id));
}

void
ComputeKernelDetailsView::UpdateSOLTable()
{
    uint32_t wid = m_compute_selection->GetSelectedWorkload();
    uint32_t kid = m_compute_selection->GetSelectedKernel();
    if(wid == ComputeSelection::INVALID_SELECTION_ID ||
       kid == ComputeSelection::INVALID_SELECTION_ID)
        return;

    auto& model = m_data_provider.ComputeModel();
    if(!model.GetWorkloads().count(wid)) return;

    const auto& tree = model.GetWorkloads().at(wid).available_metrics.tree;
    if(!tree.count(2) || !tree.at(2).tables.count(1)) return;

    m_sol_table.Populate(tree.at(2).tables.at(1), [&](uint32_t eid) {
        return model.GetMetricValue(m_client_id, kid, 2, 1, eid);
    });
}

}  // namespace View
}  // namespace RocProfVis
