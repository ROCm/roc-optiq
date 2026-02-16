// Copyright Advanced Micro Devices, Inc.
// SPDX-License-Identifier: MIT

#pragma once
#include "widgets/rocprofvis_widget.h"
#include "widgets/rocprofvis_split_containers.h"

namespace RocProfVis
{
namespace View
{

class ComputeDataProvider;
class ComputeTableLegacy;
class ComputePlotBarLegacy;
class ComputePlotPieLegacy;

class ComputeSummaryViewLegacy : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeSummaryViewLegacy(std::string owner_id, std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeSummaryViewLegacy(); 

private:
    void RenderMenuBar();
    void RenderLeftColumn();
    void RenderRightColumn();

    std::shared_ptr<HSplitContainer> m_container;
    std::shared_ptr<RocCustomWidget> m_left_column;
    std::shared_ptr<RocCustomWidget> m_right_column;
    std::string m_owner_id;

    std::unique_ptr<ComputeTableLegacy> m_sysinfo_table;
    std::unique_ptr<ComputePlotPieLegacy> m_kernel_pie;
    std::unique_ptr<ComputePlotBarLegacy> m_kernel_bar;
    std::unique_ptr<ComputeTableLegacy> m_kernel_table;
    std::unique_ptr<ComputeTableLegacy> m_dispatch_table;
};

}  // namespace View
}  // namespace RocProfVis
