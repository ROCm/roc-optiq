// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_kernel_details.h"
#include "rocprofvis_data_provider.h"
#include <imgui.h>

namespace RocProfVis
{
namespace View
{

ComputeKernelDetailsView::ComputeKernelDetailsView(DataProvider& data_provider)
: RocWidget()
, m_data_provider(data_provider)
, m_memory_chart(data_provider)
, m_memory_chart_fetched(false)
{}

ComputeKernelDetailsView::~ComputeKernelDetailsView() {}

void
ComputeKernelDetailsView::Update()
{
    if(!m_memory_chart_fetched)
    {
        const std::unordered_map<uint32_t, WorkloadInfo>& workloads =
            m_data_provider.ComputeModel().GetWorkloads();
        if(!workloads.empty())
        {
            const WorkloadInfo& workload = workloads.begin()->second;
            std::vector<uint32_t> kernel_ids;
            for(const std::pair<const uint32_t, KernelInfo>& kernel : workload.kernels)
            {
                kernel_ids.push_back(kernel.second.id);
            }
            m_memory_chart.FetchMemChartMetrics(workload.id, kernel_ids);
            m_memory_chart_fetched = true;
        }
    }

    m_memory_chart.Update();
}

void
ComputeKernelDetailsView::Render()
{
    ImGui::Text("Kernel Details");
    m_memory_chart.Render();
}

}  // namespace View
}  // namespace RocProfVis
