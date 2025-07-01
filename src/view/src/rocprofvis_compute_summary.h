// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#pragma once
#include "widgets/rocprofvis_widget.h"

namespace RocProfVis
{
namespace View
{

class ComputeDataProvider;
class ComputeTable;
class ComputePlotBar;
class ComputePlotPie;

class ComputeSummaryView : public RocWidget
{
public:
    void Render();
    void Update();
    ComputeSummaryView(std::string owner_id, std::shared_ptr<ComputeDataProvider> data_provider);
    ~ComputeSummaryView(); 

private:
    void RenderMenuBar();
    void RenderLeftColumn();
    void RenderRightColumn();

    std::shared_ptr<HSplitContainer> m_container;
    std::shared_ptr<RocCustomWidget> m_left_column;
    std::shared_ptr<RocCustomWidget> m_right_column;
    std::string m_owner_id;

    std::unique_ptr<ComputeTable> m_sysinfo_table;
    std::unique_ptr<ComputePlotPie> m_kernel_pie;
    std::unique_ptr<ComputePlotBar> m_kernel_bar;
    std::unique_ptr<ComputeTable> m_kernel_table;
    std::unique_ptr<ComputeTable> m_dispatch_table;
};

}  // namespace View
}  // namespace RocProfVis
