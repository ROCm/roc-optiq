// Copyright (C) 2025 Advanced Micro Devices, Inc. All rights reserved.

#include "rocprofvis_compute_roofline.h"
#include "widgets/rocprofvis_compute_widget.h"

namespace RocProfVis
{
namespace View
{

ComputeRooflineView::ComputeRooflineView(std::string owner_id, std::shared_ptr<ComputeDataProvider> data_provider) 
: m_roofline_fp64(nullptr)
, m_roofline_fp32(nullptr)
, m_roofline_fp16(nullptr)
, m_roofline_i8(nullptr)
, m_group_mode(ComputePlotRoofline::GroupModeKernel)
, m_owner_id(owner_id)
{
    m_roofline_fp64 = std::make_unique<ComputePlotRoofline>(data_provider, kRPVControllerComputePlotTypeRooflineFP64);
    m_roofline_fp32 = std::make_unique<ComputePlotRoofline>(data_provider, kRPVControllerComputePlotTypeRooflineFP32);
    m_roofline_fp16 = std::make_unique<ComputePlotRoofline>(data_provider, kRPVControllerComputePlotTypeRooflineFP16);
    m_roofline_i8 = std::make_unique<ComputePlotRoofline>(data_provider, kRPVControllerComputePlotTypeRooflineINT8);
}

ComputeRooflineView::~ComputeRooflineView() {}

void ComputeRooflineView::Update()
{
    if (m_roofline_fp64)
    {
        m_roofline_fp64->Update();
    }
    if (m_roofline_fp32)
    {
        m_roofline_fp32->Update();
    }
    if (m_roofline_fp16)
    {
        m_roofline_fp16->Update();
    }
    if (m_roofline_i8)
    {
        m_roofline_i8->Update();
    }
}

void ComputeRooflineView::RenderMenuBar()
{
    ImVec2 cursor_position = ImGui::GetCursorScreenPos();
    ImVec2 content_region = ImGui::GetContentRegionAvail();
    ImGui::GetWindowDrawList()->AddRectFilled(
    ImVec2(cursor_position.x, cursor_position.y),
    ImVec2(cursor_position.x + content_region.x,
            cursor_position.y + ImGui::GetFrameHeightWithSpacing()),
    ImGui::ColorConvertFloat4ToU32(ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg]), 
    0.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_MenuBarBg));
    if (ImGui::Button("View"))
    {
        ImGui::OpenPopup("compute_view_menu");
    }
    ImGui::PopStyleColor();
    ImGui::SetNextWindowPos(ImGui::GetCursorScreenPos());
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    if (ImGui::BeginPopup("compute_view_menu"))
    {
        if (ImGui::MenuItem("Group by Dispatch", "", m_group_mode == ComputePlotRoofline::GroupModeDispatch))
        {
            m_roofline_fp64->SetGroupMode(ComputePlotRoofline::GroupModeDispatch);
            m_roofline_fp32->SetGroupMode(ComputePlotRoofline::GroupModeDispatch);
            m_roofline_fp16->SetGroupMode(ComputePlotRoofline::GroupModeDispatch);
            m_roofline_i8->SetGroupMode(ComputePlotRoofline::GroupModeDispatch);
            m_group_mode = ComputePlotRoofline::GroupModeDispatch;
        }
        if(ImGui::MenuItem("Group by Kernel", "", m_group_mode == ComputePlotRoofline::GroupModeKernel))
        {
            m_roofline_fp64->SetGroupMode(ComputePlotRoofline::GroupModeKernel);
            m_roofline_fp32->SetGroupMode(ComputePlotRoofline::GroupModeKernel);
            m_roofline_fp16->SetGroupMode(ComputePlotRoofline::GroupModeKernel);
            m_roofline_i8->SetGroupMode(ComputePlotRoofline::GroupModeKernel);
            m_group_mode = ComputePlotRoofline::GroupModeKernel;
        }

        ImGui::EndPopup();
    }
    ImGui::PopStyleVar(2);
}

void ComputeRooflineView::Render()
{
    RenderMenuBar();

    ImGui::BeginChild("compute_roofline", ImVec2(-1, -1), ImGuiChildFlags_Borders);
    if (m_roofline_fp64)
    {
        m_roofline_fp64->Render();
    }
    if (m_roofline_fp32)
    {
        m_roofline_fp32->Render();
    }
    if (m_roofline_fp16)
    {
        m_roofline_fp16->Render();
    }
    if (m_roofline_i8)
    {
        m_roofline_i8->Render();
    }
    ImGui::EndChild();
}

}  // namespace View
}  // namespace RocProfVis
