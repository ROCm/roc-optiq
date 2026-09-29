// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
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

    void RenderProfilingConfig(const WorkloadInfo& workload_info);
    void RenderSystemInfo(const WorkloadInfo& workload_info);

    void RenderUnavailableMessage(const char* label);

    std::unique_ptr<HSplitContainer> m_content_container;

    const WorkloadInfo* m_workload_info;
    bool                m_follow_selection;
    uint32_t            m_shown_workload_id;  // Used while not following the selection.

    friend struct ComputeWorkloadViewTestPeer;
};

}  // namespace View
}  // namespace RocProfVis
