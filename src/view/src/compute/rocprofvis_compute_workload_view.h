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
struct AnalysisInfo;
struct WorkloadInfo;

class ComputeWorkloadView : public RocWidget
{
public:
    static constexpr const char* TAB_ID = "compute_workload_view";

    static TabItem CreateTabItem(
        DataProvider& data_provider,
        const std::shared_ptr<ComputeSelection>& compute_selection);

    ComputeWorkloadView(DataProvider&                     data_provider,
                        std::shared_ptr<ComputeSelection> compute_selection);
    ~ComputeWorkloadView();

    void Render() override;
    void Update() override;

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

    friend struct ComputeWorkloadViewTestPeer;
};

}  // namespace View
}  // namespace RocProfVis
