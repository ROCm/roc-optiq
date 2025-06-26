// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_summary.h"
#include "widgets/rocprofvis_compute_widget.h"

namespace RocProfVis
{
namespace View
{

constexpr ImVec2 ITEM_SPACING_DEFAULT = ImVec2(8, 4);

ComputeSummaryView::ComputeSummaryView(std::string owner_id, std::shared_ptr<ComputeDataProvider2> data_provider)
: m_container(nullptr)
, m_left_column(nullptr)
, m_right_column(nullptr)
, m_owner_id(owner_id)
, m_sysinfo_table(nullptr)
, m_kernel_pie(nullptr)
, m_kernel_bar(nullptr)
, m_kernel_table(nullptr)
, m_dispatch_table(nullptr)
{
    m_sysinfo_table = std::make_unique<ComputeTable>(data_provider, kRPVControllerComputeTableTypeSysInfo);
    m_kernel_pie = std::make_unique<ComputePlotPie>(data_provider, kRPVControllerComputePlotTypeKernelDurationPercentage);
    m_kernel_bar = std::make_unique<ComputePlotBar>(data_provider, kRPVControllerComputePlotTypeKernelDuration);
    m_kernel_table = std::make_unique<ComputeTable>(data_provider, kRPVControllerComputeTableTypeKernelList);
    m_dispatch_table = std::make_unique<ComputeTable>(data_provider, kRPVControllerComputeTableTypeDispatchList);

    m_left_column = std::make_shared<RocCustomWidget>([this]()
    {
        this->RenderLeftColumn();
    });
    m_right_column = std::make_shared<RocCustomWidget>([this]()
    {
        this->RenderRightColumn();
    });

    LayoutItem left;
    left.m_item = m_left_column;
    LayoutItem right;
    right.m_item = m_right_column;
    m_container = std::make_shared<HSplitContainer>(left, right);
    m_container->SetSplit(0.5f);
}

ComputeSummaryView::~ComputeSummaryView() {}

void ComputeSummaryView::RenderMenuBar()
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(
    ImVec2(cursor_position.x, cursor_position.y),
    ImVec2(cursor_position.x + content_region.x,
            cursor_position.y + ImGui::GetFrameHeightWithSpacing()),
    ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]), 
    0.0f);
    
    ImGui::Dummy(ImVec2(content_region.x, ImGui::GetFrameHeightWithSpacing()));
}

void ComputeSummaryView::RenderLeftColumn()
{
    if (m_sysinfo_table)
    {
        m_sysinfo_table->Render();
    }
}

void ComputeSummaryView::RenderRightColumn()
{
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ITEM_SPACING_DEFAULT);
    ImGui::SeparatorText("Top Stats");
    ImGui::PopStyleVar();
    if (m_kernel_pie)
    {
        m_kernel_pie->Render();
    }
    if (m_kernel_bar)
    {
        m_kernel_bar->Render();
    }
    if (m_kernel_table)
    {
        m_kernel_table->Render();
    }
    if (m_dispatch_table)
    {
        m_dispatch_table->Render();
    }
}

void ComputeSummaryView::Update()
{
    if (m_sysinfo_table)
    {
        m_sysinfo_table->Update();
    }
    if (m_kernel_pie)
    {
        m_kernel_pie->Update();
    }
    if (m_kernel_bar)
    {
        m_kernel_bar->Update();
    }
    if (m_kernel_table)
    {
        m_kernel_table->Update();
    }
    if (m_dispatch_table)
    {
        m_dispatch_table->Update();
    }
}

void ComputeSummaryView::Render()
{
    RenderMenuBar();
    if(m_container)
    {
        m_container->Render();
        return;
    }
}

}  // namespace View
}  // namespace RocProfVis
