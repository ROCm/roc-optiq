// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_details.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"

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
    // Add event listener for selection changes to trigger metric fetch for the newly
    // selected kernel
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto selection_changed_event =
               std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() !=
               selection_changed_event->GetSourceId())
            {
                return;
            }
            // TODO: fetch pivot table data
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto kernel_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto selection_changed_event =
               std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() !=
               selection_changed_event->GetSourceId())
            {
                return;
            }
            m_memory_chart.FetchMemChartMetrics();
            if(m_roofline)
            {
                m_roofline->SetWorkload(m_compute_selection->GetSelectedWorkload());
                m_roofline->SetKernel(selection_changed_event->GetId());
            }
        }
    };

    m_kernel_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeKernelSelectionChanged),
        kernel_changed_handler);

    // subscribe to fetch metrics event
    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto metrics_fetched_event =
               std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() != metrics_fetched_event->GetSourceId())
            {
                return;
            }

            if(m_memory_chart.GetClientId() == metrics_fetched_event->GetClientId())
            {
                m_memory_chart.UpdateMetrics();
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
    if(m_roofline)
    {
        m_roofline->Render();
    }
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
