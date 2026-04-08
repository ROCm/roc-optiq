// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#include "rocprofvis_compute_selection.h"
#include "rocprofvis_data_provider.h"
#include "rocprofvis_event_manager.h"

namespace RocProfVis
{
namespace View
{

ComputeSelection::ComputeSelection(DataProvider& dp)
: m_selected_workload_id(INVALID_SELECTION_ID)
, m_selected_kernel_id(INVALID_SELECTION_ID)
, m_data_provider(dp)
{}

void
ComputeSelection::SelectKernel(uint32_t kernel_id)
{
    if(kernel_id == m_selected_kernel_id)
    {
        return;
    }
    m_selected_kernel_id = kernel_id;
    SendKernelSelectionChanged();
}

void
ComputeSelection::SelectWorkload(uint32_t workload_id)
{
    if(workload_id == m_selected_workload_id)
    {
        return;
    }
    m_selected_workload_id = workload_id;
    SendWorkloadSelectionChanged();

    // reset kernel selection when workload changes
    // select first kernel of the workload by default
    const std::vector<const KernelInfo*> kernel_info_list =
        m_data_provider.ComputeModel().GetKernelInfoList(workload_id);
    if(!kernel_info_list.empty())
    {
        SelectKernel(kernel_info_list[0]->id);
    }
    else
    {
        SelectKernel(INVALID_SELECTION_ID);
    }
}

uint32_t
ComputeSelection::GetSelectedWorkload() const
{
    return m_selected_workload_id;
}

uint32_t
ComputeSelection::GetSelectedKernel() const
{
    return m_selected_kernel_id;
}

void
ComputeSelection::SendWorkloadSelectionChanged()
{
    EventManager::GetInstance()->AddEvent(std::make_shared<ComputeSelectionChangedEvent>(
    static_cast<int>(RocEvents::kComputeWorkloadSelectionChanged), m_selected_workload_id,
        m_data_provider.GetTraceFilePath()));
}

void
ComputeSelection::SendKernelSelectionChanged()
{
    EventManager::GetInstance()->AddEvent(std::make_shared<ComputeSelectionChangedEvent>(
    static_cast<int>(RocEvents::kComputeKernelSelectionChanged), m_selected_kernel_id,
        m_data_provider.GetTraceFilePath()));
}

}  // namespace View
}  // namespace RocProfVis
