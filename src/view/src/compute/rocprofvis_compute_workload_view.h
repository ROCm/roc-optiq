// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "widgets/rocprofvis_widget.h"
#include <memory>

namespace RocProfVis
{
namespace View
{

class DataProvider;
class ComputeSelection;
class HSplitContainer;
struct WorkloadInfo;

class ComputeWorkloadView : public RocWidget
{
public:
    ComputeWorkloadView(DataProvider&                     data_provider,
                        std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeWorkloadView();

    void Render() override;
    void Update() override;

protected:
    DataProvider&                     m_data_provider;
    std::shared_ptr<ComputeSelection> m_compute_selection;

    void CreateLayout();

    void RenderProfilingConfig(const WorkloadInfo& workload_info);
    void RenderSystemInfo(const WorkloadInfo& workload_info);

    void RenderUnavailableMessage(const char* label);

    std::unique_ptr<HSplitContainer> m_content_container;

    const WorkloadInfo* m_workload_info;
};

}  // namespace View
}  // namespace RocProfVis
