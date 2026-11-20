// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "rocprofvis_compute_kernel_details_panes.h"
#include "widgets/rocprofvis_widget.h"
#include <cstdint>
#include <memory>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
class HSplitContainer;
struct AnalysisInfo;
struct WorkloadInfo;

// The Workload Details panel of Kernel Details: system information and profiling
// configuration of the selected kernel's workload, or of a compare target's.
class ComputeWorkloadView : public RocWidget
{
public:
    ComputeWorkloadView(DataProvider&                     data_provider,
                        std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeWorkloadView();

    void Render() override;
    void Update() override;

    // Show this workload (e.g. a compare target's) instead of the selected
    // kernel's until FollowSelection().
    void ShowWorkload(uint32_t workload_id);
    void FollowSelection();

protected:
    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;

    void CreateLayout();

    void RenderAnalysisInfo(const AnalysisInfo& analysis_info);
    void RenderProfilingConfig(const WorkloadInfo& workload_info);
    void RenderSystemInfo(const WorkloadInfo& workload_info);

    void RenderInfoRow(int row_id, const char* name, const char* value);
    void RenderUnavailableMessage(const char* label);

    std::unique_ptr<HSplitContainer> m_content_container;

    const WorkloadInfo* m_workload_info;
    bool                m_follow_selection;
    uint32_t            m_shown_workload_id;  // Used while not following the selection.

    friend struct ComputeWorkloadViewTestPeer;
};

// The Kernel Details panel of the workload details of A's or B's workload.
class ComputeWorkloadPane : public ComputeSidedPane
{
public:
    ComputeWorkloadPane(const IKernelCompare& compare, DataProvider& data_provider,
                        std::shared_ptr<ComputeSelection> compute_selection);

    const char* Title() const override;
    void        RenderBody(bool maximized) override;
    void        Update() override;
    void        ApplyCompare(bool target_changed) override;

    friend struct ComputeKernelDetailsViewTestPeer;

protected:
    const char* ShowATooltip() const override;
    const char* ShowBTooltip() const override;

private:
    std::shared_ptr<ComputeSelection>    m_compute_selection;
    std::shared_ptr<ComputeWorkloadView> m_workload_view;
};

}  // namespace View
}  // namespace RocProfVis
