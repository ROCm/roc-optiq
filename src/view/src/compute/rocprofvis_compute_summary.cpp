// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_summary.h"
#include "rocprofvis_compute_roofline.h"
#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"

namespace RocProfVis
{
namespace View
{

ComputeSummaryView::ComputeSummaryView(
    DataProvider& data_provider, std::shared_ptr<ComputeSelection> compute_selection)
: RocWidget()
, m_data_provider(data_provider)
, m_roofline(nullptr)
, m_compute_selection(compute_selection)
, m_client_id(IdGenerator::GetInstance().GenerateId())
{
    auto workload_changed_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto selection_changed_event =
               std::dynamic_pointer_cast<ComputeSelectionChangedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() !=
               selection_changed_event->GetSourceId())
            {
                return;
            }
            if(m_roofline)
            {
                m_roofline->SetWorkload(selection_changed_event->GetId());
            }
        }
    };

    m_workload_selection_changed_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        workload_changed_handler);

    auto metrics_fetched_handler = [this](std::shared_ptr<RocEvent> e) {
        if(auto metrics_fetched_event =
               std::dynamic_pointer_cast<ComputeMetricsFetchedEvent>(e))
        {
            if(m_data_provider.GetTraceFilePath() != metrics_fetched_event->GetSourceId())
            {
                return;
            }
        }
    };

    m_metrics_fetched_token = EventManager::GetInstance()->Subscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), metrics_fetched_handler);

    m_roofline = std::make_unique<Roofline>(m_data_provider);

    m_widget_name = GenUniqueName("ComputeSummaryView");
}

ComputeSummaryView::~ComputeSummaryView() {
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged),
        m_workload_selection_changed_token);
    EventManager::GetInstance()->Unsubscribe(
        static_cast<int>(RocEvents::kComputeMetricsFetched), m_metrics_fetched_token);
}

void
ComputeSummaryView::Update()
{
    if(m_roofline)
    {
        m_roofline->Update();
    }
}

void
ComputeSummaryView::Render()
{
    ImGui::BeginChild("summary");
    if(m_roofline)
    {
        m_roofline->Render();
    }
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
