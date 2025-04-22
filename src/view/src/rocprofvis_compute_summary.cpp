// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_summary.h"

namespace RocProfVis
{
namespace View
{

void ComputeSummaryLeft::Update()
{
    if (m_system_info)
    {
        m_system_info->Update();
    }
}

void ComputeSummaryLeft::Render()
{
    if (m_system_info)
    {
        m_system_info->Render();
    }
}

ComputeSummaryLeft::ComputeSummaryLeft(std::shared_ptr<ComputeDataProvider> data_provider)
: m_system_info(nullptr)
{
    m_system_info = std::make_shared<ComputeMetric>(data_provider, "1.1.csv");
}

ComputeSummaryLeft::~ComputeSummaryLeft() {}

void ComputeSummaryRight::Update()
{
    if (m_kernel_list)
    {
        m_kernel_list->Update();
    }
    if (m_dispatch_list)
    {
        m_dispatch_list->Update();
    }
}

void ComputeSummaryRight::Render()
{    
    if (m_kernel_list)
    {
        m_kernel_list->Render();
    }
    if (m_dispatch_list)
    {
        m_dispatch_list->Render();
    }
}

ComputeSummaryRight::ComputeSummaryRight(std::shared_ptr<ComputeDataProvider> data_provider)
: m_kernel_list(nullptr)
, m_dispatch_list(nullptr)
{
    m_kernel_list = std::make_shared<ComputeMetric>(data_provider, "0.1_Top_Kernels.csv", kComputeMetricTable | kComputeMetricPie | kComputeMetricBar);
    m_dispatch_list = std::make_shared<ComputeMetric>(data_provider, "0.2_Dispatch_List.csv");
}

ComputeSummaryRight::~ComputeSummaryRight() {}

ComputeSummaryView::ComputeSummaryView(std::shared_ptr<ComputeDataProvider> data_provider)
: m_container(nullptr)
, m_left_view(nullptr)
, m_right_view(nullptr)
{
    m_left_view = std::make_shared<ComputeSummaryLeft>(data_provider);
    m_right_view = std::make_shared<ComputeSummaryRight>(data_provider);

    LayoutItem left;
    left.m_item = m_left_view;
    LayoutItem right;
    right.m_item = m_right_view;
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

void ComputeSummaryView::Update()
{
    if (m_left_view)
    {
        m_left_view->Update();
    }
    if (m_right_view)
    {
        m_right_view->Update();
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
